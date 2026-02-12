#include "utils.h"
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// ---------------- helpers ----------------

static std::string trim_copy(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    s.erase(0, i);
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    return s;
}

static bool read_first_line(const std::string& path, std::string& outLine) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::getline(f, outLine);
    outLine = trim_copy(outLine);
    return !outLine.empty();
}

static bool run_command_capture(const std::string& cmd, std::string& out) {
    out.clear();
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return false;

    char buf[512];
    while (fgets(buf, sizeof(buf), fp)) out += buf;

    pclose(fp);
    return !out.empty();
}

static std::string env_or(const char* name, const char* defv) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : std::string(defv);
}

static bool file_exists(const char* path) {
    return ::access(path, F_OK) == 0;
}

static bool write_text_file(const char* path, const std::string& s) {
    int fd = ::open(path, O_WRONLY);
    if (fd < 0) return false;
    ssize_t w = ::write(fd, s.c_str(), s.size());
    ::close(fd);
    return w == (ssize_t)s.size();
}

// Дуже важливо: не смикати це постійно (щоб не флапати USB/мережу)
static void ensure_quectel_option_driver_once() {
    static bool done = false;
    static time_t lastTry = 0;

    // якщо вже є ttyUSB — нічого не робимо
    if (file_exists("/dev/ttyUSB0") || file_exists("/dev/ttyUSB1") || file_exists("/dev/ttyUSB2")) {
        done = true;
        return;
    }

    // throttle: не частіше ніж раз на 10 секунд
    time_t now = time(nullptr);
    if (done) return;
    if (lastTry != 0 && (now - lastTry) < 10) return;
    lastTry = now;

    // Підняти модулі
    ::system("modprobe usbserial >/dev/null 2>&1");
    ::system("modprobe option >/dev/null 2>&1");

    // Знайти new_id
    const char* new_id_paths[] = {
        "/sys/bus/usb-serial/drivers/option1/new_id",
        "/sys/bus/usb-serial/drivers/option/new_id"
    };

    const char* new_id = nullptr;
    for (int i = 0; i < 30; ++i) { // ~3s
        for (auto p : new_id_paths) {
            if (file_exists(p)) { new_id = p; break; }
        }
        if (new_id) break;
        usleep(100000);
    }

    if (!new_id) return;

    // Прив'язати 2c7c:6005 (може вже бути додано — це ок)
    (void)write_text_file(new_id, "2c7c 6005");

    // Дочекатися ttyUSB
    for (int i = 0; i < 50; ++i) { // ~5s
        if (file_exists("/dev/ttyUSB0") || file_exists("/dev/ttyUSB1") || file_exists("/dev/ttyUSB2")) {
            done = true;
            return;
        }
        usleep(100000);
    }
}

// auto-detect ttyUSB port
static std::string detect_modem_tty() {
    // якщо користувач явно задав MODEM_AT_DEV — беремо його
    std::string dev = env_or("MODEM_AT_DEV", "");
    if (!dev.empty() && file_exists(dev.c_str())) return dev;

    // типово ти казав "в мене usb1"
    if (file_exists("/dev/ttyUSB1")) return "/dev/ttyUSB1";
    if (file_exists("/dev/ttyUSB0")) return "/dev/ttyUSB0";
    if (file_exists("/dev/ttyUSB2")) return "/dev/ttyUSB2";

    // запасний варіант: пробігтися по ttyUSB0..ttyUSB7
    for (int i = 0; i < 8; ++i) {
        std::string p = "/dev/ttyUSB" + std::to_string(i);
        if (file_exists(p.c_str())) return p;
    }

    return "";
}

// ---------------- public API ----------------

NetworkUsage getNetworkUsage() {
    std::ifstream netDevFile("/proc/net/dev");
    NetworkUsage usage{0, 0};
    if (!netDevFile.is_open()) return usage;

    std::string line;
    std::getline(netDevFile, line); // header
    std::getline(netDevFile, line); // header

    while (std::getline(netDevFile, line)) {
        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string iface = trim_copy(line.substr(0, colonPos));
        if (iface == "lo") continue;

        std::istringstream iss(line.substr(colonPos + 1));

        unsigned long rx_bytes = 0, tx_bytes = 0;
        unsigned long dummy = 0;

        if (!(iss >> rx_bytes)) continue;
        for (int i = 0; i < 7; ++i) { if (!(iss >> dummy)) break; }
        if (!(iss >> tx_bytes)) continue;

        usage.rx_bytes += rx_bytes;
        usage.tx_bytes += tx_bytes;
    }

    return usage;
}

int get_cpu_temperature() {
    // 1) mstar TEMP_R
    {
        std::string line;
        if (read_first_line("/sys/devices/virtual/mstar/msys/TEMP_R", line)) {
            std::regex re(R"(Temperature\s+(-?\d+))");
            std::smatch m;
            if (std::regex_search(line, m, re) && m.size() > 1) {
                try { return std::stoi(m[1].str()); } catch (...) {}
            }
        }
    }

    // 2) fallback thermal_zone0/temp (millideg)
    {
        std::string line;
        if (read_first_line("/sys/class/thermal/thermal_zone0/temp", line)) {
            try {
                long v = std::stol(line);
                if (v > 1000) return (int)(v / 1000);
                return (int)v;
            } catch (...) {}
        }
    }

    return -1;
}

void getSignalStrength(int &rssi, int &snr) {
    // тримаємо останні валідні значення
    static int lastRssi = -1;
    static int lastSnr  = -1;

    // віддаємо останні
    rssi = lastRssi;
    snr  = lastSnr;

    // якщо ttyUSB ще нема — спробувати підняти option driver (один раз з throttle)
    ensure_quectel_option_driver_once();

    std::string dev = detect_modem_tty();
    if (dev.empty()) return;

    const std::string bin = env_or("AT_COMMAND_BIN", "/usr/bin/at_command");

    // /usr/bin/at_command /dev/ttyUSB1
    std::string out;
    if (!run_command_capture(bin + " " + dev, out)) return;

    // парсимо:
    // RSSI(dBm)=-89  SINR(dB)=-8
    std::regex rssiRe(R"(RSSI\(dBm\)\s*=\s*(-?\d+))");
    std::regex sinrRe(R"((SINR|SNR)\(dB\)\s*=\s*(-?\d+))");

    std::smatch m;
    int rr = -1, ss = -1;

    if (std::regex_search(out, m, rssiRe) && m.size() > 1) {
        try { rr = std::stoi(m[1].str()); } catch (...) { rr = -1; }
    }
    if (std::regex_search(out, m, sinrRe) && m.size() > 2) {
        try { ss = std::stoi(m[2].str()); } catch (...) { ss = -1; }
    }

    // якщо модем тимчасово в SEARCH/NO SERVICE і повернув -1 — не перетираємо валідні
    if (rr != -1) lastRssi = rr;
    if (ss != -1) lastSnr  = ss;

    rssi = lastRssi;
    snr  = lastSnr;
}

std::string getServingCellInfo() {
    ensure_quectel_option_driver_once();

    std::string dev = detect_modem_tty();
    if (dev.empty()) return "";

    const std::string bin = env_or("AT_COMMAND_BIN", "/usr/bin/at_command");

    std::string out;
    if (!run_command_capture(bin + " " + dev, out)) return "";
    return out;
}

