/*
 * "Play Kirby Access.exe": starts the Kirby Wii Recomp game (KirbyRecomp.exe)
 * and loads the mod (KirbyAccess\KirbyAccess.dll) into it. It replaces the
 * port's own launcher window, which a screen reader cannot read.
 *
 * The game folder and user folder are the ones the port's launcher set up:
 * %LOCALAPPDATA%\kirby-wii-recomp\default-game.txt names the extracted game.
 * If that is missing (first start), it looks for a Kirby's Return to Dream
 * Land .wbfs or .iso next to this program, then in the Downloads folder,
 * and has the port's launcher extract it ("KirbyWiiRecomp.exe --extract"),
 * which is what the launcher's own Browse button does.
 *
 * Arguments are passed on to the game. "--user-dir DIR" picks another
 * settings/saves folder (used for testing).
 *
 * What it did is written to KirbyAccess\launch.log.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#include <wininet.h>
#include <bcrypt.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <ctype.h>
#include "access.h"   /* MOD_VERSION */

static wchar_t g_dir[MAX_PATH];
static FILE *g_log;

static void logw(const wchar_t *fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fwprintf(g_log, L"%04d-%02d-%02d %02d:%02d:%02d ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    va_list ap; va_start(ap, fmt); vfwprintf(g_log, fmt, ap); va_end(ap);
    fputwc(L'\n', g_log); fflush(g_log);
}

static void fail(const wchar_t *msg) {
    logw(L"error: %ls", msg);
    MessageBoxW(NULL, msg, L"Kirby Access", MB_OK | MB_ICONWARNING);
}

static int read_line(const wchar_t *path, wchar_t *out, int cap);

/* default-game.txt names an extracted game that is really there. */
static int game_ok(const wchar_t *def, wchar_t *game) {
    if (!read_line(def, game, MAX_PATH)) return 0;
    wchar_t dol[MAX_PATH];
    swprintf(dol, MAX_PATH, L"%ls/sys/main.dol", game);
    return GetFileAttributesW(dol) != INVALID_FILE_ATTRIBUTES;
}

/* The port shows frames per second in the window title by default; a screen
 * reader reads every change of the title, so turn it off in config.ini. */
static void fps_title_off(const wchar_t *user) {
    wchar_t ini[MAX_PATH];
    swprintf(ini, MAX_PATH, L"%ls/config.ini", user);
    FILE *f = _wfopen(ini, L"rb");
    if (!f) return;
    char buf[16384]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f);
    buf[n] = 0;
    static const char on[] = "show_fps_in_title=true", off[] = "show_fps_in_title=false";
    char *p = strstr(buf, on);
    if (!p) return;
    f = _wfopen(ini, L"wb");
    if (!f) return;
    fwrite(buf, 1, (size_t)(p - buf), f);
    fwrite(off, 1, sizeof off - 1, f);
    fputs(p + sizeof on - 1, f);
    fclose(f);
    logw(L"turned off the FPS display in the window title");
}

/* Kirby only takes a Wii Remote on its own. If the port's controller profile
 * attaches an extension (Nunchuk, Classic Controller...), the title screen
 * stops on "Please disconnect the unsupported external extension controller
 * from the Wii Remote." Set every "Extension = ..." line to None. */
static void no_extension(const wchar_t *user) {
    wchar_t ini[MAX_PATH];
    swprintf(ini, MAX_PATH, L"%ls/Config/WiimoteNew.ini", user);
    FILE *f = _wfopen(ini, L"rb");
    if (!f) return;
    static char buf[65536], out[65536 + 4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f);
    if (n == sizeof buf - 1) return;               /* not a profile we know */
    buf[n] = 0;
    size_t o = 0; int changed = 0;
    for (char *p = buf; *p; ) {
        char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p + 1) : strlen(p);
        char *q = p;
        int key = strncmp(q, "Extension", 9) == 0;
        if (key) { q += 9; while (*q == ' ' || *q == '\t') q++; key = *q == '='; }
        if (key) {
            q++; while (*q == ' ' || *q == '\t') q++;
            if (strncmp(q, "None", 4) != 0) {
                const char *nl = (eol && eol > p && eol[-1] == '\r') ? "\r\n" : "\n";
                o += (size_t)sprintf(out + o, "Extension = None%s", eol ? nl : "");
                changed = 1;
                p += len;
                continue;
            }
        }
        memcpy(out + o, p, len); o += len; p += len;
    }
    if (!changed) return;
    f = _wfopen(ini, L"wb");
    if (!f) return;
    fwrite(out, 1, o, f);
    fclose(f);
    logw(L"removed the Wii Remote extension from the controller profile (the game only takes a Wii Remote on its own)");
}

