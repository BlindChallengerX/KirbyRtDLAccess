/*
 * On-screen text for Kirby Access.
 *
 * Every text on screen is an nw4r::lyt::TextBox pane (vtable
 * __vt__Q34nw4r3lyt7TextBox, 0x80751660). Pane fields (SUKE01):
 *   +0x0c parent pane          +0x84 global matrix (3x4 floats; x +0x90, y +0xa0)
 *   +0xb8 alpha                +0xbb flags (bit 0 visible)
 *   +0xbc name (16 chars)      +0xd8 text buffer (UTF-16BE), +0xfe length
 *
 * Many layouts stay in memory while hidden, with their panes still marked
 * visible. To know what is really drawn, the mod flips the lowest bit of
 * each candidate's global matrix: the game recalculates that matrix every
 * frame it draws the pane, so the bit comes back only for panes on screen.
 * (A one-in-eight-million change in scale: nothing you could see.)
 *
 * New text is spoken as it appears, top to bottom. The gameplay HUD
 * (lives, stars, player names) is left out.
 */
#include "access.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <stdlib.h>

#define VT_TEXTBOX 0x80751660u
#define MAXBOX 3000

typedef struct {
    uint32_t addr;
    uint32_t mark;          /* the matrix word we wrote, 0 if none */
    int shown;              /* drawn and visible at the last check */
    uint32_t hash;          /* text hash at the last check */
    DWORD shown_since;      /* when it last came on screen */
    float minsc;            /* smallest scale seen since then (after the intro animation) */
    int focus;              /* drawn clearly larger than its smallest: the focused item */
    const wchar_t *said_full;   /* the whole known line already spoken for this box */
    wchar_t prev[160];      /* its text at the last check */
    int pending;            /* typing out; speak when it stops changing */
    DWORD changed_at;
} Box;

static Box s_box[MAXBOX];
static int s_nbox;
static DWORD s_last_scan, s_last_check;
volatile int g_text_on = 1;
volatile int g_text_log;

static void scan_region(uint32_t base, uint32_t size) {
    const uint8_t *m = guest_ptr(base, 4);
    if (!m) return;
    const uint8_t pat[4] = { 0x80, 0x75, 0x16, 0x60 };
    for (uint32_t off = 0; off + 0x100 <= size && s_nbox < MAXBOX; off += 4) {
        if (m[off] != 0x80 || memcmp(m + off, pat, 4)) continue;
        uint32_t a = base + off;
        int found = 0;
        for (int i = 0; i < s_nbox; i++) if (s_box[i].addr == a) { found = 1; break; }
        if (found) continue;
        memset(&s_box[s_nbox], 0, sizeof s_box[s_nbox]);
        s_box[s_nbox].addr = a; s_box[s_nbox].mark = 0; s_box[s_nbox].shown = 0; s_box[s_nbox].hash = 0;
        s_nbox++;
    }
}

static void rescan(void) {
    /* drop entries that are no longer text boxes */
    int k = 0;
    for (int i = 0; i < s_nbox; i++) if (r32(s_box[i].addr) == VT_TEXTBOX) s_box[k++] = s_box[i];
    s_nbox = k;
    scan_region(0x80000000, 0x01800000);
    scan_region(0x90000000, 0x04000000);
}

/* Pane names whose text is the gameplay HUD or otherwise not worth reading. */
static int is_hud(uint32_t pane) {
    static const char *const skip[] = { "RestPlayerPanelN", "StarPanelN", "NameN", "PlayerPanel", "HeroPanel", "LifeBar", "ScoreN", "TimeN" };
    uint32_t p = pane;
    for (int d = 0; d < 24 && is_ptr(p); d++) {
        char name[17]; guest_str(p + 0xbc, name, sizeof name);
        for (size_t i = 0; i < sizeof skip / sizeof *skip; i++) if (!strncmp(name, skip[i], strlen(skip[i]))) return 1;
        p = r32(p + 0x0c);
    }
    return 0;
}

static int chain_visible(uint32_t pane) {
    /* The pane's own computed global alpha (+0xb9, the product the game uses when drawing) must be
     * nonzero, and every ancestor must be visible. A parent's raw alpha (+0xb8) alone is not enough:
     * a pane that does not inherit alpha (flag bit 1 clear) ignores it, and the Exit Stage / Quit
     * confirmation sits under a window pane with alpha 0 while it is plainly on screen. */
    if (r8(pane + 0xb9) == 0 || r8(pane + 0xb8) == 0) return 0;
    uint32_t p = pane;
    for (int d = 0; d < 24 && is_ptr(p); d++) {
        if (!(r8(p + 0xbb) & 1)) return 0;
        p = r32(p + 0x0c);
    }
    return 1;
}

/* Wii Remote button icons in text (tag 0x0E, group 1). Confirmed in game:
 * 0 and 1 ("Press 1 and 2 together"), 12 ("A Energy Spheres" on the world
 * map), 15 ("Hold down minus for a Quick Quit"). Others are read as
 * "button N" until they are known. */
static const char *button_name(int group, int idx) {
    if (group != 1) return NULL;
    switch (idx) {
    case 0: return "1"; case 1: return "2"; case 12: return "A"; case 15: return "minus";
    }
    return NULL;
}

/* Read a box's text into plain words: icon tags become button names,
 * other control codes are dropped. Returns 0 for nothing readable. */
static int box_text(uint32_t a, wchar_t *out, int cap) {
    uint32_t tp = r32(a + 0xd8);
    int n = r16(a + 0xfe);
    if (!is_ptr(tp) || n <= 0 || n > 600) return 0;
    int k = 0, letters = 0, cjk = 0;
    for (int i = 0; i < n && k < cap - 24; i++) {
        uint16_t c = r16(tp + 2 * i);
        if (c == 0x0E && i + 3 < n) {
            int group = r16(tp + 2 * (i + 1)), idx = r16(tp + 2 * (i + 2)), plen = r16(tp + 2 * (i + 3));
            const char *b = button_name(group, idx);
            char tmp[24];
            if (!b) { snprintf(tmp, sizeof tmp, "button %d", idx); b = (group == 1) ? tmp : NULL; if (!b) b = ""; }
            for (const char *q = b; *q && k < cap - 2; q++) out[k++] = (wchar_t)*q;
            i += 3 + plen / 2;
            continue;
        }
        if (c == '\n' || c == '\r') { if (k && out[k - 1] != ' ') out[k++] = ' '; continue; }
        if (c < 0x20) continue;
        if (c == 0xFF58) c = 'x';                 /* fullwidth x in "x07" */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) letters++;
        if (c >= 0x3000 && c < 0xA000) cjk++;
        out[k++] = c;
    }
    while (k && out[k - 1] == ' ') k--;
    out[k] = 0;
    if (k == 1 && out[0] == '%') { wcscpy(out, L"percent"); return 7; }   /* "100" "%" in separate boxes */
    if (!letters || cjk > letters) return 0;      /* empty, symbols only, or Japanese placeholder text */
    return k;
}

