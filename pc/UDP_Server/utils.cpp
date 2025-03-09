#include "utils.h"

std::wstring convert_to_wstring(const std::string& str) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(str);
}

double calculateAngle(double lat1, double lon1, double lat2, double lon2)
{
	double dLon = DEGTORAD(lon2 - lon1);
	double y = sin(dLon) * cos(DEGTORAD(lat2));
	double x = cos(DEGTORAD(lat1)) * sin(DEGTORAD(lat2)) -
		sin(DEGTORAD(lat1)) * cos(DEGTORAD(lat2)) * cos(dLon);
	double angle = atan2(y, x);
	angle = RADTODEG(angle);
	if (angle < 0)
	{
		angle += 360.0;
	}
	return angle;
}
double calculateHaversine(double lat1, double lon1, double lat2, double lon2)
{
	// Convert latitude and longitude from degrees to radians
	lat1 = DEGTORAD(lat1);
	lon1 = DEGTORAD(lon1);
	lat2 = DEGTORAD(lat2);
	lon2 = DEGTORAD(lon2);

	// Calculate differences
	double dlat = lat2 - lat1;
	double dlon = lon2 - lon1;

	// Haversine formula
	double a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	const double EarthRadiusKm = 6371.0; // Earth radius in kilometers
	// Distance in kilometers
	double distance = EarthRadiusKm * c;

	return distance * 1000.0;
}
std::wstring Compass(int angle, double latitude, double longitude, double homeLat, double homeLon,double pinLat, double pinLon)
{
	int scale = 10;
	angle += 180 + std::round((double)scale / 2.0);
	if (angle >= 360)
		angle -= 360;
	if (angle >= 360 || angle < 0)
		return L"";
	angle /= scale;
	wchar_t buffer[37] = { 0 }; // size should be (360 / scale) + 1
	wmemset(buffer, L'-', 360 / scale);
	buffer[0] = L'N';
	buffer[90 / scale] = L'E';
	buffer[180 / scale] = L'S';
	buffer[270 / scale] = L'W';

	int homeIndex = (int)(calculateAngle((double)latitude / 10000000.0, (double)longitude / 10000000.0, homeLat, homeLon) / scale);
	//std::wstring homeSymbol = L"🏠";
	//wcsncpy(buffer + homeIndex, homeSymbol.c_str(), homeSymbol.size());
	buffer[homeIndex] = L'H';
	buffer[(int)(calculateAngle((double)latitude / 10000000.0, (double)longitude / 10000000.0, pinLat, pinLon) / scale)] = L'M';
	std::wstring s(buffer);

	return s.substr(angle + 1) + s.substr(0, angle);
}

uint8_t CRC(const std::vector<uint8_t>& data, std::size_t start, std::size_t length)
{
	uint8_t crc = 0;
	std::size_t end = start + length;

	if (end > data.size())
		end = data.size();

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
void printHex(const char* arr, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		printf("%02X ", (unsigned char)arr[i]);
	}
	printf("\n");
}
void printVectorHex(const std::vector<uint8_t>& vec, std::size_t maxLength)
{
	std::size_t count = 0;
	for (const auto& val : vec) {
		if (count >= maxLength) break;
		printf("%02X ", val);
		count++;
	}
	printf("\n");
}

int Http_Post(const char* domain, int port, const char* path, char* body, uint16_t bodyLen, char* retBuffer, int bufferLen) {
	bool flag = false;
	uint16_t recvLen = 0;

	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		fprintf(stderr, "WSAStartup failed\n");
		return -1;
	}

	// Allocate memory for request
	static char request[512];
	snprintf(request, 512, "POST %s HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nConnection: Keep-Alive\r\nHost: %s\r\nContent-Length: %d\r\n\r\n",
		path, domain, bodyLen);

	// Create socket
	SOCKET fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == INVALID_SOCKET) {
		fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	// Resolve domain to IP address
	struct addrinfo hints, * res, * p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // AF_INET for IPv4
	hints.ai_socktype = SOCK_STREAM;

	int status;
	if ((status = getaddrinfo(domain, NULL, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo fail: %s\n", gai_strerror(status));
		closesocket(fd);
		WSACleanup();
		return -1;
	}

	// Setup sockaddr_in
	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;

	if (connect(fd, (struct sockaddr*)&sockaddr, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		fprintf(stderr, "socket connect fail: %d\n", WSAGetLastError());
		closesocket(fd);
		WSACleanup();
		freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res);

	// Send request headers
	if (send(fd, request, (int)strlen(request), 0) == SOCKET_ERROR) {
		fprintf(stderr, "socket send fail: %d\n", WSAGetLastError());
		closesocket(fd);
		WSACleanup();
		return -1;
	}

	// Send request body
	if (send(fd, body, bodyLen, 0) == SOCKET_ERROR) {
		fprintf(stderr, "socket send fail: %d\n", WSAGetLastError());
		closesocket(fd);
		WSACleanup();
		return -1;
	}

	fd_set fds;
	struct timeval timeout = { 12, 0 };
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	while (!flag) {
		int ret = select(0, &fds, NULL, NULL, &timeout);
		switch (ret) {
		case SOCKET_ERROR:
			fprintf(stderr, "select error: %d\n", WSAGetLastError());
			flag = true;
			break;
		case 0:
			fprintf(stderr, "select timeout\n");
			flag = true;
			break;
		default:
			if (FD_ISSET(fd, &fds)) {
				memset(retBuffer, 0, bufferLen);
				ret = recv(fd, retBuffer, bufferLen, 0);
				recvLen += ret;
				if (ret == SOCKET_ERROR) {
					fprintf(stderr, "recv error: %d\n", WSAGetLastError());
					flag = true;
					break;
				}
				else if (ret == 0) {
					break;
				}
				else if (ret < bufferLen) {
					closesocket(fd);
					WSACleanup();
					return recvLen;
				}
			}
			break;
		}
	}

	closesocket(fd);
	WSACleanup();
	return -1;
}
void TraccarUpdate()
{
	char requestPath[256];
	char buffer[1024];
	while (true)
	{
		{
			std::lock_guard<std::mutex> lock(sharedMutex);
			snprintf(requestPath, sizeof(requestPath), "/?id=0192837465&timestamp=%d&lat=%f&lon=%f&speed=%d&altitude=%d", (int)time(NULL), ((double)tel.latitude) / 10000000.0, ((double)tel.longitude) / 10000000.0, tel.groundspeed / 10, tel.altitude);
		}
		if (Http_Post("your.ddns.net", 8082, requestPath, NULL, 0, buffer, sizeof(buffer)) < 0)
			fprintf(stderr, "send location to traccar fail\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}