// at_command.cpp - EC200A AT probe (RSSI + SINR) + auto-load option driver for 2c7c:6005
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>

#include <sys/stat.h>
#include <sys/types.h>

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

static void ensure_quectel_option_driver() {
    // якщо ttyUSB вже є — не чіпаємо нічого
    if (file_exists("/dev/ttyUSB0") || file_exists("/dev/ttyUSB1") || file_exists("/dev/ttyUSB2"))
        return;

    // Підняти модулі (як ти робив руками)
    ::system("modprobe usbserial >/dev/null 2>&1");
    ::system("modprobe option >/dev/null 2>&1");

    // дочекатися sysfs new_id
    const char* new_id_paths[] = {
        "/sys/bus/usb-serial/drivers/option1/new_id",
        "/sys/bus/usb-serial/drivers/option/new_id"
    };

    const char* new_id = nullptr;
    for (int i = 0; i < 20; ++i) { // ~2s
        for (auto p : new_id_paths) {
            if (file_exists(p)) { new_id = p; break; }
        }
        if (new_id) break;
        usleep(100000);
    }

    if (!new_id) {
        // Нема куди писати new_id — значить option драйвер не з’явився
        return;
    }

    // Підв'язуємо 2c7c:6005 (може повернути помилку, якщо вже додано — це ок)
    // В sysfs бажано без \n, але з \n теж часто ок. Зробимо без.
    (void)write_text_file(new_id, "2c7c 6005");

    // Чекаємо ttyUSB
    for (int i = 0; i < 40; ++i) { // ~4s
        if (file_exists("/dev/ttyUSB0") || file_exists("/dev/ttyUSB1") || file_exists("/dev/ttyUSB2"))
            return;
        usleep(100000);
    }
}

static bool configure_serial(int fd, speed_t baud) {
    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "tcgetattr: " << strerror(errno) << "\n";
        return false;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "tcsetattr: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

static std::string read_until_ok(int fd, int timeout_ms) {
    std::string out;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        pollfd pfd{fd, POLLIN, 0};
        int pr = poll(&pfd, 1, 50);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            char buf[512];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) out.append(buf, buf + n);

            if (out.find("\r\nOK") != std::string::npos || out.find("\nOK") != std::string::npos) break;
            if (out.find("ERROR") != std::string::npos) break;
        }
    }
    return out;
}

static bool send_cmd(int fd, const std::string& cmd, std::string& resp) {
    tcflush(fd, TCIOFLUSH);

    std::string c = cmd;
    if (c.find('\r') == std::string::npos) c += "\r";

    if (write(fd, c.c_str(), c.size()) < 0) {
        std::cerr << "write: " << strerror(errno) << "\n";
        return false;
    }
    resp = read_until_ok(fd, 1200);
    return !resp.empty();
}

static bool parse_csq_dbm(const std::string& s, int& rssi_dbm) {
    std::regex re(R"(\+CSQ:\s*(\d+)\s*,\s*(\d+))");
    std::smatch m;
    if (!std::regex_search(s, m, re)) return false;
    int v = std::stoi(m[1].str());
    if (v == 99) return false;
    rssi_dbm = -113 + 2 * v;
    return true;
}

static bool parse_qcsq_sinr(const std::string& s, int& sinr_db) {
    std::regex lineRe(R"(\+QCSQ:\s*([^\r\n]+))");
    std::smatch m;
    if (!std::regex_search(s, m, lineRe)) return false;

    std::string line = m[1].str();
    if (line.find("NOSERVICE") != std::string::npos) return false;
    if (line.find("SEARCH") != std::string::npos) return false;

    std::regex numRe(R"((-?\d+))");
    std::sregex_iterator it(line.begin(), line.end(), numRe), end;

    std::vector<int> nums;
    for (; it != end; ++it) nums.push_back(std::stoi((*it)[1].str()));
    if (nums.empty()) return false;

    // для LTE у Quectel часто SINR останнім числом
    sinr_db = nums.back();
    return true;
}

int main(int argc, char** argv) {
    // 0) якщо портів нема — спробувати підняти драйвер
    ensure_quectel_option_driver();

    std::string port = "/dev/ttyUSB1";
    if (argc >= 2) port = argv[1];

    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        // якщо порт не з’явився — ще раз спробуємо ініціалізацію і повтор
        ensure_quectel_option_driver();
        fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) {
            std::cerr << "open(" << port << "): " << strerror(errno) << "\n";
            return 1;
        }
    }

    if (!configure_serial(fd, B115200)) {
        close(fd);
        return 1;
    }

    std::string resp;

    send_cmd(fd, "AT", resp);
    send_cmd(fd, "ATI", resp);

    int rssi_dbm = -1;
    int sinr_db  = -1;

    if (send_cmd(fd, "AT+CSQ", resp))  parse_csq_dbm(resp, rssi_dbm);
    if (send_cmd(fd, "AT+QCSQ", resp)) parse_qcsq_sinr(resp, sinr_db);

    std::string qeng;
    send_cmd(fd, "AT+QENG=\"servingcell\"", qeng);

    close(fd);

    std::cout << "RSSI(dBm)=" << rssi_dbm << "  SINR(dB)=" << sinr_db << "\n";
    if (!qeng.empty()) {
        size_t p = qeng.find("+QENG:");
        if (p != std::string::npos) {
            size_t e = qeng.find('\n', p);
            std::string line = qeng.substr(p, (e == std::string::npos) ? std::string::npos : (e - p));
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::cout << line << "\n";
        }
    }

    return 0;
}