/* The same words, ignoring spaces and line breaks (shadow copies differ there). */
static int same_words(const wchar_t *a, const wchar_t *b) {
    for (;;) {
        while (*a == L' ' || *a == 10 || *a == 0x3000) a++;
        while (*b == L' ' || *b == 10 || *b == 0x3000) b++;
        if (*a != *b) return 0;
        if (!*a) return 1;
        a++; b++;
    }
}
static uint32_t hash_w(const wchar_t *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint32_t)*s) * 16777619u;
    return h ? h : 1;
}

typedef struct { float x, y; uint32_t root, name, addr; int raw; wchar_t text[300]; } Line;
static void speak_lines(Line *lines, int nl);
static Line s_pend[64]; static int s_npend; static DWORD s_pend_first, s_pend_last;
static DWORD s_new_t;   /* when a new screen appeared (several texts at once) */
/* each item of the menus on screen (the pane holding all its text), from the last check */
static uint32_t s_conts[64]; static int s_nconts;
static DWORD s_last_lines_t;      /* when a batch of screen text was last read */
static wchar_t s_last_all[1024];  /* and what it said */
/* "1 Planet Popstar" (a button picture, then what it does) as "Planet Popstar: press 1" */
static void button_prompt(uint32_t box, wchar_t *t, int cap) {
    char nm[17]; guest_str(box + 0xbc, nm, sizeof nm);
    if (strncmp(nm, "AllSphereText", 13)) return;
    wchar_t *sp = wcschr(t, L' ');
    if (!sp || sp - t > 6) return;
    wchar_t btn[8], out[300]; int bl = (int)(sp - t);
    wcsncpy(btn, t, bl); btn[bl] = 0;
    swprintf(out, 300, L"%ls: press %ls", sp + 1, btn);
    wcsncpy(t, out, cap - 1); t[cap - 1] = 0;
}
/* what was said lately (screen text, menu items), so it isn't read again */
static struct { uint32_t h; DWORD t; } s_recent[32]; static int s_nrec;
static uint32_t hash_w(const wchar_t *s);
static void note_said(const wchar_t *w) { s_recent[s_nrec].h = hash_w(w); s_recent[s_nrec].t = GetTickCount(); s_nrec = (s_nrec + 1) % 32; }
static int said_lately(const wchar_t *w, DWORD ms) {
    uint32_t h = hash_w(w); DWORD now = GetTickCount();
    for (int q = 0; q < 32; q++) if (s_recent[q].h == h && now - s_recent[q].t < ms) return 1;
    return 0;
}

/* Normal (smallest) size of menu items, by pane name and text. */
static struct { uint32_t key; float sc; } s_sizes[512];
static float size_get(uint32_t key) {
    for (int i = 0; i < 512; i++) if (s_sizes[i].key == key) return s_sizes[i].sc;
    return 0;
}
static void size_put(uint32_t key, float sc) {
    if (sc < 0.05f) return;
    int free_i = -1;
    for (int i = 0; i < 512; i++) {
        if (s_sizes[i].key == key) { if (sc < s_sizes[i].sc) s_sizes[i].sc = sc; return; }
        if (!s_sizes[i].key && free_i < 0) free_i = i;
    }
    if (free_i < 0) free_i = (int)(key % 512);
    s_sizes[free_i].key = key; s_sizes[free_i].sc = sc;
}

/* ---- menu selection ----
 * In this game's menus the chosen item is drawn larger (its ScaleN parent
 * goes from 0.9 to 1.0). Text boxes with the same pane name under the same
 * layout root are one menu; when one of them is clearly the largest, that
 * is the selection. */
typedef struct { uint32_t addr, root, name; float scale, x, y, dx; wchar_t *text; } Item;
typedef struct { uint32_t root, name, sel, sel_hash, said_h; int stable, scroll, mlast; uint32_t cand; DWORD seen, msince, said_t; } Menu;
static Menu s_menu[32];

static uint32_t root_of(uint32_t p) {
    for (int d = 0; d < 30; d++) { uint32_t q = r32(p + 0x0c); if (!is_ptr(q)) return p; p = q; }
    return p;
}

static Item *s_all; static int s_nall;
static uint32_t s_modal_prev; static DWORD s_modal_t;   /* the open confirmation dialog, and when it appeared */
static uint32_t s_announced[8]; static int s_nannounced;   /* menu roots announced this check */      /* every shown text this check, reading order */

static int is_under(uint32_t p, uint32_t anc) {
    for (int d = 0; d < 30 && is_ptr(p); d++) { if (p == anc) return 1; p = r32(p + 0x0c); }
    return 0;
}

/* Ancestor of sel just below the deepest ancestor shared by the whole group. */
static uint32_t item_container(Item *it, int *g, int m, uint32_t sel) {
    uint32_t chain[32]; int n = 0;
    for (uint32_t p = sel; is_ptr(p) && n < 32; p = r32(p + 0x0c)) chain[n++] = p;
    for (int i = 1; i < n; i++) {          /* chain[i] is an ancestor; is it shared by all? */
        int all = 1;
        for (int j = 0; j < m && all; j++) if (!is_under(it[g[j]].addr, chain[i])) all = 0;
        if (all) return chain[i - 1];
    }
    return sel;
}

/* Focus by growth: per layout root, the boxes drawn larger than their own
 * smallest size. When that set changes, read it (menus whose items have
 * different pane names, like the main menu). */
