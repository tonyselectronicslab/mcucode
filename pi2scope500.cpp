#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <array>
#include <string>
#include <atomic>
#include <mutex>

typedef std::lock_guard<std::mutex> MutexLock;

const int SAMPLE_COUNT = 500;
const double SAMPLE_RATE = 500000.0; // 500 kSa/s (2 us per sample)

// UI Control IDs
#define ID_COMBO_PORT   101
#define ID_BTN_REFRESH  102
#define ID_BTN_RUNSTOP  103
#define ID_COMBO_TIME   104
#define ID_COMBO_TRIG   105
#define ID_BTN_EDGE     106

HWND g_hComboPort = NULL;
HWND g_hBtnRunStop = NULL;
HWND g_hComboTime = NULL;
HWND g_hComboTrig = NULL;
HWND g_hBtnEdge = NULL;

std::vector<uint16_t> g_samples(SAMPLE_COUNT, 2048);
std::mutex g_dataMutex;
std::mutex g_portMutex;
std::wstring g_selectedPort = L"";

std::atomic<bool>  g_appRunning(true);
std::atomic<bool>  g_scopeRunning(true);
std::atomic<bool>  g_connected(false);
std::atomic<bool>  g_portChanged(false);
std::atomic<int>   g_usPerDiv(100);
std::atomic<float> g_trigVolts(1.65f);
std::atomic<bool>  g_trigRising(true);
std::atomic<bool>  g_trigDirty(true);

void PopulateComPorts(HWND hCombo) {
    std::wstring currentSel;
    int idx = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (idx != CB_ERR) {
        std::vector<wchar_t> buf(256, 0);
        SendMessageW(hCombo, CB_GETLBTEXT, idx, (LPARAM)buf.data());
        currentSel = buf.data();
    }

    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        std::vector<wchar_t> valueName(256, 0);
        std::vector<wchar_t> data(256, 0);
        DWORD i = 0, valSize, dataSize, type;
        while (true) {
            valSize = 256;
            dataSize = (DWORD)(data.size() * sizeof(wchar_t));
            if (RegEnumValueW(hKey, i++, valueName.data(), &valSize, NULL, &type, (LPBYTE)data.data(), &dataSize) != ERROR_SUCCESS)
                break;
            if (type == REG_SZ) {
                SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)data.data());
            }
        }
        RegCloseKey(hKey);
    }

    int count = (int)SendMessageW(hCombo, CB_GETCOUNT, 0, 0);
    if (count > 0) {
        int selectIdx = 0;
        if (!currentSel.empty()) {
            int found = (int)SendMessageW(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)currentSel.c_str());
            if (found != CB_ERR) selectIdx = found;
        }
        SendMessageW(hCombo, CB_SETCURSEL, selectIdx, 0);

        std::vector<wchar_t> chosen(256, 0);
        SendMessageW(hCombo, CB_GETLBTEXT, selectIdx, (LPARAM)chosen.data());
        MutexLock lock(g_portMutex);
        g_selectedPort = chosen.data();
        g_portChanged = true;
    }
}

void SendTriggerConfig(HANDLE hSerial) {
    float v = g_trigVolts.load();
    if (v < 0.0f) v = 0.0f;
    if (v > 3.3f) v = 3.3f;

    uint8_t lvlByte = (uint8_t)((v / 3.3f) * 255.0f + 0.5f);
    uint8_t edgeByte = g_trigRising.load() ? 1 : 0;

    std::array<uint8_t, 3> cmd = { 0xCC, edgeByte, lvlByte };
    DWORD written = 0;
    WriteFile(hSerial, cmd.data(), (DWORD)cmd.size(), &written, NULL);
    g_trigDirty = false;
}

