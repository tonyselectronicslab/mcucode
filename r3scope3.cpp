#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <algorithm>

// --- CONFIGURATION ---
const char* COM_PORT = "\\\\.\\COM4"; // Change COM number here if needed
const DWORD BAUD_RATE = 1000000;     // 1 Mbps (matches Arduino sketch)
const int   NUM_SAMPLES = 800;
const float V_REF = 5.0f;

// Packet structure: 4-byte Header + 4-byte Duration + 800 Samples + 2-byte Footer = 810 bytes
const int PACKET_SIZE = 4 + 4 + NUM_SAMPLES + 2;

// Shared state
HANDLE hSerial = INVALID_HANDLE_VALUE;
std::vector<uint8_t> rxBuffer;
float    volts[NUM_SAMPLES] = { 0 };
uint32_t lastDurationUs = 6500;
bool     isPaused = false;
bool     hasSignal = false;

// --- SERIAL PORT INITIALIZATION ---
bool OpenSerialPort(const char* portName, DWORD baudRate) {
    hSerial = CreateFileA(portName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) return false;

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hSerial, &dcb)) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fDtrControl = DTR_CONTROL_ENABLE; // Resets Arduino cleanly on connect
    SetCommState(hSerial, &dcb);

    // Non-blocking read timeouts so UI stays at a smooth 60 FPS
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    SetCommTimeouts(hSerial, &timeouts);

    PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;
}

// --- READ & PARSE VERIFIED PACKETS ---
void PollSerialData() {
    if (hSerial == INVALID_HANDLE_VALUE || isPaused) return;

    uint8_t tempBuf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hSerial, tempBuf, sizeof(tempBuf), &bytesRead, NULL) && bytesRead > 0) {
        rxBuffer.insert(rxBuffer.end(), tempBuf, tempBuf + bytesRead);
    }

    // Prevent buffer lag by keeping only the most recent ~3 packets
    if (rxBuffer.size() > (size_t)(PACKET_SIZE * 4)) {
        rxBuffer.erase(rxBuffer.begin(), rxBuffer.end() - (PACKET_SIZE * 3));
    }

    // Search backward for the newest valid packet (Header + exact size + Footer)
    for (int i = (int)rxBuffer.size() - PACKET_SIZE; i >= 0; --i) {
        if (rxBuffer[i] == 0xFF && rxBuffer[i + 1] == 0x00 &&
            rxBuffer[i + 2] == 0xAA && rxBuffer[i + 3] == 0x55 &&
            rxBuffer[i + PACKET_SIZE - 2] == 0x55 &&
            rxBuffer[i + PACKET_SIZE - 1] == 0xAA) {

            uint32_t dur = *(uint32_t*)&rxBuffer[i + 4];
            if (dur > 1000 && dur < 100000) {
                lastDurationUs = dur;
                for (int s = 0; s < NUM_SAMPLES; ++s) {
                    volts[s] = (rxBuffer[i + 8 + s] * V_REF) / 255.0f;
                }
                hasSignal = true;
            }
            // Discard everything up to the end of this verified packet
            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + i + PACKET_SIZE);
            break;
        }
    }
}

// --- SIGNAL MEASUREMENTS (MIN, MAX, FREQ, DUTY) ---
void CalculateStats(float& vMin, float& vMax, float& freqHz, float& dutyPct) {
    vMin = 5.0f;
    vMax = 0.0f;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (volts[i] < vMin) vMin = volts[i];
        if (volts[i] > vMax) vMax = volts[i];
    }

    float vPP = vMax - vMin;
    if (vPP < 0.3f) {
        freqHz = 0.0f;
        dutyPct = 0.0f;
        return;
    }

    float mid = (vMax + vMin) * 0.5f;
    float hyst = vPP * 0.15f;
    int highCount = 0;

    std::vector<int> edges;
    bool armed = (volts[0] < (mid - hyst));

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        if (volts[i] > mid) highCount++;
        if (!armed && volts[i] < (mid - hyst)) {
            armed = true;
        }
        else if (armed && volts[i] >= (mid + hyst)) {
            edges.push_back(i);
            armed = false;
        }
    }

    dutyPct = (highCount * 100.0f) / NUM_SAMPLES;

    if (edges.size() >= 2) {
        float sampleRateHz = (NUM_SAMPLES / (float)lastDurationUs) * 1000000.0f;
        float avgPeriodSamples = (edges.back() - edges.front()) / (float)(edges.size() - 1);
        freqHz = sampleRateHz / avgPeriodSamples;
    }
    else {
        freqHz = 0.0f;
    }
}

