// client.cpp (FULL)
// - Telemetry: smooth KB/s (EMA)
// - CRSF: freeze last valid RC, configurable failsafe base + stages (0=hold_last)
// - Stages are evaluated relative to FS entry (prevents jitter if link returns quickly)

#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdint>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <sys/resource.h>
#include <termios.h>

// ===== termios2 (ручний опис) =====
struct termios2 {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[19];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#ifndef BOTHER
#define BOTHER 0010000
#endif
#ifndef CBAUD
#define CBAUD 0010017
#endif
#ifndef TCGETS2
#define TCGETS2 _IOR('T', 0x2A, struct termios2)
#endif
#ifndef TCSETS2
#define TCSETS2 _IOW('T', 0x2B, struct termios2)
#endif

// ===== extern utils =====
extern int get_cpu_temperature();
extern void getSignalStrength(int &rssi, int &snr);
struct NetworkUsage { unsigned long rx_bytes, tx_bytes; };
extern NetworkUsage getNetworkUsage();

// ===== Config =====
static std::string hostname;
static int MAV_BAUD = 115200;
static int CONTROL_PORT = 2223;
static int CAM_INFO_PORT = 2224;
static int MAV_PORT = 14550;

// legacy keys (kept)
static int FAILSAFE_TIMEOUT = 5000;   // used as default LINK_LOST_MS if LINK_LOST_MS not set
static int LOCAL_TIMEOUT    = 300000;
static int ELRS_SWITCH_PIN  = 0;

// ===== FS new keys =====
static int FS_ENABLED        = 1;
static int LINK_LOST_MS      = -1;     // if -1 -> FAILSAFE_TIMEOUT
static int FS_STAGE1_AFTER_MS = -1;    // relative to FS entry
static int FS_STAGE2_AFTER_MS = -1;    // relative to FS entry

// 0 = hold_last
static int FAILSAFE_PWM_US[16] = {
    0,0,0,0,  0,0,0,0,
    0,0,0,0,  0,0,0,0
};

// stage overrides: -1 = not touched, 0 = unset back to base, >0 = pwm us
static int FS_STAGE1_OVR[16];
static int FS_STAGE2_OVR[16];

// ===== RT priority for CRSF =====
static void set_realtime_priority()
{
    struct sched_param param;
    param.sched_priority = 60; // НЕ 99
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        setpriority(PRIO_PROCESS, 0, -20);
    }
}

static std::string trim(const std::string& s)
{
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, (last - first + 1));
}

