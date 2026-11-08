/*
 * Development commands, read from KirbyAccess\cmd.txt (deleted after reading):
 *   say TEXT        speak TEXT
 *   dump            write MEM1/MEM2 to KirbyAccess\mem1.bin, mem2.bin
 *   peek ADDR [N]   log N words (default 8) at a guest address
 *   tone F          play a test tone
 *   hist on|off     count dispatched addresses, log them on off
 *   watch ADDR [N]  log r3-r6 the next N times ADDR is dispatched
 */
#include "access.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void game_cmd(const char *line);

void tools_poll(void) {
    static DWORD last;
    DWORD now = GetTickCount();
    if (now - last < 250) return;
    last = now;
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%lscmd.txt", g_mod_dir);
    FILE *f = _wfopen(path, L"r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        log_line("cmd: %s", line);
        if (!strncmp(line, "say ", 4)) speak_utf8(line + 4, 1);
        else if (!strcmp(line, "dump")) guest_dump(g_mod_dir);
        else if (!strcmp(line, "audiostat")) { void audio_stat(void); audio_stat(); }
        else if (!strncmp(line, "peek ", 5)) {
            char *e; uint32_t a = (uint32_t)strtoul(line + 5, &e, 16); int n = atoi(e); if (n <= 0) n = 8;
            for (int i = 0; i < n; i += 4) {
                log_line("%08x: %08x %08x %08x %08x", a + i * 4, r32(a + i * 4), r32(a + i * 4 + 4), r32(a + i * 4 + 8), r32(a + i * 4 + 12));
            }
        } else if (!strncmp(line, "watch ", 6)) {
            char *e; uint32_t a = (uint32_t)strtoul(line + 6, &e, 16); int n = atoi(e); guest_watch(a, n > 0 ? n : 5);
        } else if (!strcmp(line, "hist on")) guest_hist(1);
        else if (!strcmp(line, "hist off")) guest_hist(0);
        else if (!strncmp(line, "tone ", 5)) tone((float)atof(line + 5), (float)atof(line + 5), 0.3f, 0.5f, 0, W_SINE);
        else game_cmd(line);
    }
    fclose(f);
    _wremove(path);
}
