#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commdlg.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <array>
#include <string>
#include <atomic>
#include <mutex>

typedef std::lock_guard<std::mutex> MutexLock;
typedef std::vector<double> DoubleVec;

const int SAMPLE_COUNT = 1024;
const double SAMPLE_RATE = 500000.0; // 500 kSa/s (2 us per sample)
const double PI_VAL = 3.14159265358979323846;

// UI Control IDs
#define ID_COMBO_PORT   101
#define ID_BTN_REFRESH  102
#define ID_BTN_RUNSTOP  103
#define ID_COMBO_TIME   104
#define ID_COMBO_TRIG   105
#define ID_BTN_EDGE     106
#define ID_BTN_VIEW     107
#define ID_BTN_RANGE    108
#define ID_BTN_CSV      109

HWND g_hComboPort = NULL;
HWND g_hBtnRunStop = NULL;
HWND g_hComboTime = NULL;
HWND g_hComboTrig = NULL;
HWND g_hBtnEdge = NULL;
HWND g_hBtnView = NULL;
HWND g_hBtnRange = NULL;

std::vector<uint16_t> g_samples(SAMPLE_COUNT, 2048);
std::mutex g_dataMutex;
std::mutex g_portMutex;
std::wstring g_selectedPort = L"";

std::atomic<bool>  g_appRunning(true);
std::atomic<bool>  g_scopeRunning(true);
std::atomic<bool>  g_connected(false);
std::atomic<bool>  g_portChanged(false);
std::atomic<int>   g_usPerDiv(20);
std::atomic<float> g_trigVolts(1.65f);
std::atomic<bool>  g_trigRising(true);
std::atomic<bool>  g_trigDirty(true);
std::atomic<bool>  g_fftMode(false);
std::atomic<bool>  g_bipolar10V(false);

float AdcToDisplayVolts(uint16_t raw12) {
    float vPin = ((raw12 & 0x0FFF) * 3.3f) / 4095.0f;
    if (!g_bipolar10V.load()) return vPin;
    return (vPin - 1.65f) * 6.0606f;
}

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

