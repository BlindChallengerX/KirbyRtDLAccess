/*
 * Finding the running stage in guest memory.
 *
 * Gameplay runs in a scn::step::SceneStep (stages, level hubs, the Lor
 * Starcutter). Its vtable is __vt__Q33scn4step9SceneStep (0x80771618 in
 * SUKE01). The scene is found by scanning MEM1 for that vtable pointer, then
 * checked through the chain the game itself uses:
 *
 *   SceneStep +4 -> core::Main, whose first member is the stage Component
 *   Component +0xb0 -> mcoll::LandManager (collision)
 *   Component +0xc8 -> hero::Manager; +0x9c MutableArray<Hero*,4>
 *                      (count, then the pointers)
 *   Hero +0xa0 -> chara::Location; +0 position (x, y, z floats, tiles)
 *
 * All addresses come from the disc's own link map (files/iuk.MAP).
 */
#include "access.h"
#include <string.h>

#define VT_SCENESTEP 0x80771618u

static uint32_t s_scene, s_comp;
static DWORD s_last_scan;

/* A valid chain from a SceneStep candidate; fills the Component. */
static int check_scene(uint32_t ss, uint32_t *comp) {
    if (r32(ss) != VT_SCENESTEP) return 0;
    uint32_t main = r32(ss + 4);
    if (!is_ptr(main)) return 0;
    uint32_t hm = r32(main + 0xc8), lm = r32(main + 0xb0);
    if (!is_ptr(hm) || !is_ptr(lm)) return 0;
    uint32_t n = r32(hm + 0x9c);
    if (n > 4) return 0;
    uint32_t layer = r32(lm + 8);
    if (!is_ptr(layer)) return 0;
    uint32_t w = r32(layer), h = r32(layer + 4);
    if (w == 0 || w > 288 || h == 0 || h > 160) return 0;
    *comp = main;
    return 1;
}

static uint32_t scan_region(uint32_t base, uint32_t from, uint32_t to) {
    const uint8_t *m = guest_ptr(base, 4);
    if (!m) return 0;
    const uint8_t pat[4] = { 0x80, 0x77, 0x16, 0x18 };
    for (uint32_t off = from; off + 4 <= to; off += 4) {
        if (m[off] != pat[0] || memcmp(m + off, pat, 4)) continue;
        uint32_t c;
        if (check_scene(base + off, &c)) return base + off;
    }
    return 0;
}

static uint32_t scan(void) {
    uint32_t a = scan_region(0x80000000, 0x008f4000, 0x01800000);   /* MEM1 heap, after the stack */
    if (!a && guest_ptr(0x90000000, 4)) a = scan_region(0x90000000, 0, 0x04000000);
    return a;
}

/* The current stage's Component, or 0 when no stage is running. */
uint32_t scene_component(void) {
    uint32_t c;
    if (s_scene && check_scene(s_scene, &c)) { s_comp = c; return c; }
    s_scene = 0; s_comp = 0;
    DWORD now = GetTickCount();
    if (now - s_last_scan < 300) return 0;
    s_last_scan = now;
    uint32_t ss = scan();
    if (ss && check_scene(ss, &c)) {
        s_scene = ss; s_comp = c;
        log_line("stage scene at %08x, component %08x", ss, c);
        return c;
    }
    return 0;
}

/* Forget the scene so the next call scans again (after a scene change). */
void scene_forget(void) { s_scene = 0; s_comp = 0; }

uint32_t scene_hero(int i) {
    uint32_t c = s_comp ? s_comp : scene_component();
    if (!c) return 0;
    uint32_t hm = r32(c + 0xc8);
    if (!is_ptr(hm) || (uint32_t)i >= r32(hm + 0x9c)) return 0;
    uint32_t h = r32(hm + 0xa0 + 4 * i);
    return is_ptr(h) ? h : 0;
}

int hero_pos(uint32_t hero, float *x, float *y) {
    uint32_t loc = r32(hero + 0xa0);
    if (!is_ptr(loc)) return 0;
    *x = rf32(loc); *y = rf32(loc + 4);
    return *x == *x && *y == *y && *x > -100 && *x < 400 && *y > -100 && *y < 300;
}

/* ---- collision grid ----
 * LandManager +8: the fixed LandLayer (width @0, height @4, cells @8).
 * A cell is 10 bytes at layer + 8 + (x + y * 288) * 10, y counted upward:
 *   +0 shape (0 empty, 1 solid block, 26..51 one-way platforms you can
 *      jump up through, others slopes and half blocks)
 *   +1 flags: 0x04 invisible, 0x08 water, 0x10 spikes ("spine")
 *   +2 bits 6-7: spike damage level
 *   +4 attribute (surface kind, e.g. ladder) */
int land_size(int *w, int *h) {
    uint32_t c = s_comp ? s_comp : scene_component();
    if (!c) return 0;
    uint32_t lm = r32(c + 0xb0), layer = is_ptr(lm) ? r32(lm + 8) : 0;
    if (!is_ptr(layer)) return 0;
    *w = (int)r32(layer); *h = (int)r32(layer + 4);
    return 1;
}

int land_cell(int x, int y, LandCell *out) {
    memset(out, 0, sizeof *out);
    uint32_t c = s_comp;
    if (!c) return 0;
    uint32_t lm = r32(c + 0xb0), layer = is_ptr(lm) ? r32(lm + 8) : 0;
    if (!is_ptr(layer)) return 0;
    int w = (int)r32(layer), h = (int)r32(layer + 4);
    if (x < 0 || y < 0 || x >= w || y >= h) { out->shape = 1; out->outside = 1; return 1; }
    const uint8_t *p = guest_ptr(layer + 8 + (uint32_t)(x + y * 288) * 10, 10);
    if (!p) return 0;
    out->shape = p[0]; out->flags = p[1]; out->extra = p[2]; out->attr = p[4];
    return 1;
}