static inline int clamp_int(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

// ================== CRSF helpers (26 bytes RC frame) ==================
static constexpr int CRSF_US_MIN = 988;
static constexpr int CRSF_US_MAX = 2012;
static constexpr int CRSF_CH_MIN = 172;
static constexpr int CRSF_CH_MAX = 1811;

static uint8_t crsf_crc8_d5(const uint8_t* data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0xD5);
            else            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t pwm_us_to_crsf11(int us)
{
    us = clamp_int(us, CRSF_US_MIN, CRSF_US_MAX);
    int v = CRSF_CH_MIN + ((us - CRSF_US_MIN) * (CRSF_CH_MAX - CRSF_CH_MIN) + 512) / (CRSF_US_MAX - CRSF_US_MIN);
    v = clamp_int(v, CRSF_CH_MIN, CRSF_CH_MAX);
    return (uint16_t)v;
}

static uint16_t crsf11_to_pwm_us(uint16_t v)
{
    int iv = clamp_int((int)v, CRSF_CH_MIN, CRSF_CH_MAX);
    int us = CRSF_US_MIN + ((iv - CRSF_CH_MIN) * (CRSF_US_MAX - CRSF_US_MIN) + (CRSF_CH_MAX - CRSF_CH_MIN)/2)
             / (CRSF_CH_MAX - CRSF_CH_MIN);
    us = clamp_int(us, CRSF_US_MIN, CRSF_US_MAX);
    return (uint16_t)us;
}

static bool crsf_decode_rc_us(const uint8_t pkt[26], uint16_t ch_us[16], bool check_crc)
{
    if (pkt[0] != 0xC8) return false; // address
    if (pkt[1] != 24)   return false; // length
    if (pkt[2] != 0x16) return false; // type RC channels packed

    if (check_crc) {
        uint8_t crc = crsf_crc8_d5(&pkt[2], 23); // type+payload
        if (crc != pkt[25]) return false;
    }

    const uint8_t* p = &pkt[3]; // 22 bytes payload
    uint32_t bitbuf = 0;
    int bitlen = 0;
    int idx = 0;

    for (int ch = 0; ch < 16; ch++) {
        while (bitlen < 11) {
            bitbuf |= (uint32_t)p[idx++] << bitlen;
            bitlen += 8;
            if (idx > 22) return false;
        }
        uint16_t v11 = (uint16_t)(bitbuf & 0x7FF);
        bitbuf >>= 11;
        bitlen -= 11;
        ch_us[ch] = crsf11_to_pwm_us(v11);
    }
    return true;
}

static void crsf_encode_rc_us(const uint16_t ch_us[16], uint8_t pkt[26])
{
    pkt[0] = 0xC8;
    pkt[1] = 24;
    pkt[2] = 0x16;

    uint8_t payload[22] = {0};
    uint32_t bitbuf = 0;
    int bitlen = 0;
    int out = 0;

    for (int ch = 0; ch < 16; ch++) {
        uint16_t v11 = pwm_us_to_crsf11((int)ch_us[ch]);
        bitbuf |= (uint32_t)v11 << bitlen;
        bitlen += 11;

        while (bitlen >= 8 && out < 22) {
            payload[out++] = (uint8_t)(bitbuf & 0xFF);
            bitbuf >>= 8;
            bitlen -= 8;
        }
    }
    while (out < 22) {
        payload[out++] = (uint8_t)(bitbuf & 0xFF);
        bitbuf >>= 8;
        bitlen -= 8;
    }

    std::memcpy(&pkt[3], payload, 22);
    pkt[25] = crsf_crc8_d5(&pkt[2], 23);
}

// ================== FS parsing helpers ==================
static void fs_clear_ovr(int ovr[16]) {
    for (int i = 0; i < 16; i++) ovr[i] = -1;
}

static bool parse_csv16_int(const std::string& s, int out16[16])
{
    std::stringstream ss(s);
    std::string tok;
    int i = 0;
    while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (tok.empty()) continue;
        if (i >= 16) break;
        out16[i++] = std::stoi(tok);
    }
    return i == 16;
}

// format: "3:1600, 6:1500, 9:0"
static void parse_stage_set(const std::string& s, int ovr16[16])
{
    fs_clear_ovr(ovr16);
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (item.empty()) continue;
        size_t c = item.find(':');
        if (c == std::string::npos) continue;
        int ch = std::stoi(trim(item.substr(0, c)));
        int pwm = std::stoi(trim(item.substr(c + 1)));
        if (ch >= 1 && ch <= 16) ovr16[ch - 1] = pwm; // 0 allowed (unset)
    }
}

static void apply_stage(long fsElapsedMs,
                        int afterMs,
                        const int ovr[16],
                        const uint16_t baseOut[16],
                        uint16_t out[16])
{
    if (afterMs < 0) return;
    if (fsElapsedMs < afterMs) return;

    for (int i = 0; i < 16; i++) {
        if (ovr[i] == -1) continue;
        if (ovr[i] == 0) out[i] = baseOut[i]; // unset -> back to base
        else             out[i] = (uint16_t)clamp_int(ovr[i], CRSF_US_MIN, CRSF_US_MAX);
    }
}

static bool readConfig(const std::string &filename)
{
    fs_clear_ovr(FS_STAGE1_OVR);
    fs_clear_ovr(FS_STAGE2_OVR);

    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, pos));
        std::string val = trim(line.substr(pos + 1));

        if (key == "host") hostname = val;
        else if (key == "MAVLINK_BAUD") MAV_BAUD = std::stoi(val);
        else if (key == "CONTROL_PORT") CONTROL_PORT = std::stoi(val);
        else if (key == "CAM_INFO_PORT") CAM_INFO_PORT = std::stoi(val);
        else if (key == "FAILSAFE_TIMEOUT") FAILSAFE_TIMEOUT = std::stoi(val);
        else if (key == "LOCAL_TIMEOUT") LOCAL_TIMEOUT = std::stoi(val);
        else if (key == "ELRS_SWITCH_PIN") ELRS_SWITCH_PIN = std::stoi(val);

        // new FS keys
        else if (key == "FS_ENABLED") FS_ENABLED = std::stoi(val);
        else if (key == "LINK_LOST_MS") LINK_LOST_MS = std::stoi(val);

        else if (key == "FAILSAFE_PWM_US") {
            int tmp[16];
            if (parse_csv16_int(val, tmp)) {
                for (int i = 0; i < 16; i++) FAILSAFE_PWM_US[i] = tmp[i];
            }
        }
        else if (key == "FS_STAGE1_AFTER_MS") FS_STAGE1_AFTER_MS = std::stoi(val);
        else if (key == "FS_STAGE1_SET") parse_stage_set(val, FS_STAGE1_OVR);
        else if (key == "FS_STAGE2_AFTER_MS") FS_STAGE2_AFTER_MS = std::stoi(val);
        else if (key == "FS_STAGE2_SET") parse_stage_set(val, FS_STAGE2_OVR);
    }

    return !hostname.empty();
}

