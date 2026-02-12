#pragma once
#include <string>

struct NetworkUsage {
    unsigned long rx_bytes;
    unsigned long tx_bytes;
};

NetworkUsage getNetworkUsage();
int get_cpu_temperature();

// rssi: dBm (від’ємне), snr/sinr: dB (може бути від’ємне)
void getSignalStrength(int &rssi, int &snr);

// сирий текст (QENG / servingcell), якщо потрібно
std::string getServingCellInfo();
