#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <gdiplus.h>

#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <locale>
#include <codecvt>
#include <cwchar>
#include <cmath>
#include <regex>
#include <mutex>
#include <fcntl.h>

#include <thread>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "Dwrite.lib")

#define M_PI 3.14159265358979323846
#define RADTODEG(radians) ((radians) * (180.0 / M_PI))
#define DEGTORAD(degrees) ((degrees) * (M_PI / 180.0))
#define CHAR_TO_WCHAR(str) \
    ([](const char* input) { \
        int length = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0); \
        wchar_t* output = new wchar_t[length]; \
        MultiByteToWideChar(CP_UTF8, 0, input, -1, output, length); \
        return output; \
    })(str)
struct Telemetry
{
	uint16_t voltage, current;
	uint32_t capacity;
	uint8_t remaining;

	int32_t latitude, longitude;
	uint16_t groundspeed, heading, altitude;
	uint8_t satellites;

	uint16_t verticalspd;

	std::string flightMode;

	int16_t pitch, roll;
	uint16_t yaw;

	uint8_t rxRssiPercent, rxRfPower;

	uint8_t txRssiPercent, txRfPower, txFps;

	int16_t rssi, rsrq, rsrp;
	int8_t snr;

	uint8_t uplink_RSSI_1;
	uint8_t uplink_RSSI_2;
	uint8_t uplink_Link_quality;
	int8_t uplink_SNR;
	uint8_t active_antenna;
	uint8_t rf_Mode;
	uint8_t uplink_TX_Power;
	uint8_t downlink_RSSI;
	uint8_t downlink_Link_quality;
	int8_t downlink_SNR;

	int pi_temp;
	int pi_read_speed;
	int pi_write_speed;
	int pi_rssi;
	int pi_snr;
};
extern std::mutex sharedMutex;
extern Telemetry tel;
extern int serCells;
extern int serCells;