static bool resolveHost(const std::string& address, int port, sockaddr_in& out)
{
    std::memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = htons(port);

    // IP?
    if (inet_pton(AF_INET, address.c_str(), &out.sin_addr) == 1)
        return true;

    // hostname
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;

    if (getaddrinfo(address.c_str(), nullptr, &hints, &res) != 0 || !res)
        return false;

    out = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
    out.sin_port = htons(port);
    freeaddrinfo(res);
    return true;
}

static int setup_uart(const char* dev, int baud, bool is_crsf)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "open(" << dev << ") error: " << strerror(errno) << "\n";
        return -1;
    }

    termios2 tio{};
    if (ioctl(fd, TCGETS2, &tio) < 0) {
        std::cerr << "TCGETS2 error: " << strerror(errno) << "\n";
        close(fd);
        return -1;
    }

    // custom baud
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER;
    tio.c_ispeed = baud;
    tio.c_ospeed = baud;

    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;

    if (is_crsf) {
        // твій робочий preset для CRSF
        tio.c_cflag = 7344;
        tio.c_cc[VTIME] = 10;
        tio.c_cc[VMIN]  = 64;
    } else {
        tio.c_cflag &= ~CSIZE; tio.c_cflag |= CS8;
        tio.c_cflag &= ~PARENB; tio.c_cflag &= ~CSTOPB;
        tio.c_cflag |= (CLOCAL | CREAD);
        tio.c_cc[VTIME] = 1;
        tio.c_cc[VMIN]  = 0;
    }

    if (ioctl(fd, TCSETS2, &tio) < 0) {
        std::cerr << "TCSETS2 error: " << strerror(errno) << "\n";
        close(fd);
        return -1;
    }

    return fd;
}

static int udp_bind_nonblock(int port)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        std::cerr << "udp socket error: " << strerror(errno) << "\n";
        return -1;
    }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (sockaddr*)&local, sizeof(local)) != 0) {
        std::cerr << "bind(:" << port << ") error: " << strerror(errno) << "\n";
        close(s);
        return -1;
    }

    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl | O_NONBLOCK);
    return s;
}

// ===== 1) TELEMETRY 2224 (FIX: плавний KB/s) =====
static void telemetry_thread()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "[TELEM] socket error: " << strerror(errno) << "\n";
        return;
    }

    sockaddr_in target{};
    if (!resolveHost(hostname, CAM_INFO_PORT, target)) {
        std::cerr << "[TELEM] resolve failed for " << hostname << "\n";
        close(sock);
        return;
    }

    NetworkUsage lastNet = getNetworkUsage();
    auto lastTs = std::chrono::steady_clock::now();

    double rxEma = 0.0, txEma = 0.0;
    bool first = true;

    const double tau = 2.0; // 2.0 норм; 3-5 плавніше; 1-1.5 швидше реагує

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int rssi = -999, snr = -999;
        getSignalStrength(rssi, snr);

        auto nowTs = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(nowTs - lastTs).count();
        if (dt < 0.001) dt = 0.001;

        NetworkUsage cur = getNetworkUsage();

        // reset/underflow counters -> не робимо спайк
        if (cur.rx_bytes < lastNet.rx_bytes || cur.tx_bytes < lastNet.tx_bytes) {
            lastNet = cur;
            lastTs  = nowTs;

            long rx_kb = (long)(rxEma + 0.5);
            long tx_kb = (long)(txEma + 0.5);

            std::string msg =
                "Temp:" + std::to_string(get_cpu_temperature()) +
                "C R:" + std::to_string(rx_kb) + "KB/s T:" + std::to_string(tx_kb) +
                "KB/s RSSI:" + std::to_string(rssi) + " SNR:" + std::to_string(snr) + "\n";

            sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&target, sizeof(target));
            continue;
        }

        unsigned long long dRx = (unsigned long long)(cur.rx_bytes - lastNet.rx_bytes);
        unsigned long long dTx = (unsigned long long)(cur.tx_bytes - lastNet.tx_bytes);

        lastNet = cur;
        lastTs  = nowTs;

        double rxInst = (double)dRx / 1024.0 / dt;
        double txInst = (double)dTx / 1024.0 / dt;

        double alpha = dt / (tau + dt);

        if (first) {
            rxEma = rxInst;
            txEma = txInst;
            first = false;
        } else {
            rxEma += alpha * (rxInst - rxEma);
            txEma += alpha * (txInst - txEma);
        }

        long rx_kb = (long)(rxEma + 0.5);
        long tx_kb = (long)(txEma + 0.5);

        std::string msg =
            "Temp:" + std::to_string(get_cpu_temperature()) +
            "C R:" + std::to_string(rx_kb) + "KB/s T:" + std::to_string(tx_kb) +
            "KB/s RSSI:" + std::to_string(rssi) + " SNR:" + std::to_string(snr) + "\n";

        sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&target, sizeof(target));
    }
}