static wchar_t s_last_menu[400];
static DWORD s_last_menu_t;
static Box *find_box(uint32_t a) { for (int i = 0; i < s_nbox; i++) if (s_box[i].addr == a) return &s_box[i]; return NULL; }

static void check_focus(Item *it, int n) {
    /* a new screen is about to be read: name the highlighted item after it */
    if (s_npend >= 3 || GetTickCount() - s_new_t < 300) return;
    static struct { uint32_t root, sig, pend, said_h; DWORD pend_t, said_t; uint32_t had[8]; int nhad; } seen[32];
    uint32_t roots[32]; int nr = 0;
    for (int i = 0; i < n; i++) {
        uint32_t r = root_of(it[i].addr); int f = 0;
        for (int j = 0; j < nr; j++) if (roots[j] == r) f = 1;
        if (!f && nr < 32) roots[nr++] = r;
    }
    /* a screen that has gone: forget its highlight, so coming back names it again */
    for (int q = 0; q < 32; q++) {
        if (!seen[q].root) continue;
        int here = 0;
        for (int j = 0; j < nr; j++) if (roots[j] == seen[q].root) here = 1;
        if (!here) { seen[q].root = 0; seen[q].said_h = 0; seen[q].nhad = 0; }
    }
    for (int k = 0; k < nr; k++) {
        uint32_t sig = 0; int cnt = 0;
        uint32_t fa[8]; const wchar_t *ft[8];
        wchar_t words[400]; words[0] = 0;
        for (int i = 0; i < n; i++) {
            if (root_of(it[i].addr) != roots[k]) continue;
            Box *b = find_box(it[i].addr);
            if (!b || !b->focus) continue;
            /* items of a same-name menu (File 1, 2, 3) are the menu rule's job */
            int same = 0;
            for (int j = 0; j < n; j++) if (j != i && it[j].name == it[i].name && root_of(it[j].addr) == roots[k]) same = 1;
            if (same) continue;
            sig = sig * 31 + it[i].addr;
            if (cnt < 8) { fa[cnt] = it[i].addr; ft[cnt] = it[i].text; }
            cnt++;
        }
        int slot = -1;
        for (int q = 0; q < 32; q++) if (seen[q].root == roots[k]) { slot = q; break; }
        if (slot < 0) for (int q = 0; q < 32; q++) if (!seen[q].root) { slot = q; seen[q].root = roots[k]; seen[q].sig = 0; break; }
        if (slot < 0) continue;
        if (sig == seen[slot].sig) { seen[slot].pend = 0; continue; }
        /* wait a moment, so the same-name rule (with "2 of 3") speaks first */
        if (seen[slot].pend != sig) { seen[slot].pend = sig; seen[slot].pend_t = GetTickCount(); continue; }
        if (GetTickCount() - seen[slot].pend_t < 110) continue;
        seen[slot].sig = sig; seen[slot].pend = 0;
        if (!cnt || cnt > 4) { seen[slot].nhad = 0; continue; }
        /* while the highlight moves, the old item is still shrinking: name only
         * what just became highlighted */
        int nnew = 0;
        for (int a = 0; a < cnt && a < 8; a++) {
            int old = 0;
            for (int q = 0; q < seen[slot].nhad; q++) if (seen[slot].had[q] == fa[a]) old = 1;
            if (!old) nnew++;
        }
        /* several new at once (one growing, one not yet back to normal): the biggest */
        int pick = -1; float pick_sc = 0;
        for (int a = 0; a < cnt && a < 8; a++) {
            int old = 0;
            for (int q = 0; q < seen[slot].nhad; q++) if (seen[slot].had[q] == fa[a]) old = 1;
            if (old && nnew) continue;
            float sc = fabsf(rf32(fa[a] + 0x84));
            if (pick < 0 || sc > pick_sc) { pick = a; pick_sc = sc; }
        }
        if (pick >= 0 && cnt == 1) { wcsncpy(words, ft[pick], 399); words[399] = 0; button_prompt(fa[pick], words, 400); pick = -1; }
        if (pick >= 0) {
            /* with the other new pieces of the same item ("Main" "100" "%") */
            uint32_t anc = fa[pick];
            for (int d = 0; d < 3 && is_ptr(r32(anc + 0x0c)); d++) anc = r32(anc + 0x0c);
            for (int a = 0; a < cnt && a < 8; a++) {
                int old = 0;
                for (int q = 0; q < seen[slot].nhad; q++) if (seen[slot].had[q] == fa[a]) old = 1;
                if (old && nnew) continue;
                if (a != pick && !is_under(fa[a], anc)) continue;
                int twice = 0;   /* a shadow copy */
                for (int c = 0; c < a; c++) if (same_words(ft[c], ft[a]) && (c == pick || is_under(fa[c], anc)) && fabsf(rf32(fa[c] + 0x90) - rf32(fa[a] + 0x90)) < 10 && fabsf(rf32(fa[c] + 0xa0) - rf32(fa[a] + 0xa0)) < 10) twice = 1;
                if (twice) continue;
                if (words[0]) wcsncat(words, L" ", 399 - wcslen(words));
                wcsncat(words, ft[a], 399 - wcslen(words));
            }
            button_prompt(fa[pick], words, 400);
        }
        if (!nnew) continue;   /* only an item going back to normal: nothing new */
        /* remember what was named as the highlight (not the ones still shrinking) */
        int nh2 = 0; uint32_t had2[8];
        for (int a = 0; a < cnt && a < 8; a++) {
            int old = 0;
            for (int q = 0; q < seen[slot].nhad; q++) if (seen[slot].had[q] == fa[a]) old = 1;
            if (!old) had2[nh2++] = fa[a];
        }
        seen[slot].nhad = nh2;
        for (int a = 0; a < nh2; a++) seen[slot].had[a] = had2[a];
        /* just read as part of the screen */
        if (GetTickCount() - s_last_lines_t < 1500 && wcsstr(s_last_all, words)) continue;
        /* already said by the same-name menu rule just now */
        if (GetTickCount() - s_last_menu_t < 2000 && wcsstr(s_last_menu, words)) continue;
        /* the same item again at once (a button's press animation) */
        uint32_t wh = hash_w(words);
        if (wh == seen[slot].said_h && GetTickCount() - seen[slot].said_t < 2000) continue;
        seen[slot].said_h = wh; seen[slot].said_t = GetTickCount();
        if (g_text_log) { char *u = to_utf8(words); log_line("focus: %s", u ? u : "?"); free(u); }
        /* right after a screen was read, don't cut that reading off */
        note_said(words);
        speak(words, GetTickCount() - s_last_lines_t > 900);
    }
}

