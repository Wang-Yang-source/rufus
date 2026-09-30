/* UI-only smoke test. Run only in an isolated Wine prefix; no disks are written. */
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include "../src/resource.h"
#include <stdio.h>
#include <string.h>
static DWORD child_pid;
static HWND main_dialog;
static void capture(HWND window, const char *name)
{
    RECT r;
    GetWindowRect(window, &r);
    int w = r.right - r.left, h = r.bottom - r.top;
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits;
    HBITMAP bitmap = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = SelectObject(dc, bitmap);
    BitBlt(dc, 0, 0, w, h, screen, r.left, r.top, SRCCOPY);
    BITMAPFILEHEADER file = {0};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(bi.bmiHeader);
    file.bfSize = file.bfOffBits + w * h * 4;
    FILE *out = fopen(name, "wb");
    if (out) {
        fwrite(&file, sizeof(file), 1, out);
        fwrite(&bi.bmiHeader, sizeof(bi.bmiHeader), 1, out);
        fwrite(bits, w * h * 4, 1, out);
        fclose(out);
    }
    printf("Capture %s: %dx%d, DPI %d, caption %d, background #%02x%02x%02x\n", name, w, h,
           GetDeviceCaps(screen, LOGPIXELSX), GetSystemMetrics(SM_CYCAPTION),
           GetRValue(GetSysColor(COLOR_BTNFACE)), GetGValue(GetSysColor(COLOR_BTNFACE)),
           GetBValue(GetSysColor(COLOR_BTNFACE)));
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
}
static BOOL CALLBACK inspect(HWND window, LPARAM unused)
{
    (void)unused;
    DWORD pid;
    char title[256];
    GetWindowThreadProcessId(window, &pid);
    if (pid != child_pid || !IsWindowVisible(window))
        return TRUE;
    GetWindowTextA(window, title, sizeof(title));
    if (strcmp(title, "TEST VERSION") == 0) {
        printf("Dismissed test version notice\n");
        PostMessageA(window, WM_COMMAND, IDOK, 0);
    } else if (GetDlgItem(window, IDYES) != NULL && GetDlgItem(window, IDNO) != NULL) {
        Sleep(500);
        capture(window, "warning.bmp");
        printf("Accepted locally built test executable in isolated prefix\n");
        PostMessageA(window, WM_COMMAND, IDYES, 0);
    } else if (strstr(title, "update policy")) {
        printf("Declined update check in isolated test\n");
        PostMessageA(window, WM_COMMAND, IDNO, 0);
    } else if (strncmp(title, "Rufus 4.", 8) == 0) {
        main_dialog = window;
    }
    return TRUE;
}
int main(int argc, char **argv)
{
    if (getenv("RUFUS_UI_TEST") == NULL || strcmp(getenv("RUFUS_UI_TEST"), "1") != 0) {
        fprintf(stderr, "Set RUFUS_UI_TEST=1 in an isolated test prefix.\n");
        return 2;
    }
    SetProcessDPIAware();
    STARTUPINFOA startup = {0};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process;
    char command[1024], title[256];
    if (argc != 3)
        return 2;
    HKEY settings;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Akeo Consulting\\Rufus", 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &settings, NULL) != ERROR_SUCCESS)
        return 2;
    RegSetValueExA(settings, "Locale", 0, REG_SZ, (const BYTE *)"zh-CN", 6);
    DWORD interval = (DWORD)-1;
    RegSetValueExA(settings, "UpdateCheckInterval", 0, REG_DWORD, (const BYTE *)&interval,
                   sizeof(interval));
    RegCloseKey(settings);
    snprintf(command, sizeof(command), "\"%s\" -g -l zh-CN", argv[1]);
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        printf("CreateProcess failed: %lu\n", GetLastError());
        return 3;
    }
    child_pid = process.dwProcessId;
    for (int i = 0; i < 100; i++) {
        EnumWindows(inspect, 0);
        if (main_dialog && IsWindowEnabled(main_dialog))
            break;
        if (WaitForSingleObject(process.hProcess, 200) == WAIT_OBJECT_0)
            break;
    }
    if (!main_dialog || !IsWindowEnabled(main_dialog)) {
        printf("Main dialog did not become ready\n");
        TerminateProcess(process.hProcess, 4);
        return 4;
    }
    GetWindowTextA(main_dialog, title, sizeof(title));
    printf("Ready main dialog: %s\n", title);
    SetForegroundWindow(main_dialog);
    Sleep(1000);
    capture(main_dialog, "main.bmp");
    HDC display = GetDC(NULL);
    int dpi = GetDeviceCaps(display, LOGPIXELSX);
    ReleaseDC(NULL, display);
    RECT main_rect;
    GetWindowRect(main_dialog, &main_rect);
    if (dpi != atoi(argv[2]) ||
        GetSystemMetrics(SM_CYCAPTION) > (main_rect.bottom - main_rect.top) / 8) {
        fprintf(stderr, "DPI/caption proportions failed\n");
        TerminateProcess(process.hProcess, 6);
        return 6;
    }
    POINT caption_point = {main_rect.right - MulDiv(24, dpi, 96),
                           main_rect.top + MulDiv(22, dpi, 96)};
    if (SendMessageA(main_dialog, WM_NCHITTEST, 0,
                     MAKELPARAM(caption_point.x, caption_point.y)) != HTCLOSE ||
        (GetWindowLongPtrA(main_dialog, GWL_STYLE) & WS_CAPTION)) {
        fprintf(stderr, "Custom caption hit test/style failed\n");
        TerminateProcess(process.hProcess, 9);
        return 9;
    }
    caption_point.x -= MulDiv(36, dpi, 96);
    if (SendMessageA(main_dialog, WM_NCHITTEST, 0,
                     MAKELPARAM(caption_point.x, caption_point.y)) != HTMINBUTTON) {
        fprintf(stderr, "Custom minimize hit test failed\n");
        TerminateProcess(process.hProcess, 9);
        return 9;
    }
    SendMessageA(main_dialog, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    Sleep(200);
    if (!IsIconic(main_dialog)) {
        TerminateProcess(process.hProcess, 9);
        return 9;
    }
    SendMessageA(main_dialog, WM_SYSCOMMAND, SC_RESTORE, 0);
    SetForegroundWindow(main_dialog);
    keybd_event(VK_TAB, 0, 0, 0);
    keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
    Sleep(200);
    GUITHREADINFO focus = {0};
    focus.cbSize = sizeof(focus);
    if (!GetGUIThreadInfo(GetWindowThreadProcessId(main_dialog, NULL), &focus) ||
        focus.hwndFocus == NULL || !IsChild(main_dialog, focus.hwndFocus)) {
        fprintf(stderr, "Keyboard navigation failed\n");
        TerminateProcess(process.hProcess, 9);
        return 9;
    }
    capture(main_dialog, "focus.bmp");
    HWND boot = GetDlgItem(main_dialog, IDC_BOOT_SELECTION);
    SendMessageA(boot, CB_SHOWDROPDOWN, TRUE, 0);
    Sleep(200);
    if (!SendMessageA(boot, CB_GETDROPPEDSTATE, 0, 0)) {
        fprintf(stderr, "Native dropdown failed\n");
        TerminateProcess(process.hProcess, 9);
        return 9;
    }
    capture(main_dialog, "dropdown.bmp");
    SendMessageA(boot, CB_SHOWDROPDOWN, FALSE, 0);
    printf("PASS: custom caption, minimize/restore, keyboard focus, native dropdown\n");
    HWND progress = GetDlgItem(main_dialog, IDC_PROGRESS);
    char progress_class[64];
    GetClassNameA(progress, progress_class, sizeof(progress_class));
    printf("Progress class %s, style %08llx, extended style %08llx\n", progress_class,
           (unsigned long long)GetWindowLongPtrA(progress, GWL_STYLE),
           (unsigned long long)GetWindowLongPtrA(progress, GWL_EXSTYLE));
    if ((GetWindowLongPtrA(progress, GWL_STYLE) & WS_BORDER) ||
        (GetWindowLongPtrA(progress, GWL_EXSTYLE) & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE))) {
        fprintf(stderr, "Progress retains native square non-client border\n");
        TerminateProcess(process.hProcess, 7);
        return 7;
    }

    SendMessageA(progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(progress, PBM_SETPOS, 0, 0);
    UpdateWindow(progress);
    RECT progress_rect;
    GetClientRect(progress, &progress_rect);
    int probe_x = MulDiv(12, dpi, 96), probe_y = progress_rect.bottom / 2;
    HDC dc = GetDC(progress);
    COLORREF corner = GetPixel(dc, 0, 0);
    if (corner != GetSysColor(COLOR_BTNFACE)) {
        fprintf(stderr, "Rounded progress corner retains a native border: %06lx\n", corner);
        ReleaseDC(progress, dc);
        TerminateProcess(process.hProcess, 7);
        return 7;
    }
    COLORREF pixel = GetPixel(dc, probe_x, probe_y);
    ReleaseDC(progress, dc);
    if (pixel != GetSysColor(COLOR_WINDOW)) {
        fprintf(stderr, "Progress background is not the theme background: %06lx\n", pixel);
        TerminateProcess(process.hProcess, 7);
        return 7;
    }
    const char *keys[] = {"ProgressColorNormal", "ProgressColorPaused", "ProgressColorError"};
    for (int state = PBST_NORMAL; state <= PBST_ERROR; state++) {
        DWORD expected = 0, size = sizeof(expected);
        if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Akeo Consulting\\Rufus", keys[state - 1],
                         RRF_RT_REG_DWORD, NULL, &expected, &size) != ERROR_SUCCESS)
            return 8;
        SendMessageA(progress, PBM_SETSTATE, state, 0);
        SendMessageA(progress, PBM_SETPOS, 50, 0);
        UpdateWindow(progress);
        Sleep(100);
        dc = GetDC(progress);
        pixel = GetPixel(dc, probe_x, probe_y);
        ReleaseDC(progress, dc);
        if (pixel != expected) {
            fprintf(stderr, "Progress state %d color mismatch: %06lx != %06lx\n", state, pixel,
                    expected);
            TerminateProcess(process.hProcess, 8);
            return 8;
        }
    }
    SendMessageA(progress, PBM_SETSTATE, PBST_NORMAL, 0);
    SendMessageA(progress, PBM_SETPOS, 0, 0);
    UpdateWindow(progress);
    printf("PASS: Chinese startup/main UI, DPI, themed progress background and 3 states\n");
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('T', 0, 0, 0);
    Sleep(100);
    keybd_event('T', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    Sleep(2500);
    PostMessageA(main_dialog, WM_CLOSE, 0, 0);
    if (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 5);
        return 5;
    }
    DWORD code;
    GetExitCodeProcess(process.hProcess, &code);
    printf("Rufus exit code: %lu\n", code);
    return code != 0;
}
