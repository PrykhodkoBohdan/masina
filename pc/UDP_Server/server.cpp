
#include "controller.h"
#include "utils.h"
#include "server.h"
#include "crsf.h"
#include "draw.h"
#define NO_CONTROLLER_

struct Telemetry;
Telemetry tel = { 0 };
std::mutex sharedMutex;

//
int serCells = 4; // 4S default
double homeLat = 48.413256, homeLon = 17.692330;
double pinLat = 48.477106, pinLon = 17.553499;
const char* TARGET_WINDOW_NAME = "Direct3D11 renderer";
//
#ifndef NO_CONTROLLER
Controller controller(0);
#endif

char rxbuffer1[1024] = { 0 };
char rxbuffer2[1024] = { 0 };

int initializeSocket(int port, struct sockaddr_in& serverAddr);

int main(int argc, char* argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-home") == 0 && i + 2 < argc)
		{
			homeLat = atof(argv[i + 1]);
			homeLon = atof(argv[i + 2]);
			i += 2;
		}
		else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
		{
			serCells = atoi(argv[i + 1]);
			++i;
		}
	}
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
		return EXIT_FAILURE;
	}

	int serverAddrLen = sizeof(sockaddr_in);
	struct sockaddr_in serverAddr1, serverAddr2;

	int serverSocket1 = initializeSocket(2223, serverAddr1);
	if (serverSocket1 == INVALID_SOCKET) {
		WSACleanup();
		return EXIT_FAILURE;
	}

	int serverSocket2 = initializeSocket(2224, serverAddr2);
	if (serverSocket2 == INVALID_SOCKET) {
		closesocket(serverSocket1);
		WSACleanup();
		return EXIT_FAILURE;
	}

	const char* CLASS_NAME = "OverlayWindowClass";
	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent background

	RegisterClass(&wc);
	HWND hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
		CLASS_NAME,
		"Overlay",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hwnd) {
		std::cerr << "[Overlay] Failed to create overlay window";
		return 1;
	}

	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY); // Make the window transparent

	ShowWindow(hwnd, SW_SHOW);

	InitD2D(hwnd);

	HWND targetWnd = FindWindow(NULL, TARGET_WINDOW_NAME);
	if (!targetWnd) {
		std::cerr << "[Overlay] Target window not found!\n";
	}
	std::thread traccarThread(TraccarUpdate);

	static std::vector<uint8_t> buffer;
	while (true)
	{
#ifndef NO_CONTROLLER
		controller.Poll();
		controller.Deadzone();
#endif
		sockaddr_in clientAddr1;
		sockaddr_in clientAddr2;
		{
			int clientAddrLen1 = sizeof(clientAddr1);

#ifndef NO_CONTROLLER
			std::string messageToSend = controller.CreatePayload();
#else
			std::string messageToSend = "CRSF\n";
#endif

			memset(rxbuffer1, 0, sizeof(rxbuffer1));
			int bytesRead = recvfrom(serverSocket1, (char*)rxbuffer1, sizeof(rxbuffer1), 0, (struct sockaddr*)&clientAddr1, &clientAddrLen1);
			if (bytesRead == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					LPSTR errorMessage = nullptr;
					FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, NULL);
					std::cerr << "Error receiving data from UDP socket: " << errorMessage;
				}
			}
			else
			{
				clientAddr2.sin_addr = clientAddr1.sin_addr;
				buffer.insert(buffer.end(), &rxbuffer1[0], &rxbuffer1[bytesRead]);
				CheckPayloads(buffer);
			}
			if (sendto(serverSocket1, messageToSend.c_str(), messageToSend.length(), 0, (struct sockaddr*)&clientAddr1, clientAddrLen1) == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				LPSTR errorMessage = nullptr;
				FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, NULL);
				std::cerr << "Error sending data to client: " << errorMessage;
			}
		}

		{
			
			int clientAddrLen2 = sizeof(clientAddr2);

			memset(rxbuffer2, 0, sizeof(rxbuffer2));
			int bytesRead = recvfrom(serverSocket2, (char*)rxbuffer2, sizeof(rxbuffer2), 0, (struct sockaddr*)&clientAddr2, &clientAddrLen2);
			if (bytesRead == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					LPSTR errorMessage = nullptr;
					FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, NULL);
					std::cerr << "Error receiving data from UDP socket: " << errorMessage;
				}
			}
			else
			{
				std::cout << "Client: " << rxbuffer2;
				clientAddr1.sin_addr = clientAddr2.sin_addr;
			}

			std::string messageToSend = "PI\n";
			/*
			if (sendto(serverSocket2, messageToSend.c_str(), messageToSend.length(), 0, (struct sockaddr*)&clientAddr2, clientAddrLen2) == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				LPSTR errorMessage = nullptr;
				FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorMessage, 0, NULL);
				std::cerr << "Error sending data to client: " << errorMessage;
			}
			*/
		}


		WindowUpdate(hwnd, targetWnd, TARGET_WINDOW_NAME);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	traccarThread.join();
	CleanupD2D();
	closesocket(serverSocket1);
	closesocket(serverSocket2);
	WSACleanup();
	return 0;
}


int initializeSocket(int port, struct sockaddr_in& serverAddr) {
	int serverSocket;

	// Create a socket
	if ((serverSocket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		std::cerr << "Socket creation error (port " << port << "): " << WSAGetLastError() << std::endl;
		return INVALID_SOCKET;
	}

	// Set up the sockaddr_in structure
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);

	const int enable = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable)) < 0) {
		std::cerr << "setsockopt(SO_REUSEADDR) failed (port " << port << "): " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		return INVALID_SOCKET;
	}

	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Binding failed (port " << port << "): " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		return INVALID_SOCKET;
	}

	u_long mode = 1; // Non-blocking mode
	if (ioctlsocket(serverSocket, FIONBIO, &mode) != NO_ERROR) {
		std::cerr << "ioctlsocket failed (port " << port << "): " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		return INVALID_SOCKET;
	}

	std::cout << "Server listening on port " << port << std::endl;
	return serverSocket; // Return the valid socket descriptor
}