// main.cpp - Ventana Win32 + contexto OpenGL (WGL) + bucle principal
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // completo (sin WIN32_LEAN_AND_MEAN): shellapi.h lo necesita
#include <shellapi.h>
#include "render/GL.h"
#include "render/GLExt.h"
#include "editor/Editor.h"
#include "core/Utils.h"
#include <string>

static Editor* gEditor = nullptr;
static int gWidth = 1600, gHeight = 900;
static bool gClosing = false;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!gEditor) return DefWindowProcW(hwnd, msg, wp, lp);
    Gui& g = gEditor->gui;
    switch (msg) {
    case WM_SIZE:
        gWidth = LOWORD(lp);
        gHeight = HIWORD(lp);
        return 0;
    case WM_MOUSEMOVE:
        g.inputMouseMove((short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hwnd);
        g.inputMouseMove((short)LOWORD(lp), (short)HIWORD(lp));
        g.inputMouseButton(0, true, msg == WM_LBUTTONDBLCLK);
        return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        SetCapture(hwnd);
        g.inputMouseButton(1, true, false);
        return 0;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        SetCapture(hwnd);
        g.inputMouseButton(2, true, false);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        int b = msg == WM_LBUTTONUP ? 0 : (msg == WM_RBUTTONUP ? 1 : 2);
        g.inputMouseButton(b, false, false);
        if (!(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) ReleaseCapture();
        return 0;
    }
    case WM_MOUSEWHEEL:
        g.inputWheel((short)HIWORD(wp) / 120.0f);
        return 0;
    case WM_CHAR:
        g.inputChar((unsigned int)wp);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        g.inputKey((int)wp);
        if (msg == WM_SYSKEYDOWN && wp != VK_F4) return 0; // evita que Alt active el menú del sistema
        break;
    case WM_SYSCHAR:
        return 0;
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp;
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH * 2];
            if (DragQueryFileW(drop, i, path, MAX_PATH * 2)) gEditor->dropFile(util::normalizeSlashes(util::narrow(path)));
        }
        DragFinish(drop);
        return 0;
    }
    case WM_CLOSE:
        if (gEditor->canQuit()) {
            gClosing = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Ubica la carpeta "assets" (junto al exe o en directorios superiores) y la usa como directorio de trabajo
static void chooseWorkingDir() {
    if (util::dirExists("assets")) return;
    std::string exe = util::exeDir();
    std::string candidates[3] = {exe, exe + "/..", exe + "/../.."};
    for (int i = 0; i < 3; ++i)
        if (util::dirExists(candidates[i] + "/assets")) {
            util::setCurrentDir(candidates[i]);
            return;
        }
    util::setCurrentDir(exe);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    typedef BOOL(WINAPI * SetDpiAwareFn)(void);
    HMODULE user = GetModuleHandleW(L"user32.dll");
    SetDpiAwareFn setDpiAware = user ? (SetDpiAwareFn)(void*)GetProcAddress(user, "SetProcessDPIAware") : NULL;
    if (setDpiAware) setDpiAware();
    chooseWorkingDir();

    HDC screen = GetDC(NULL);
    float uiScale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, screen);
    if (uiScale < 1.0f) uiScale = 1.0f;

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = L"Editor3D_PA4";
    RegisterClassExW(&wc);

    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int ww = (int)((work.right - work.left) * 0.92f), wh = (int)((work.bottom - work.top) * 0.92f);
    HWND hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, wc.lpszClassName, L"Editor 3D - Proyecto PA4", WS_OVERLAPPEDWINDOW,
                                work.left + ((work.right - work.left) - ww) / 2, work.top + ((work.bottom - work.top) - wh) / 2,
                                ww, wh, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;

    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8; // necesario para sombras planas y contorno de selección
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(dc, &pfd);
    if (!pf || !SetPixelFormat(dc, pf, &pfd)) {
        MessageBoxW(hwnd, L"No se pudo configurar el formato de píxel OpenGL.", L"Editor 3D", MB_ICONERROR);
        return 1;
    }
    HGLRC rc = wglCreateContext(dc);
    if (!rc || !wglMakeCurrent(dc, rc)) {
        MessageBoxW(hwnd, L"No se pudo crear el contexto OpenGL.", L"Editor 3D", MB_ICONERROR);
        return 1;
    }
    loadGLExtensions();
    setVSync(true);

    static Editor editor;
    if (!editor.init(hwnd, dc, uiScale)) {
        MessageBoxW(hwnd, L"No se pudo inicializar el editor.", L"Editor 3D", MB_ICONERROR);
        return 1;
    }
    gEditor = &editor;

    RECT cr;
    GetClientRect(hwnd, &cr);
    gWidth = cr.right;
    gHeight = cr.bottom;
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    std::string lastTitle;
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running || gClosing) break;
        if (IsIconic(hwnd)) { Sleep(30); QueryPerformanceCounter(&last); continue; }
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        editor.frame(dt, gWidth, gHeight);
        SwapBuffers(dc);
        std::string t = editor.title();
        if (t != lastTitle) {
            SetWindowTextW(hwnd, util::widen(t).c_str());
            lastTitle = t;
        }
        if (editor.wantsQuit()) {
            gClosing = true;
            DestroyWindow(hwnd);
        }
    }
    gEditor = nullptr;
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    return 0;
}