DWORD WINAPI SerialReaderThread(LPVOID lpParam) {
    HWND hwnd = (HWND)lpParam;

    while (g_appRunning) {
        std::wstring portName;
        {
            MutexLock lock(g_portMutex);
            portName = g_selectedPort;
        }
        g_portChanged = false;

        if (portName.empty()) {
            g_connected = false;
            InvalidateRect(hwnd, NULL, FALSE);
            Sleep(300);
            continue;
        }

        std::wstring fullPath = L"\\\\.\\" + portName;
        HANDLE hSerial = CreateFileW(fullPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hSerial == INVALID_HANDLE_VALUE) {
            g_connected = false;
            InvalidateRect(hwnd, NULL, FALSE);
            Sleep(500);
            continue;
        }

        DCB dcb = { 0 };
        dcb.DCBlength = sizeof(dcb);
        GetCommState(hSerial, &dcb);
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        SetCommState(hSerial, &dcb);

        COMMTIMEOUTS timeouts = { 0 };
        timeouts.ReadIntervalTimeout = 20;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        SetCommTimeouts(hSerial, &timeouts);

        g_connected = true;
        g_trigDirty = true;

        uint8_t b = 0, prev = 0;
        std::vector<uint16_t> localBuf(SAMPLE_COUNT);

        while (g_appRunning && !g_portChanged) {
            if (g_trigDirty.load()) {
                SendTriggerConfig(hSerial);
            }

            DWORD bytesRead = 0;
            if (!ReadFile(hSerial, &b, 1, &bytesRead, NULL)) break;
            if (bytesRead == 0) continue;

            if (prev == 0xAA && b == 0x55) {
                size_t totalBytes = SAMPLE_COUNT * sizeof(uint16_t);
                size_t offset = 0;
                uint8_t* rawPtr = (uint8_t*)localBuf.data();

                while (offset < totalBytes && g_appRunning && !g_portChanged) {
                    DWORD chunk = 0;
                    if (!ReadFile(hSerial, rawPtr + offset, (DWORD)(totalBytes - offset), &chunk, NULL) || chunk == 0)
                        break;
                    offset += chunk;
                }

                if (offset == totalBytes && g_scopeRunning) {
                    {
                        MutexLock lock(g_dataMutex);
                        g_samples = localBuf;
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                prev = 0;
            }
            else {
                prev = b;
            }
        }
        CloseHandle(hSerial);
        g_connected = false;
    }
    return 0;
}

double MeasureFrequencyHz(const std::vector<uint16_t>& snap, uint16_t midLevel) {
    std::vector<int> risingEdges;
    for (int i = 1; i < (int)snap.size(); i++) {
        uint16_t v0 = snap.at(i - 1) & 0x0FFF;
        uint16_t v1 = snap.at(i) & 0x0FFF;
        if (v0 < midLevel && v1 >= midLevel) {
            risingEdges.push_back(i);
        }
    }
    if (risingEdges.size() < 2) return 0.0;

    double totalSamples = (double)(risingEdges.back() - risingEdges.front());
    double avgPeriodSamples = totalSamples / (double)(risingEdges.size() - 1);
    return SAMPLE_RATE / avgPeriodSamples;
}

void DrawScope(HDC hdc, RECT rc) {
    int topBarH = 42;
    rc.top += topBarH;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    RECT localRc = { 0, 0, w, h };
    HBRUSH bgBrush = CreateSolidBrush(RGB(16, 20, 24));
    FillRect(memDC, &localRc, bgBrush);
    DeleteObject(bgBrush);

    int padL = 60, padR = 24, padT = 36, padB = 34;
    int plotW = w - padL - padR;
    int plotH = h - padT - padB;

    if (plotW > 40 && plotH > 40) {
        // Graticule grid (10x8 divisions)
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(38, 48, 58));
        HPEN oldPen = (HPEN)SelectObject(memDC, gridPen);

        for (int i = 0; i <= 10; i++) {
            int x = padL + (plotW * i) / 10;
            MoveToEx(memDC, x, padT, NULL);
            LineTo(memDC, x, padT + plotH);
        }
        for (int j = 0; j <= 8; j++) {
            int y = padT + (plotH * j) / 8;
            MoveToEx(memDC, padL, y, NULL);
            LineTo(memDC, padL + plotW, y);
        }

        // Voltage Y-axis labels
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(165, 180, 195));
        std::vector<wchar_t> textBuf(256, 0);

        for (int j = 0; j <= 4; j++) {
            int y = padT + (plotH * j) / 4;
            float volts = 3.3f * (4 - j) / 4.0f;
            swprintf_s(textBuf.data(), textBuf.size(), L"%.2fV", volts);
            TextOutW(memDC, 10, y - 8, textBuf.data(), (int)wcslen(textBuf.data()));
        }

        // Time X-axis labels
        int usDiv = g_usPerDiv.load();
        for (int i = 0; i <= 5; i++) {
            int x = padL + (plotW * (i * 2)) / 10;
            int us = i * 2 * usDiv;
            swprintf_s(textBuf.data(), textBuf.size(), L"%d us", us);
            TextOutW(memDC, x - 18, padT + plotH + 8, textBuf.data(), (int)wcslen(textBuf.data()));
        }

        // Draw dashed yellow trigger level line
        float trigV = g_trigVolts.load();
        int trigY = padT + plotH - (int)((trigV / 3.3f) * plotH);
        HPEN trigPen = CreatePen(PS_DASH, 1, RGB(255, 210, 50));
        SelectObject(memDC, trigPen);
        MoveToEx(memDC, padL, trigY, NULL);
        LineTo(memDC, padL + plotW, trigY);

        SetTextColor(memDC, RGB(255, 210, 50));
        TextOutW(memDC, padL + plotW - 36, trigY - 16, L"TRIG", 4);

        std::vector<uint16_t> snap;
        {
            MutexLock lock(g_dataMutex);
            snap = g_samples;
        }

        // Compute Vmin, Vmax, and Frequency
        uint16_t rawMin = 4095, rawMax = 0;
        for (int i = 0; i < SAMPLE_COUNT; i++) {
            uint16_t r = snap.at(i) & 0x0FFF;
            if (r < rawMin) rawMin = r;
            if (r > rawMax) rawMax = r;
        }
        float vMin = (rawMin * 3.3f) / 4095.0f;
        float vMax = (rawMax * 3.3f) / 4095.0f;
        double freqHz = (rawMax - rawMin > 200) ? MeasureFrequencyHz(snap, (rawMin + rawMax) / 2) : 0.0;

        int totalWindowUs = usDiv * 10;
        int visibleSamples = totalWindowUs / 2;
        if (visibleSamples > SAMPLE_COUNT) visibleSamples = SAMPLE_COUNT;
        if (visibleSamples < 2) visibleSamples = 2;

        // Draw waveform trace
        HPEN tracePen = CreatePen(PS_SOLID, 2, RGB(0, 230, 118));
        SelectObject(memDC, tracePen);
        for (int i = 0; i < visibleSamples; i++) {
            uint16_t raw = snap.at(i) & 0x0FFF;
            int x = padL + (i * plotW) / (visibleSamples - 1);
            int y = padT + plotH - (int)((raw / 4095.0f) * plotH);
            if (i == 0) MoveToEx(memDC, x, y, NULL);
            else LineTo(memDC, x, y);
        }
        // Top Measurement & Status Readout
        std::wstring portCopy;
        {
            MutexLock lock(g_portMutex);
            portCopy = g_selectedPort;
        }
        if (g_connected) {
            const wchar_t* stateStr = g_scopeRunning ? L"RUN" : L"STOPPED";
            const wchar_t* edgeStr = g_trigRising ? L"Rising" : L"Falling";
            swprintf_s(textBuf.data(), textBuf.size(),
                L"[%s] %s | Freq: %.1f Hz | Vmin: %.2fV  Vmax: %.2fV  Vpp: %.2fV | Trig: %.2fV (%s)",
                stateStr, portCopy.c_str(), freqHz, vMin, vMax, vMax - vMin, trigV, edgeStr);
            SetTextColor(memDC, g_scopeRunning ? RGB(0, 230, 118) : RGB(255, 200, 60));
        }
        else {
            swprintf_s(textBuf.data(), textBuf.size(),
                L"Disconnected (%s) — Select Pico 2 COM port above and close Arduino Serial Monitor",
                portCopy.empty() ? L"No Port" : portCopy.c_str());
            SetTextColor(memDC, RGB(255, 110, 90));
        }
        TextOutW(memDC, padL, 10, textBuf.data(), (int)wcslen(textBuf.data()));
        SelectObject(memDC, oldPen);
        DeleteObject(tracePen);
        DeleteObject(trigPen);
        DeleteObject(gridPen);
    }
    BitBlt(hdc, 0, topBarH, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND lblPort = CreateWindowW(L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE,
            12, 12, 35, 20, hwnd, NULL, NULL, NULL);
        SendMessageW(lblPort, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hComboPort = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            48, 8, 95, 200, hwnd, (HMENU)ID_COMBO_PORT, NULL, NULL);
        SendMessageW(g_hComboPort, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND btnRefresh = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            150, 8, 65, 24, hwnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);
        SendMessageW(btnRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hBtnRunStop = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            224, 8, 65, 24, hwnd, (HMENU)ID_BTN_RUNSTOP, NULL, NULL);
        SendMessageW(g_hBtnRunStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND lblTime = CreateWindowW(L"STATIC", L"Time/Div:", WS_CHILD | WS_VISIBLE,
            305, 12, 58, 20, hwnd, NULL, NULL, NULL);
        SendMessageW(lblTime, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hComboTime = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            365, 8, 95, 150, hwnd, (HMENU)ID_COMBO_TIME, NULL, NULL);
        SendMessageW(g_hComboTime, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"100 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"50 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"20 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"10 us/div");
        SendMessageW(g_hComboTime, CB_SETCURSEL, 0, 0);
        HWND lblTrig = CreateWindowW(L"STATIC", L"Trigger:", WS_CHILD | WS_VISIBLE,
            476, 12, 48, 20, hwnd, NULL, NULL, NULL);
        SendMessageW(lblTrig, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hComboTrig = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            526, 8, 80, 220, hwnd, (HMENU)ID_COMBO_TRIG, NULL, NULL);
        SendMessageW(g_hComboTrig, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"3.00 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"2.50 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"2.00 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"1.65 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"1.00 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"0.50 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"0.25 V");
        SendMessageW(g_hComboTrig, CB_SETCURSEL, 3, 0); // Default: 1.65 V
        g_hBtnEdge = CreateWindowW(L"BUTTON", L"Rising", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            614, 8, 70, 24, hwnd, (HMENU)ID_BTN_EDGE, NULL, NULL);
        SendMessageW(g_hBtnEdge, WM_SETFONT, (WPARAM)hFont, TRUE);
        PopulateComPorts(g_hComboPort);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == ID_BTN_REFRESH && code == BN_CLICKED) {
            PopulateComPorts(g_hComboPort);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (id == ID_COMBO_PORT && code == CBN_SELCHANGE) {
            int idx = (int)SendMessageW(g_hComboPort, CB_GETCURSEL, 0, 0);
            if (idx != CB_ERR) {
                std::vector<wchar_t> chosen(256, 0);
                SendMessageW(g_hComboPort, CB_GETLBTEXT, idx, (LPARAM)chosen.data());
                MutexLock lock(g_portMutex);
                g_selectedPort = chosen.data();
                g_portChanged = true;
            }
        }
        else if (id == ID_BTN_RUNSTOP && code == BN_CLICKED) {
            bool nextState = !g_scopeRunning.load();
            g_scopeRunning = nextState;
            SetWindowTextW(g_hBtnRunStop, nextState ? L"Stop" : L"Run");
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (id == ID_COMBO_TIME && code == CBN_SELCHANGE) {
            int idx = (int)SendMessageW(g_hComboTime, CB_GETCURSEL, 0, 0);
            std::array<int, 4> scales = { 100, 50, 20, 10 };
            if (idx >= 0 && idx < 4) {
                g_usPerDiv = scales.at(idx);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (id == ID_COMBO_TRIG && code == CBN_SELCHANGE) {
            int idx = (int)SendMessageW(g_hComboTrig, CB_GETCURSEL, 0, 0);
            std::array<float, 7> levels = { 3.00f, 2.50f, 2.00f, 1.65f, 1.00f, 0.50f, 0.25f };
            if (idx >= 0 && idx < 7) {
                g_trigVolts = levels.at(idx);
                g_trigDirty = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (id == ID_BTN_EDGE && code == BN_CLICKED) {
            bool nextRising = !g_trigRising.load();
            g_trigRising = nextRising;
            g_trigDirty = true;
            SetWindowTextW(g_hBtnEdge, nextRising ? L"Rising" : L"Falling");
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        RECT topRc = { 0, 0, rc.right, 42 };
        FillRect(hdc, &topRc, (HBRUSH)(COLOR_BTNFACE + 1));
        DrawScope(hdc, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        g_appRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"Pico2ScopeClass";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(L"Pico2ScopeClass", L"Raspberry Pi Pico 2 - 500 kSa/s DMA Oscilloscope",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 540,
        NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nCmdShow);
    HANDLE hThread = CreateThread(NULL, 0, SerialReaderThread, hwnd, 0, NULL);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_appRunning = false;
    WaitForSingleObject(hThread, 1000);
    CloseHandle(hThread);
    return 0;
}