// --- DOUBLE-BUFFERED GDI RENDERER ---
void RenderScope(HDC hdc, RECT clientRect) {
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    if (width <= 0 || height <= 0) return;

    // Create off-screen memory buffer (zero flicker)
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // 1. Dark background
    HBRUSH bgBrush = CreateSolidBrush(RGB(12, 14, 18));
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Plot margins
    int left = 60, right = width - 25, top = 55, bottom = height - 45;
    int plotW = right - left;
    int plotH = bottom - top;

    if (plotW > 50 && plotH > 50) {
        // 2. Graticule Grid (10 horizontal x 5 vertical divisions: 0V to 5V)
        HPEN gridPen = CreatePen(PS_DOT, 1, RGB(45, 55, 65));
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(0, 180, 210));
        HGDIOBJ oldPen = SelectObject(memDC, gridPen);

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(170, 185, 200));

        for (int gx = 0; gx <= 10; ++gx) {
            int x = left + (gx * plotW) / 10;
            MoveToEx(memDC, x, top, NULL);
            LineTo(memDC, x, bottom);

            char timeLbl[32];
            float tMs = ((lastDurationUs / 1000.0f) * gx) / 10.0f;
            sprintf_s(timeLbl, "%.2f", tMs);
            TextOutA(memDC, x - 12, bottom + 6, timeLbl, (int)strlen(timeLbl));
        }

        for (int gy = 0; gy <= 5; ++gy) {
            int y = top + (gy * plotH) / 5;
            MoveToEx(memDC, left, y, NULL);
            LineTo(memDC, right, y);

            char voltLbl[16];
            sprintf_s(voltLbl, "%d.0V", 5 - gy);
            TextOutA(memDC, 15, y - 8, voltLbl, (int)strlen(voltLbl));
        }

        SelectObject(memDC, borderPen);
        MoveToEx(memDC, left, top, NULL);
        LineTo(memDC, right, top);
        LineTo(memDC, right, bottom);
        LineTo(memDC, left, bottom);
        LineTo(memDC, left, top);

        // 3. Draw Waveform Trace
        if (hasSignal) {
            HPEN tracePen = CreatePen(PS_SOLID, 2, RGB(0, 255, 102));
            SelectObject(memDC, tracePen);

            for (int i = 0; i < NUM_SAMPLES; ++i) {
                int px = left + (i * plotW) / (NUM_SAMPLES - 1);
                float clampedV = std::max(0.0f, std::min(5.0f, volts[i]));
                int py = bottom - (int)((clampedV / 5.0f) * plotH);

                if (i == 0) MoveToEx(memDC, px, py, NULL);
                else        LineTo(memDC, px, py);
            }
            SelectObject(memDC, oldPen);
            DeleteObject(tracePen);
        }
        else {
            SelectObject(memDC, oldPen);
        }

        DeleteObject(gridPen);
        DeleteObject(borderPen);
    }

    // 4. Top Status & Measurements HUD
    HFONT hudFont = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Consolas");
    HGDIOBJ oldFont = SelectObject(memDC, hudFont);

    char hudText[256];
    if (hSerial == INVALID_HANDLE_VALUE) {
        SetTextColor(memDC, RGB(255, 80, 80));
        sprintf_s(hudText, "ERROR: Could not open COM4! Close Arduino IDE Serial Monitor & restart.");
    }
    else if (!hasSignal) {
        SetTextColor(memDC, RGB(255, 220, 60));
        sprintf_s(hudText, "Connected to COM4 @ 1 Mbps - Waiting for trigger...");
    }
    else {
        float vMin, vMax, freqHz, dutyPct;
        CalculateStats(vMin, vMax, freqHz, dutyPct);
        float rateKhz = (NUM_SAMPLES / (float)lastDurationUs) * 1000.0f;

        char freqStr[32];
        if (freqHz >= 1000.0f) sprintf_s(freqStr, "%.2f kHz", freqHz / 1000.0f);
        else if (freqHz > 0)   sprintf_s(freqStr, "%.1f Hz", freqHz);
        else                   sprintf_s(freqStr, "DC / Flat");

        SetTextColor(memDC, isPaused ? RGB(255, 170, 0) : RGB(0, 230, 255));
        sprintf_s(hudText, "%sFreq: %s | Duty: %.1f%% | Min: %.2fV | Max: %.2fV | Rate: %.1f kS/s  [SPACE: Pause | Q: Quit]",
            isPaused ? "[PAUSED] " : "", freqStr, dutyPct, vMin, vMax, rateKhz);
    }

    TextOutA(memDC, 15, 16, hudText, (int)strlen(hudText));

    // Axis Title
    SetTextColor(memDC, RGB(140, 160, 180));
    TextOutA(memDC, width / 2 - 45, height - 22, "Time (ms)", 9);

    SelectObject(memDC, oldFont);
    DeleteObject(hudFont);

    // Blit off-screen buffer to screen in one instant copy
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

// --- WINDOW PROCEDURE ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        OpenSerialPort(COM_PORT, BAUD_RATE);
        SetTimer(hwnd, 1, 16, NULL); // ~60 FPS refresh timer
        return 0;

    case WM_TIMER:
        PollSerialData();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_KEYDOWN:
        if (wParam == 'Q' || wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        else if (wParam == VK_SPACE) {
            isPaused = !isPaused;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        RenderScope(hdc, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // Handled by double-buffer to prevent flicker
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// --- ENTRY POINT ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "Win32ArduinoScopeClass";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(
        0, "Win32ArduinoScopeClass",
        "Native Win32 C++ Oscilloscope (120 kS/s)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1080, 560,
        NULL, NULL, hInstance, NULL
    );
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}