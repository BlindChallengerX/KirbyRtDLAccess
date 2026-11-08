/*
 * Where Kirby is: a short description the first time he enters a level's
 * door room, the Lor Starcutter or an Arena room, and the stage's name when
 * a stage starts, followed by the stage's hint (hints.inc). Shift+F5 says it
 * again; Ctrl+F5 turns hints on or off (kept in KirbyAccess\options.ini),
 * Ctrl+Shift+F5 repeats the hint.
 *
 * The map kind is at context +0x520 (context = Component +0, see
 * t_nextMapKind). Kinds 0-335 are in the game's table (areas.inc): level
 * stages, Challenge stages and the Arena. Kinds after that (level door
 * rooms 344.., the ship 356/357 on a 100% file) are told apart by their
 * size, which differs for every one of them (map/step/other/lvmap and
 * ship files: width at 0x4c, height at 0x50).
 */
#include "access.h"
#include <stdio.h>
#include <string.h>

#include "areas.inc"
#include "hints.inc"

static const char *const k_level_names[7] = {
    "Cookie Country", "Raisin Ruins", "Onion Ocean", "White Wafers", "Nutty Noon", "Egg Engines", "Dangerous Dinner",
};
static const int k_level_stages[7] = { 4, 4, 4, 5, 5, 5, 3 };   /* then the boss stage */
/* Dangerous Dinner: stage 4 is Landia, 5 and 6 are Another Dimension (Lor Starcutter, Magolor) */

/* door rooms: size, then what the level is like */
static const struct { int w, h; const char *text; } k_hubs[7] = {
    { 57, 27, "Cookie Country, level 1: the green hills, forests and caves of Dream Land. This is the level's door room. Each door leads to a stage: walk left and right, and press up at a door to go in. The doors open one after another as you clear the stages, and the last one leads to the boss, Whispy Woods. F4 lists the doors." },
    { 71, 31, "Raisin Ruins, level 2: a sandy desert with ancient ruins and pyramids. This is the level's door room: each door leads to a stage, and the last one to the boss, Mr. Dooter. F4 lists the doors." },
    { 49, 30, "Onion Ocean, level 3: beaches and the sea, with stages where Kirby swims under water. This is the level's door room: each door leads to a stage, and the last one to the boss, Fatty Puffer. F4 lists the doors." },
    { 70, 26, "White Wafers, level 4: snowfields, slippery ice and frozen caves. This is the level's door room: each door leads to a stage, and the last one to the boss, Goriath. F4 lists the doors." },
    { 35, 68, "Nutty Noon, level 5: high in the sky, among clouds and floating islands. This door room is tall: the doors are above one another, so float upward to reach them. The last door leads to the boss, Grand Doomer. F4 lists the doors." },
    { 51, 31, "Egg Engines, level 6: a huge mechanical fortress full of machines and moving parts. This is the level's door room: each door leads to a stage, and the last one to the boss, Metal General. F4 lists the doors." },
    { 33, 52, "Dangerous Dinner, level 7: the volcanic land of Halcandra, full of lava and fire, where Landia waits. This door room is tall: float upward to reach the doors above. F4 lists the doors." },
};

static const char *k_ship_out = "The deck of the Lor Starcutter, Magolor's flying ship, which is your base between levels. A Warp Star floats above the deck: jump into it to fly to the World Map. A door leads inside the ship.";
static const char *k_ship_in = "Inside the Lor Starcutter. You come in at the main room, where Magolor is. Rooms branch off from it: the Copy Ability rooms to the left, and the sub-game rooms down below. They open as you collect Energy Spheres. F4 lists the doors and says which are still locked.";
static const char *k_ship_ability = "A Copy Ability room on the Lor Starcutter: a small room where you can take a copy ability to use in the stages.";

static int s_last_kind = -1;
static char s_where[1100];
static unsigned char s_told[512];
static DWORD s_due;            /* when to say it (after the stage's own title) */
static int s_hints = -1;       /* stage hints on (options.ini), -1 = not read yet */

static void options_path(wchar_t *p) { swprintf(p, MAX_PATH, L"%lsoptions.ini", g_mod_dir); }

static int hints_on(void) {
    if (s_hints < 0) { wchar_t p[MAX_PATH]; options_path(p); s_hints = GetPrivateProfileIntW(L"options", L"hints", 1, p) != 0; }
    return s_hints;
}

/* The hint for a level stage map kind, or NULL. */
static const char *stage_hint(int kind) {
    if (kind < 0 || kind >= (int)(sizeof k_areas / sizeof *k_areas) || k_areas[kind][0] != 0) return NULL;
    const unsigned char *a = k_areas[kind];
    if (a[1] > 6 || a[2] < 1 || a[2] > 6) return NULL;
    return k_stage_hints[a[1]][a[2] - 1];
}