/* Menu buttons that are only pictures (no text to read). The pane under the
 * chosen one is drawn at about 1.05, the others at 0.95. Found by walking
 * the pane tree of each shown layout: pane +0x10 child count, +0x14 first
 * child's link (child + 4), link +0 next. */
static const struct { const char *pane, *label; } k_icons[] = {
    { "BungyN", "Ninja Dojo" }, { "GunN", "Scope Shot" }, { "GourmetN", "Gourmet Race" },
    { "KakutouN0", "The Arena" }, { "KakutouN1", "The Arena" }, { "ShinKakutouN", "The True Arena" },
};
#define N_ICONS (int)(sizeof k_icons / sizeof *k_icons)
static void icon_walk(uint32_t p, int depth, float *best, int *budget) {
    if (depth > 10 || --*budget < 0) return;
    uint32_t cnt = r32(p + 0x10), node = r32(p + 0x14);
    for (uint32_t i = 0; i < cnt && i < 64 && is_ptr(node); i++) {
        uint32_t c = node - 4;
        if (r32(c + 0x0c) != p) break;
        if ((r8(c + 0xbb) & 1) && r8(c + 0xb8)) {
            char nm[17]; guest_str(c + 0xbc, nm, sizeof nm);
            for (int k = 0; k < N_ICONS; k++)
                if (!strcmp(nm, k_icons[k].pane)) {
                    /* the inner pane against the outer one of the same name (its slot
                     * on the board): the chosen button grows about 10% inside its slot;
                     * the others stay the same size as their slot */
                    uint32_t a = r32(c + 0x0c);
                    for (int d = 0; d < 6 && is_ptr(a); d++, a = r32(a + 0x0c)) {
                        char an[17]; guest_str(a + 0xbc, an, sizeof an);
                        if (!strcmp(an, nm)) {
                            float so = fabsf(rf32(a + 0x84)), si = fabsf(rf32(c + 0x84));
                            if (so > 0.01f && si / so > best[k]) best[k] = si / so;
                            break;
                        }
                    }
                }
            icon_walk(c, depth + 1, best, budget);
        }
        node = r32(c + 4);
    }
}
static void check_icons(Item *it, int n) {
    static int s_icon = -1;
    float best[N_ICONS] = { 0 };
    uint32_t roots[8]; int nr = 0;
    for (int i = 0; i < n && nr < 8; i++) {
        uint32_t r = root_of(it[i].addr); int f = 0;
        for (int j = 0; j < nr; j++) if (roots[j] == r) f = 1;
        if (!f) roots[nr++] = r;
    }
    for (int j = 0; j < nr; j++) { int budget = 1500; icon_walk(roots[j], 0, best, &budget); }
    int top = -1;
    for (int k = 0; k < N_ICONS; k++)
        if (best[k] > 0 && (top < 0 || best[k] > best[top])) top = k;
    /* chosen: grown inside its own slot */
    int sel = (top >= 0 && best[top] > 1.04f) ? top : -1;
    /* screens fading in or out scale everything: the choice must hold a moment */
    static int s_cand = -1, s_stable;
    if (sel != s_cand) { s_cand = sel; s_stable = 0; }
    if (sel >= 0 && ++s_stable < 2) return;
    if (sel >= 0 && (s_icon < 0 || strcmp(k_icons[sel].label, k_icons[s_icon].label))) {
        if (g_text_log) log_line("icon: %s", k_icons[sel].label);
        speak_utf8(k_icons[sel].label, 1);
    }
    s_icon = sel;
}

