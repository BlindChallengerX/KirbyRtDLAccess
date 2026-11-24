/* Updating Kirby Access itself (included by launch.c).
 *
 * Every start, before the game (so no mod file is in use), the launcher asks
 * GitHub for the latest release. If it is newer than this build, a
 * Yes/No/Cancel box with the release notes offers it; Cancel skips that
 * version (KirbyAccess\update.ini). The zip is checked against the SHA-256
 * GitHub lists for it, unpacked into KirbyAccess\update\new, and each file is
 * moved into place. Files it replaces are renamed *.old first (the running
 * launcher too: Windows lets a running program be renamed) and are moved
 * back if anything fails. Then the new launcher is started with --updated and
 * this one exits; the new one deletes the .old files (listed in
 * KirbyAccess\update\old.txt) and the mod says "Kirby Access updated to
 * version X" (KirbyAccess\updated.txt).
 *
 * Test arguments: --update-api URL (another releases/latest JSON, http is
 * fine), --fake-version V (pretend to be V), --update-only (stop after the
 * check instead of starting the game). The test bots pass --user-dir and
 * never update unless --update-api is given.
 */
#define UPDATE_API L"https://api.github.com/repos/BlindChallengerX/KirbyRtDLAccess/releases/latest"

static int ver_cmp(const char *a, const char *b) {
    if (*a == 'v' || *a == 'V') a++;
    if (*b == 'v' || *b == 'V') b++;
    for (int i = 0; i < 4; i++) {
        char *ea, *eb;
        long x = strtol(a, &ea, 10), y = strtol(b, &eb, 10);
        if (x != y) return x < y ? -1 : 1;
        a = *ea == '.' ? ea + 1 : ea;
        b = *eb == '.' ? eb + 1 : eb;
    }
    return 0;
}

/* The JSON string after "key", searched from p on, unescaped into out (UTF-8).
 * Returns the position after it, or NULL. */
static const char *json_str(const char *p, const char *key, char *out, size_t cap) {
    char k[64];
    snprintf(k, sizeof k, "\"%s\"", key);
    const char *q = strstr(p, k);
    if (!q) return NULL;
    q += strlen(k);
    while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
    if (*q++ != ':') return NULL;
    while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
    if (*q++ != '"') return NULL;
    size_t o = 0;
    while (*q && *q != '"') {
        unsigned c = (unsigned char)*q++;
        if (c == '\\' && *q) {
            char e = *q++;
            if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else if (e == 'u') {
                unsigned u = 0;
                for (int i = 0; i < 4 && isxdigit((unsigned char)*q); i++, q++)
                    u = u * 16 + (unsigned)(isdigit((unsigned char)*q) ? *q - '0' : tolower((unsigned char)*q) - 'a' + 10);
                char t[3]; int n = 0;
                if (u < 0x80) t[n++] = (char)u;
                else if (u < 0x800) { t[n++] = (char)(0xC0 | (u >> 6)); t[n++] = (char)(0x80 | (u & 0x3F)); }
                else { t[n++] = (char)(0xE0 | (u >> 12)); t[n++] = (char)(0x80 | ((u >> 6) & 0x3F)); t[n++] = (char)(0x80 | (u & 0x3F)); }
                for (int i = 0; i < n && o + 1 < cap; i++) out[o++] = t[i];
                continue;
            } else c = (unsigned char)e;
        }
        if (o + 1 < cap) out[o++] = (char)c;
    }
    out[o] = 0;
    return *q ? q + 1 : q;
}

/* GET src into buf (NUL-terminated). 1 on HTTP 200 with a body. */
static int http_get(const wchar_t *src, char *buf, size_t cap) {
    HINTERNET net = NULL;
    HINTERNET url = open_url(&net, src, 5000, L"Accept: application/vnd.github+json\r\n");
    size_t n = 0;
    DWORD got = 0, status = 0, len = sizeof status;
    if (url) HttpQueryInfoW(url, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &len, NULL);
    if (url && status == 200)
        while (n + 1 < cap && InternetReadFile(url, buf + n, (DWORD)(cap - 1 - n), &got) && got) n += got;
    else if (url) logw(L"update check: HTTP %lu", status);
    buf[n] = 0;
    if (url) InternetCloseHandle(url);
    if (net) InternetCloseHandle(net);
    return status == 200 && n > 0;
}

