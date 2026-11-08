/*
 * Keys and dev commands for Kirby Access.
 *
 *   F1        keys help
 *   F2        enemies nearby, nearest first (pad: L3); Shift+F2 items
 *   F3        status: health, ability, where Kirby is (pad: L3 + R3)
 *   F4        the way to the exit door (pad: R3)
 *   F5        surroundings: walls, the ground ahead, what is above;
 *             Shift+F5 where Kirby is; Ctrl+F5 stage hints on/off,
 *             Ctrl+Shift+F5 the stage hint again
 *   F6        wall hums on/off; Shift+F6 enemy sounds; Ctrl+F6 route tones;
 *             Alt is not used (the port uses it)
 *   F7 / F8   mod sounds quieter / louder
 *   F11       Learn Sounds menu (learn.c)
 *   F12       repeat the last thing spoken
 *   I J K L   tile camera: move the look point a tile (pad: right stick);
 *             U says the tile (pad: L3 while away), O back to Kirby (R3)
 */
#include "access.h"
#include <stdio.h>
#include <string.h>

extern volatile int g_walls_on, g_radar_on, g_route_on, g_ground_on;
void play_poll(void);
void play_say_enemies(void);
void play_say_status(void);
void play_say_items(void);
void play_say_boss(void);
void play_say_moves(void);
void play_say_where(void);
void play_hint_key(int shift);
void play_say_route(void);
void play_say_surroundings(void);
void play_cmd(const char *line);

void game_init(void) {}

void game_poll(void) { learn_poll(); play_poll(); }

static void toggle(volatile int *v, const char *what) {
    *v = !*v;
    speakf(1, "%s %s", what, *v ? "on" : "off");
    if (!*v) hum_all_off();
}

void game_key(int vk, int shift, int ctrl) {
    speech_protect(0);   /* the player asked for something: say it now */
    switch (vk) {
    case VK_F1:
        speak(L"Kirby Access keys. F2 enemies, Shift F2 items, Control F2 what the boss does. F3 health and ability, Shift F3 the ability's moves. F4 the way to the exit. F5 walls and ground ahead, Shift F5 where you are, Control F5 stage hints on or off, Control Shift F5 the hint again. "
              L"F6 wall hums on or off, F9 enemy count on or off, Shift F6 enemy sounds, Control F6 route tones. F7 and F8 mod volume. F11 learn sounds. F12 repeat. "
              L"On a controller: left stick click enemies, right stick click the way, both together status. "
              L"Tile camera: push the right stick, or press I J K L, to look one tile at a time around Kirby and hear what is there. While Kirby is in the air, the left stick moves the camera too. U, or the left stick click, says the tile; O, or the right stick click, brings the camera back to Kirby. It also comes back by itself after 8 seconds.", 1);
        break;
    case VK_F2: if (shift) play_say_items(); else if (ctrl) play_say_boss(); else play_say_enemies(); break;
    case VK_F3: if (shift) play_say_moves(); else play_say_status(); break;
    case VK_F4: play_say_route(); break;
    case VK_F5: if (ctrl) play_hint_key(shift); else if (shift) play_say_where(); else play_say_surroundings(); break;
    case VK_F6:
        if (shift) toggle(&g_radar_on, "Enemy sounds");
        else if (ctrl) toggle(&g_route_on, "Route tones");
        else toggle(&g_walls_on, "Wall hums");
        break;
    case VK_F7: case VK_F8: {
        float m = g_master + (vk == VK_F8 ? 0.1f : -0.1f);
        if (m < 0.1f) m = 0.1f;
        if (m > 1.5f) m = 1.5f;
        g_master = m;
        speakf(1, "Mod volume %d", (int)(m * 10 + 0.5f));
        tone(660, 660, 0.12f, 0.4f, 0, W_SINE);
        break;
    }
    case VK_F9: toggle(&g_count_on, "Enemy count"); break;
    case VK_F11: learn_toggle(); break;
    case VK_F12: speak_repeat(); break;
    }
}

/* Dev: write the collision grid to KirbyAccess\map.txt, K = Kirby. */
static void dump_map(void) {
    int w, h; float kx = -1, ky = -1;
    if (!land_size(&w, &h)) { log_line("no stage"); return; }
    uint32_t hero = scene_hero(0);
    if (hero) hero_pos(hero, &kx, &ky);
    wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%lsmap.txt", g_mod_dir);
    FILE *f = _wfopen(p, L"w");
    if (!f) return;
    fprintf(f, "%dx%d kirby %.2f %.2f\n", w, h, kx, ky);
    for (int y = h - 1; y >= 0; y--) {
        fprintf(f, "%3d ", y);
        for (int x = 0; x < w; x++) {
            LandCell c; land_cell(x, y, &c);
            char ch = c.shape == 0 ? '.' : c.shape == 1 ? '#' : (c.shape >= 26 && c.shape <= 51) ? '=' : '/';
            if (c.flags & 0x08) ch = c.shape ? 'W' : '~';
            if (c.flags & 0x10) ch = '^';
            if ((int)kx == x && (int)ky == y) ch = 'K';
            fputc(ch, f);
        }
        fputc('\n', f);
    }
    fprintf(f, "cells with unusual values:\n");
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        LandCell c; land_cell(x, y, &c);
        if (c.shape > 1 || c.attr > 1 || (c.flags & ~0x04)) fprintf(f, "%d,%d shape %d flags %02x extra %02x attr %d\n", x, y, c.shape, c.flags, c.extra, c.attr);
    }
    fclose(f);
    log_line("map written");
}

void game_cmd(const char *line) {
    if (!strcmp(line, "pos")) {
        uint32_t c = scene_component(), h = scene_hero(0); float x = 0, y = 0;
        if (h) hero_pos(h, &x, &y);
        log_line("pos comp %08x hero %08x %.3f %.3f", c, h, x, y);
    } else if (!strncmp(line, "tp ", 3)) {
        /* dev: move Kirby (Location x, y; the game may pull him back if inside a wall) */
        uint32_t h = scene_hero(0), loc = h ? r32(h + 0xa0) : 0; float x, y;
        if (is_ptr(loc) && sscanf(line + 3, "%f %f", &x, &y) == 2) {
            union { float f; uint32_t u; } a = { x }, b = { y };
            guest_w32(loc, a.u); guest_w32(loc + 4, b.u);
            log_line("tp %.2f %.2f", x, y);
        }
    } else if (!strncmp(line, "wf ", 3)) {   /* dev: write a float, "wf <hex addr> <value>" */
        unsigned a; float v;
        if (sscanf(line + 3, "%x %f", &a, &v) == 2) { union { float f; uint32_t u; } c = { v }; guest_w32(a, c.u); log_line("wf %08x %g", a, v); }
    } else if (!strcmp(line, "map")) dump_map();
    else if (!strcmp(line, "switches")) { void objects_dump_switches(uint32_t); objects_dump_switches(scene_component()); }
    else if (!strcmp(line, "textlog on")) g_text_log = 1;
    else if (!strcmp(line, "textlog all")) g_text_log = 2;
    else if (!strcmp(line, "textlog off")) g_text_log = 0;
    else play_cmd(line);
}