// ===== 2) MAVLINK 14550 (як було) =====
static void mavlink_thread()
{
    int serialFd = setup_uart("/dev/ttyS3", MAV_BAUD, false);
    if (serialFd < 0) {
        std::cerr << "[MAV] serial open failed\n";
        return;
    }

    int udpFd = udp_bind_nonblock(MAV_PORT);
    if (udpFd < 0) {
        close(serialFd);
        return;
    }

    sockaddr_in target{};
    if (!resolveHost(hostname, MAV_PORT, target)) {
        std::cerr << "[MAV] resolve failed for " << hostname << "\n";
        close(udpFd);
        close(serialFd);
        return;
    }

    uint8_t buf[256];
    pollfd fds[2];
    fds[0].fd = serialFd; fds[0].events = POLLIN;
    fds[1].fd = udpFd;    fds[1].events = POLLIN;

    while (true)
    {
        int pr = poll(fds, 2, 10);
        if (pr > 0)
        {
            if (fds[0].revents & POLLIN) {
                int n = read(serialFd, buf, sizeof(buf));
                if (n > 0) sendto(udpFd, buf, n, 0, (sockaddr*)&target, sizeof(target));
            }
            if (fds[1].revents & POLLIN) {
                int n = recvfrom(udpFd, buf, sizeof(buf), 0, NULL, NULL);
                if (n > 0) (void)write(serialFd, buf, n);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

// ===== 3) CRSF 2223 (LINK/FAILSAFE + STAGES) =====
int main()
{
    if (!readConfig("/root/config.txt")) {
        std::cerr << "Config error: host= missing\n";
        return 1;
    }

    set_realtime_priority();

    std::thread t_tele(telemetry_thread);
    std::thread t_mav(mavlink_thread);

    // UART CRSF
    int serialFd = setup_uart("/dev/ttyS2", 420000, true);
    if (serialFd < 0) {
        std::cerr << "[CRSF] serial open failed\n";
        return 1;
    }

    int crsfSock = udp_bind_nonblock(CONTROL_PORT);
    if (crsfSock < 0) {
        close(serialFd);
        return 1;
    }

    sockaddr_in targetAddr{};
    if (!resolveHost(hostname, CONTROL_PORT, targetAddr)) {
        std::cerr << "[CRSF] resolve failed for " << hostname << "\n";
        close(crsfSock);
        close(serialFd);
        return 1;
    }

    // init ping (як було)
    const char initmsg[] = "INIT";
    sendto(crsfSock, initmsg, sizeof(initmsg)-1, 0, (sockaddr*)&targetAddr, sizeof(targetAddr));

    // state
    uint8_t  lastRxPacket[26] = {0};
    bool     haveLastPacket = false;

    uint16_t lastChUs[16] = {0};
    bool     haveLastCh = false;

    uint8_t  outPacket[26] = {0};
    uint16_t baseOut[16]   = {0};
    uint16_t outCh[16]     = {0};

    auto lastUdpRx = std::chrono::steady_clock::now(); // last ANY 26
    auto lastInit  = std::chrono::steady_clock::now();
    auto lastSent  = std::chrono::steady_clock::now();

    uint8_t buf[256];

    // thresholds
    int linkLostThreshold = (LINK_LOST_MS >= 0) ? LINK_LOST_MS : FAILSAFE_TIMEOUT;

    while (true)
    {
        auto now = std::chrono::steady_clock::now();

        // UART -> UDP (як було)
        int n = read(serialFd, buf, sizeof(buf));
        if (n > 0) {
            sendto(crsfSock, buf, n, 0, (sockaddr*)&targetAddr, sizeof(targetAddr));
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[CRSF] uart read error: " << strerror(errno) << "\n";
        }

        // UDP -> (RC packet or something else)
        sockaddr_in src{};
        socklen_t sl = sizeof(src);
        int m = recvfrom(crsfSock, buf, sizeof(buf), 0, (sockaddr*)&src, &sl);
        if (m == 26) {
            // any 26 bytes -> consider link activity (prevents false FS due to crc quirks)
            lastUdpRx = now;

            std::memcpy(lastRxPacket, buf, 26);
            haveLastPacket = true;

            // try decode with crc, then without (so hold_last still works even if crc is weird)
            uint16_t tmp[16];
            if (crsf_decode_rc_us(lastRxPacket, tmp, true) || crsf_decode_rc_us(lastRxPacket, tmp, false)) {
                std::memcpy(lastChUs, tmp, sizeof(tmp));
                haveLastCh = true;
            }
        } else if (m < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[CRSF] udp recv error: " << strerror(errno) << "\n";
        }

        long sinceRx = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUdpRx).count();
        if (sinceRx < 0) sinceRx = 0;

        // INIT resend (як було)
        long sinceInit = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInit).count();
        if (sinceRx > 1000 && sinceInit > 1000) {
            sendto(crsfSock, initmsg, sizeof(initmsg)-1, 0, (sockaddr*)&targetAddr, sizeof(targetAddr));
            lastInit = now;
        }

        // LOCAL watchdog (як було)
        if (sinceRx >= LOCAL_TIMEOUT) {
            (void)std::system(("gpio clear " + std::to_string(ELRS_SWITCH_PIN)).c_str());
        }

        bool fsActive = (FS_ENABLED != 0) && (sinceRx >= linkLostThreshold);

        // elapsed since FS entry (stages use this, so they don't jitter on short drops)
        long fsElapsedMs = 0;
        if (fsActive) {
            fsElapsedMs = sinceRx - linkLostThreshold;
            if (fsElapsedMs < 0) fsElapsedMs = 0;
        }

        // ===== Build output packet =====
        if (!fsActive) {
            // NORMAL: pass through last received (freeze if ground stops briefly)
            if (haveLastPacket) {
                std::memcpy(outPacket, lastRxPacket, 26);
            } else {
                // no RC yet -> neutral
                for (int i = 0; i < 16; i++) outCh[i] = 1500;
                outCh[2] = 1000; // throttle safe default (CH3)
                crsf_encode_rc_us(outCh, outPacket);
            }
        } else {
            // FAILSAFE: base + stages
            // baseOut: 0 = hold_last (real last), else pwm
            for (int i = 0; i < 16; i++) {
                int v = FAILSAFE_PWM_US[i];
                if (v == 0) {
                    if (haveLastCh) baseOut[i] = lastChUs[i];
                    else if (haveLastPacket) {
                        // can't decode? -> keep packet frozen by using neutral base here
                        baseOut[i] = 1500;
                    } else {
                        baseOut[i] = 1500;
                    }
                } else {
                    baseOut[i] = (uint16_t)clamp_int(v, CRSF_US_MIN, CRSF_US_MAX);
                }
                outCh[i] = baseOut[i];
            }

            // If user wants pure hold_last and we still don't have decoded channels,
            // we avoid jumping to 1500 by freezing last packet (no stages possible in that case).
            bool pureHold = true;
            for (int i = 0; i < 16; i++) {
                if (FAILSAFE_PWM_US[i] != 0) { pureHold = false; break; }
            }
            bool anyStageConfigured = (FS_STAGE1_AFTER_MS >= 0) || (FS_STAGE2_AFTER_MS >= 0);

            if (pureHold && haveLastPacket && !haveLastCh && !anyStageConfigured) {
                std::memcpy(outPacket, lastRxPacket, 26);
            } else {
                // apply stages (stage2 can overwrite stage1)
                apply_stage(fsElapsedMs, FS_STAGE1_AFTER_MS, FS_STAGE1_OVR, baseOut, outCh);
                apply_stage(fsElapsedMs, FS_STAGE2_AFTER_MS, FS_STAGE2_OVR, baseOut, outCh);

                crsf_encode_rc_us(outCh, outPacket);
            }
        }

        // periodic UART write (як було)
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSent).count() > 15) {
            (void)write(serialFd, outPacket, 26);
            lastSent = now;
        }

        usleep(400);
    }

    t_tele.join();
    t_mav.join();
    return 0;
}