static wchar_t *widen(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc((size_t)(n > 0 ? n : 1) * sizeof *w);
    if (w) { w[0] = 0; MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n); }
    return w;
}

/* The files of an unpacked update, relative to its folder. */
typedef struct { wchar_t rel[MAX_PATH]; int had_old, placed; } upd_file;
static upd_file g_uf[256];
static int g_nuf;

static void list_files(const wchar_t *dir, size_t base_len) {
    wchar_t q[MAX_PATH];
    WIN32_FIND_DATAW fd;
    swprintf(q, MAX_PATH, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(q, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.') continue;
        wchar_t sub[MAX_PATH];
        swprintf(sub, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) list_files(sub, base_len);
        else if (g_nuf < 256) { wcsncpy(g_uf[g_nuf].rel, sub + base_len, MAX_PATH - 1); g_uf[g_nuf].had_old = g_uf[g_nuf].placed = 0; g_nuf++; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void remove_tree(const wchar_t *dir) {
    wchar_t q[MAX_PATH];
    WIN32_FIND_DATAW fd;
    swprintf(q, MAX_PATH, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(q, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == L'.') continue;
            wchar_t sub[MAX_PATH];
            swprintf(sub, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) remove_tree(sub); else DeleteFileW(sub);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir);
}

/* Delete the .old files a finished update left (the old launcher may still be closing). */
static void update_cleanup(void) {
    wchar_t list[MAX_PATH];
    swprintf(list, MAX_PATH, L"%lsKirbyAccess\\update\\old.txt", g_dir);
    FILE *f = _wfopen(list, L"r");
    if (!f) return;
    static char keep[64][MAX_PATH];
    int nk = 0, n = 0;
    char line[MAX_PATH];
    while (fgets(line, sizeof line, f)) {
        size_t l = strlen(line);
        while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (!l) continue;
        wchar_t *rel = widen(line), path[MAX_PATH];
        swprintf(path, MAX_PATH, L"%ls%ls.old", g_dir, rel ? rel : L"");
        free(rel);
        int ok = 0;
        for (int t = 0; t < 30; t++) {
            if (DeleteFileW(path) || GetLastError() == ERROR_FILE_NOT_FOUND) { ok = 1; break; }
            Sleep(100);
        }
        if (ok) n++;
        else if (nk < 64) strcpy(keep[nk++], line);
    }
    fclose(f);
    if (!nk) {
        DeleteFileW(list);
        swprintf(list, MAX_PATH, L"%lsKirbyAccess\\update", g_dir);
        remove_tree(list);
    } else if ((f = _wfopen(list, L"w"))) {
        for (int i = 0; i < nk; i++) fprintf(f, "%s\n", keep[i]);
        fclose(f);
    }
    logw(L"update: removed %d old file%ls%ls", n, n == 1 ? L"" : L"s", nk ? L" (some still in use; tried again next start)" : L"");
}

static void update_failed(const wchar_t *why) {
    wchar_t m[1024];
    swprintf(m, 1024, L"The Kirby Access update could not be installed: %ls\n\n"
                      L"Kirby Access %hs starts as before. You can also download the new version yourself from "
                      L"github.com/BlindChallengerX/KirbyRtDLAccess/releases.", why, MOD_VERSION);
    fail(m);
}

/* Move every unpacked file into the game folder, keeping what it replaces as
 * .old. On failure everything done so far is undone and 0 is returned. */
static int update_install(const wchar_t *newdir) {
    g_nuf = 0;
    list_files(newdir, wcslen(newdir) + 1);
    int bad = 0;
    for (int i = 0; i < g_nuf && !bad; i++) {
        wchar_t src[MAX_PATH], dst[MAX_PATH], old[MAX_PATH];
        swprintf(src, MAX_PATH, L"%ls\\%ls", newdir, g_uf[i].rel);
        swprintf(dst, MAX_PATH, L"%ls%ls", g_dir, g_uf[i].rel);
        swprintf(old, MAX_PATH, L"%ls.old", dst);
        for (wchar_t *p = wcschr(dst + wcslen(g_dir), L'\\'); p; p = wcschr(p + 1, L'\\')) { *p = 0; CreateDirectoryW(dst, NULL); *p = L'\\'; }
        if (GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW(old);
            if (!MoveFileExW(dst, old, MOVEFILE_REPLACE_EXISTING)) {
                logw(L"update: could not move %ls aside (error %lu)", g_uf[i].rel, GetLastError());
                bad = 1; break;
            }
            g_uf[i].had_old = 1;
        }
        if (!MoveFileExW(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            logw(L"update: could not put %ls in place (error %lu)", g_uf[i].rel, GetLastError());
            bad = 1; break;
        }
        g_uf[i].placed = 1;
    }
    if (!bad) return 1;
    for (int j = 0; j < g_nuf; j++) {
        wchar_t dst[MAX_PATH], old[MAX_PATH];
        swprintf(dst, MAX_PATH, L"%ls%ls", g_dir, g_uf[j].rel);
        swprintf(old, MAX_PATH, L"%ls.old", dst);
        if (g_uf[j].had_old) MoveFileExW(old, dst, MOVEFILE_REPLACE_EXISTING);
        else if (g_uf[j].placed) DeleteFileW(dst);
    }
    logw(L"update: rolled back");
    return 0;
}

/* Returns 1 if this launcher should exit (the updated one has been started). */
static int update_check(const wchar_t *api, const char *cur, const wchar_t *pass_args) {
    static char json[262144], body[8192];
    if (!http_get(api, json, sizeof json)) { logw(L"update check: no answer from GitHub"); return 0; }
    char tag[64] = "", url[1024] = "", digest[128] = "", name[256] = "";
    if (!json_str(json, "tag_name", tag, sizeof tag)) { logw(L"update check: no release found"); return 0; }
    if (ver_cmp(tag, cur) <= 0) { logw(L"update check: %hs is the latest", cur); return 0; }
    /* the KirbyAccess-*.zip asset: its name, then (same object) its digest and download link */
    for (const char *p = json; (p = strstr(p, "\"name\"")); ) {
        const char *after = json_str(p, "name", name, sizeof name);
        if (!after) break;
        size_t l = strlen(name);
        if (!strncmp(name, "KirbyAccess-", 12) && l > 4 && !_stricmp(name + l - 4, ".zip")) {
            const char *link = json_str(after, "browser_download_url", url, sizeof url);
            const char *dg = strstr(after, "\"digest\"");
            if (dg && (!link || dg < link)) json_str(dg, "digest", digest, sizeof digest);
            break;
        }
        p = after;
    }
    json_str(json, "body", body, sizeof body);
    if (!url[0]) { logw(L"update check: %hs has no KirbyAccess zip", tag); return 0; }
    const char *ver = (tag[0] == 'v' || tag[0] == 'V') ? tag + 1 : tag;

    wchar_t ini[MAX_PATH], skip[64] = L"";
    swprintf(ini, MAX_PATH, L"%lsKirbyAccess\\update.ini", g_dir);
    GetPrivateProfileStringW(L"update", L"skip", L"", skip, 64, ini);
    wchar_t wver[64];
    swprintf(wver, 64, L"%hs", ver);
    if (!wcscmp(skip, wver)) { logw(L"update check: %ls is out, skipped by the player", wver); return 0; }

    if (strlen(body) > 1500) strcpy(body + 1497, "...");
    wchar_t *wbody = widen(body);
    static wchar_t m[4096];
    swprintf(m, 4096, L"Kirby Access %ls is out. You have %hs.\n\nWhat's new:\n%ls\n\n"
                      L"Update now? It takes a few seconds, then the game starts.\n"
                      L"Yes: update now.\nNo: not now, ask again next time.\nCancel: skip this version.",
             wver, cur, wbody ? wbody : L"");
    free(wbody);
    logw(L"update check: %ls is out (this is %hs), asking", wver, cur);
    int a = MessageBoxW(NULL, m, L"Kirby Access update", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (a == IDCANCEL) { WritePrivateProfileStringW(L"update", L"skip", wver, ini); logw(L"update: player skipped %ls", wver); return 0; }
    if (a != IDYES) { logw(L"update: player said not now"); return 0; }

    wchar_t dir[MAX_PATH], zip[MAX_PATH], newdir[MAX_PATH], line[4 * MAX_PATH], tar[MAX_PATH], sysroot[MAX_PATH];
    swprintf(dir, MAX_PATH, L"%lsKirbyAccess\\update", g_dir);
    remove_tree(dir);
    CreateDirectoryW(dir, NULL);
    swprintf(zip, MAX_PATH, L"%ls\\download.zip", dir);
    swprintf(newdir, MAX_PATH, L"%ls\\new", dir);
    CreateDirectoryW(newdir, NULL);
    wchar_t *wurl = widen(url);
    char hex[65] = "";
    logw(L"update: downloading %ls", wurl ? wurl : L"?");
    int ok = wurl && download(wurl, zip, hex);
    free(wurl);
    if (!ok) { remove_tree(dir); update_failed(L"the download failed. Check your internet connection."); return 0; }
    if (digest[0] && (strncmp(digest, "sha256:", 7) || _stricmp(digest + 7, hex))) {
        logw(L"update: zip hash %hs, GitHub lists %hs", hex, digest);
        remove_tree(dir); update_failed(L"the download was damaged (its checksum does not match)."); return 0;
    }
    if (!digest[0]) logw(L"update: GitHub lists no checksum for the zip");

    GetEnvironmentVariableW(L"SystemRoot", sysroot, MAX_PATH);
    swprintf(tar, MAX_PATH, L"%ls\\System32\\tar.exe", sysroot);
    swprintf(line, sizeof line / sizeof *line, L"\"%ls\" -xf \"%ls\" -C \"%ls\"", tar, zip, newdir);
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    if (CreateProcessW(tar, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    wchar_t c1[MAX_PATH], c2[MAX_PATH];
    swprintf(c1, MAX_PATH, L"%ls\\Play Kirby Access.exe", newdir);
    swprintf(c2, MAX_PATH, L"%ls\\KirbyAccess\\KirbyAccess.dll", newdir);
    if (code != 0 || GetFileAttributesW(c1) == INVALID_FILE_ATTRIBUTES || GetFileAttributesW(c2) == INVALID_FILE_ATTRIBUTES) {
        logw(L"update: unzip failed (tar exit %lu)", code);
        remove_tree(dir); update_failed(L"the download could not be unzipped."); return 0;
    }
    if (!update_install(newdir)) {
        remove_tree(dir);
        update_failed(L"a file could not be replaced. If the game is still running, close it and start Play Kirby Access again.");
        return 0;
    }
    DeleteFileW(zip);
    remove_tree(newdir);
    wchar_t p[MAX_PATH];
    swprintf(p, MAX_PATH, L"%ls\\old.txt", dir);
    FILE *f = _wfopen(p, L"w");
    if (f) {
        for (int j = 0; j < g_nuf; j++) if (g_uf[j].had_old) { char u[MAX_PATH * 3]; WideCharToMultiByte(CP_UTF8, 0, g_uf[j].rel, -1, u, sizeof u, NULL, NULL); fprintf(f, "%s\n", u); }
        fclose(f);
    }
    swprintf(p, MAX_PATH, L"%lsKirbyAccess\\updated.txt", g_dir);
    if ((f = _wfopen(p, L"w"))) { fprintf(f, "%s\n", ver); fclose(f); }
    logw(L"update: installed %ls (%d files)", wver, g_nuf);

    /* start the new launcher with the same arguments */
    wchar_t self[MAX_PATH], args[5 * MAX_PATH];
    swprintf(self, MAX_PATH, L"%lsPlay Kirby Access.exe", g_dir);
    swprintf(args, sizeof args / sizeof *args, L"\"%ls\" %ls --updated", self, pass_args);
    STARTUPINFOW si2 = { sizeof si2 };
    PROCESS_INFORMATION pi2;
    if (!CreateProcessW(self, args, NULL, NULL, FALSE, 0, NULL, g_dir, &si2, &pi2)) {
        fail(L"Kirby Access was updated. Please start Play Kirby Access again.");
        return 1;
    }
    CloseHandle(pi2.hThread); CloseHandle(pi2.hProcess);
    return 1;
}

/* Take "name" (and its value) out of a command line. */
static void strip_arg(wchar_t *cmd, const wchar_t *name, int has_value, wchar_t *value, int cap) {
    wchar_t *p = wcsstr(cmd, name);
    if (!p) return;
    wchar_t *e = p + wcslen(name);
    while (*e == L' ') e++;
    if (has_value) {
        wchar_t end = L' ';
        int i = 0;
        if (*e == L'"') { end = L'"'; e++; }
        while (*e && *e != end) { if (value && i < cap - 1) value[i++] = *e; e++; }
        if (value) value[i] = 0;
        if (*e) e++;
        while (*e == L' ') e++;
    }
    memmove(p, e, (wcslen(e) + 1) * sizeof *e);
}