static void check_menus(Item *it, int n) {
    DWORD now = GetTickCount();
    s_nconts = 0;
    /* menus gone from the screen for a while are forgotten */
    for (int k = 0; k < 32; k++) if (s_menu[k].root && now - s_menu[k].seen > 3000) s_menu[k].root = 0;
    /* every same-name group first, with its items' areas: a group lying inside
     * a bigger one (the "Empty" labels inside the three file slots) is part of
     * that menu, not a menu of its own */
    static struct { uint32_t root, name; int m; uint32_t cont[32]; } grp[48];
    int ngrp = 0;
    for (int i = 0; i < n && ngrp < 48; i++) {
        int known = 0;
        for (int q = 0; q < ngrp; q++) if (grp[q].root == it[i].root && grp[q].name == it[i].name) known = 1;
        if (known) continue;
        int gg[32], mm = 0;
        for (int j = i; j < n && mm < 32; j++) if (it[j].root == it[i].root && it[j].name == it[i].name) gg[mm++] = j;
        if (mm < 2) continue;
        grp[ngrp].root = it[i].root; grp[ngrp].name = it[i].name; grp[ngrp].m = mm;
        for (int j = 0; j < mm; j++) grp[ngrp].cont[j] = item_container(it, gg, mm, it[gg[j]].addr);
        ngrp++;
    }
    for (int i = 0; i < n; i++) {
        if (it[i].root == 0) continue;
        /* gather the group */
        int g[32], m = 0;
        for (int j = i; j < n && m < 32; j++)
            if (it[j].root == it[i].root && it[j].name == it[i].name) g[m++] = j;
        uint32_t root = it[i].root, name = it[i].name;
        for (int j = 0; j < m; j++) it[g[j]].root = 0;      /* consumed */
        if (m < 2) continue;
        int inside = 0;
        for (int q = 0; q < ngrp && !inside; q++) {
            if (grp[q].root != root || grp[q].name == name || grp[q].m <= m) continue;
            int all = 1;
            for (int j = 0; j < m && all; j++) {
                int under = 0;
                for (int c = 0; c < grp[q].m && !under; c++) if (is_under(it[g[j]].addr, grp[q].cont[c])) under = 1;
                if (!under) all = 0;
            }
            if (all) inside = 1;
        }
        if (inside) continue;
        int best = -1; float b1 = 0, b2 = 0;
        for (int j = 0; j < m; j++) {
            float sc = fabsf(it[g[j]].scale);
            if (sc > b1) { b2 = b1; b1 = sc; best = g[j]; } else if (sc > b2) b2 = sc;
        }
        Menu *mu = NULL;
        for (int k = 0; k < 32; k++) if (s_menu[k].root == root && s_menu[k].name == name) { mu = &s_menu[k]; break; }
        if (!mu) for (int k = 0; k < 32; k++) if (!s_menu[k].root) { mu = &s_menu[k]; mu->root = root; mu->name = name; mu->sel = 0; mu->stable = 0; mu->scroll = 0; mu->cand = 0; break; }
        if (!mu) continue;
        mu->seen = now;
        for (int j = 0; j < m && s_nconts < 64; j++) s_conts[s_nconts++] = item_container(it, g, m, it[g[j]].addr);
        /* the list is still filling in (sliding onto the screen): its size isn't known yet */
        if (m != mu->mlast) { mu->mlast = m; mu->msince = now; }
        int scroll = 0;
        if (b1 < b2 * 1.05f) {
            /* same size: a scrolling list (the Theater) marks its row by
             * nudging the text sideways in its slot. One item off, the rest
             * all at the same offset. */
            best = -1;
            if (m >= 3) {
                for (int j = 0; j < m && best < 0; j++) {
                    int odd = 1, same = 1; float ref = 1e9f;
                    for (int q = 0; q < m; q++) {
                        if (q == j) continue;
                        if (fabsf(it[g[q]].dx - it[g[j]].dx) < 2) odd = 0;
                        if (ref > 1e8f) ref = it[g[q]].dx; else if (fabsf(it[g[q]].dx - ref) >= 1) same = 0;
                    }
                    if (odd && same && it[g[j]].dx > ref) best = g[j];
                }
            }
            if (best < 0) { mu->cand = 0; mu->stable = 0; continue; }
            scroll = 1;
            mu->scroll = 1;
        }
        uint32_t sel = it[best].addr, sh = hash_w(it[best].text);
        if (sel == mu->sel && sh != mu->sel_hash && mu->sel_hash) {
            /* a list scrolling under a fixed highlight: the selected text changed */
            mu->sel_hash = sh;
            mu->stable = 2;
            goto announce;
        }
        if (sel != mu->cand) { mu->cand = sel; mu->stable = 1; continue; }
        if (++mu->stable < 2 || sel == mu->sel) continue;
        mu->sel_hash = sh;
      announce:;
        int first = mu->sel == 0;
        mu->sel = sel;
        /* position in the menu: top to bottom, left to right */
        int pos = 1;
        for (int j = 0; j < m; j++) {
            Item *o = &it[g[j]];
            if (o->addr == sel) continue;
            if (o->y > it[best].y + 8 || (fabsf(o->y - it[best].y) <= 8 && o->x < it[best].x)) pos++;
        }
        /* the item is the subtree just below the menu's common ancestor:
         * read every text in it ("File" "1" -> "File 1") */
        uint32_t cont = item_container(it, g, m, sel);
        static uint32_t said[32]; static int nsaid; static DWORD said_t;
        if (GetTickCount() - said_t > 100) nsaid = 0;
        said_t = GetTickCount();
        int dup = 0;
        for (int q = 0; q < nsaid; q++) if (said[q] == cont) dup = 1;
        if (dup) continue;
        if (nsaid < 32) said[nsaid++] = cont;
        wchar_t words[300]; words[0] = 0;
        for (int j = 0; j < s_nall; j++) {
            if (!is_under(s_all[j].addr, cont)) continue;
            if (words[0]) wcsncat(words, L" ", 299 - wcslen(words));
            wcsncat(words, s_all[j].text, 299 - wcslen(words));
        }
        if (!words[0]) wcscpy(words, it[best].text);
        /* the same item twice in a row within a second (a list re-laying out) */
        if (hash_w(words) == mu->said_h && now - mu->said_t < 1000) continue;
        mu->said_h = hash_w(words); mu->said_t = now;
        wchar_t msg[400];
        if (scroll) swprintf(msg, 400, L"%ls", words);   /* rows on screen are not the whole list */
        else if (now - mu->msince < 400) swprintf(msg, 400, L"%ls", words);   /* count not settled */
        else swprintf(msg, 400, L"%ls, %d of %d", words, pos, m);
        if (g_text_log) {
            char *u = to_utf8(msg); log_line("menu: %s (m %d, settled %d ms)", u ? u : "?", m, (int)(now - mu->msince)); free(u);
            for (int j = 0; j < m; j++) { char *v = to_utf8(it[g[j]].text); log_line("   member %08x y %.0f sc %.2f %s", it[g[j]].addr, it[g[j]].y, it[g[j]].scale, v ? v : "?"); free(v); }
        }
        wcsncpy(s_last_menu, msg, 399); s_last_menu[399] = 0; s_last_menu_t = GetTickCount();
        note_said(words);
        speak(msg, !first);
        if (s_nannounced < 8) s_announced[s_nannounced++] = root;
    }
}

