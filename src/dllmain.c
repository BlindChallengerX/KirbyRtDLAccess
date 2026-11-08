/*
 * Kirby Access - screen reader support and audio cues for Kirby's Return to
 * Dream Land on the Kirby Wii Recomp PC port (KirbyRecomp.exe, ModernGekko).
 *
 * KirbyAccess\KirbyAccess.dll is loaded into the game by "Play Kirby
 * Access.exe" (launch.c). DllMain only starts the mod thread.
 */
#include "access.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

wchar_t g_game_dir[MAX_PATH];
wchar_t g_mod_dir[MAX_PATH];
static FILE *g_logf;
static CRITICAL_SECTION g_loglock;
static long g_log_bytes;

char *to_utf8(const wchar_t *w) {
    if (!w) return NULL;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = (char *)malloc(n > 0 ? (size_t)n : 1);
    if (!s) return NULL;
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL); else s[0] = 0;
    return s;
}

wchar_t *from_utf8(const char *s) {
    if (!s) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = (wchar_t *)malloc(sizeof(wchar_t) * (n > 0 ? (size_t)n : 1));
    if (!w) return NULL;
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n); else w[0] = 0;
    return w;
}

void log_line(const char *fmt, ...) {
    if (!g_logf) return;
    EnterCriticalSection(&g_loglock);
    if (g_log_bytes < 50L * 1024 * 1024) {
        SYSTEMTIME st; GetLocalTime(&st);
        g_log_bytes += fprintf(g_logf, "%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        va_list ap; va_start(ap, fmt);
        g_log_bytes += vfprintf(g_logf, fmt, ap);
        va_end(ap);
        fputc('\n', g_logf);
        fflush(g_logf);
    }
    LeaveCriticalSection(&g_loglock);
}

int game_has_focus(void) {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

/* Mod keys: F1-F8 and F11/F12 (the port's own hotkeys are in its ImGui
 * overlay; the Wii Remote is mapped to the controller). */
static const int k_keys[] = { VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F11, VK_F12 };
#define NKEYS (int)(sizeof k_keys / sizeof k_keys[0])
void pad_poll(void);
void camera_key(int vk);

static DWORD WINAPI mod_thread(LPVOID unused) {
    (void)unused;
    speech_init();
    audio_init();
    if (!guest_init()) {
        speak(L"Kirby Access could not find the game's code. It is turned off.", 0);
        return 0;
    }
    while (!guest_ready()) Sleep(50);
    msg_init();
    game_init();
    speak(L"Kirby Access loaded. Press F1 for keys.", 0);
    int down[NKEYS] = { 0 };
    for (;;) {
        Sleep(16);
        if (game_has_focus()) {
            int shift = GetAsyncKeyState(VK_SHIFT) < 0, ctrl = GetAsyncKeyState(VK_CONTROL) < 0;
            for (int i = 0; i < NKEYS; i++) {
                int d = GetAsyncKeyState(k_keys[i]) < 0;
                if (d && !down[i]) game_key(k_keys[i], shift, ctrl);
                down[i] = d;
            }
            /* tile camera keys (repeat while held, after a short wait) */
            static const int ck[] = { 'I', 'J', 'K', 'L', 'U', 'O' };
            static DWORD cnext[6]; static int cdown[6];
            for (int i = 0; i < 6; i++) {
                int d = GetAsyncKeyState(ck[i]) < 0 && !shift && !ctrl && !g_learn_open;
                DWORD t = GetTickCount();
                if (d && !cdown[i]) { camera_key(ck[i]); cnext[i] = t + 350; }
                else if (d && i < 4 && (int)(t - cnext[i]) >= 0) { camera_key(ck[i]); cnext[i] = t + 170; }
                cdown[i] = d;
            }
        }
        {   /* any keyboard key down (it makes NVDA stop talking): for speak_long */
            static unsigned char was[256]; int any = 0, pressed = 0;
            if (game_has_focus()) for (int vk = 0x08; vk <= 0xDF; vk++) {
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;   /* the L/R versions are checked */
                int d = GetAsyncKeyState(vk) < 0;
                if (d) { any = 1; if (!was[vk] && !pressed) pressed = vk; }
                was[vk] = (unsigned char)d;
            }
            speech_long_tick(pressed, any);
        }
        pad_poll();
        game_poll();
        text_poll();
        movie_poll();
        startup_poll();
        tools_poll();
    }
    return 0;
}

static void start(void) {
    wchar_t name[64];
    swprintf(name, 64, L"KirbyAccess_running_%lu", GetCurrentProcessId());
    HANDLE once = CreateMutexW(NULL, FALSE, name);
    if (once && GetLastError() == ERROR_ALREADY_EXISTS) return;
    GetModuleFileNameW(NULL, g_game_dir, MAX_PATH);
    wchar_t *slash = wcsrchr(g_game_dir, L'\\');
    if (slash) slash[1] = 0;
    swprintf(g_mod_dir, MAX_PATH, L"%lsKirbyAccess\\", g_game_dir);
    InitializeCriticalSection(&g_loglock);
    wchar_t logpath[MAX_PATH];
    swprintf(logpath, MAX_PATH, L"%lsKirbyAccess.log", g_mod_dir);
    g_logf = _wfopen(logpath, L"w");
    log_line("Kirby Access " MOD_VERSION " starting");
    CloseHandle(CreateThread(NULL, 0, mod_thread, NULL, 0, NULL));
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(inst); start(); }
    return TRUE;
}
