#include "crsf.h"
void ProcessPayload(std::vector<uint8_t> payload)
{
	if (payload.size() == 0)
		return;
	std::lock_guard<std::mutex> lock(sharedMutex);
	if (payload[0] == 0x08) // CRSF_FRAMETYPE_BATTERY_SENSOR
	{
		tel.voltage = (payload[1] << 8) | payload[2];
		tel.current = (payload[3] << 8) | payload[4];
		tel.capacity = (payload[5] << 16) | (payload[6] << 8) | payload[7];
		tel.remaining = payload[8];
#ifdef _DEBUG
		printf("BATTERY_SENSOR: %.1fV\t%.1fA\t%dmAh\t%d%%\n", ((float)tel.voltage) / 10, ((float)tel.current) / 10, tel.capacity, tel.remaining);
#endif // DEBUG

	}
	else if (payload[0] == 0x02) // CRSF_FRAMETYPE_GPS
	{
		tel.latitude = (payload[1] << 24) | (payload[2] << 16) | (payload[3] << 8) | payload[4];  // degree / 10,000,000 big endian
		tel.longitude = (payload[5] << 24) | (payload[6] << 16) | (payload[7] << 8) | payload[8]; // degree / 10,000,000 big endian
		tel.groundspeed = (payload[9] << 8) | payload[10];                                        // km/h / 10 big endian
		tel.heading = (payload[11] << 8) | payload[12];                                           // GPS heading, degree/100 big endian
		tel.altitude = ((payload[13] << 8) | payload[14]) - 1000;                                 // meters, +1000m big endian
		tel.satellites = payload[15];                                                             // satellites
#ifdef _DEBUG
		printf("GPS: %lf, %lf\tGspd: %dkm/h\tHdg: %d°\tAlt: %dm\tSat: %d\n", ((double)tel.latitude) / 10000000.0, ((double)tel.longitude) / 10000000.0, tel.groundspeed / 10, tel.heading / 100, tel.altitude, tel.satellites);
#endif // DEBUG
		printf("GPS: %lf, %lf\tGspd: %dkm/h\tHdg: %d°\tAlt: %dm\tSat: %d\n", ((double)tel.latitude) / 10000000.0, ((double)tel.longitude) / 10000000.0, tel.groundspeed / 10, tel.heading / 100, tel.altitude, tel.satellites);
	}
	else if (payload[0] == 0x07) // CRSF_FRAMETYPE_VARIO
	{
		tel.verticalspd = (payload[1] << 8) | payload[2]; // Vertical speed in cm/s, BigEndian
#ifdef _DEBUG
		printf("VSpd: %dcm/s\n", tel.verticalspd);
#endif // DEBUG

	}
	else if (payload[0] == 0x21) // CRSF_FRAMETYPE_FLIGHT_MODE
	{
		char flightMode[32] = { 0 };
		memcpy(flightMode, &payload[1], payload.size() - 1);
		tel.flightMode = flightMode;
#ifdef _DEBUG
		printf("FM: %s\n", flightMode);
#endif // DEBUG

	}
	else if (payload[0] == 0x1E) // CRSF_FRAMETYPE_ATTITUDE
	{
		tel.pitch = (payload[1] << 8) | payload[2]; // pitch in radians, BigEndian
		tel.roll = (payload[3] << 8) | payload[4];  // roll in radians, BigEndian
		tel.yaw = (payload[5] << 8) | payload[6];   // yaw in radians, BigEndian
#ifdef _DEBUG
		printf("Gyro:\tP%.1f\tR%.1f\tY%.1f\n", RADTODEG(tel.pitch) / 10000.f, RADTODEG(tel.roll) / 10000.f, RADTODEG(tel.yaw) / 10000.f);
#endif // DEBUG
	}
	else if (payload[0] == 0x1C) // CRSF_FRAMETYPE_LINK_RX_ID
	{
		tel.rxRssiPercent = payload[1];
		tel.rxRfPower = payload[2]; // should be signed int?
#ifdef _DEBUG
		printf("RX: RSSI: %d\tPWR:\t%d\n", tel.rxRssiPercent, tel.rxRfPower);
#endif // DEBUG
	}
	else if (payload[0] == 0x1D) // CRSF_FRAMETYPE_LINK_RX_ID
	{
		tel.txRssiPercent = payload[1];
		tel.txRfPower = payload[2]; // should be signed int?
		tel.txFps = payload[3];
#ifdef _DEBUG
		printf("TX: RSSI: %d\tPWR: %d\tFps: %d\n", tel.txRssiPercent, tel.txRfPower, tel.txFps);
#endif // DEBUG
	}
	else if (payload[0] == 0x88) // CRSF_FRAMETYPE_LINK_RX_ID
	{
		for (size_t i = 0; i < 8; i++)
		{
			std::cout << payload[i];
		}

		tel.rssi = (payload[2] << 8) | payload[1];
		tel.rsrq = (payload[4] << 8) | payload[3];
		tel.rsrp = (payload[5] << 8) | payload[6];
		tel.rsrp = payload[7];
#ifdef _DEBUG
		printf("RSSI: %d\tRSQR: %d\tRSRP: %d\tSNR: %d\n", tel.rssi, tel.rsrq, tel.rsrp, tel.rsrp);
#endif // DEBUG
	}
	else if (payload[0] == 0x14) // CRSF_FRAMETYPE_LINK_RX_ID
	{
		tel.uplink_RSSI_1 = payload[1];
		tel.uplink_RSSI_2 = payload[2];
		tel.uplink_Link_quality = payload[3];
		tel.uplink_SNR = payload[4];
		tel.active_antenna = payload[5];
		tel.rf_Mode = payload[6];
		tel.uplink_TX_Power = payload[7];
		tel.downlink_RSSI = payload[8];
		tel.downlink_Link_quality = payload[9];
		tel.downlink_SNR = payload[10];
#ifdef _DEBUG
		printf("RSSI1: %d\tRSSI2: %d\tRQly: %d\tRSNR: %d\tAnt: %d\tRF_MD: %d\tTXP: %d\tTRSSI: %d\tTQly: %d\tTSNR: %d\n", tel.uplink_RSSI_1, tel.uplink_RSSI_2, tel.uplink_Link_quality, tel.uplink_SNR, tel.active_antenna, tel.rf_Mode, tel.uplink_TX_Power, tel.downlink_RSSI, tel.downlink_Link_quality, tel.downlink_SNR);
#endif // DEBUG
	}
}
void CheckPayloads(std::vector<uint8_t>& buffer)
{
	while (true)
	{
		size_t start = -1;
		// Scan for Start byte
		for (int i = 0; i < buffer.size(); i++)
		{
			if (buffer[i] == 0xC8)
			{
				start = i;
				break;
			}
		}
		// if Start byte not found return and read more from serial
		if (start == -1)
			return;
		/*..SYNC .?*/
		// Trim to start byte
		buffer.erase(buffer.begin(), buffer.begin() + start);
		/*SYNC .?*/
		// Check for payload size - aka anti out of bounds - aka sync byte is not last byte in buffer
		if (buffer.size() < 2)
			return;
		/*SYNC LEN .?*/
		size_t payload_length = buffer[1];
		// Check if entire payload is in buffer / aka anti out of bounds
		if (buffer.size() < payload_length + 3)
			return;
		// YES we have entire payload in buffer
		/*SYNC 04 01 02 03 04 CRC*/
		if (payload_length > 60)
		{
			buffer.clear();
			std::cerr << "invalid crsf packet detected! - payload length > 60\n";
			return;
		}
		//printf("CRC: %02X\n", CRC(buffer,2,payload_length-1));
		if (CRC(buffer, 2, payload_length - 1) != buffer[payload_length + 1])
		{
			buffer.clear();
			std::cerr << "invalid crsf packet detected! - CRC mismatch\n";
			return;
		}

		/*SYNC 04 01 02 03 04 CRC SYNC*/
		if (buffer[payload_length + 2] != 0xC8)
		{
			buffer.clear();
			std::cerr << "invalid crsf packet detected!\n";
			return;
		}
#ifdef _DEBUG
		printVectorHex(buffer, payload_length + 2);
#endif
		// Process it and remove from buffer

		ProcessPayload((std::vector<uint8_t>(buffer.begin() + 2, buffer.begin() + payload_length + 2)));
		buffer.erase(buffer.begin(), buffer.begin() + payload_length + 2);
	}
}