void play_pause_hint(void);
volatile int g_pause_open;   /* the game's pause menu is on screen (its "Pause Menu" title) */
void text_poll(void) {
    DWORD now = GetTickCount();
    /* checks every 50 ms so a menu move is spoken about 0.1 s after the key */
    if (now - s_last_scan > 600) { s_last_scan = now; rescan(); }
    if (now - s_last_check < 50) return;
    s_last_check = now;
    static Line lines[64];
    static Item items[256];
    static wchar_t itext[256][300];
    int nl = 0, ni = 0, nnew = 0, pause_seen = 0;
    for (int i = 0; i < s_nbox; i++) {
        Box *b = &s_box[i];
        uint32_t a = b->addr;
        if (r32(a) != VT_TEXTBOX) { b->shown = 0; b->mark = 0; continue; }
        uint32_t cur = r32(a + 0x84);
        int drawn = b->mark ? cur != b->mark : 0;
        /* mark again for the next check */
        b->mark = cur ^ 1;
        guest_w32(a + 0x84, b->mark);
        if (!b->mark) b->mark = 1;
        wchar_t t[300];
        /* on screen: the layout screen is about 832 x 456 around (0, 0); screens
         * the game keeps parked to the side (the file select behind the main
         * menu, at x 540..950) or shrunk to nothing are not read */
        float gx = rf32(a + 0x90), gy = rf32(a + 0xa0), gs = fabsf(rf32(a + 0x84)) + fabsf(rf32(a + 0x88));
        int on_screen = gx > -440 && gx < 440 && gy > -250 && gy < 250 && gs > 0.05f;
        int shown = drawn && on_screen && chain_visible(a) && box_text(a, t, 300);
        if (shown && !wcscmp(t, L"Pause Menu")) pause_seen = 1;
        if (g_text_log == 2 && on_screen && box_text(a, t, 300)) {   /* dev: "textlog all" - every box on screen and why it is (not) read */
            char *u = to_utf8(t); log_line("box %08x %s | drawn %d vis %d hud %d sc %.2f | %s", a, u ? u : "?", drawn, chain_visible(a), is_hud(a), rf32(a + 0x84), shown ? "READ" : "skipped"); free(u);
        }
        uint32_t h = shown ? hash_w(t) : 0;
        uint32_t nh = 2166136261u;
        if (shown) { char nm[17]; guest_str(a + 0xbc, nm, sizeof nm); for (char *q = nm; *q; q++) nh = (nh ^ (uint8_t)*q) * 16777619u; }
        if (shown && ni < 256 && !is_hud(a)) {
            items[ni].addr = a; items[ni].root = root_of(a); items[ni].name = nh;
            items[ni].scale = rf32(a + 0x84); items[ni].x = rf32(a + 0x90); items[ni].y = rf32(a + 0xa0);
            { uint32_t par = r32(a + 0x0c); items[ni].dx = is_ptr(par) ? items[ni].x - rf32(par + 0x90) : 0; }
            wcscpy(itext[ni], t); items[ni].text = itext[ni]; ni++;
        }
        /* Text that types itself out letter by letter: as soon as the start
         * matches one of the game's own lines, say the whole line once. */
        const wchar_t *say = NULL;
        if (shown && !is_hud(a) && g_text_on) {
            int changed = !b->shown || h != b->hash;
            msg_normalise(t);
            if (changed) {
                const wchar_t *full;
                size_t pl = wcslen(b->prev), tl = wcslen(t);
                int typing = b->shown && pl && tl > pl && !wcsncmp(t, b->prev, pl);
                if (b->said_full && !wcsncmp(b->said_full, t, tl)) { /* still typing a line already said */ }
                else if (typing) {
                    /* typing out: say the whole known line now, or wait until it stops */
                    if (msg_match(t, &full) == 1) { say = full; b->said_full = full; b->pending = 0; }
                    else { b->pending = 1; b->changed_at = now; }
                } else {
                    int m = msg_match(t, &full);
                    b->said_full = NULL;
                    if (m == 0 || (m == 1 && !wcscmp(full, t))) { say = t; b->pending = 0; }
                    else { b->pending = 1; b->changed_at = now; }     /* maybe the start of a line being typed */
                }
                wcsncpy(b->prev, t, 159); b->prev[159] = 0;
            } else if (b->pending && now - b->changed_at > 400) { say = t; b->pending = 0; }
        }
        if (!shown) { b->said_full = NULL; b->pending = 0; b->prev[0] = 0; }
        /* the "Next" prompt under every line of dialogue is just noise */
        if (say && !wcscmp(say, L"Next")) say = NULL;
        if (say && nl < 64) {
            lines[nl].x = rf32(a + 0x90); lines[nl].y = rf32(a + 0xa0); lines[nl].root = root_of(a); lines[nl].name = nh;
            lines[nl].addr = a; lines[nl].raw = say == t;   /* its own text, not a whole message line */
            if (g_text_log) {   /* dev: why a line is read */
                float al = 1; char chain[120] = ""; uint32_t p = a;
                for (int d = 0; d < 24 && is_ptr(p); d++) {
                    al *= r8(p + 0xb8) / 255.0f;
                    if (d < 7) { char nm[17]; guest_str(p + 0xbc, nm, sizeof nm); strncat(chain, nm, 119 - strlen(chain)); strncat(chain, "<", 119 - strlen(chain)); }
                    p = r32(p + 0x0c);
                }
                char *u = to_utf8(say); log_line("line: %s | alpha %.2f x %.0f y %.0f sx %.2f | %s", u ? u : "?", al, lines[nl].x, lines[nl].y, rf32(a + 0x84), chain); free(u);
            }
            wcsncpy(lines[nl].text, say, 299); lines[nl].text[299] = 0;
            nl++;
        }
        if (shown && !b->shown) { b->shown_since = now; b->minsc = 0; nnew++; }
        b->focus = 0;
        if (shown) {
            /* a menu item's normal size is remembered by its pane name and
             * text, so coming back to a menu its highlighted item is known at once */
            float sc = fabsf(rf32(a + 0x84));
            uint32_t key = (nh * 31u) ^ h;
            float known = size_get(key);
            if (now - b->shown_since > 600) {
                if (b->minsc == 0 || sc < b->minsc) b->minsc = sc;
                size_put(key, sc);
            }
            float base = b->minsc;
            if (known > 0 && (base == 0 || known < base)) base = known;
            b->focus = base > 0 && sc > base * 1.04f;
        }
        b->shown = shown; b->hash = h;
    }
    g_pause_open = pause_seen;
    if (pause_seen) play_pause_hint();
    if (nnew >= 3) s_new_t = now;
    /* A confirmation dialog (Quit / Exit Stage: "Are you sure?" Yes / No) sits in front of the
     * pause menu, whose buttons share the same pane names and stay on screen. While one is
     * open, only the dialog's own texts count, so moving between its buttons is a menu of two. */
    {
        uint32_t modal = 0, cn = 2166136261u;
        for (const char *q = "ConfirmText"; *q; q++) cn = (cn ^ (uint8_t)*q) * 16777619u;
        for (int i = 0; i < ni && !modal; i++) if (items[i].name == cn) {
            uint32_t p = items[i].addr;
            for (int d = 0; d < 12 && is_ptr(p); d++) {
                char nm[17]; guest_str(p + 0xbc, nm, sizeof nm);
                if (!strcmp(nm, "WinN")) { modal = p; break; }
                p = r32(p + 0x0c);
            }
        }
        if (modal) {
            int k3 = 0;
            for (int i = 0; i < ni; i++) if (is_under(items[i].addr, modal)) items[k3++] = items[i];
            ni = k3;
            /* the buttons are not read as screen text (that would sound as if Yes were chosen):
             * the question is read, then the chosen button by the menu rule */
            int k4 = 0;
            for (int i = 0; i < nl; i++) {
                char nm[17]; guest_str(lines[i].addr + 0xbc, nm, sizeof nm);
                if (is_under(lines[i].addr, modal) && !strcmp(nm, "ButtonText")) continue;
                if (lines[i].name == cn) { speak_lines(&lines[i], 1); continue; }   /* the question at once, before the chosen button */
                lines[k4++] = lines[i];
            }
            nl = k4;
            if (modal != s_modal_prev) { s_modal_prev = modal; s_modal_t = now; }
        } else s_modal_prev = 0;
    }
    static Item sorted[256];
    memcpy(sorted, items, sizeof(Item) * ni);
    for (int i = 0; i < ni; i++) for (int j = i + 1; j < ni; j++) {
        /* rows (within 30 units of height), then left to right */
        int sw = sorted[j].y > sorted[i].y + 30 || (fabsf(sorted[j].y - sorted[i].y) <= 30 && sorted[j].x < sorted[i].x);
        if (sw) { Item t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    }
    s_all = sorted; s_nall = ni;
    s_nannounced = 0;
    if (g_text_on) check_menus(items, ni);
    if (nl) {
        /* lines of a menu whose selection was just announced are not read again */
        int k2 = 0;
        for (int i = 0; i < nl; i++) {
            int skip = 0;
            for (int j = 0; j < s_nannounced; j++) if (lines[i].root == s_announced[j]) skip = 1;
            /* rows of a scrolling list change text as it scrolls: only the selection is read */
            for (int j = 0; j < 32; j++) if (s_menu[j].scroll && s_menu[j].sel && s_menu[j].root == lines[i].root && s_menu[j].name == lines[i].name) skip = 1;
            if (!skip) lines[k2++] = lines[i];
        }
        /* new screen text waits until nothing more has appeared for 0.12 s
         * (0.4 s at most), so pieces drawn a frame apart are read together */
        for (int i = 0; i < k2; i++) {
            int at = -1;
            for (int q = 0; q < s_npend; q++) if (s_pend[q].addr == lines[i].addr && s_pend[q].raw == lines[i].raw) at = q;
            if (at < 0 && s_npend < 64) at = s_npend++;
            if (at >= 0) s_pend[at] = lines[i];
        }
        if (k2) { if (!s_pend_first) s_pend_first = now; s_pend_last = now; }
    }
    if (g_text_on) check_focus(sorted, ni);
    if (s_npend && (now - s_pend_last >= 120 || now - s_pend_first >= 400)) {
        speak_lines(s_pend, s_npend); s_npend = 0; s_pend_first = 0;
    }
    if (g_text_on) check_icons(items, ni);
}

static void speak_lines(Line *lines, int nl) {
    /* numbers alone (counters flickering on and off) are not read */
    int words = 0;
    for (int i = 0; i < nl; i++) for (const wchar_t *q = lines[i].text; *q; q++) if ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z')) { words = 1; break; }
    if (!words) return;
    /* where each text is now: screens slide in, so where it was when it
     * first appeared is not where it ends up */
    for (int i = 0; i < nl; i++) if (lines[i].addr && r32(lines[i].addr) == VT_TEXTBOX) { lines[i].x = rf32(lines[i].addr + 0x90); lines[i].y = rf32(lines[i].addr + 0xa0); }
    /* top to bottom, then left to right (layout y grows upward) */
    for (int i = 0; i < nl; i++) for (int j = i + 1; j < nl; j++) {
        int swap = lines[j].y > lines[i].y + 8 || (fabsf(lines[j].y - lines[i].y) <= 8 && lines[j].x < lines[i].x);
        if (swap) { Line t = lines[i]; lines[i] = lines[j]; lines[j] = t; }
    }
    /* pieces the game draws side by side under one pane ("Main" "100" "%",
     * "File" "1") are one phrase: read the whole group, once */
    uint32_t done[64]; int ndone = 0;
    /* counters drawn as "Count" and "Max" ("120" "120"): "120 of 120",
     * with "Energy Spheres" when they count spheres */
    for (int i = 0; i < nl; i++) {
        if (!lines[i].raw) continue;
        char nm[17]; guest_str(lines[i].addr + 0xbc, nm, sizeof nm);
        if (strcmp(nm, "Count") && strcmp(nm, "Max")) continue;
        uint32_t anc = lines[i].addr;
        for (int d = 0; d < 3 && is_ptr(r32(anc + 0x0c)); d++) anc = r32(anc + 0x0c);
        int seen = 0;
        for (int q = 0; q < ndone; q++) if (done[q] == anc) seen = 1;
        if (seen) { lines[i].text[0] = 0; continue; }
        const wchar_t *cnt = NULL, *mx = NULL;
        for (int j = 0; j < s_nall; j++) {
            if (!is_under(s_all[j].addr, anc)) continue;
            char n2[17]; guest_str(s_all[j].addr + 0xbc, n2, sizeof n2);
            if (!strcmp(n2, "Count")) cnt = s_all[j].text; else if (!strcmp(n2, "Max")) mx = s_all[j].text;
        }
        if (!cnt || !mx) continue;
        int spheres = 0;
        for (uint32_t p = lines[i].addr; is_ptr(p); p = r32(p + 0x0c)) { char n3[17]; guest_str(p + 0xbc, n3, sizeof n3); if (strstr(n3, "Sphe")) { spheres = 1; break; } }
        if (ndone < 64) done[ndone++] = anc;
        /* a count with nothing to say what it counts is noise */
        if (!spheres) { lines[i].text[0] = 0; continue; }
        swprintf(lines[i].text, 300, L"Energy Spheres: %ls of %ls", cnt, mx);
        lines[i].raw = 3;
    }
    /* a button prompt on the world map ("1 Planet Popstar"): what it does, then the button */
    for (int i = 0; i < nl; i++) {
        if (!lines[i].raw) continue;
        char nm[17]; guest_str(lines[i].addr + 0xbc, nm, sizeof nm);
        if (strncmp(nm, "AllSphereText", 13)) continue;
        button_prompt(lines[i].addr, lines[i].text, 300); lines[i].raw = 3;
    }
    /* first, text inside a menu item: the whole item, in reading order */
    for (int i = 0; i < nl; i++) {
        if (lines[i].raw != 1 || !lines[i].text[0]) continue;
        uint32_t cont = 0;
        for (int c = 0; c < s_nconts && !cont; c++) if (is_under(lines[i].addr, s_conts[c])) cont = s_conts[c];
        if (!cont) continue;
        int seen = 0;
        for (int q = 0; q < ndone; q++) if (done[q] == cont) seen = 1;
        if (seen) { lines[i].text[0] = 0; continue; }
        if (ndone < 64) done[ndone++] = cont;
        wchar_t g[300]; g[0] = 0;
        for (int j = 0; j < s_nall; j++) {
            if (!is_under(s_all[j].addr, cont)) continue;
            int twice = 0;   /* a shadow copy of the same words */
            for (int c = 0; c < j; c++) if (is_under(s_all[c].addr, cont) && same_words(s_all[c].text, s_all[j].text) && fabsf(s_all[c].x - s_all[j].x) < 10 && fabsf(s_all[c].y - s_all[j].y) < 10) twice = 1;
            if (twice) continue;
            if (g[0]) wcsncat(g, L" ", 299 - wcslen(g));
            wcsncat(g, s_all[j].text, 299 - wcslen(g));
        }
        msg_normalise(g);
        if (g[0]) { wcsncpy(lines[i].text, g, 299); lines[i].text[299] = 0; lines[i].raw = 2; }
    }
    for (int i = 0; i < nl; i++) {
        if (lines[i].raw != 1 || !lines[i].text[0]) continue;
        uint32_t par = r32(lines[i].addr + 0x0c);
        if (!is_ptr(par)) continue;
        int idx[8], k = 0, many = 0;
        for (int j = 0; j < s_nall; j++) if (r32(s_all[j].addr + 0x0c) == par) { if (k < 8) idx[k++] = j; else many = 1; }
        if (k < 2 || many || k > 5) continue;
        int seen = 0;
        for (int q = 0; q < ndone; q++) if (done[q] == par) seen = 1;
        if (seen) { lines[i].text[0] = 0; continue; }
        if (ndone < 64) done[ndone++] = par;
        for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) if (s_all[idx[b]].x < s_all[idx[a]].x) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
        wchar_t g[300]; g[0] = 0;
        for (int a = 0; a < k; a++) {
            int twice = 0;   /* a shadow copy of the same words */
            for (int c = 0; c < a; c++) if (same_words(s_all[idx[c]].text, s_all[idx[a]].text) && fabsf(s_all[idx[c]].x - s_all[idx[a]].x) < 10 && fabsf(s_all[idx[c]].y - s_all[idx[a]].y) < 10) twice = 1;
            if (twice) continue;
            if (g[0]) wcsncat(g, L" ", 299 - wcslen(g));
            wcsncat(g, s_all[idx[a]].text, 299 - wcslen(g));
        }
        msg_normalise(g);
        wcsncpy(lines[i].text, g, 299); lines[i].text[299] = 0;
    }
    /* a number left on its own, with no words, is noise */
    for (int i = 0; i < nl; i++) {
        int letters = 0;
        for (const wchar_t *q = lines[i].text; *q; q++) if ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z')) { letters = 1; break; }
        if (!letters) lines[i].text[0] = 0;
    }
    /* not again what was just said: the same words in the last 4 seconds,
     * or words the menu announcement just included */
    DWORD now = GetTickCount();
    static wchar_t all[4096];
    all[0] = 0;
    for (int i = 0; i < nl; i++) {
        if (!lines[i].text[0]) continue;
        /* the same words twice in one go (shadow text, duplicates) once */
        int dup = 0;
        for (int j = 0; j < i; j++) if (same_words(lines[i].text, lines[j].text)) dup = 1;
        if (said_lately(lines[i].text, 4000)) dup = 1;
        if (now - s_last_menu_t < 2000 && wcsstr(s_last_menu, lines[i].text)) dup = 1;
        if (dup) continue;
        note_said(lines[i].text);
        if (all[0]) wcsncat(all, L". ", 4095 - wcslen(all));
        wcsncat(all, lines[i].text, 4095 - wcslen(all));
    }
    if (!all[0]) return;
    /* a word like "percent" or "Back" alone, with nothing else new, is not worth saying */
    if (!wcscmp(all, L"percent")) return;
    if (g_text_log) { char *u = to_utf8(all); log_line("text: %s", u ? u : "?"); free(u); }
    s_last_lines_t = now;
    wcsncpy(s_last_all, all, 1023); s_last_all[1023] = 0;
    speak(all, 0);
}