void ComputeFFT1024(const std::vector<uint16_t>& snap, DoubleVec& magDb, double& peakFreqHz, int& peakBinOut, double& peakDbOut) {
    const int N = 1024;
    DoubleVec re(N, 0.0), im(N, 0.0);

    // Apply Hann window and remove DC offset
    double mean = 0.0;
    for (int i = 0; i < N; i++) mean += AdcToDisplayVolts(snap.at(i));
    mean /= N;

    for (int i = 0; i < N; i++) {
        double w = 0.5 * (1.0 - cos(2.0 * PI_VAL * i / (N - 1)));
        re.at(i) = (AdcToDisplayVolts(snap.at(i)) - mean) * w;
    }

    // In-place Radix-2 Cooley-Tukey FFT
    int j = 0;
    for (int i = 0; i < N; i++) {
        if (i < j) {
            std::swap(re.at(i), re.at(j));
            std::swap(im.at(i), im.at(j));
        }
        int m = N >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    for (int len = 2; len <= N; len <<= 1) {
        double ang = -2.0 * PI_VAL / len;
        double wlenRe = cos(ang), wlenIm = sin(ang);
        for (int i = 0; i < N; i += len) {
            double wRe = 1.0, wIm = 0.0;
            for (int k = 0; k < len / 2; k++) {
                double uRe = re.at(i + k), uIm = im.at(i + k);
                double vRe = re.at(i + k + len / 2) * wRe - im.at(i + k + len / 2) * wIm;
                double vIm = re.at(i + k + len / 2) * wIm + im.at(i + k + len / 2) * wRe;
                re.at(i + k) = uRe + vRe;
                im.at(i + k) = uIm + vIm;
                re.at(i + k + len / 2) = uRe - vRe;
                im.at(i + k + len / 2) = uIm - vIm;
                double nextWRe = wRe * wlenRe - wIm * wlenIm;
                wIm = wRe * wlenIm + wIm * wlenRe;
                wRe = nextWRe;
            }
        }
    }

    magDb.resize(N / 2);
    double maxDb = -120.0;
    int maxBin = 1;

    for (int k = 0; k < N / 2; k++) {
        double mag = sqrt(re.at(k) * re.at(k) + im.at(k) * im.at(k)) / (N / 4.0);
        double db = 20.0 * log10(mag + 1e-6);
        if (db < -80.0) db = -80.0;
        if (db > 20.0) db = 20.0;
        magDb.at(k) = db;

        if (k >= 1 && db > maxDb) {
            maxDb = db;
            maxBin = k;
        }
    }

    // Parabolic interpolation around peak bin for sub-bin accuracy
    double exactBin = (double)maxBin;
    if (maxBin > 1 && maxBin < (N / 2) - 1) {
        double yL = magDb.at(maxBin - 1);
        double yC = magDb.at(maxBin);
        double yR = magDb.at(maxBin + 1);
        double denom = (yL - 2.0 * yC + yR);
        if (fabs(denom) > 1e-9) {
            exactBin += 0.5 * (yL - yR) / denom;
        }
    }

    peakBinOut = maxBin;
    peakDbOut = maxDb;
    peakFreqHz = (exactBin * SAMPLE_RATE) / N;
}

void ExportCsv(HWND hwnd) {
    std::vector<wchar_t> fileName(MAX_PATH, 0);
    wcscpy_s(fileName.data(), fileName.size(), L"pico2_capture_1024.csv");

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"CSV Files (.csv)\0.csv\0All Files (.)\0*.*\0";
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";
    if (!GetSaveFileNameW(&ofn)) return;
    std::vector<uint16_t> snap;
    {
        MutexLock lock(g_dataMutex);
        snap = g_samples;
    }
    FILE* fp = NULL;
    if (_wfopen_s(&fp, fileName.data(), L"w") == 0 && fp) {
        fprintf(fp, "Sample,Time_us,Raw_ADC,Voltage_V\n");
        for (int i = 0; i < SAMPLE_COUNT; i++) {
            uint16_t raw = snap.at(i) & 0x0FFF;
            float volts = AdcToDisplayVolts(raw);
            fprintf(fp, "%d,%.1f,%u,%.4f\n", i, i * 2.0, raw, volts);
        }
        fclose(fp);
        MessageBoxW(hwnd, L"1024-sample waveform saved to CSV.", L"Pico 2 Scope", MB_OK | MB_ICONINFORMATION);
    }
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
    int padL = 65, padR = 24, padT = 36, padB = 34;
    int plotW = w - padL - padR;
    int plotH = h - padT - padB;
    if (plotW > 40 && plotH > 40) {
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
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(165, 180, 195));
        std::vector<wchar_t> textBuf(256, 0);
        std::vector<uint16_t> snap;
        {
            MutexLock lock(g_dataMutex);
            snap = g_samples;
        }
        std::wstring portCopy;
        {
            MutexLock lock(g_portMutex);
            portCopy = g_selectedPort;
        }
        if (g_fftMode.load()) {
            // --- 1024-PT FFT SPECTRUM VIEW (0 kHz to 250 kHz, 488.3 Hz/bin) ---
            for (int j = 0; j <= 5; j++) {
                int y = padT + (plotH * j) / 5;
                int db = 20 - j * 20;
                swprintf_s(textBuf.data(), textBuf.size(), L"%+d dBV", db);
                TextOutW(memDC, 6, y - 8, textBuf.data(), (int)wcslen(textBuf.data()));
            }
            for (int i = 0; i <= 5; i++) {
                int x = padL + (plotW * (i * 2)) / 10;
                int khz = i * 50;
                swprintf_s(textBuf.data(), textBuf.size(), L"%d kHz", khz);
                TextOutW(memDC, x - 20, padT + plotH + 8, textBuf.data(), (int)wcslen(textBuf.data()));
            }
            DoubleVec magDb;
            double peakFreqHz = 0.0, peakDb = -80.0;
            int peakBin = 1;
            ComputeFFT1024(snap, magDb, peakFreqHz, peakBin, peakDb);
            HPEN fftPen = CreatePen(PS_SOLID, 2, RGB(64, 196, 255));
            SelectObject(memDC, fftPen);
            int bins = (int)magDb.size();
            for (int k = 0; k < bins; k++) {
                int x = padL + (k * plotW) / (bins - 1);
                double norm = (magDb.at(k) + 80.0) / 100.0;
                int y = padT + plotH - (int)(norm * plotH);
                if (k == 0) MoveToEx(memDC, x, y, NULL);
                else LineTo(memDC, x, y);
            }
            // Highlight fundamental peak with a red marker dot
            if (g_connected && peakDb > -65.0) {
                int px = padL + (peakBin * plotW) / (bins - 1);
                int py = padT + plotH - (int)(((peakDb + 80.0) / 100.0) * plotH);
                HBRUSH dotBrush = CreateSolidBrush(RGB(255, 82, 82));
                HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, dotBrush);
                Ellipse(memDC, px - 4, py - 4, px + 4, py + 4);
                SelectObject(memDC, oldBrush);
                DeleteObject(dotBrush);
            }
            if (g_connected) {
                swprintf_s(textBuf.data(), textBuf.size(),
                    L"[1024-PT FFT] %s  |  Peak: %.2f kHz (%.1f dBV)  |  Res: 488 Hz/bin  |  Span: 0 - 250 kHz",
                    portCopy.c_str(), peakFreqHz / 1000.0, peakDb);
                SetTextColor(memDC, RGB(64, 196, 255));
            }
            else {
                swprintf_s(textBuf.data(), textBuf.size(), L"Disconnected (%s)", portCopy.empty() ? L"No Port" : portCopy.c_str());
                SetTextColor(memDC, RGB(255, 110, 90));
            }
            TextOutW(memDC, padL, 10, textBuf.data(), (int)wcslen(textBuf.data()));
            SelectObject(memDC, oldPen);
            DeleteObject(fftPen);
        }
        else {
            // --- TIME DOMAIN OSCILLOSCOPE VIEW ---
            bool bip = g_bipolar10V.load();
            for (int j = 0; j <= 4; j++) {
                int y = padT + (plotH * j) / 4;
                float volts = bip ? (10.0f - j * 5.0f) : (3.3f * (4 - j) / 4.0f);
                swprintf_s(textBuf.data(), textBuf.size(), bip ? L"%+.1fV" : L"%.2fV", volts);
                TextOutW(memDC, 8, y - 8, textBuf.data(), (int)wcslen(textBuf.data()));
            }
            int usDiv = g_usPerDiv.load();
            for (int i = 0; i <= 5; i++) {
                int x = padL + (plotW * (i * 2)) / 10;
                int us = i * 2 * usDiv;
                swprintf_s(textBuf.data(), textBuf.size(), L"%d us", us);
                TextOutW(memDC, x - 18, padT + plotH + 8, textBuf.data(), (int)wcslen(textBuf.data()));
            }
            float trigPinV = g_trigVolts.load();
            int trigY = padT + plotH - (int)((trigPinV / 3.3f) * plotH);
            HPEN trigPen = CreatePen(PS_DASH, 1, RGB(255, 210, 50));
            SelectObject(memDC, trigPen);
            MoveToEx(memDC, padL, trigY, NULL);
            LineTo(memDC, padL + plotW, trigY);
            SetTextColor(memDC, RGB(255, 210, 50));
            TextOutW(memDC, padL + plotW - 36, trigY - 16, L"TRIG", 4);
            uint16_t rawMin = 4095, rawMax = 0;
            for (int i = 0; i < SAMPLE_COUNT; i++) {
                uint16_t r = snap.at(i) & 0x0FFF;
                if (r < rawMin) rawMin = r;
                if (r > rawMax) rawMax = r;
            }
            float vMin = AdcToDisplayVolts(rawMin);
            float vMax = AdcToDisplayVolts(rawMax);
            double freqHz = (rawMax - rawMin > 200) ? MeasureFrequencyHz(snap, (rawMin + rawMax) / 2) : 0.0;
            int totalWindowUs = usDiv * 10;
            int visibleSamples = totalWindowUs / 2;
            if (visibleSamples > SAMPLE_COUNT) visibleSamples = SAMPLE_COUNT;
            if (visibleSamples < 2) visibleSamples = 2;
            HPEN tracePen = CreatePen(PS_SOLID, 2, RGB(0, 230, 118));
            SelectObject(memDC, tracePen);
            for (int i = 0; i < visibleSamples; i++) {
                uint16_t raw = snap.at(i) & 0x0FFF;
                int x = padL + (i * plotW) / (visibleSamples - 1);
                int y = padT + plotH - (int)((raw / 4095.0f) * plotH);
                if (i == 0) MoveToEx(memDC, x, y, NULL);
                else LineTo(memDC, x, y);
            }
            if (g_connected) {
                const wchar_t* stateStr = g_scopeRunning ? L"RUN" : L"STOP";
                const wchar_t* edgeStr = g_trigRising ? L"Rise" : L"Fall";
                float dispTrig = bip ? ((trigPinV - 1.65f) * 6.0606f) : trigPinV;
                swprintf_s(textBuf.data(), textBuf.size(),
                    L"[%s] %s  |  Freq: %.1f Hz  |  Vmin: %.2fV  Vmax: %.2fV  Vpp: %.2fV  |  Trig: %.2fV (%s)",
                    stateStr, portCopy.c_str(), freqHz, vMin, vMax, vMax - vMin, dispTrig, edgeStr);
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
        }
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
        g_hComboPort = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            10, 8, 85, 200, hwnd, (HMENU)ID_COMBO_PORT, NULL, NULL);
        SendMessageW(g_hComboPort, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND btnRefresh = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, 8, 58, 24, hwnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);
        SendMessageW(btnRefresh, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hBtnRunStop = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            164, 8, 52, 24, hwnd, (HMENU)ID_BTN_RUNSTOP, NULL, NULL);
        SendMessageW(g_hBtnRunStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hComboTime = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            224, 8, 92, 160, hwnd, (HMENU)ID_COMBO_TIME, NULL, NULL);
        SendMessageW(g_hComboTime, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"200 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"100 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"50 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"20 us/div");
        SendMessageW(g_hComboTime, CB_ADDSTRING, 0, (LPARAM)L"10 us/div");
        SendMessageW(g_hComboTime, CB_SETCURSEL, 3, 0); // Default: 20 us/div
        g_hComboTrig = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            324, 8, 76, 220, hwnd, (HMENU)ID_COMBO_TRIG, NULL, NULL);
        SendMessageW(g_hComboTrig, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"3.00 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"2.50 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"2.00 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"1.65 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"1.00 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"0.50 V");
        SendMessageW(g_hComboTrig, CB_ADDSTRING, 0, (LPARAM)L"0.25 V");
        SendMessageW(g_hComboTrig, CB_SETCURSEL, 3, 0);
        g_hBtnEdge = CreateWindowW(L"BUTTON", L"Rising", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            406, 8, 58, 24, hwnd, (HMENU)ID_BTN_EDGE, NULL, NULL);
        SendMessageW(g_hBtnEdge, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hBtnView = CreateWindowW(L"BUTTON", L"View: Time", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            474, 8, 82, 24, hwnd, (HMENU)ID_BTN_VIEW, NULL, NULL);
        SendMessageW(g_hBtnView, WM_SETFONT, (WPARAM)hFont, TRUE);
        g_hBtnRange = CreateWindowW(L"BUTTON", L"0..3.3V", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            564, 8, 72, 24, hwnd, (HMENU)ID_BTN_RANGE, NULL, NULL);
        SendMessageW(g_hBtnRange, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND btnCsv = CreateWindowW(L"BUTTON", L"Save CSV", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            644, 8, 74, 24, hwnd, (HMENU)ID_BTN_CSV, NULL, NULL);
        SendMessageW(btnCsv, WM_SETFONT, (WPARAM)hFont, TRUE);
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
            std::array<int, 5> scales = { 200, 100, 50, 20, 10 };
            if (idx >= 0 && idx < 5) {
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
        else if (id == ID_BTN_VIEW && code == BN_CLICKED) {
            bool nextFft = !g_fftMode.load();
            g_fftMode = nextFft;
            SetWindowTextW(g_hBtnView, nextFft ? L"View: FFT" : L"View: Time");
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (id == ID_BTN_RANGE && code == BN_CLICKED) {
            bool nextBip = !g_bipolar10V.load();
            g_bipolar10V = nextBip;
            SetWindowTextW(g_hBtnRange, nextBip ? L"+/-10V" : L"0..3.3V");
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (id == ID_BTN_CSV && code == BN_CLICKED) {
            ExportCsv(hwnd);
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
    HWND hwnd = CreateWindowW(L"Pico2ScopeClass", L"Raspberry Pi Pico 2 - 500 kSa/s Scope & 1024-pt FFT",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1020, 560,
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