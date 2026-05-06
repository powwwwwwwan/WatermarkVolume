#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include "resource.h"
#include <psapi.h>
#include <cmath>
#include <gdiplus.h>
#include <stdio.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// 全局变量
IAudioEndpointVolume* g_volume = nullptr;
HWND g_hwnd = nullptr;
int g_current_memory = 0;
int g_current_volume = 0;
float g_water_height = 0.0f;
float g_volume_height = 0.0f;
float g_wave_offset = 0.0f;
float g_target_water = 0.0f;
float g_target_volume = 0.0f;
POINT g_ptLast;
bool g_dragging = false;
bool g_is_muted = false;
int g_mode = 1;  // 1: 完全映射, 2: 基准偏移
int g_base_memory = 40;
int g_base_volume = 15;

HBITMAP g_hBitmap = nullptr;
HDC g_hMemDC = nullptr;
int g_width = 0, g_height = 0;

int GetMemoryPercent() {
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    return mem.dwMemoryLoad;
}

void SetVolume(float percent) {
    if (g_volume) {
        g_volume->SetMasterVolumeLevelScalar(percent / 100.0f, NULL);
    }
}

void SetMute(bool mute) {
    if (g_volume) {
        g_volume->SetMute(mute, NULL);
        g_is_muted = mute;
    }
}

bool GetMute() {
    if (g_volume) {
        BOOL mute;
        g_volume->GetMute(&mute);
        return mute == TRUE;
    }
    return false;
}

void InitAudio() {
    CoInitialize(NULL);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&g_volume);
}

// 根据模式计算目标音量
float CalculateTargetVolume(int memory) {
    if (g_mode == 1) {
        // 完全映射: 内存0% -> 音量0%, 100% -> 100%
        return (float)memory;
    }
    else {
        // 基准偏移模式: 以用户设定的基准点线性映射
        // 公式: 音量 = 基准音量 + (当前内存 - 基准内存) * 斜率
        // 边界: 内存0% -> 音量0%, 100% -> 100%
        float slope;
        if (g_base_memory <= 50) {
            slope = (100.0f - g_base_volume) / (100.0f - g_base_memory);
            if (memory >= g_base_memory) {
                float vol = g_base_volume + (memory - g_base_memory) * slope;
                return max(0.0f, min(100.0f, vol));
            }
            else {
                slope = g_base_volume / (float)g_base_memory;
                float vol = memory * slope;
                return max(0.0f, min(100.0f, vol));
            }
        }
        else {
            slope = g_base_volume / (float)g_base_memory;
            if (memory <= g_base_memory) {
                float vol = memory * slope;
                return max(0.0f, min(100.0f, vol));
            }
            else {
                slope = (100.0f - g_base_volume) / (100.0f - g_base_memory);
                float vol = g_base_volume + (memory - g_base_memory) * slope;
                return max(0.0f, min(100.0f, vol));
            }
        }
    }
}

// 显示模式选择菜单
void ShowModeMenu() {
    printf("\n");
    printf("========================================\n");
    printf("     Participation Award for the 999th Volume Control Design Competition by powan\n");
    printf("========================================\n");
    printf("\n");
    printf("Select Mode:\n");
    printf("  1. Full Mapping\n");
    printf("     0%% Volume -> 0%% Memory, 100%% Volume -> 100%% Memory\n");
    printf("     Volume and memory soar together\n");
    printf("\n");
    printf("  2. Custom Mapping\n");
    printf("     Uses your current volume and memory as the baseline to recalculate the linear change, okay~\n");
    printf("\n");
    printf("========================================\n");
    printf("Reply with a number!(1 or 2): ");

    int choice;
    scanf("%d", &choice);

    if (choice == 1) {
        g_mode = 1;
        printf("\n[OK] Volume is explosion!\n");
    }
    else {
        g_mode = 2;

        // 获取当前音量作为基准音量
        float current_vol;
        g_volume->GetMasterVolumeLevelScalar(&current_vol);
        int current_vol_percent = (int)(current_vol * 100);
        int current_mem = GetMemoryPercent();

        printf("\n[Current Status]\n");
        printf("  Volume: %d%%\n", current_mem);
        printf("  Memory: %d%%\n", current_vol_percent);
        printf("\nUse this? (y/n): ");

        char confirm;
        scanf(" %c", &confirm);

        if (confirm == 'y' || confirm == 'Y') {
            g_base_memory = current_mem;
            g_base_volume = current_vol_percent;
        }
        else {
            printf("Alright then, give me a volume number (0-100): ");
            scanf("%d", &g_base_memory);
            printf("Now give me a memory number (0-100): ");
            scanf("%d", &g_base_volume);
        }

        g_base_memory = max(0, min(100, g_base_memory));
        g_base_volume = max(0, min(100, g_base_volume));

        printf("\n[OK] Got it\n");
        printf("  基准: %d%% volume -> %d%% Memory\n", g_base_memory, g_base_volume);
    }

    printf("\nPress any key to start...");
    system("pause > nul");
}