/* Text of a HUD box: the box named `box` under a pane named `panel`, e.g.
 * lives = "Count" under "RestPlayerPanelN". Empty if not on screen. */
int text_hud(const char *panel, const char *box, wchar_t *out, int cap) {
    out[0] = 0;
    for (int i = 0; i < s_nbox; i++) {
        uint32_t a = s_box[i].addr;
        if (!s_box[i].shown || r32(a) != VT_TEXTBOX) continue;
        char nm[17]; guest_str(a + 0xbc, nm, sizeof nm);
        if (strcmp(nm, box)) continue;
        uint32_t p = r32(a + 0x0c); int under = 0;
        for (int d = 0; d < 8 && is_ptr(p); d++) { guest_str(p + 0xbc, nm, sizeof nm); if (!strcmp(nm, panel)) { under = 1; break; } p = r32(p + 0x0c); }
        if (!under) continue;
        /* digits only: box_text drops number-only text */
        uint32_t tp = r32(a + 0xd8); int n = r16(a + 0xfe), k = 0;
        for (int j = 0; j < n && k < cap - 1 && is_ptr(tp); j++) { uint16_t c = r16(tp + 2 * j); if (c >= '0' && c <= '9') out[k++] = c; }
        out[k] = 0;
        return k;
    }
    return 0;
}
