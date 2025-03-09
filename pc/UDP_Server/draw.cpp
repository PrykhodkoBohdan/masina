#include "draw.h"
extern double homeLat, homeLon, pinLat, pinLon;
extern char rxbuffer2[1024];
extern Controller controller;
ID2D1Factory* pFactory = NULL;
ID2D1HwndRenderTarget* pRenderTarget = NULL;
IDWriteFactory* pDWriteFactory = NULL;
IDWriteTextFormat* pTextFormat = NULL;
ID2D1SolidColorBrush* pBrush = NULL;
ID2D1SolidColorBrush* pOutlineBrush = NULL;
void ResizeRenderTarget(HWND hwnd)
{
	if (pRenderTarget)
	{
		RECT rc;
		GetClientRect(hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(
			rc.right - rc.left,
			rc.bottom - rc.top
		);

		pRenderTarget->Resize(size);
	}
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_PAINT:
		DrawOverlay(hwnd,rxbuffer2);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		ResizeRenderTarget(hwnd);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


void WindowUpdate(HWND hwnd, HWND targetWnd, const char* windowName)
{
	InvalidateRect(hwnd, 0, true);
	MSG msg = { };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (!targetWnd)
	{
		std::cerr << "[Overlay] Target window not found!" << std::endl;
		targetWnd = FindWindow(NULL, windowName);
		ShowWindow(hwnd, SW_HIDE);
	}

	if (targetWnd)
	{
		// Check if target window is in the foreground
		HWND foregroundWnd = GetForegroundWindow();
		if (foregroundWnd == targetWnd)
		{
			ShowWindow(hwnd, SW_SHOW);  // Show overlay
		}
		else
		{
			ShowWindow(hwnd, SW_HIDE);  // Hide overlay
			return; // Skip positioning if hidden
		}

		RECT rect;
		if (GetWindowRect(targetWnd, &rect))
		{
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;
			SetWindowPos(hwnd, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
		}
	}
}


void DrawOutlineText(const wchar_t* text, D2D1_RECT_F rect)
{
	D2D1_RECT_F outlineRect = rect;
	outlineRect.left -= 1;
	pRenderTarget->DrawText(text, wcslen(text), pTextFormat, outlineRect, pOutlineBrush);
	outlineRect.left += 1;
	pRenderTarget->DrawText(text, wcslen(text), pTextFormat, outlineRect, pOutlineBrush);
	outlineRect.left -= 1;
	outlineRect.top -= 1;
	pRenderTarget->DrawText(text, wcslen(text), pTextFormat, outlineRect, pOutlineBrush);
	outlineRect.top += 2;
	pRenderTarget->DrawText(text, wcslen(text), pTextFormat, outlineRect, pOutlineBrush);
	pRenderTarget->DrawText(text, wcslen(text), pTextFormat, rect, pBrush);
}
void DrawOverlay(HWND hwnd, const char* telemetryStr) //DrawText already exists
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);

	if (!pRenderTarget)
	{
		HRESULT hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(
					DXGI_FORMAT_B8G8R8A8_UNORM,
					D2D1_ALPHA_MODE_IGNORE)
			),
			D2D1::HwndRenderTargetProperties(
				hwnd,
				D2D1::SizeU(
					ps.rcPaint.right - ps.rcPaint.left,
					ps.rcPaint.bottom - ps.rcPaint.top)
			),
			&pRenderTarget);

		if (SUCCEEDED(hr))
		{
			pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::White),
				&pBrush);

			pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(0x010101),
				&pOutlineBrush);
		}
	}

	pRenderTarget->BeginDraw();
	pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)); // Fully transparent background

	D2D1_RECT_F layoutRect = D2D1::RectF(
		static_cast<FLOAT>(ps.rcPaint.left),
		static_cast<FLOAT>(ps.rcPaint.top),
		static_cast<FLOAT>(ps.rcPaint.right),
		static_cast<FLOAT>(ps.rcPaint.bottom)
	);


	std::regex pattern("Temp: (\\d+) C, R: (\\d+) KB/s, T: (\\d+) KB/s, RSSI: (-?\\d+), SNR: (-?\\d+)");
	std::smatch matches;
	std::string telemetry(telemetryStr);
	std::lock_guard<std::mutex> lock(sharedMutex);
	static std::string QENG;
	if (std::regex_search(telemetry, matches, pattern))
	{
		tel.pi_temp = std::stoi(matches[1]);
		tel.pi_read_speed = std::stoi(matches[2]);
		tel.pi_write_speed = std::stoi(matches[3]);
		tel.pi_rssi = std::stoi(matches[4]);
		tel.pi_snr = std::stoi(matches[5]);

		//swprintf(telemetryBuffer, 128, L"CPU %4d °C\n↓ %4d KB/s\n↑ %4d KB/s\nRSSI: %4d\nSNR: %5d", temp, read_speed, write_speed, rssi, snr);
	}
	else if (telemetry.size() >= 6 && telemetry.compare(0, 6, "+QENG:") == 0) 
	{
		if (telemetry.size() >= 30) {
			QENG = telemetry.substr(30);
		}
	}
	static wchar_t leftBuffer[512], centerBuffer[512], rightBuffer[512], centerBottomBuffer[512];
	time_t currentTime;
	time(&currentTime);
	std::wstring bottomText;
	if (currentTime % 2)
	{
		if (((float)tel.voltage) / (serCells * 10) < 2.6f)
			bottomText = L"LAND NOW!!!";
		else if (((float)tel.voltage) / (serCells * 10) < 3.0f)
			bottomText = L"BATTERY LOW!";
	}
	int distanceToHome = calculateHaversine((double)tel.latitude / 10000000.0, (double)tel.longitude / 10000000.0, homeLat, homeLon);
	swprintf(leftBuffer, 512, L"🌡️%4d °C\n↓%4d KB/s\n↑%4d KB/s\n\n\n%4.1fV 🔋 %4.2fV\n%4.1fA  %4dmAh\n\nRSSI %4d\nSNR %5d",
		tel.pi_temp, tel.pi_read_speed, tel.pi_write_speed, ((float)tel.voltage) / 10, ((float)tel.voltage) / (10 * serCells), ((float)tel.current) / 10, tel.capacity, tel.pi_rssi, tel.pi_snr);
	swprintf(centerBuffer, 512, L"%s\n%s\n|\n\n\n\n%s", convert_to_wstring(tel.flightMode).c_str(), Compass(tel.heading / 100, tel.latitude, tel.longitude, homeLat, homeLon, pinLat, pinLon).c_str(), bottomText.c_str());
	swprintf(rightBuffer, 512, L"%s\n\nLat%10.6f\nLon%10.6f\n↕️%4dm 🧭%3d°\n🛰️%4d­­\n⏱%4d km/h\n%3.1f km/h/A\n🏠%5dm\n\n\nP %5.1f°\nR %5.1f°\nY %5.1f°", convert_to_wstring(ctime(&currentTime)).c_str(), ((float)tel.latitude) / 10000000.f, ((float)tel.longitude) / 10000000.f, tel.altitude, tel.heading / 100, tel.satellites, tel.groundspeed / 10, (tel.groundspeed / 10) / (((float)tel.current) / 10), distanceToHome, RADTODEG(tel.pitch) / 10000.f, RADTODEG(tel.roll) / 10000.f, RADTODEG(tel.yaw) / 10000.f);
	swprintf(centerBottomBuffer, 512, L"%s%s\n", controller.GetFlags().c_str(), convert_to_wstring(QENG).c_str());

	pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	DrawOutlineText(leftBuffer, layoutRect);
	pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	DrawOutlineText(centerBuffer, layoutRect);
	pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
	DrawOutlineText(rightBuffer, layoutRect);
	pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
	pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	DrawOutlineText(centerBottomBuffer, layoutRect);
	pRenderTarget->EndDraw();

	EndPaint(hwnd, &ps);
}

void InitD2D(HWND hwnd)
{
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
	pDWriteFactory->CreateTextFormat(
		L"Consolas",
		NULL,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		28.0f,
		L"",
		&pTextFormat);
}

void CleanupD2D()
{
	if (pBrush) pBrush->Release();
	if (pTextFormat) pTextFormat->Release();
	if (pDWriteFactory) pDWriteFactory->Release();
	if (pRenderTarget) pRenderTarget->Release();
	if (pFactory) pFactory->Release();
}
