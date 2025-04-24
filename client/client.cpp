#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <netdb.h>
#include <regex>
#include <fstream>
#include <fcntl.h>
#include <ctime>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "/usr/include/asm-generic/termbits.h"
#include "/usr/include/asm-generic/ioctls.h"
#include <errno.h>
#include <cmath>
#define CRSF_CHANNEL_VALUE_MIN 172
#define CRSF_CHANNEL_VALUE_1000 191
#define CRSF_CHANNEL_VALUE_MID 992
#define CRSF_CHANNEL_VALUE_2000 1792
#define CRSF_CHANNEL_VALUE_MAX 1811
#define BUFFER_SIZE 128
#define RADTODEG(radians) ((radians) * (180.0 / M_PI))
#define CRSF_CHANNEL_TO_RC(value) ((value - 1000) * (1792 - 191) / (2000 - 1000) + 191)

int LOCAL_TIMEOUT = 300000;
int FAILSAFE_TIMEOUT = 5000;
int STABILIZE_TIMEOUT = 250;
int ELRS_SWITCH_PIN = 1;
int HOVER_VALUE = 1200;
int CONTROL_PORT = 2223;
int CAM_INFO_PORT = 2224;
std::string hostname;

int get_cpu_temperature();
void getSignalStrength(int &rssi, int &snr);

struct NetworkUsage
{
	unsigned long rx_bytes;
	unsigned long tx_bytes;
};

NetworkUsage getNetworkUsage();
std::string getServingCellInfo();

int16_t CRC16(uint16_t *data, size_t length)
{
	uint16_t crc = 0x0000;		  // Initial value
	uint16_t polynomial = 0x1021; // Polynomial for CRC-16-CCITT

	for (size_t i = 0; i < length; ++i)
	{
		uint16_t current_data = data[i];
		for (size_t j = 0; j < 16; ++j)
		{
			bool bit = (current_data >> (15 - j) & 1) ^ ((crc >> 15) & 1);
			crc <<= 1;
			if (bit)
			{
				crc ^= polynomial;
			}
		}
	}

	return crc;
}

uint8_t CRC(const uint8_t *data, size_t start, size_t length)
{
	uint8_t crc = 0;
	size_t end = start + length;

	for (std::size_t i = start; i < end; ++i)
	{
		crc ^= data[i];

		for (uint8_t j = 0; j < 8; ++j)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0xD5;
			else
				crc <<= 1;
		}
	}

	return crc;
}

bool readConfig(const std::string &filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		std::cerr << "Error: Could not open configuration file " << filename << std::endl;
		return false;
	}

	std::string line;
	while (std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string key, value;
		if (std::getline(iss, key, '=') && std::getline(iss, value))
		{
			if (key == "host")
				hostname = value;
			else if (key == "LOCAL_TIMEOUT")
				LOCAL_TIMEOUT = std::stoi(value);
			else if (key == "FAILSAFE_TIMEOUT")
				FAILSAFE_TIMEOUT = std::stoi(value);
			else if (key == "STABILIZE_TIMEOUT")
				STABILIZE_TIMEOUT = std::stoi(value);
			else if (key == "ELRS_SWITCH_PIN")
				ELRS_SWITCH_PIN = std::stoi(value);
			else if (key == "CONTROL_PORT")
				CONTROL_PORT = std::stoi(value);
			else if (key == "CAM_INFO_PORT")
				CAM_INFO_PORT = std::stoi(value);
			else if (key == "HOVER_VALUE")
				HOVER_VALUE = CRSF_CHANNEL_TO_RC(std::stoi(value));
		}
	}
	return true;
}

int initializeSocket(const std::string &address, int port, struct sockaddr_in &serverAddr)
{
	int sockfd;
	struct addrinfo hints, *res;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;		// IPv4
	hints.ai_socktype = SOCK_DGRAM; // UDP

	// Check if the input is an IP address
	if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) == 1)
	{
		// It's an IP address
		serverAddr.sin_family = AF_INET;
	}
	else
	{
		// It's a hostname, resolve it
		if (getaddrinfo(address.c_str(), NULL, &hints, &res) != 0)
		{
			perror("getaddrinfo failed");
			exit(EXIT_FAILURE);
		}

		// Copy the resolved address to serverAddr
		serverAddr = *(struct sockaddr_in *)(res->ai_addr);
		freeaddrinfo(res);
	}

	serverAddr.sin_port = htons(port);

	// Set socket to non-blocking
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	return sockfd;
}