void CreateDoubleBuffer(int width, int height) {
    if (g_hMemDC) DeleteDC(g_hMemDC);
    if (g_hBitmap) DeleteObject(g_hBitmap);
    g_width = width;
    g_height = height;
    HDC hdc = GetDC(g_hwnd);
    g_hMemDC = CreateCompatibleDC(hdc);
    g_hBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(g_hMemDC, g_hBitmap);
    ReleaseDC(g_hwnd, hdc);
}

void RenderToMemoryDC() {
    Graphics graphics(g_hMemDC);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

    int w = g_width, h = g_height;

    // 1. 完全透明背景
    SolidBrush clearBrush(Color(0, 0, 0, 0));
    graphics.FillRectangle(&clearBrush, 0, 0, w, h);

    // 2. 水位线高度
    int waterH = (int)(h * g_water_height / 100.0f);
    int waterTop = h - waterH;

    // 3. 水位下蓝色水体
    if (waterH > 0) {
        LinearGradientBrush waterBrush(
            Point(0, waterTop),
            Point(0, waterTop + waterH),
            Color(200, 60, 160, 255),
            Color(230, 20, 80, 200)
        );
        Rect waterRect(0, waterTop, w, waterH);
        graphics.FillRectangle(&waterBrush, waterRect);
    }

    // 4. 水面波纹
    float waveAmp = 2.0f + (g_water_height / 100.0f) * 1.5f;
    Pen wavePen(Color(220, 255, 255, 255), 1.2f);

    for (int px = 0; px <= w; px++) {
        float angle = px * 0.12f + g_wave_offset;
        float waveY = waterTop + sin(angle) * waveAmp;
        if (px > 0) {
            Gdiplus::Point p1(px - 1, (int)waveY);
            Gdiplus::Point p2(px, (int)waveY);
            graphics.DrawLine(&wavePen, p1, p2);
        }
    }

    // 5. 水面高光线
    Pen waterlinePen(Color(180, 255, 255, 255), 1.0f);
    Gdiplus::Point wp1(0, waterTop);
    Gdiplus::Point wp2(w, waterTop);
    graphics.DrawLine(&waterlinePen, wp1, wp2);

    // 6. 音量条参数
    int barW = 6;
    int barX = 6;
    int barY = 8;
    int barH = h - 20;

    // 7. 音量填充
    int visibleH = (int)(barH * min(100.0f, g_volume_height) / 100.0f);
    int fillTop = waterTop - visibleH;

    if (fillTop < barY) {
        fillTop = barY;
        visibleH = waterTop - barY;
    }

    if (visibleH > 0 && fillTop < waterTop) {
        int fillH = min(visibleH, max(0, waterTop - barY));
        if (fillH > 2) {
            Rect fillRect(barX, fillTop, barW, fillH);

            GraphicsPath* fillPath = new GraphicsPath();
            fillPath->AddArc(fillRect.X, fillRect.Y, 3, 3, 180, 90);
            fillPath->AddArc(fillRect.X + fillRect.Width - 3, fillRect.Y, 3, 3, 270, 90);
            fillPath->AddArc(fillRect.X + fillRect.Width - 3, fillRect.Y + fillRect.Height - 3, 3, 3, 0, 90);
            fillPath->AddArc(fillRect.X, fillRect.Y + fillRect.Height - 3, 3, 3, 90, 90);
            fillPath->CloseFigure();

            LinearGradientBrush fillBrush(
                Point(0, fillTop),
                Point(0, fillTop + fillH),
                Color(230, 255, 220, 120),
                Color(200, 255, 200, 80)
            );
            graphics.FillPath(&fillBrush, fillPath);
            delete fillPath;
        }
    }

    // 8. 音量条边框
    GraphicsPath* barPath = new GraphicsPath();
    barPath->AddArc(barX, barY, 4, 4, 180, 90);
    barPath->AddArc(barX + barW - 4, barY, 4, 4, 270, 90);
    barPath->AddArc(barX + barW - 4, barY + barH - 4, 4, 4, 0, 90);
    barPath->AddArc(barX, barY + barH - 4, 4, 4, 90, 90);
    barPath->CloseFigure();

    Pen borderPen(Color(180, 255, 210, 110), 1.0f);
    graphics.DrawPath(&borderPen, barPath);
    delete barPath;

    // 9. 字体
    Font font(L"Segoe UI", 10, FontStyleBold);
    Font smallFont(L"Segoe UI", 8, FontStyleBold);
    SolidBrush memBrush(Color(255, 255, 255, 255));
    SolidBrush volBrush(Color(230, 255, 220, 200));

    // 10. 内存数字 (水位下面一点，白色)
    wchar_t memText[32];
    wsprintfW(memText, L"%d%%", g_current_memory);
    RectF memRect;
    graphics.MeasureString(memText, -1, &font, PointF(0, 0), &memRect);
    float memX = (w - memRect.Width) / 2;
    float memY = waterTop + 4;
    if (memY + memRect.Height > h) memY = waterTop - 18;
    if (memY < 3) memY = waterTop + 4;
    graphics.DrawString(memText, -1, &font, PointF(memX, memY), &memBrush);

    // 11. 音量数字 (上方)
    wchar_t volText[32];
    wsprintfW(volText, L"%d%%", g_current_volume);
    RectF volRect;
    graphics.MeasureString(volText, -1, &font, PointF(0, 0), &volRect);
    float volX = (w - volRect.Width) / 2;
    float volY = 6;
    graphics.DrawString(volText, -1, &font, PointF(volX, volY), &volBrush);

    // 12. 模式指示
    Font modeFont(L"Segoe UI", 7, FontStyleRegular);
    SolidBrush modeBrush(Color(180, 200, 200, 200));
    if (g_mode == 1) {
        graphics.DrawString(L"M1", -1, &modeFont, PointF(w - 14, 125), &modeBrush);
    }
    else {
        graphics.DrawString(L"M2", -1, &modeFont, PointF(w - 14, 125), &modeBrush);
    }

    // 13. 静音指示器
    if (g_is_muted) {
        Font iconFont(L"Segoe UI", 10, FontStyleBold);
        SolidBrush muteBrush(Color(255, 200, 100, 100));
        graphics.DrawString(L"X", -1, &iconFont, PointF(w - 12, 4), &muteBrush);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_hMemDC) {
            BitBlt(hdc, 0, 0, g_width, g_height, g_hMemDC, 0, 0, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width > 0 && height > 0) {
            CreateDoubleBuffer(width, height);
        }
        return 0;
    }

    case WM_TIMER: {
        if (wParam == 1) {
            // 获取静音状态
            g_is_muted = GetMute();

            // 更新目标值
            g_target_water = (float)GetMemoryPercent();
            g_target_volume = CalculateTargetVolume((int)g_target_water);

            // 平滑动画
            g_water_height = g_water_height * 0.85f + g_target_water * 0.15f;
            g_volume_height = g_volume_height * 0.85f + g_target_volume * 0.15f;
            g_current_memory = (int)g_target_water;
            g_current_volume = (int)g_target_volume;

            // 锁定音量（不锁静音）
            if (!g_is_muted) {
                SetVolume(g_target_volume);
            }

            g_wave_offset += 0.12f;
            if (g_wave_offset > 6.28f * 10) g_wave_offset = 0;

            if (g_hMemDC) {
                RenderToMemoryDC();
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        g_dragging = true;
        SetCapture(hwnd);
        g_ptLast.x = GET_X_LPARAM(lParam);
        g_ptLast.y = GET_Y_LPARAM(lParam);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_dragging) {
            POINT pt;
            GetCursorPos(&pt);
            RECT rect;
            GetWindowRect(hwnd, &rect);
            SetWindowPos(hwnd, NULL,
                pt.x - g_ptLast.x,
                pt.y - g_ptLast.y,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        g_dragging = false;
        ReleaseCapture();
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_hMemDC) DeleteDC(g_hMemDC);
        if (g_hBitmap) DeleteObject(g_hBitmap);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL);

    InitAudio();

    // 显示模式选择菜单
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    ShowModeMenu();

    FreeConsole();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = L"MemVolClass";
    RegisterClassExW(&wc);

    int w = 70, h = 140;
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"MemVolClass", L"Memory Volume", WS_POPUP,
        0, 0, w, h, NULL, NULL, hInstance, NULL
    );


    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    HRGN hRgn = CreateRoundRectRgn(0, 0, w, h, 6, 6);
    SetWindowRgn(g_hwnd, hRgn, TRUE);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(g_hwnd, HWND_TOPMOST, screenW - w - 15, screenH - h - 50,
        w, h, SWP_NOZORDER);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    CreateDoubleBuffer(w, h);
    SetTimer(g_hwnd, 1, 30, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}