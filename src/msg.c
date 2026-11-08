/*
 * The game's own message files (files/msg/US_English, the .msbt files in Nintendo's LMS
 * format), loaded once so that:
 *   - text that types itself out letter by letter (Magolor's talk, signs)
 *     can be spoken whole as soon as its first words appear, and
 *   - movie subtitles can be looked up by label (Movie.msbt).
 *
 * Text is normalised the same way as on screen: control tags removed,
 * line breaks and runs of spaces become one space.
 */
#include "access.h"
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct { char label[48]; wchar_t *text; int len; } Msg;
static Msg *s_msg;
static int s_nmsg, s_cap;
static wchar_t s_game[MAX_PATH];

/* Collapse whitespace, trim. In place; returns the new length. */
int msg_normalise(wchar_t *s) {
    int k = 0, sp = 0;
    for (int i = 0; s[i]; i++) {
        wchar_t c = s[i];
        if (c == L'\n' || c == L'\r' || c == L'\t' || c == 0x3000) c = L' ';
        if (c == L' ') { sp = 1; continue; }
        if (sp && k) s[k++] = L' ';
        sp = 0;
        s[k++] = c;
    }
    s[k] = 0;
    return k;
}

static uint32_t rd32(const uint8_t *p, int be) { return be ? (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3] : (uint32_t)p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0]; }
static uint16_t rd16(const uint8_t *p, int be) { return be ? (uint16_t)(p[0] << 8 | p[1]) : (uint16_t)(p[1] << 8 | p[0]); }

static void add(const char *label, const wchar_t *t) {
    if (s_nmsg == s_cap) { s_cap = s_cap ? s_cap * 2 : 1024; s_msg = (Msg *)realloc(s_msg, sizeof(Msg) * s_cap); }
    Msg *m = &s_msg[s_nmsg++];
    snprintf(m->label, sizeof m->label, "%s", label);
    m->text = _wcsdup(t);
    m->len = msg_normalise(m->text);
}

static void load_file(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = (uint8_t *)malloc(n);
    if (!d || fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return; }
    fclose(f);
    if (n < 0x20 || memcmp(d, "MsgStdBn", 8)) { free(d); return; }
    int be = d[8] == 0xFE;
    int nsec = rd16(d + 0xe, be);
    long off = 0x20;
    /* labels: index -> name */
    char (*labels)[48] = NULL; int nlab = 0;
    const uint8_t *txt = NULL; uint32_t txtsize = 0;
    for (int s = 0; s < nsec && off + 16 <= n; s++) {
        uint32_t size = rd32(d + off + 4, be);
        const uint8_t *b = d + off + 16;
        if (off + 16 + (long)size > n) break;
        if (!memcmp(d + off, "LBL1", 4)) {
            uint32_t groups = rd32(b, be);
            for (uint32_t g = 0; g < groups; g++) {
                uint32_t cnt = rd32(b + 4 + 8 * g, be), o = rd32(b + 8 + 8 * g, be);
                for (uint32_t c = 0; c < cnt && o < size; c++) {
                    int ln = b[o];
                    uint32_t idx = rd32(b + o + 1 + ln, be);
                    if ((int)idx >= nlab) {
                        int nn = idx + 64;
                        labels = realloc(labels, sizeof *labels * nn);
                        memset(labels + nlab, 0, sizeof *labels * (nn - nlab));
                        nlab = nn;
                    }
                    int cl = ln < 47 ? ln : 47;
                    memcpy(labels[idx], b + o + 1, cl); labels[idx][cl] = 0;
                    o += 5 + ln;
                }
            }
        } else if (!memcmp(d + off, "TXT2", 4)) { txt = b; txtsize = size; }
        off += 16 + size; off = (off + 15) & ~15L;
    }
    if (txt) {
        uint32_t cnt = rd32(txt, be);
        static wchar_t buf[4096];
        for (uint32_t i = 0; i < cnt; i++) {
            uint32_t a = rd32(txt + 4 + 4 * i, be), z = i + 1 < cnt ? rd32(txt + 8 + 4 * i, be) : txtsize;
            int k = 0;
            for (uint32_t j = a; j + 1 < z && k < 4095; ) {
                uint16_t c = rd16(txt + j, be);
                if (c == 0) break;
                if (c == 0x0E && j + 8 <= z) { j += 8 + rd16(txt + j + 6, be); continue; }   /* tag */
                if (c == 0x0F) { j += 6; continue; }                                           /* end tag */
                if (c >= 0x20 || c == L'\n') buf[k++] = c;
                j += 2;
            }
            buf[k] = 0;
            add((labels && (int)i < nlab) ? labels[i] : "", buf);
        }
    }
    free(labels);
    free(d);
}

void msg_init(void) {
    /* the game folder from our own command line: --game "<dir>" */
    int argc; wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 0; argv && i + 1 < argc; i++) if (!wcscmp(argv[i], L"--game")) wcsncpy(s_game, argv[i + 1], MAX_PATH - 1);
    if (argv) LocalFree(argv);
    if (!s_game[0]) { log_line("msg: no --game on the command line"); return; }
    wchar_t pat[MAX_PATH], path[MAX_PATH];
    swprintf(pat, MAX_PATH, L"%ls/files/msg/US_English/*.msbt", s_game);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { log_line("msg: no message files found"); return; }
    do {
        swprintf(path, MAX_PATH, L"%ls/files/msg/US_English/%ls", s_game, fd.cFileName);
        load_file(path);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    log_line("msg: %d messages loaded", s_nmsg);
}

/* Known messages starting with the given (normalised) text:
 * 0 none, 1 exactly one (*full set to it), 2 several different ones. */
int msg_match(const wchar_t *start, const wchar_t **full) {
    int n = (int)wcslen(start);
    *full = NULL;
    if (n == 0) return 0;
    for (int i = 0; i < s_nmsg; i++) {
        if (s_msg[i].len < n || wcsncmp(s_msg[i].text, start, n)) continue;
        if (*full && wcscmp(*full, s_msg[i].text)) return 2;
        *full = s_msg[i].text;
    }
    return *full ? 1 : 0;
}

const wchar_t *msg_by_label(const char *label) {
    for (int i = 0; i < s_nmsg; i++) if (!strcmp(s_msg[i].label, label)) return s_msg[i].text;
    return NULL;
}

const wchar_t *msg_game_dir(void) { return s_game; }