void receiveMessages(int sockfd, struct sockaddr_in &serverAddr)
{
	char buffer[BUFFER_SIZE];
	socklen_t addrLen = sizeof(serverAddr);

	while (true)
	{
		usleep(1000);
		int len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serverAddr, &addrLen);
		if (len > 0)
		{
			buffer[len] = '\0';
			std::cout << "Received: " << buffer;
		}
	}
}

void OIPCTelemetry()
{
	struct sockaddr_in serverAddr;
	int sockfd = initializeSocket(hostname, CAM_INFO_PORT, serverAddr);

	std::thread receiveThread(receiveMessages, sockfd, std::ref(serverAddr));

	while (true)
	{
		const double interval = 0.5; // interval in seconds
		const double bytes_to_kb = 1024.0;
		NetworkUsage usage1 = getNetworkUsage();
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval * 1000)));
		NetworkUsage usage2 = getNetworkUsage();
		unsigned long rx_diff = usage2.rx_bytes - usage1.rx_bytes;
		unsigned long tx_diff = usage2.tx_bytes - usage1.tx_bytes;
		unsigned long rx_kbps = static_cast<unsigned long>(rx_diff / bytes_to_kb / interval);
		unsigned long tx_kbps = static_cast<unsigned long>(tx_diff / bytes_to_kb / interval);
		int rssi = 0;
		int snr = 0;
		std::string telemetryString = "Temp: " + std::to_string(get_cpu_temperature()) + " C, R: " + std::to_string(rx_kbps) + " KB/s, T: " + std::to_string(tx_kbps) + " KB/s, RSSI: " + std::to_string(rssi) + ", SNR: " + std::to_string(snr) + "\n\0";
		sendto(sockfd, telemetryString.c_str(), telemetryString.length(), 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
		telemetryString = getServingCellInfo();
		sendto(sockfd, telemetryString.c_str(), telemetryString.length(), 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
	}

	receiveThread.join();
	close(sockfd);
}