/* The folder after --user-dir on the command line, quoted or not. */
static int user_dir_arg(const wchar_t *cmd, wchar_t *out) {
    const wchar_t *p = cmd ? wcsstr(cmd, L"--user-dir") : NULL;
    if (!p) return 0;
    p += 10;
    while (*p == L' ') p++;
    wchar_t end = L' ';
    if (*p == L'"') { end = L'"'; p++; }
    int i = 0;
    while (*p && *p != end && i < MAX_PATH - 1) out[i++] = *p++;
    out[i] = 0;
    return i > 0;
}

/* First .wbfs or .iso in dir (non-recursive, one level of subfolders). */
static int find_disc(const wchar_t *dir, wchar_t *out, int depth) {
    static const wchar_t *const pats[] = { L"*.wbfs", L"*.iso" };
    wchar_t q[MAX_PATH];
    WIN32_FIND_DATAW fd;
    for (int i = 0; i < 2; i++) {
        swprintf(q, MAX_PATH, L"%ls/%ls", dir, pats[i]);
        HANDLE h = FindFirstFileW(q, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                wchar_t low[MAX_PATH]; wcsncpy(low, fd.cFileName, MAX_PATH - 1); low[MAX_PATH - 1] = 0; _wcslwr(low);
                if (wcsstr(low, L"kirby") || wcsstr(low, L"suke01")) {
                    swprintf(out, MAX_PATH, L"%ls/%ls", dir, fd.cFileName); FindClose(h); return 1;
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    if (depth <= 0) return 0;
    swprintf(q, MAX_PATH, L"%ls/*", dir);
    HANDLE h = FindFirstFileW(q, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int found = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        wchar_t sub[MAX_PATH]; swprintf(sub, MAX_PATH, L"%ls/%ls", dir, fd.cFileName);
        if (find_disc(sub, out, depth - 1)) { found = 1; break; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

/* Run the port's launcher to extract the disc; waits for it. */
static int extract_disc(const wchar_t *disc) {
    wchar_t exe[MAX_PATH], line[2 * MAX_PATH + 64];
    swprintf(exe, MAX_PATH, L"%lsKirbyWiiRecomp.exe", g_dir);
    swprintf(line, sizeof line / sizeof *line, L"\"%ls\" --extract \"%ls\"", exe, disc);
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    logw(L"extracting %ls", disc);
    if (!CreateProcessW(exe, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_dir, &si, &pi)) return 0;
    WaitForSingleObject(pi.hProcess, 15 * 60 * 1000);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}

/* Keyboard profile, written when the port's launcher finds no controller it
 * knows (it writes nothing then, and the game falls back to Dolphin's own
 * defaults, which attach a Nunchuk: "Please disconnect the unsupported
 * external extension controller"). The letter keys match the controller's
 * button names so the mod's move lists stay right: X key = X button (the Wii
 * Remote's 1), A key = A button (its 2). Q and E are the bumpers (Wii A, B),
 * Z the triggers (shake), Tab is Back (minus), Enter is Start (plus).
 * The last line marks it as ours; the number is how many controllers Windows
 * saw when it was written, so a controller plugged in later gets the port's
 * own profile on the next start. */
#define KBD_MARK "KirbyAccess = keyboard profile, controllers "
static const char k_kbd_profile[] =
    "[Wiimote1]\r\n"
    "Device = DInput/0/Keyboard Mouse\r\n"
    "Buttons/A = `Q`\r\n"
    "Buttons/B = `E`\r\n"
    "Buttons/1 = `X`\r\n"
    "Buttons/2 = `A`\r\n"
    "Buttons/- = `TAB`\r\n"
    "Buttons/+ = `RETURN`\r\n"
    "Buttons/Home = `H`\r\n"
    "D-Pad/Up = `UP`\r\n"
    "D-Pad/Down = `DOWN`\r\n"
    "D-Pad/Left = `LEFT`\r\n"
    "D-Pad/Right = `RIGHT`\r\n"
    "IR/Up = `Cursor Y-`\r\n"
    "IR/Down = `Cursor Y+`\r\n"
    "IR/Left = `Cursor X-`\r\n"
    "IR/Right = `Cursor X+`\r\n"
    "Shake/X = `Z`\r\n"
    "Shake/Y = `Z`\r\n"
    "Shake/Z = `Z`\r\n"
    "Extension = None\r\n"
    "Options/Sideways Wiimote = True\r\n";

/* Game controllers Windows sees right now (XInput pads and DirectInput ones). */
static int controllers(void) {
    int n = 0;
    for (UINT i = 0; i < 16; i++) {
        JOYINFOEX ji = { sizeof ji, JOY_RETURNBUTTONS };
        if (joyGetPosEx(i, &ji) == JOYERR_NOERROR) n++;
    }
    return n;
}

/* -1 if the profile is not our keyboard one, else the controller count in its mark. */
static int kbd_profile_count(const wchar_t *ini) {
    FILE *f = _wfopen(ini, L"rb");
    if (!f) return -1;
    char buf[8192]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f);
    buf[n] = 0;
    char *m = strstr(buf, KBD_MARK);
    return m ? atoi(m + sizeof KBD_MARK - 1) : -1;
}

/* The port's own config.ini names the input device too ("[Input]
 * controller1=..."); with no controller it is left empty and the game reads
 * no keys at all. Set it to dev, or remove it when dev is NULL. */
#define KBD_DEVICE "DInput/0/Keyboard Mouse"
static void set_controller1(const wchar_t *user, const char *dev) {
    wchar_t ini[MAX_PATH];
    swprintf(ini, MAX_PATH, L"%ls/config.ini", user);
    static char buf[16384], out[16384 + 256];
    size_t n = 0;
    FILE *f = _wfopen(ini, L"rb");
    if (f) { n = fread(buf, 1, sizeof buf - 1, f); fclose(f); if (n == sizeof buf - 1) return; }
    buf[n] = 0;
    const char *nl = strstr(buf, "\r\n") || !n ? "\r\n" : "\n";
    size_t o = 0; int put = 0;
    for (char *p = buf; *p; ) {
        char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p + 1) : strlen(p);
        if (strncmp(p, "controller1=", 12) != 0) {
            memcpy(out + o, p, len); o += len;
            if (!eol) o += (size_t)sprintf(out + o, "%s", nl);
            if (dev && !put && !strncmp(p, "[Input]", 7)) { o += (size_t)sprintf(out + o, "controller1=%s%s", dev, nl); put = 1; }
        }
        p += len;
    }
    if (dev && !put) o += (size_t)sprintf(out + o, "[Input]%scontroller1=%s%s", nl, dev, nl);
    f = _wfopen(ini, L"wb");
    if (!f) return;
    fwrite(out, 1, o, f);
    fclose(f);
}

static void write_kbd_profile(const wchar_t *user, const wchar_t *ini, int pads) {
    wchar_t dir[MAX_PATH];
    swprintf(dir, MAX_PATH, L"%ls/Config", user);
    CreateDirectoryW(user, NULL); CreateDirectoryW(dir, NULL);
    FILE *f = _wfopen(ini, L"wb");
    if (!f) { logw(L"could not write the keyboard profile"); return; }
    fputs(k_kbd_profile, f);
    fprintf(f, KBD_MARK "%d\r\n", pads);
    fclose(f);
    set_controller1(user, KBD_DEVICE);
    logw(L"no controller profile from the port (%d controller%ls seen by Windows): wrote a keyboard profile", pads, pads == 1 ? L"" : L"s");
}

/* The port's launcher writes the controller profile (a sideways Wii Remote on
 * the connected pad) when it opens and sees a controller. If there is no
 * profile yet, or ours is there and a new controller has been plugged in,
 * open it minimized until the file appears, then close it. If it writes
 * nothing, write the keyboard profile. */
static void ensure_profile(const wchar_t *user) {
    wchar_t ini[MAX_PATH], exe[MAX_PATH];
    swprintf(ini, MAX_PATH, L"%ls/Config/WiimoteNew.ini", user);
    int pads = -1;
    if (GetFileAttributesW(ini) != INVALID_FILE_ATTRIBUTES) {
        int had = kbd_profile_count(ini);
        if (had < 0) return;                       /* the port's (or the player's own) profile */
        pads = controllers();
        if (pads <= had) { set_controller1(user, KBD_DEVICE); return; }   /* no new controller: keep the keyboard */
        logw(L"a controller was plugged in since the keyboard profile was written: asking the port to set it up");
        DeleteFileW(ini);
        set_controller1(user, NULL);
    }
    if (pads < 0) pads = controllers();
    swprintf(exe, MAX_PATH, L"%lsKirbyWiiRecomp.exe", g_dir);
    STARTUPINFOW si = { sizeof si };
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWMINNOACTIVE;
    PROCESS_INFORMATION pi;
    if (CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, g_dir, &si, &pi)) {
        for (int i = 0; i < 100 && GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES; i++) Sleep(100);
        Sleep(300);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    if (GetFileAttributesW(ini) != INVALID_FILE_ATTRIBUTES) logw(L"controller profile written by the port");
    else write_kbd_profile(user, ini, pads);
}

static int load_mod(HANDLE proc, const wchar_t *dll) {
    SIZE_T bytes = (wcslen(dll) + 1) * sizeof(wchar_t);
    void *remote = VirtualAllocEx(proc, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return 0;
    int ok = 0;
    if (WriteProcessMemory(proc, remote, dll, bytes, NULL)) {
        LPTHREAD_START_ROUTINE ll = (LPTHREAD_START_ROUTINE)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        HANDLE t = CreateRemoteThread(proc, NULL, 0, ll, remote, 0, NULL);
        if (t) {
            WaitForSingleObject(t, 15000);
            DWORD code = 0; GetExitCodeThread(t, &code);
            ok = code != 0;
            CloseHandle(t);
        }
    }
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    return ok;
}

/* First line of a text file, trimmed. */
static int read_line(const wchar_t *path, wchar_t *out, int cap) {
    FILE *f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return 0;
    out[0] = 0;
    if (!fgetws(out, cap, f)) out[0] = 0;
    fclose(f);
    size_t n = wcslen(out);
    while (n && (out[n - 1] == L'\n' || out[n - 1] == L'\r' || out[n - 1] == L' ')) out[--n] = 0;
    return n > 0;
}

/* ---- the port, downloaded on first start ---- */
#define PORT_PAGE L"https://github.com/ExpansionPak/ModernGekko/releases (release \"kirby\")"
#define PORT_URL  L"https://github.com/ExpansionPak/ModernGekko/releases/download/kirby/KirbyWiiRecomp-Windows-x86_64.zip"
static const char k_port_sha256[] = "4e6db092d9f94d5f135f30f551f324e8e3986583d26b59212cc326174f56d050";

static void port_manual(const wchar_t *why) {
    wchar_t m[1024];
    swprintf(m, 1024, L"%ls\n\nYou can set it up yourself: download KirbyWiiRecomp-Windows-x86_64.zip from %ls, "
                      L"and unzip everything inside its KirbyWiiRecomp folder into this folder, next to Play Kirby Access.exe. Then start Play Kirby Access again.", why, PORT_PAGE);
    fail(m);
}

/* An open request for src (https, or http for the updater test server). */
static HINTERNET open_url(HINTERNET *net, const wchar_t *src, DWORD timeout_ms, const wchar_t *headers) {
    *net = InternetOpenW(L"KirbyAccess-launcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!*net) return NULL;
    if (timeout_ms) {
        InternetSetOptionW(*net, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof timeout_ms);
        InternetSetOptionW(*net, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof timeout_ms);
    }
    DWORD fl = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | (wcsncmp(src, L"https:", 6) ? 0 : INTERNET_FLAG_SECURE);
    return InternetOpenUrlW(*net, src, headers, headers ? (DWORD)-1 : 0, fl, 0);
}

/* Download src to zip, hashing as it goes; a short tick for every tenth. */
static int download(const wchar_t *src, const wchar_t *zip, char *hex) {
    int ok = 0;
    HINTERNET net = NULL;
    HINTERNET url = open_url(&net, src, 0, NULL);
    FILE *f = url ? _wfopen(zip, L"wb") : NULL;
    BCRYPT_ALG_HANDLE alg = NULL; BCRYPT_HASH_HANDLE h = NULL;
    if (f && BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0 && BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0) {
        DWORD total = 0, len = sizeof total, idx = 0, got = 0, done = 0; int tenth = 0;
        HttpQueryInfoW(url, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &total, &len, &idx);
        static unsigned char buf[1 << 16];
        for (;;) {
            if (!InternetReadFile(url, buf, sizeof buf, &got)) { logw(L"download read failed (%lu)", GetLastError()); break; }
            if (!got) { ok = 1; break; }
            fwrite(buf, 1, got, f); BCryptHashData(h, buf, got, 0); done += got;
            if (total && (int)((unsigned long long)done * 10 / total) > tenth) { tenth = (int)((unsigned long long)done * 10 / total); Beep(400 + 60 * tenth, 60); }
        }
        logw(L"downloaded %lu of %lu bytes", done, total);
        unsigned char dig[32];
        if (ok && BCryptFinishHash(h, dig, 32, 0) == 0) for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", dig[i]);
        else ok = 0;
    } else logw(L"download could not start (%lu)", GetLastError());
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (f) fclose(f);
    if (url) InternetCloseHandle(url);
    if (net) InternetCloseHandle(net);
    return ok;
}

static int install_port(void) {
    if (MessageBoxW(NULL, L"Kirby Access needs the Kirby Wii Recomp game program, made by ExpansionPak and ModernGekko. It is not in this folder yet.\n\n"
                          L"Download it now from its official GitHub page? It is about 42 MB. You will hear a short rising tick for every tenth, and a message when it is ready.",
                    L"Kirby Access", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        port_manual(L"Kirby Wii Recomp was not downloaded.");
        return 0;
    }
    wchar_t zip[MAX_PATH], here[MAX_PATH], tar[MAX_PATH], sysroot[MAX_PATH], line[4 * MAX_PATH];
    swprintf(zip, MAX_PATH, L"%lsKirbyWiiRecomp-download.zip", g_dir);
    wcsncpy(here, g_dir, MAX_PATH); here[wcslen(here) - 1] = 0;   /* no trailing backslash: it would escape the quote */
    char hex[65] = "";
    logw(L"downloading the port from %ls", PORT_URL);
    if (!download(PORT_URL, zip, hex)) { DeleteFileW(zip); port_manual(L"The download failed. Check your internet connection."); return 0; }
    if (strcmp(hex, k_port_sha256)) {
        logw(L"port download hash %hs, expected %hs", hex, k_port_sha256);
        DeleteFileW(zip);
        port_manual(L"The downloaded file is not the Kirby Wii Recomp version this mod was tested with (the release may have been updated).");
        return 0;
    }
    GetEnvironmentVariableW(L"SystemRoot", sysroot, MAX_PATH);
    swprintf(tar, MAX_PATH, L"%ls\\System32\\tar.exe", sysroot);
    swprintf(line, sizeof line / sizeof *line, L"\"%ls\" -xf \"%ls\" --strip-components=2 -C \"%ls\"", tar, zip, here);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi; DWORD code = 1;
    if (CreateProcessW(tar, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, here, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 600000); GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    logw(L"unzipped the port: tar exit %lu", code);
    DeleteFileW(zip);
    wchar_t exe[MAX_PATH]; swprintf(exe, MAX_PATH, L"%lsKirbyRecomp.exe", g_dir);
    if (code != 0 || GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) { port_manual(L"The download could not be unzipped."); return 0; }
    MessageBoxW(NULL, L"Kirby Wii Recomp is installed. Next, Kirby Access sets up the game from your disc image.", L"Kirby Access", MB_OK | MB_ICONINFORMATION);
    return 1;
}

#include "update.inc"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show) {
    (void)inst; (void)prev; (void)show;
    GetModuleFileNameW(NULL, g_dir, MAX_PATH);
    wchar_t *slash = wcsrchr(g_dir, L'\\');
    if (slash) slash[1] = 0;
    wchar_t exe[MAX_PATH], dll[MAX_PATH], logp[MAX_PATH], user[MAX_PATH], game[MAX_PATH], def[MAX_PATH];
    swprintf(exe, MAX_PATH, L"%lsKirbyRecomp.exe", g_dir);
    swprintf(dll, MAX_PATH, L"%lsKirbyAccess\\KirbyAccess.dll", g_dir);
    swprintf(logp, MAX_PATH, L"%lsKirbyAccess\\launch.log", g_dir);
    g_log = _wfopen(logp, L"a");

    /* the launcher's own arguments come out; the rest goes to the game */
    static wchar_t args[4 * MAX_PATH];
    wcsncpy(args, cmd ? cmd : L"", sizeof args / sizeof *args - 1);
    wchar_t api[512] = L"", fake[64] = L"";
    int updated = wcsstr(args, L"--updated") != NULL, only = wcsstr(args, L"--update-only") != NULL;
    int api_arg = wcsstr(args, L"--update-api") != NULL;
    strip_arg(args, L"--updated", 0, NULL, 0);
    strip_arg(args, L"--update-only", 0, NULL, 0);
    strip_arg(args, L"--update-api", 1, api, 512);
    strip_arg(args, L"--fake-version", 1, fake, 64);
    cmd = args;
    update_cleanup();
    if (!updated && (api_arg || !wcsstr(args, L"--user-dir"))) {
        char cur[64];
        if (fake[0]) WideCharToMultiByte(CP_UTF8, 0, fake, -1, cur, sizeof cur, NULL, NULL); else strcpy(cur, MOD_VERSION);
        /* the new launcher gets the same arguments, minus the pretend version */
        static wchar_t pass[5 * MAX_PATH];
        swprintf(pass, sizeof pass / sizeof *pass, L"%ls%ls%ls%ls%ls", args, only ? L" --update-only" : L"",
                 api_arg ? L" --update-api \"" : L"", api_arg ? api : L"", api_arg ? L"\"" : L"");
        if (update_check(api_arg ? api : UPDATE_API, cur, pass)) { if (g_log) fclose(g_log); return 0; }
    }
    if (only) { logw(L"update-only run: not starting the game"); if (g_log) fclose(g_log); return 0; }

    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES && !install_port()) return 1;
    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) { fail(L"KirbyAccess\\KirbyAccess.dll is missing."); return 1; }

    wchar_t local[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    swprintf(user, MAX_PATH, L"%ls\\kirby-wii-recomp", local);
    swprintf(def, MAX_PATH, L"%ls\\default-game.txt", user);
    if (!game_ok(def, game)) {
        wchar_t disc[MAX_PATH], downloads[MAX_PATH], prof[MAX_PATH];
        wchar_t here[MAX_PATH]; wcsncpy(here, g_dir, MAX_PATH); here[wcslen(here) - 1] = 0;
        GetEnvironmentVariableW(L"USERPROFILE", prof, MAX_PATH);
        swprintf(downloads, MAX_PATH, L"%ls/Downloads", prof);
        if (!find_disc(here, disc, 1) && !find_disc(downloads, disc, 2)) {
            fail(L"The game has not been set up yet, and no Kirby's Return to Dream Land WBFS or ISO was found next to this program or in Downloads. "
                 L"Put your disc image (USA version) in this folder and start Play Kirby Access again.");
            return 1;
        }
        MessageBoxW(NULL, L"First start: Kirby Access will now copy the game from your disc image. This takes a minute or two. Press OK to begin; the game starts when it is done.",
                    L"Kirby Access", MB_OK | MB_ICONINFORMATION);
        if (!extract_disc(disc) || !game_ok(def, game)) {
            fail(L"The disc image could not be set up. It must be the USA version of Kirby's Return to Dream Land (SUKE01).");
            return 1;
        }
    }

    if (!(cmd && wcsstr(cmd, L"--user-dir"))) { ensure_profile(user); fps_title_off(user); }
    wchar_t used[MAX_PATH];
    no_extension(user_dir_arg(cmd, used) ? used : user);

    wchar_t line[4 * MAX_PATH + 512];
    if (cmd && wcsstr(cmd, L"--user-dir"))
        swprintf(line, sizeof line / sizeof *line, L"\"%ls\" --game \"%ls\" %ls", exe, game, cmd);
    else
        swprintf(line, sizeof line / sizeof *line, L"\"%ls\" --game \"%ls\" --user-dir \"%ls\" %ls", exe, game, user, cmd ? cmd : L"");
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, line, NULL, NULL, FALSE, 0, NULL, g_dir, &si, &pi)) {
        fail(L"KirbyRecomp.exe could not be started."); return 1;
    }
    logw(L"started %ls (pid %lu)", line, pi.dwProcessId);
    /* Load the mod right away so it catches the game's first frames. */
    Sleep(300);
    if (load_mod(pi.hProcess, dll)) logw(L"mod loaded");
    else fail(L"The game started, but Kirby Access could not be loaded into it.");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (g_log) fclose(g_log);
    return 0;
}