/* The description for this place, or "" if there is none. */
static void describe(int kind, int w, int h, char *out, size_t cap) {
    out[0] = 0;
    if (kind >= 0 && kind < (int)(sizeof k_areas / sizeof *k_areas)) {
        const unsigned char *a = k_areas[kind];
        switch (a[0]) {
        case 0: {
            int lv = a[1], st = a[2];
            if (lv > 6) return;
            if (a[3] != 1) return;                     /* only the first area of a stage */
            if (lv == 6 && st > 4) snprintf(out, cap, "Another Dimension, %s.", st == 5 ? "the Lor Starcutter" : "the final battle");
            else if (st > k_level_stages[lv]) snprintf(out, cap, "%s, the boss stage.", k_level_names[lv]);
            else snprintf(out, cap, "%s, stage %d of %d.", k_level_names[lv], st, k_level_stages[lv]);
            return;
        }
        case 1: if (a[3] == 1) snprintf(out, cap, "A Challenge stage: race to the goal and collect point stars before time runs out. The more you collect, the better the medal."); return;
        case 3: case 6:
            snprintf(out, cap, "%s. Fight the bosses one after another. Copy Ability bubbles sit on stands in this room: take one if you like, then jump into the Warp Star above to start.", a[0] == 3 ? "The Arena" : "The True Arena");
            return;
        case 4: case 7:
            snprintf(out, cap, "A rest room between Arena fights: Copy Ability bubbles on stands, and Maxim Tomatoes to heal. Jump into the Warp Star above for the next fight.");
            return;
        }
        return;
    }
    for (int i = 0; i < 7; i++) if (w == k_hubs[i].w && h == k_hubs[i].h) { snprintf(out, cap, "%s", k_hubs[i].text); return; }
    if (w == 95 && h == 32) snprintf(out, cap, "%s", k_ship_in);
    else if ((w == 38 && (h == 31 || h == 32)) || (w == 50 && h == 30)) snprintf(out, cap, "%s", k_ship_out);
    else if (w == 24 && h == 26) snprintf(out, cap, "%s", k_ship_ability);
}

int area_kind(uint32_t comp) {
    uint32_t ctx = r32(comp);
    return is_ptr(ctx) ? (int)r32(ctx + 0x520) : -1;
}

/* A new area: say where Kirby is, the first time in this session (stage names every time). */
void areas_enter(uint32_t comp, int w, int h) {
    int kind = area_kind(comp);
    if (kind == s_last_kind) return;
    s_last_kind = kind;
    describe(kind, w, h, s_where, sizeof s_where);
    if (!s_where[0]) return;
    const char *hint = stage_hint(kind);
    if (hint && hints_on()) snprintf(s_where + strlen(s_where), sizeof s_where - strlen(s_where), " %s", hint);
    int stage = kind >= 0 && kind < (int)(sizeof k_areas / sizeof *k_areas) && k_areas[kind][0] == 0;
    if (!stage && kind >= 0 && kind < 512) {
        if (s_told[kind]) return;
        s_told[kind] = 1;
    }
    s_due = GetTickCount() + 900;
}

void areas_poll(void) {
    if (s_due && (int)(GetTickCount() - s_due) >= 0) { s_due = 0; if (s_where[0]) speak_utf8(s_where, 0); }
}

/* Shift+F5 */
void areas_say(uint32_t comp, int w, int h) {
    char buf[700];
    describe(area_kind(comp), w, h, buf, sizeof buf);
    if (!buf[0]) {
        int kind = area_kind(comp);
        if (kind >= 0 && kind < (int)(sizeof k_areas / sizeof *k_areas) && k_areas[kind][0] == 0 && k_areas[kind][1] < 7) {
            const unsigned char *a = k_areas[kind];
            if (a[1] == 6 && a[2] > 4) snprintf(buf, sizeof buf, "Another Dimension, %s, area %d.", a[2] == 5 ? "the Lor Starcutter" : "the final battle", a[3]);
            else if (a[2] > k_level_stages[a[1]]) snprintf(buf, sizeof buf, "%s, the boss stage, area %d.", k_level_names[a[1]], a[3]);
            else snprintf(buf, sizeof buf, "%s, stage %d, area %d.", k_level_names[a[1]], a[2], a[3]);
        } else snprintf(buf, sizeof buf, "No description for this place.");
    }
    speak_utf8(buf, 1);
}

/* Ctrl+F5: hints on/off. Ctrl+Shift+F5: this stage's hint again. */
void areas_hint_key(uint32_t comp, int shift) {
    if (shift) {
        const char *hint = comp ? stage_hint(area_kind(comp)) : NULL;
        speak_utf8(hint ? hint : "No hint here. Hints are for the level stages.", 1);
        return;
    }
    s_hints = !hints_on();
    wchar_t p[MAX_PATH]; options_path(p);
    WritePrivateProfileStringW(L"options", L"hints", s_hints ? L"1" : L"0", p);
    speak_utf8(s_hints ? "Stage hints on" : "Stage hints off", 1);
}

/* The level stage area Kirby is in: level 0-6, stage folder, area (map file) number. */
int area_place(uint32_t comp, int *lv, int *st, int *ar) {
    int kind = area_kind(comp);
    if (kind < 0 || kind >= (int)(sizeof k_areas / sizeof *k_areas) || k_areas[kind][0] != 0) return 0;
    *lv = k_areas[kind][1]; *st = k_areas[kind][2]; *ar = k_areas[kind][3];
    return 1;
}