int main()
{
	if (!readConfig("/root/config.txt"))
	{
		return 1;
	}

	std::cout << "QuadroFleet Masina" << std::endl;
	std::cout << "Using hostname: " << hostname << std::endl;
	std::cout << "LOCAL_TIMEOUT: " << LOCAL_TIMEOUT << std::endl;
	std::cout << "FAILSAFE_TIMEOUT: " << FAILSAFE_TIMEOUT << std::endl;
	std::cout << "STABILIZE_TIMEOUT: " << STABILIZE_TIMEOUT << std::endl;
	std::cout << "CONTROL_PORT: " << CONTROL_PORT << std::endl;
	std::cout << "CAM_INFO_PORT: " << CAM_INFO_PORT << std::endl;

	std::thread OIPCTelemetryThread(OIPCTelemetry);

	int serialPort = open("/dev/ttyS2", O_RDWR);
	int baudrate = 420000;
	struct termios2 tio;
	ioctl(serialPort, TCGETS2, &tio);
	tio.c_cflag &= ~CBAUD;
	tio.c_cflag |= BOTHER;
	tio.c_ispeed = baudrate;
	tio.c_ospeed = baudrate;
	tio.c_cc[VTIME] = 10;
	tio.c_cc[VMIN] = 64;

	tio.c_cflag = 7344;
	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_lflag = 0;

	if (ioctl(serialPort, TCSETS2, &tio) != 0)
		printf("Serial error");

	// Set the serial port to non-blocking mode
	int flags = fcntl(serialPort, F_GETFL, 0);
	if (flags == -1)
	{
		perror("Failed to get file status flags");
		close(serialPort);
		return 1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(serialPort, F_SETFL, flags) == -1)
	{
		perror("Failed to set file status flags");
		close(serialPort);
		return 1;
	}

	struct sockaddr_in serverAddr;
	int sockfd = initializeSocket(hostname, CONTROL_PORT, serverAddr);
	uint8_t dummybuf[5] = "INIT";
	sendto(sockfd, dummybuf, 5, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
	std::cout << "INIT SENT";

	int recvSock = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in localAddr{};
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons(CONTROL_PORT);
	localAddr.sin_addr.s_addr = INADDR_ANY;
	int recvFlags = fcntl(recvSock, F_GETFL, 0);
	fcntl(recvSock, F_SETFL, recvFlags | O_NONBLOCK);
	bind(recvSock, (sockaddr*)&localAddr, sizeof(localAddr));

	while (true)
	{
		usleep(1000);
		static uint16_t channels[16] = {992, 992, 1716, 992, 191, 191, 191, 191, 997, 997, 997, 997, 0, 0, 1811, 1811};
		static uint16_t crsfPacket[26] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		static bool fsMode = false;
		static auto lastValidPayload = std::chrono::high_resolution_clock::now();
		static auto lastSentPayload = std::chrono::high_resolution_clock::now();
		uint8_t serialBuffer[128] = {0};

		int serialReadBytes = read(serialPort, &serialBuffer, sizeof(serialBuffer));

		try
		{
			if (serialReadBytes < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
				}
				else
				{
					perror("Failed to read from serial port");
					usleep(100000);
				}
			}
			else if (serialReadBytes == 0)
			{
				printf("EOF\n");
			}
			else
			{
				sendto(sockfd, serialBuffer, serialReadBytes, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
			}

			char rxBuffer[128];
			sockaddr_in clientAddr{};
			socklen_t addrLen = sizeof(clientAddr);

			ssize_t bytesRead = recvfrom(recvSock, rxBuffer, sizeof(rxBuffer), 0, (struct sockaddr *)&clientAddr, &addrLen);

			if (bytesRead == -1)
			{
				if (errno == EWOULDBLOCK || errno == EAGAIN)
				{
					auto currentTime = std::chrono::high_resolution_clock::now();
					auto elapsedTimeValid = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastValidPayload).count();

					if (elapsedTimeValid >= LOCAL_TIMEOUT)
					{
						// No data for 5m - Switch to local controller
						// std::cerr << "LOCAL_TIMEOUT\n";
						std::string command = "gpio clear " + std::to_string(ELRS_SWITCH_PIN);
						std::system(command.c_str());
					}
					else if (elapsedTimeValid >= FAILSAFE_TIMEOUT)
					{
						// No data for 5s - Failsafe
						// std::cerr << "FAILSAFE_TIMEOUT\n";
						channels[7] = fsMode ? CRSF_CHANNEL_VALUE_2000 : CRSF_CHANNEL_VALUE_1000;
					}
					else if (elapsedTimeValid >= STABILIZE_TIMEOUT)
					{
						// No data for 250ms - STABILIZE
						// std::cerr << "STABILIZE_TIMEOUT\n";
						channels[0] = CRSF_CHANNEL_VALUE_MID;   // ROLL
						channels[1] = CRSF_CHANNEL_VALUE_MID;   // PITCH
						channels[2] = HOVER_VALUE;              // THROTTLE
						channels[3] = CRSF_CHANNEL_VALUE_MID;   // YAW
						channels[4] = CRSF_CHANNEL_VALUE_2000;  // ARMED
						channels[5] = CRSF_CHANNEL_VALUE_2000;  // ANGLE MODE
						channels[6] = CRSF_CHANNEL_VALUE_1000;  // ALT HOLD MODE
					}

					auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastSentPayload).count();

					if (elapsedTime > 20)
					{
						ssize_t bytes_written = write(serialPort, crsfPacket, 26);

						lastSentPayload = currentTime;
					}

					usleep(5000);
				}
				else
				{
					perror("Error");
					usleep(100000);
				}
			}
			else if (bytesRead == 0)
			{
				std::cout << "Connection closed by the server" << std::endl;
				usleep(100000);
			}
			else if (bytesRead > 0)
			{
                                lastValidPayload = std::chrono::high_resolution_clock::now();

                                memcpy(crsfPacket, rxBuffer, 26);
			}
		}
		catch (const std::exception &e)
		{
			std::cerr << "Exception: " << e.what() << std::endl;
			usleep(10000); // Sleep for 1 second on exception
		}
	}

	close(sockfd);
	close(serialPort);
	OIPCTelemetryThread.join();

	return 0;
}
