/*
 * Gameplay sounds for Kirby Access.
 *
 * Units are tiles (the game's own: Kirby is about one tile wide). y grows
 * upward, Kirby's position is his feet. Every sound that points at
 * something uses one code: left/right is the ear, and pitch rises one
 * semitone per tile the thing is above Kirby (falls when below).
 *
 *   landing tone   while Kirby is in the air: a steady tone for the ground
 *                  right under him, a note higher (2 semitones) per tile
 *                  closer, in clear steps. Sine over solid
 *                  ground, a softer triangle over a platform you can drop
 *                  through, buzzy over spikes, a low double pulse when there
 *                  is nothing under him (a pit). Kept quiet and low under
 *                  Kirby's own float sound. A wood-block knock when a new
 *                  platform comes under him, a thud when he lands.
 *   platforms      two quick knocks when a platform is just above
 *                  Kirby (up to 5 tiles), so you know you can go up there.
 *   edges          walking: a falling slide when the ground ahead drops, a
 *                  rising one when it steps up, a buzz before a pit.
 *   walls          a hum left or right, louder the closer (5 tiles): steady
 *                  for a real wall, throbbing for one to float over, ticking
 *                  for a step; the wall behind Kirby a little softer. A soft
 *                  rush of air for a ceiling above, a knock when you walk
 *                  into a wall.
 *   enemies        about once a second, each enemy within 14 tiles gives its
 *                  own sound (nearest first, up to 5). Obstacles you can't
 *                  beat (Gordo, Shotzo, spiky plants, falling rocks) give a
 *                  harsh buzz. Spikes in the ground buzz from where they are.
 *   route          the way to the exit door is worked out through the open
 *                  space of the stage (Kirby can float anywhere). When it
 *                  goes up, three rising notes; when it goes down, three
 *                  falling notes. A bell from the exit door when it is near,
 *                  and "Door, press up" when you stand in front of it.
 */
#include "access.h"
#include "bossdesc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GW 288
#define GH 160

/* ---- tunables / toggles ---- */
volatile int g_walls_on = 1, g_radar_on = 1, g_route_on = 1, g_ground_on = 1;
volatile int g_cue_log;
volatile int g_count_on = 1;   /* F9: say how many enemies come on screen */   /* dev: "cues on" logs every cue */
#define CUE(...) do { if (g_cue_log) log_line(__VA_ARGS__); } while (0)

/* ---- names ---- */
static const char *const k_enemy_names[147] = {
    "Sir Kibble", "Bronto Burt", "Waddle Dee", "Blade Knight", "Cappy", "Hot Head",
    "Knuckle Joe", "Puppet Waddle Dee", "Armor Waddle Dee", "Como", "Gordo", "Spiky plant",
    "Whippy", "Pacto", "Shotzo", "Leafan", "Waddle Doo", "Bouncy",
    "Sparky", "Needlous", "Broom Hatter", "Scarfy", "Nruff", "Flamer",
    "Tick", "Water Galbo", "Rocky", "Bulby", "Starman", "Twister",
    "Poppy Bros Jr", "Noddy", "Cerulean", "Blipper", "Gigatzo", "Babut",
    "Glunk", "Bouncy Sis", "Rolling ball", "Mumbies", "Waddle Dee", "Kabu",
    "Waddle Doo", "Mopoo", "Foley", "Jackle", "Bowby", "Owgulf",
    "Degout", "Shelt", "Lanzer", "Chilly", "Moonja", "Pluid",
    "Gemra", "Wapod", "Rolling ball", "Volttzo", "Barbar", "Barracu",
    "Squishy", "Clayn", "Pierce", "Searches", "Walky", "Elec",
    "Waddle Dee", "Waddle Doo", "Sodory", "Galbo", "Snowl", "Aqua rock",
    "Waddle Dee", "Coldtzo", "Coconut", "Craby", "Pacto", "Iron Barbar",
    "Walf", "Aqua rock", "Waddle Dee", "Darpa", "Dippa", "Dupa",
    "Leaper", "Bronto Burt", "Armor Waddle Dee", "Nruff", "Degout", "Scarfy",
    "Waddle Dee carrying a block", "Rolling ball", "Sandbag", "Waddle Dee", "Bouncy", "Bowby",
    "Sun", "Sun", "Crystal meteor", "Rolling ball", "Star", "Cappy's cap",
    "Magolor shot", "Magolor shot", "Magolor shot", "Magolor shot", "Apple", "Apple",
    "ball", "ball", "snake", "snake", "bomb", "Parasol",
    "Dubior junior", "bomb", "shot", "cutter blade", "bomb", "spark",
    "ball", "bit", "bomb", "missile", "missile", "thunder",
    "Fatty Buffer junior", "water shot", "stalactite", "Wapod", "snowball", "ice ball",
    "icicle", "missile", "magic ball", "Magolor", "defeated boss", "Landia",
    "Landia", "Waddle Dee", "shot", "Apple", "missile", "falling rock",
    "hammer", "Magolor", "Cappy",
};
#define NKINDS (int)(sizeof k_enemy_names / sizeof *k_enemy_names)

static const char *const k_ability_names[35] = {
    "no ability", "Sword", "Cutter", "Leaf", "Whip", "Fire", "Needle", "Beam", "Spark", "Stone",
    "Sleep", "Parasol", "Water", "Hi-Jump", "Tornado", "Bomb", "Spear", "Hammer", "Ice", "Wing",
    "Ninja", "Fighter", "Crash", "Mike", "Landia", "Ultra Sword", "Monster Flame", "Flare Beam",
    "Grand Hammer", "Snow Bowl", "Mike", "Mike", "Meta Knight", "King Dedede", "Bandana Dee",
};

const char *enemy_name(uint32_t kind) { return kind < (uint32_t)NKINDS ? k_enemy_names[kind] : "enemy"; }
const char *ability_name(uint32_t kind) { return kind < 35 ? k_ability_names[kind] : "an ability"; }

/* ---- shapes (T_GRID_SHAPE_INFO_TABLE in LandLayer.o): top edge height at the
 * left and right side of a cell ---- */
static const float k_top_l[52] = { 0, 1, 0, 0, 0.5f, 0, 0.3333f, 0.6667f, 1, 1, 0.5f, 1, 0.6667f, 0.3333f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0.5f, 0, 0.3333f, 0.6667f, 1, 1, 0.5f, 1, 0.6667f, 0.3333f, 1, 0, 0, 0.5f, 0, 0.3333f, 0.6667f, 1, 1, 0.5f, 1, 0.6667f, 0.3333f };
static const float k_top_r[52] = { 0, 1, 1, 0.5f, 1, 0.3333f, 0.6667f, 1, 0, 0.5f, 0, 0.6667f, 0.3333f, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.5f, 1, 0.3333f, 0.6667f, 1, 0, 0.5f, 0, 0.6667f, 0.3333f, 0, 1, 1, 0.5f, 1, 0.3333f, 0.6667f, 1, 0, 0.5f, 0, 0.6667f, 0.3333f, 0 };

/* ---- grid snapshot ---- */
static uint8_t g_shape[GW * GH], g_flag[GW * GH], g_block[GW * GH];
static int g_w, g_h;
static uint32_t g_layer;

static int grid_refresh(uint32_t comp) {
    uint32_t lm = r32(comp + 0xb0), layer = is_ptr(lm) ? r32(lm + 8) : 0;
    if (!is_ptr(layer)) return 0;
    int w = (int)r32(layer), h = (int)r32(layer + 4);
    if (w <= 0 || w > GW || h <= 0 || h > GH) return 0;
    const uint8_t *p = guest_ptr(layer + 8, (uint32_t)(GW * h * 10));
    if (!p) return 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t *c = p + (x + y * GW) * 10;
            g_shape[x + y * GW] = c[0] < 52 ? c[0] : 1;
            g_flag[x + y * GW] = c[1];
        }
    /* breakable blocks are not in the land layer (their cells read empty):
     * they stand on the gimmick BlockManager's grid (+0x3144, Block* per
     * cell, cleared when one breaks). Kirby stands and bumps on them. */
    uint32_t gm = r32(comp + 0xb4), bm = is_ptr(gm) ? r32(gm + 0x48) : 0;
    const uint8_t *bp = is_ptr(bm) ? guest_ptr(bm + 0x3144, (uint32_t)(GW * h * 4)) : NULL;
    memset(g_block, 0, sizeof g_block);
    if (bp)
        for (int i = 0; i < GW * h; i++)
            if (bp[4 * i] == 0x80 || bp[4 * i] == 0x81 || bp[4 * i] == 0x90 || bp[4 * i] == 0x91 || bp[4 * i] == 0x92 || bp[4 * i] == 0x93) { g_shape[i] = 1; g_block[i] = 1; }
    /* locked gates (gimmick lockshutter, +0x120): a column 3 tiles tall from
     * its foot, solid until unlocked; not in the land layer either */
    uint32_t lk = is_ptr(gm) ? r32(gm + 0x120) : 0;
    if (is_ptr(lk)) {
        uint32_t n = r32(lk + 4);
        for (uint32_t i = 0; i < n && i < 8; i++) {
            uint32_t g = r32(lk + 8 + 4 * i);
            if (!is_ptr(g) || r32(g + 0x240) != 0) continue;
            int gx = (int)floorf(rf32(g + 0xc)), gy = (int)floorf(rf32(g + 0x10) + 0.01f);
            for (int r = gy; r < gy + 3; r++) if (gx >= 0 && gx < GW && r >= 0 && r < h) { g_shape[gx + r * GW] = 1; g_block[gx + r * GW] = 2; }
        }
    }
    /* moving land (mcoll MoveGridManager: LandManager +0x20, MoveGridGroup*[64]).
     * Group: +4 u16 x, +6 u16 y (base cell), +0x10 byte 1 = in use, +0x1c/+0x20
     * float offset now, +0x44 cell count, +0x54 cells, 8 bytes each (1, cx, cy,
     * shape). Not in the land layer: the switch gates of the Landia chase in
     * Another Dimension are these, and moving walls and floors elsewhere. */
    for (int i = 0; i < 64; i++) {
        uint32_t gp = r32(lm + 0x20 + 4 * i);
        if (!is_ptr(gp)) break;
        if (r8(gp + 0x10) != 1) continue;
        uint32_t n = r32(gp + 0x44), cells = r32(gp + 0x54);
        if (n == 0 || n > 2000 || !is_ptr(cells)) continue;
        const uint8_t *c = guest_ptr(cells, n * 8);
        if (!c) continue;
        float ox = rf32(gp + 0x1c), oy = rf32(gp + 0x20);
        if (!(fabsf(ox) < 300 && fabsf(oy) < 300)) continue;
        int bx = (int)(r32(gp + 4) >> 16), by = (int)(r32(gp + 4) & 0xffff);
        for (uint32_t j = 0; j < n; j++, c += 8) {
            if (c[0] != 1 || c[3] == 0) continue;
            int x = (int)floorf(bx + c[1] + ox + 0.5f), y = (int)floorf(by + c[2] + oy + 0.5f);
            if (x >= 0 && x < w && y >= 0 && y < h) g_shape[x + y * GW] = c[3] < 52 ? c[3] : 1;
        }
    }
    g_w = w; g_h = h; g_layer = layer;
    return 1;
}

static inline int shape_at(int x, int y) { return (x < 0 || y < 0 || x >= g_w || y >= g_h) ? 1 : g_shape[x + y * GW]; }
static inline int flag_at(int x, int y) { return (x < 0 || y < 0 || x >= g_w || y >= g_h) ? 0 : g_flag[x + y * GW]; }
static inline int is_platform(int s) { return s >= 26; }
static inline int is_slope(int s) { return s >= 2 && s <= 13; }
/* A cell that blocks walking sideways at full height. */
static inline int is_wall(int x, int y) { int s = shape_at(x, y); return s == 1 || (s >= 14 && s <= 25); }
static inline int is_spike(int x, int y) { return (flag_at(x, y) & 0x10) != 0; }
static inline int is_water(int x, int y) { return (flag_at(x, y) & 0x08) != 0; }
/* ladder cells: flag 0x02 (LandManager::searchLadder), a column whose top cell is the thin platform on it */
static inline int is_ladder(int x, int y) { return (flag_at(x, y) & 0x02) != 0; }
int land_is_water(int x, int y) { return is_water(x, y); }

/* Top of a cell at fractional u (0 left .. 1 right), or -1 if it has no top. */
static float cell_top(int s, float u) {
    if (s == 0) return -1;
    if (s >= 14 && s <= 25) return 1;
    return k_top_l[s] + (k_top_r[s] - k_top_l[s]) * u;
}

/* The surface under (x, y): its height, cell kind, and whether it is a
 * platform; -1000 when nothing is under it down to the bottom of the map. */
typedef struct { float h; int platform, spike, water, pit; } Ground;
static Ground ground_below(float x, float y) {
    Ground g = { -1000, 0, 0, 0, 1 };
    int cx = (int)floorf(x);
    float u = x - cx;
    int cy = (int)floorf(y - 0.02f);
    if (cy >= g_h) cy = g_h - 1;
    for (; cy >= 0; cy--) {
        int s = shape_at(cx, cy);
        float t = cell_top(s, u);
        if (t < 0) continue;
        float top = cy + t;
        if (top > y + 0.05f) {
            if (is_platform(s)) continue;          /* platform above the feet: pass through */
            if (s == 1 || s >= 14) { /* inside solid (wall overlap): keep searching below */ continue; }
        }
        g.h = top; g.platform = is_platform(s); g.spike = is_spike(cx, cy) || is_spike(cx, cy + 1);
        g.water = is_water(cx, cy + 1); g.pit = 0;
        return g;
    }
    return g;
}

/* How high the first wall in direction dir rises above Kirby's feet: the
 * top of its column, where there is room for Kirby (two open cells).
 * 99 when it goes on past 12 tiles or to the top of the map (a real wall). */
static float wall_height(float x, float y, int dir, float range) {
    int r0 = (int)floorf(y + 0.6f), r1 = (int)floorf(y + 0.95f);
    int cx = (int)floorf(x);
    for (int i = 1; i <= (int)range + 1; i++) {
        int c = cx + dir * i;
        if (is_wall(c, r0) || is_wall(c, r1)) {
            for (int r = r0; r < r0 + 13; r++) {
                if (r + 1 >= g_h) return 99;
                if (!is_wall(c, r) && !is_wall(c, r + 1)) return (float)r - y;
            }
            return 99;
        }
    }
    return -1;
}

/* Distance from Kirby to the first wall in direction dir (-1/1) at body height. */
static float wall_dist(float x, float y, int dir, float range) {
    /* body height above a step: a block whose top is within half a tile of
     * the feet is a step Kirby walks onto (slopes, stairs), not a wall */
    int r0 = (int)floorf(y + 0.6f), r1 = (int)floorf(y + 0.95f);
    int cx = (int)floorf(x);
    for (int i = 1; i <= (int)range + 1; i++) {
        int c = cx + dir * i;
        if (is_wall(c, r0) || is_wall(c, r1)) {
            float edge = dir > 0 ? (float)c : (float)(c + 1);
            float d = fabsf(edge - x) - 0.5f;
            return d < 0 ? 0 : d;
        }
    }
    return -1;
}

/* ---- enemy sounds ---- */
typedef struct { float f; int wave; int pat; int hazard; } Sig;
/* pat: 0 single blip, 1 double, 2 trill, 3 slide up, 4 slide down, 5 long buzz, 6 thud */
static Sig enemy_sig(uint32_t kind) {
    const char *n = enemy_name(kind);
    static const struct { const char *n; Sig s; } t[] = {
        { "Waddle Dee", { 520, W_SINE, 0, 0 } },  { "Waddle Doo", { 520, W_SINE, 1, 0 } },
        { "Bronto Burt", { 900, W_TRI, 2, 0 } },  { "Sir Kibble", { 1250, W_SQUARE, 1, 0 } },
        { "Blade Knight", { 760, W_SQUARE, 4, 0 } }, { "Hot Head", { 330, W_NOISE, 0, 0 } },
        { "Sparky", { 640, W_SAW, 2, 0 } },       { "Gordo", { 140, W_SAW, 5, 1 } },
        { "Spiky plant", { 230, W_SAW, 5, 1 } },  { "Shotzo", { 180, W_SQUARE, 1, 1 } },
        { "Cappy", { 440, W_TRI, 3, 0 } },        { "Knuckle Joe", { 400, W_SQUARE, 1, 0 } },
        { "Leafan", { 660, W_TRI, 4, 0 } },       { "Chilly", { 1400, W_SINE, 1, 0 } },
        { "Rocky", { 120, W_SINE, 6, 0 } },       { "Bouncy", { 300, W_SINE, 3, 0 } },
        { "Poppy Bros Jr", { 820, W_SQUARE, 0, 0 } }, { "Broom Hatter", { 600, W_NOISE, 2, 0 } },
        { "Scarfy", { 980, W_SAW, 0, 0 } },       { "Flamer", { 280, W_NOISE, 2, 0 } },
        { "Twister", { 500, W_TRI, 2, 0 } },      { "Noddy", { 260, W_SINE, 4, 0 } },
        { "Starman", { 1100, W_TRI, 3, 0 } },     { "Needlous", { 350, W_SAW, 1, 0 } },
        { "Pacto", { 200, W_SQUARE, 6, 0 } },     { "Blipper", { 700, W_SINE, 2, 0 } },
        { "Glunk", { 240, W_TRI, 1, 0 } },        { "Rolling ball", { 110, W_NOISE, 5, 1 } },
        { "Sun", { 200, W_SAW, 5, 1 } },          { "Crystal meteor", { 1500, W_TRI, 5, 1 } },
        { "falling rock", { 100, W_NOISE, 5, 1 } }, { "stalactite", { 160, W_NOISE, 5, 1 } },
        { "icicle", { 1800, W_SAW, 4, 1 } },      { "Whippy", { 560, W_TRI, 0, 0 } },
        { "Puppet Waddle Dee", { 520, W_SINE, 5, 0 } }, { "Armor Waddle Dee", { 260, W_SQUARE, 6, 0 } },
        { "Como", { 1500, W_TRI, 4, 0 } },        { "Bouncy Sis", { 600, W_SINE, 3, 0 } },
        { "Nruff", { 170, W_SQUARE, 2, 0 } },     { "Tick", { 1600, W_SAW, 0, 0 } },
        { "Water Galbo", { 380, W_NOISE, 3, 0 } }, { "Bulby", { 700, W_TRI, 6, 0 } },
        { "Cerulean", { 880, W_SINE, 1, 0 } },    { "Gigatzo", { 150, W_SQUARE, 5, 1 } },
        { "Coldtzo", { 300, W_TRI, 5, 1 } },      { "Volttzo", { 600, W_SAW, 5, 1 } },
        { "Babut", { 1300, W_SQUARE, 2, 0 } },    { "Mumbies", { 200, W_SINE, 5, 0 } },
        { "Kabu", { 150, W_TRI, 0, 0 } },         { "Mopoo", { 800, W_SINE, 4, 0 } },
        { "Foley", { 1300, W_TRI, 1, 0 } },       { "Jackle", { 1000, W_SAW, 3, 0 } },
        { "Bowby", { 900, W_SQUARE, 3, 0 } },     { "Owgulf", { 600, W_TRI, 1, 0 } },
        { "Degout", { 220, W_SQUARE, 0, 0 } },    { "Shelt", { 700, W_SQUARE, 2, 0 } },
        { "Lanzer", { 500, W_SQUARE, 3, 0 } },    { "Moonja", { 1800, W_TRI, 0, 0 } },
        { "Pluid", { 1200, W_SINE, 4, 0 } },      { "Gemra", { 950, W_SAW, 1, 0 } },
        { "Wapod", { 330, W_TRI, 4, 0 } },        { "Barbar", { 120, W_SQUARE, 5, 1 } },
        { "Iron Barbar", { 180, W_NOISE, 1, 1 } }, { "Barracu", { 400, W_SQUARE, 4, 0 } },
        { "Squishy", { 950, W_SINE, 3, 0 } },     { "Clayn", { 180, W_SINE, 2, 0 } },
        { "Pierce", { 1600, W_SQUARE, 1, 0 } },   { "Searches", { 300, W_SAW, 0, 0 } },
        { "Walky", { 1050, W_SINE, 2, 0 } },      { "Elec", { 800, W_SAW, 0, 0 } },
        { "Galbo", { 260, W_NOISE, 1, 0 } },      { "Snowl", { 1700, W_SINE, 2, 0 } },
        { "Coconut", { 400, W_TRI, 6, 0 } },      { "Craby", { 560, W_SQUARE, 0, 0 } },
        { "Walf", { 340, W_SINE, 1, 0 } },        { "Darpa", { 1250, W_TRI, 2, 0 } },
        { "Dippa", { 1250, W_TRI, 2, 0 } },       { "Dupa", { 1250, W_TRI, 2, 0 } },
        { "Leaper", { 750, W_TRI, 3, 0 } },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) if (!strcmp(t[i].n, n)) return t[i].s;
    /* everyone else: a stable sound made from the name */
    uint32_t hsh = 2166136261u;
    for (const char *p = n; *p; p++) hsh = (hsh ^ (uint8_t)*p) * 16777619u;
    Sig s;
    s.f = 300.0f * powf(2.0f, (float)(hsh % 25) / 12.0f);
    s.wave = (int)((hsh >> 8) % 3);   /* sine, square, tri */
    s.pat = (int)((hsh >> 12) % 5);
    s.hazard = 0;
    return s;
}

/* A real enemy (something to fight), not an obstacle or a thrown object:
 * regular enemy kinds, and the minions bosses call in. */
static int creature_kind(uint32_t kind) {
    if (enemy_sig(kind).hazard) return 0;
    if (kind < 96) return 1;
    const char *n = enemy_name(kind);
    return !strcmp(n, "Dubior junior") || !strcmp(n, "Fatty Buffer junior") || !strcmp(n, "snake") ||
           !strcmp(n, "Wapod") || !strcmp(n, "Waddle Dee") || !strcmp(n, "Cappy");
}
int enemy_is_creature(uint32_t kind) { return creature_kind(kind); }

/* Every enemy's sound starts with a short click, so enemies are never
 * mistaken for obstacles, items or blocks. */
static void play_click(float delay, float gain, float pan) {
    /* a crisp tick, never so quiet that it vanishes on a far enemy */
    float g = gain * 1.2f; if (g < 0.16f) g = 0.16f;
    tone_at(delay, 3200, 2400, 0.014f, g, pan, W_SQUARE);
}
/* The copy ability an enemy gives when swallowed, 0 for none
 * (k_ability_names numbering; from the game's own lists, checked on WiKirby). */
static uint32_t enemy_ability(uint32_t kind) {
    static const struct { const char *n; uint32_t ab; } t[] = {
        { "Waddle Doo", 7 }, { "Sir Kibble", 2 }, { "Blade Knight", 1 }, { "Hot Head", 5 }, { "Sparky", 8 },
        { "Chilly", 18 }, { "Knuckle Joe", 21 }, { "Poppy Bros Jr", 15 }, { "Leafan", 3 }, { "Whippy", 4 },
        { "Needlous", 6 }, { "Rocky", 9 }, { "Flamer", 5 }, { "Twister", 14 }, { "Noddy", 10 }, { "Starman", 13 },
        { "Tick", 6 }, { "Water Galbo", 12 }, { "Bulby", 3 }, { "Foley", 15 }, { "Jackle", 2 }, { "Owgulf", 19 },
        { "Lanzer", 16 }, { "Moonja", 20 }, { "Pluid", 12 }, { "Gemra", 7 }, { "Pierce", 16 }, { "Searches", 22 },
        { "Walky", 23 }, { "Elec", 8 }, { "Galbo", 5 }, { "Snowl", 18 }, { "Sodory", 1 },
    };
    const char *n = enemy_name(kind);
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) if (!strcmp(t[i].n, n)) return t[i].ab;
    return 0;
}
uint32_t enemy_gives_ability(uint32_t kind) { return enemy_ability(kind); }
/* After an ability enemy's sound: a tiny two-note sparkle. */
static void play_sparkle(float delay, float gain, float pan) {
    float g = gain * 0.9f; if (g < 0.15f) g = 0.15f;   /* 0.6 / 0.08 was easy to miss behind a metal click */
    tone_at(delay, 2637, 2637, 0.05f, g, pan, W_SINE);
    tone_at(delay + 0.055f, 3520, 3520, 0.07f, g, pan, W_SINE);
}
static void play_sig_raw(Sig s, float delay, float semis, float gain, float pan);
static void play_sig(Sig s, float delay, float semis, float gain, float pan) { play_sig_raw(s, delay, semis, gain, pan); }
static void play_enemy(uint32_t kind, float delay, float semis, float gain, float pan) {
    if (creature_kind(kind)) { play_click(delay, gain, pan); delay += 0.045f; }
    Sig s = enemy_sig(kind);
    play_sig_raw(s, delay, semis, gain, pan);
    if (enemy_ability(kind)) {
        float len = s.pat == 2 ? 0.14f : s.pat == 1 ? 0.14f : s.pat == 5 ? 0.24f : 0.13f;
        play_sparkle(delay + len, gain, pan);
    }
}
static void play_sig_raw(Sig s, float delay, float semis, float gain, float pan) {
    float f = s.f * powf(2.0f, semis / 12.0f);
    switch (s.pat) {
    case 1: tone_at(delay, f, f, 0.05f, gain, pan, s.wave); tone_at(delay + 0.08f, f, f, 0.05f, gain, pan, s.wave); break;
    case 2: for (int i = 0; i < 3; i++) tone_at(delay + i * 0.045f, f, f, 0.035f, gain, pan, s.wave); break;
    case 3: tone_at(delay, f * 0.75f, f * 1.25f, 0.12f, gain, pan, s.wave); break;
    case 4: tone_at(delay, f * 1.25f, f * 0.75f, 0.12f, gain, pan, s.wave); break;
    case 5: tone_at(delay, f, f, 0.22f, gain * 0.9f, pan, s.wave); break;
    case 6: tone_at(delay, f * 1.5f, f * 0.6f, 0.1f, gain * 1.3f, pan, s.wave); break;
    default: tone_at(delay, f, f, 0.07f, gain, pan, s.wave);
    }
}


/* ---- the cue sounds, shared with the Learn Sounds menu (learn.c) ---- */
static float semi(float s) { return powf(2.0f, s / 12.0f); }
void snd_ability(int got) { tone(got ? 660 : 440, got ? 990 : 330, 0.18f, 0.3f, 0, W_TRI); }
void snd_pit(void) { tone(95, 80, 0.12f, 0.45f, 0, W_SQUARE); tone_at(0.16f, 95, 80, 0.12f, 0.45f, 0, W_SQUARE); }
/* Thin platforms knock like a hollow wood block: a short body that drops in
 * pitch, an off-key overtone and a tiny click. Far from the wall knock (a low
 * square thud) and from the tonal cues. */
static void wood_knock(float delay, float f, float gain) {
    tone_at(delay, f, f * 0.82f, 0.045f, gain, 0, W_SINE);
    tone_at(delay, f * 2.76f, f * 2.3f, 0.025f, gain * 0.3f, 0, W_SINE);
    tone_at(delay, 3200, 1600, 0.008f, gain * 0.2f, 0, W_NOISE);
}
/* Two knocks falling: you can drop through the platform you stand on. */
void snd_drop_through(void) { wood_knock(0, 700, 0.32f); wood_knock(0.12f, 470, 0.32f); }
/* An enemy in reach to inhale: a soft breathy draw-in. */
void snd_in_reach(float pan) { tone(700, 2200, 0.09f, 0.22f, pan, W_NOISE); tone_at(0.02f, 523, 784, 0.07f, 0.12f, pan, W_SINE); }
/* One firm knock: a platform has just come under Kirby in the air. */
void snd_platform_under(void) { wood_knock(0, 560, 0.4f); }
void snd_ground_after_pit(void) { tone(784, 784, 0.06f, 0.3f, 0, W_TRI); tone_at(0.07f, 1175, 1175, 0.08f, 0.3f, 0, W_TRI); }
void snd_land(int platform) { tone(170, 80, 0.09f, platform ? 0.35f : 0.5f, 0, W_SINE); }
/* Two quick knocks, higher the higher it is: a platform just above Kirby. */
/* Ladder: two metal rung clinks, rising when the ladder goes up from Kirby's
 * height, falling when it goes down; pan = side. Metal (square + inharmonic
 * ring) so it can't be taken for the wooden platform knocks. */
static void rung_clink(float delay, float f, float gain, float pan) {
    tone_at(delay, f, f * 0.98f, 0.035f, gain, pan, W_SQUARE);
    tone_at(delay, f * 2.41f, f * 2.38f, 0.06f, gain * 0.35f, pan, W_TRI);
}
void snd_ladder(int dir, float gain, float pan) {
    float a = 784, b = dir > 0 ? 784 * semi(5) : dir < 0 ? 784 * semi(-5) : 784;
    rung_clink(0, a, gain, pan); rung_clink(0.09f, b, gain, pan);
}
void snd_rung_at(float delay, int height) { rung_clink(delay, 660 * semi((float)(height % 12)), 0.12f, 0); }
void snd_platform_above(int up) { float f = 640 * semi((float)up); wood_knock(0, f, 0.26f); wood_knock(0.09f, f, 0.22f); }
void snd_edge_pit(float pan) { tone(300, 150, 0.25f, 0.4f, pan, W_SAW); }
void snd_edge_spikes(float pan) { tone(200, 200, 0.18f, 0.4f, pan, W_SAW); }
void snd_drop(float pan) { tone(700, 350, 0.18f, 0.3f, pan, W_SINE); }
void snd_step_up(float pan) { tone(350, 700, 0.15f, 0.3f, pan, W_SINE); }
void snd_knock(float pan) { tone(150, 110, 0.07f, 0.55f, pan, W_SQUARE); }
void snd_spikes(float dy, float gain, float pan) { float f = 220 * semi(dy); tone(f, f, 0.12f, gain, pan, W_SAW); }
void snd_hub_door(float dy, float pan) { float f = 1319 * semi(dy); tone(f, f, 0.3f, 0.18f, pan, W_SINE); tone_at(0.12f, f * 1.26f, f * 1.26f, 0.3f, 0.12f, pan, W_SINE); }
void snd_warp_star(float dy, float pan) { tone(784 * semi(dy), 1568 * semi(dy), 0.25f, 0.18f, pan, W_SINE); }
/* Dimensional rift: a swirling rush of air around a wobbling low tone, like a
 * vortex. Muffled (hidden) = where a Super Ability will open one. */
void snd_rift(float dy, float gain, float pan, int hidden) {
    float m = semi(dy), g = (hidden ? gain * 0.5f : gain) * 0.5f, c = hidden ? 0.6f : 1.0f;
    tone(500 * m * c, 1600 * m * c, 0.22f, g * 0.55f, pan, W_NOISE);
    tone_at(0.2f, 1600 * m * c, 500 * m * c, 0.22f, g * 0.55f, pan, W_NOISE);
    tone(196 * m, 294 * m, 0.2f, g * 0.7f, pan, W_WARM);
    tone_at(0.2f, 294 * m, 196 * m, 0.24f, g * 0.6f, pan, W_WARM);
}
/* Fuse cannon: a low boom with a metal ring. */
void snd_cannon(float dy, float gain, float pan) {
    float m = semi(dy);
    tone(196 * m, 110 * m, 0.16f, gain * 0.7f, pan, W_WARM);
    tone(900, 200, 0.08f, gain * 0.35f, pan, W_NOISE);
    tone_at(0.05f, 1175 * m, 1175 * m, 0.12f, gain * 0.25f, pan, W_TRI);
}
/* Barrel cannon: a hollow barrel thump and a little puff. */
void snd_barrel(float dy, float gain, float pan) {
    float m = semi(dy);
    tone(220 * m, 150 * m, 0.07f, gain, pan, W_WARM);
    tone_at(0.06f, 700 * m, 1400 * m, 0.06f, gain * 0.3f, pan, W_NOISE);
}
void snd_route(int up, float pan) {
    float b = up > 0 ? 523 : 1047;
    for (int i = 0; i < 3; i++) { float f = b * semi((float)((up > 0 ? i : -i) * 4)); tone_at(i * 0.09f, f, f, 0.07f, 0.28f, pan, W_SINE); }
}
/* a door that is not the way on (side room, way back): lower, softer, a falling third */
void snd_side_door(float dy, float pan) { float f = 988 * semi(dy); tone(f, f, 0.25f, 0.13f, pan, W_SINE); tone_at(0.12f, f * 0.84f, f * 0.84f, 0.25f, 0.09f, pan, W_SINE); }
void snd_exit_door(float dy, float pan) { float f = 1319 * semi(dy); tone(f, f, 0.35f, 0.2f, pan, W_SINE); tone_at(0.12f, f * 1.5f, f * 1.5f, 0.3f, 0.12f, pan, W_SINE); }
/* Steady sounds (hum slots): 0 landing tone, 1 wall left, 2 wall right, 3 ceiling. */
/* Landing tone: steps of 2 semitones per tile (snapped, so heights can be
 * counted rather than guessed); hysteresis keeps it from flickering between two. */
void snd_landing_tone(float drop, int platform, int spike) {
    static float s_step = -1;
    if (s_step < 0 || fabsf(drop - s_step) > 0.6f) s_step = roundf(drop);
    /* kept low, quiet and dull so Kirby's own puffing while he floats stays
     * in front (0.6.6: was 880 Hz base at 0.13, bright) */
    float f = 523.0f * powf(2.0f, -s_step / 6.0f);
    if (f < 98) f = 98;
    hum_set_ex(0, f, spike ? 0.05f : 0.045f, 0, spike ? W_SAW : platform ? W_TRI : W_SINE, 0, 0, 0, spike ? 1400.0f : 900.0f);
}
/* Walls by height, told apart by texture as well as pitch:
 *   real wall (too high)        low warm hum, steady
 *   wall to float over (3-12)   a little higher, slow soft throb
 *   step to jump onto (1-2)     bright, ticking
 * Pan 0.8 (not hard); louder the closer (up to 5 tiles). */
void snd_wall_h(int side, float closeness, float height) {
    int slot = side < 0 ? 1 : 2; float pan = side < 0 ? -0.8f : 0.8f;
    float c = closeness > 0 ? powf(closeness, 1.5f) : 0;
    if (height <= 2.2f)    hum_set_ex(slot, 330, c * 0.38f, pan, W_TRI, 4.0f, 0.8f, 1, 0);
    else if (height < 90)  hum_set_ex(slot, 247, c * 0.30f, pan, W_WARM, 2.5f, 0.7f, 0, 0);
    else                   hum_set_ex(slot, 196, c * 0.25f, pan, W_WARM, 0, 0, 0, 0);
}
void snd_wall(int side, float closeness) { snd_wall_h(side, closeness, 99); }
/* Ceiling: soft filtered rush of air in the middle, nothing like the wall hums.
 * Heard up to 6 tiles above (a ceiling 3-5 up already stops a float); louder and
 * brighter the lower it is. */
void snd_ceiling(int dist) {
    if (dist <= 0) { hum_set_ex(3, 400, 0, 0, W_NOISE, 0, 0, 0, 500); return; }
    float closeness = 1.0f - (dist - 1) / 6.0f;                 /* 1 at 1 tile .. 0.17 at 6 */
    hum_set_ex(3, 400, 0.35f + 0.75f * closeness, 0, W_NOISE, 0, 0, 0, 350 + 650 * closeness);
}
void play_enemy_sound(uint32_t kind, float delay, float semis, float gain, float pan);
int  enemy_is_hazard(uint32_t kind);

/* ---- objects ---- */
typedef struct { uint32_t addr, kind, id; float x, y; } Enemy;
static int read_enemies(uint32_t comp, Enemy *out, int cap) {
    uint32_t em = r32(comp + 0xd0);
    if (!is_ptr(em)) return 0;
    uint32_t n = r32(em + 0x90);
    if (n > 48) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t e = r32(em + 0x94 + 4 * i);
        if (!is_ptr(e) || r8(e + 0x38)) continue;
        uint32_t loc = r32(e + 0x68);
        if (!is_ptr(loc)) continue;
        float x = rf32(loc), y = rf32(loc + 4);
        if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
        out[k].addr = e; out[k].kind = r32(e + 0x10); out[k].id = r32(e + 0xc); out[k].x = x; out[k].y = y;
        k++;
    }
    return k;
}

typedef struct { float x, y; int kind, role; const char *label; } Door;
#include "doors.inc"
int area_place(uint32_t comp, int *lv, int *st, int *ar);
/* Where a stage door leads (doors.inc), matched by area and position; role -1 when unknown. */
static void door_label(uint32_t comp, Door *d) {
    int lv, st, ar;
    d->role = -1; d->label = NULL;
    if (!area_place(comp, &lv, &st, &ar)) return;
    for (size_t i = 0; i < sizeof k_doors / sizeof *k_doors; i++)
        if (k_doors[i].lv == lv && k_doors[i].st == st && k_doors[i].area == ar &&
            fabsf(k_doors[i].x16 / 16.0f - d->x) < 1.0f && fabsf(k_doors[i].y16 / 16.0f - d->y) < 1.0f)
            { d->role = k_doors[i].role; d->label = k_doors[i].text; return; }
}
static int read_doors(uint32_t comp, Door *out, int cap) {
    uint32_t gm = r32(comp + 0xb4), dm = is_ptr(gm) ? r32(gm + 0xc0) : 0;
    if (!is_ptr(dm)) return 0;
    uint32_t n = r32(dm + 4);
    if (n > 20) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t d = r32(dm + 8 + 4 * i);
        if (!is_ptr(d) || r32(d + 0x844) == 1) continue;   /* hidden doors (door::Door::isHide: +0x844 == 1; 2 = appeared) */
        out[k].x = rf32(d + 0x78); out[k].y = rf32(d + 0x7c); out[k].kind = r8(d + 0x74);
        door_label(comp, &out[k]);
        k++;
    }
    return k;
}

/* ---- items (item::Manager at Component +0xcc; MutableArray<Item*,32> at
 * +0x94; Item: kind +0x14, Location +0x48). Kinds from the item model names. */
static const char *const k_item_names[15] = {
    "point star", "1-Up", "Maxim Tomato", "food", "Energy Sphere", "Invincible Candy", "point star",
    "big point star", "copy ability star", "point star", "Warp Star", "ship part", "super ability star",
    "Energy Sphere", "Energy Sphere",
};
static int item_is_minor(uint32_t k) { return k == 0 || k == 6 || k == 7 || k == 9; }
typedef struct { uint32_t kind, ab; float x, y; } ItemPos;
static int read_items(uint32_t comp, ItemPos *out, int cap) {
    uint32_t im = r32(comp + 0xcc);
    if (!is_ptr(im)) return 0;
    uint32_t n = r32(im + 0x94);
    if (n > 32) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t it = r32(im + 0x98 + 4 * i);
        if (!is_ptr(it)) continue;
        uint32_t loc = r32(it + 0x48), kind = r32(it + 0x14);
        if (!is_ptr(loc) || kind >= 15) continue;
        float x = rf32(loc), y = rf32(loc + 4);
        if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
        out[k].kind = kind; out[k].ab = kind == 8 ? r32(it + 0x18) : 0; out[k].x = x; out[k].y = y; k++;
    }
    return k;
}
/* Kirby is right under an item: a bright bell "bling", a little lower the higher the item is. */
void snd_under_item(float dy) {
    float m = powf(2.0f, -(dy > 8 ? 8 : dy) / 24.0f);
    tone_at(0, 2093 * m, 4186 * m, 0.04f, 0.22f, 0, W_SINE);
    tone_at(0.04f, 4186 * m, 4186 * m, 0.18f, 0.16f, 0, W_SINE);
    tone_at(0.04f, 6272 * m, 6272 * m, 0.10f, 0.05f, 0, W_SINE);
}
static void item_sound(uint32_t kind, float delay, float semis, float gain, float pan) {
    float m = powf(2.0f, semis / 12.0f);
    switch (kind) {
    case 2: case 3:        /* food: a warm bloop */
        tone_at(delay, 330 * m, 495 * m, 0.12f, gain, pan, W_SINE); break;
    case 4:                /* Energy Sphere: a sparkling chime */
        for (int i = 0; i < 3; i++) tone_at(delay + i * 0.05f, 1568 * m * (1 + i * 0.25f), 1568 * m * (1 + i * 0.25f), 0.08f, gain * 0.8f, pan, W_SINE);
        break;
    case 1:                /* 1-Up: a rising arpeggio */
        tone_at(delay, 659 * m, 659 * m, 0.05f, gain, pan, W_SQUARE); tone_at(delay + 0.06f, 988 * m, 988 * m, 0.08f, gain, pan, W_SQUARE); break;
    case 5:                /* candy */
        tone_at(delay, 880 * m, 1760 * m, 0.15f, gain, pan, W_TRI); break;
    case 8: case 12:       /* ability star */
        tone_at(delay, 1175 * m, 1175 * m, 0.05f, gain, pan, W_TRI); tone_at(delay + 0.06f, 1760 * m, 1760 * m, 0.05f, gain, pan, W_TRI); break;
    case 10:               /* warp star */
        tone_at(delay, 523 * m, 2093 * m, 0.25f, gain, pan, W_SINE); break;
    default:               /* point stars: a tiny ting */
        tone_at(delay, 2637 * m, 2637 * m, 0.03f, gain * 0.5f, pan, W_SINE);
    }
}

/* ---- level hub doors (gimmick::lvmapdoor, gimmick Manager +0x128):
 * count +8, LvMapDoor pointers from +0xc; door: +0x1918 index in the map
 * file (NOT the stage: Dangerous Dinner lists its doors top down),
 * +0x10 x, +0x14 y, +0x1910 active. The stage is entry word 6 of the
 * lvMapDoor entries in map/other/lvmap/LevelNN.dat (tools/gmkmap.py 9). ---- */
typedef struct { float x, y; int stage, active; } HubDoor;
static const struct { unsigned char lv; float x, y; unsigned char stage; } k_hub_stage[] = {
    {1, 16.5f, 7.0f, 0}, {1, 27.5f, 9.0f, 1}, {1, 42.5f, 5.0f, 2}, {1, 47.5f, 17.0f, 4}, {1, 36.0f, 15.0f, 3},
    {2, 21.5f, 8.0f, 0}, {2, 31.5f, 9.0f, 1}, {2, 44.5f, 12.0f, 2}, {2, 56.0f, 13.0f, 3}, {2, 54.5f, 22.0f, 4},
    {3, 18.0f, 21.0f, 0}, {3, 28.0f, 22.0f, 1}, {3, 36.0f, 21.0f, 2}, {3, 31.0f, 10.0f, 3}, {3, 17.0f, 10.0f, 4},
    {4, 18.0f, 10.0f, 0}, {4, 32.5f, 7.0f, 1}, {4, 44.0f, 5.0f, 2}, {4, 59.5f, 14.0f, 4}, {4, 44.0f, 17.0f, 5}, {4, 59.5f, 7.0f, 3},
    {5, 11.5f, 14.0f, 0}, {5, 22.0f, 17.0f, 1}, {5, 23.0f, 26.0f, 2}, {5, 22.5f, 49.0f, 4}, {5, 22.0f, 59.0f, 5}, {5, 22.5f, 37.0f, 3},
    {6, 10.0f, 6.0f, 3}, {6, 25.5f, 7.0f, 2}, {6, 43.0f, 11.0f, 1}, {6, 35.0f, 21.0f, 0}, {6, 25.5f, 13.0f, 5}, {6, 8.0f, 15.0f, 4},
    {7, 16.5f, 43.0f, 3}, {7, 16.5f, 26.0f, 2}, {7, 16.5f, 20.0f, 1}, {7, 16.5f, 11.0f, 0},
};
static int hub_stage(uint32_t comp, float x, float y, int fallback) {
    int lv = area_kind(comp) - 343;
    for (unsigned i = 0; i < sizeof k_hub_stage / sizeof k_hub_stage[0]; i++)
        if (k_hub_stage[i].lv == lv && fabsf(k_hub_stage[i].x - x) < 0.6f && fabsf(k_hub_stage[i].y - y) < 0.6f) return k_hub_stage[i].stage;
    return fallback;
}
static int read_hub_doors(uint32_t comp, HubDoor *out, int cap) {
    uint32_t gm = r32(comp + 0xb4), lm = is_ptr(gm) ? r32(gm + 0x128) : 0;
    if (!is_ptr(lm)) return 0;
    uint32_t n = r32(lm + 8);
    if (n > 10) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t d = r32(lm + 0xc + 4 * i);
        if (!is_ptr(d)) continue;
        float x = rf32(d + 0x10), y = rf32(d + 0x14);
        if (!(x > -10 && x < 300 && y > -10 && y < 200)) continue;
        out[k].x = x; out[k].y = y; out[k].stage = hub_stage(comp, x, y, (int)r32(d + 0x1918)); out[k].active = r8(d + 0x1910) != 0;
        k++;
    }
    return k;
}

/* ---- bosses (boss::Manager at Component +0xd4; MutableArray<Boss*,4> at
 * +0xe4: count, then pointers). Boss: +0x10 kind (BinBossKind), +0x34 dead,
 * +0x1c0 Location, +0x220 HitPoint {cur, max}. Names from the boss
 * T_INDIVI_TABLE (ModelDesc.o). ---- */
static const char *const k_boss_names[22] = {
    "Sphere Doomer", "Whispy Woods", "Gigant Edge", "Mr. Dooter", "Water Galboros", "King Doo", "Kibble Blade",
    "Bonkers", "Dubior", "Grand Doomer", "Moundo", "Metal General", "Fatty Puffer", "Goriath", "the Lor Starcutter",
    "Magolor", "Landia", "the Lor Starcutter", "Magolor", "HR-D3", "Galacta Knight", "HR-D3's pilot",
};
static int boss_is_mid(uint32_t k) { return (k >= 4 && k <= 8) || k == 0 || k == 2 || k == 10; }   /* boss::Utility::IsBossMBoss */
const char *boss_name(uint32_t k) { return k < 22 ? k_boss_names[k] : "the boss"; }
static const char *const k_boss_desc[22] = {
    BD_SPHERE_DOOMER, BD_WHISPY, BD_GIGANT_EDGE, BD_DOOTER, BD_WATER_GALBOROS, BD_KING_DOO, BD_KIBBLE_BLADE,
    BD_BONKERS, BD_DUBIOR, BD_GRAND_DOOMER, BD_MOUNDO, BD_METAL_GENERAL, BD_FATTY_PUFFER, BD_GORIATH, BD_LOR,
    BD_MAGOLOR, BD_LANDIA, BD_LOR, BD_MAGOLOR_CROWN, BD_HRD3, BD_GALACTA, BD_UNKNOWN,
};
const char *boss_desc(uint32_t k) { return k < 22 ? k_boss_desc[k] : BD_UNKNOWN; }
typedef struct { uint32_t addr, kind; float x, y; int hp, maxhp; } BossPos;
static int read_bosses(uint32_t comp, BossPos *out, int cap) {
    uint32_t bm = r32(comp + 0xd4);
    if (!is_ptr(bm)) return 0;
    uint32_t n = r32(bm + 0xe4);
    if (n > 4) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t b = r32(bm + 0xe8 + 4 * i);
        if (!is_ptr(b) || r8(b + 0x34)) continue;
        uint32_t loc = r32(b + 0x1c0), hp = r32(b + 0x220);
        if (!is_ptr(loc)) continue;
        float x = rf32(loc), y = rf32(loc + 4);
        if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
        out[k].addr = b; out[k].kind = r32(b + 0x10); out[k].x = x; out[k].y = y;
        out[k].hp = is_ptr(hp) ? (int)r32(hp) : -1; out[k].maxhp = is_ptr(hp) ? (int)r32(hp + 4) : -1;
        k++;
    }
    return k;
}
/* The Lor Starcutter fight has a second boss object of the same name with 1
 * health point, off screen (kind 17); the real one (kind 14, 680) sits where
 * Magolor stands on the ship's deck. Drop such helpers when a real one is there. */
static int merge_boss_parts(BossPos *bp, int n) {
    int real = 0, k = 0;
    for (int i = 0; i < n; i++) if (bp[i].maxhp > 1) real = 1;
    if (!real) return n;
    for (int i = 0; i < n; i++) if (bp[i].maxhp > 1) bp[k++] = bp[i];
    return k;
}
/* A heavy double drum beat (a mid-boss: a lighter metal clang). */
void snd_boss(int mid, float dy, float gain, float pan) {
    float m = powf(2.0f, (dy > 12 ? 12 : dy < -12 ? -12 : dy) / 12.0f);
    if (mid) {   /* a light metal clang: a short square strike over a softer triangle ring (the old full square was harsh) */
        tone(330 * m, 300 * m, 0.03f, gain * 0.45f, pan, W_SQUARE);
        tone(330 * m, 300 * m, 0.14f, gain * 0.6f, pan, W_TRI); tone_at(0.02f, 660 * m, 620 * m, 0.2f, gain * 0.3f, pan, W_TRI);
    }
    else { tone(90 * m, 60 * m, 0.14f, gain * 1.3f, pan, W_SINE); tone_at(0.17f, 90 * m, 60 * m, 0.14f, gain * 1.3f, pan, W_SINE); tone_at(0, 180 * m, 120 * m, 0.08f, gain * 0.5f, pan, W_NOISE); }
}

/* ---- Lor Starcutter doors (gimmick::shipdoor, gimmick Manager +0x1b0):
 * +4 count, +8 ShipDoor*; door +0x78 x, +0x7c y, +0x8c room (0xff for a
 * plain passage). Doors that need Energy Spheres have a LockSmith:
 * +0x440 count (0 or 1), LockSmith* at +0x444; LockSmith +4 which unlock
 * (SphereUnlockKind: 7, 8 the sub-game rooms below, 9-12 the Copy Ability
 * rooms), +0x378 state: 0 locked; 2 open and 3 open with Kirby at it on a
 * 100% file ---- */
typedef struct { float x, y; int kind, open; } ShipDoor;
static const char *ship_door_name(const ShipDoor *d) {
    return d->kind >= 9 && d->kind <= 12 ? "Copy Ability room door" : (d->kind == 7 || d->kind == 8) ? "Sub-game room door" : "Door";
}
static int read_ship_doors(uint32_t comp, ShipDoor *out, int cap) {
    uint32_t gm = r32(comp + 0xb4), sm = is_ptr(gm) ? r32(gm + 0x1b0) : 0;
    if (!is_ptr(sm)) return 0;
    uint32_t n = r32(sm + 4);
    if (n > 20) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t d = r32(sm + 8 + 4 * i);
        if (!is_ptr(d)) continue;
        float x = rf32(d + 0x78), y = rf32(d + 0x7c);
        if (!(x > -10 && x < 300 && y > -10 && y < 200)) continue;
        out[k].x = x; out[k].y = y; uint32_t ls = r32(d + 0x440) ? r32(d + 0x444) : 0;
        out[k].kind = is_ptr(ls) ? (int)r32(ls + 4) : 0xff; out[k].open = !is_ptr(ls) || r32(ls + 0x378) != 0; k++;
    }
    return k;
}

/* ---- warp star gimmick (Lor Starcutter: to the World Map; gimmick
 * Manager +0x250 -> manager, +4 -> WarpStar; position +0xc, +0x10) ---- */
static int read_warp_star(uint32_t comp, float *x, float *y) {
    uint32_t gm = r32(comp + 0xb4), wm = is_ptr(gm) ? r32(gm + 0x250) : 0;
    uint32_t ws = is_ptr(wm) ? r32(wm + 4) : 0;
    if (!is_ptr(ws) || !r8(ws + 0xba0)) return 0;          /* +0xba0: visible */
    *x = rf32(ws + 0xc); *y = rf32(ws + 0x10);
    return *x > -10 && *x < 300 && *y > -10 && *y < 200;
}

/* ---- dimensional rift (gimmick::warplowper, gimmick Manager +0x248 -> manager,
 * +4 -> WarpLowper). One per area, in the 16 stages with a Super Ability: a
 * Super Ability smashes the scenery there and the rift opens; jumping into it
 * leads to Another Dimension, and a second rift at the end of that leads back.
 * WarpLowper +0xc x, +0x10 y (centre), +0x14 step shift from the map (> 0 into
 * Another Dimension, < 0 back out), +0x7e2 already entered, +0x7e3 cleared,
 * +0x7e8 state (0 hidden, 1-8 opening, 9 open, 10 open from the start, 11 not
 * shown); Kirby goes in by touching it in state 9 or 10 (isHit). ---- */
typedef struct { uint32_t addr; float x, y; int state, out, entered; } Rift;
static int read_rift(uint32_t comp, Rift *r) {
    uint32_t gm = r32(comp + 0xb4), m = is_ptr(gm) ? r32(gm + 0x248) : 0;
    uint32_t w = is_ptr(m) ? r32(m + 4) : 0;
    if (!is_ptr(w)) return 0;
    r->addr = w; r->x = rf32(w + 0xc); r->y = rf32(w + 0x10);
    r->state = (int)r32(w + 0x7e8); r->out = (int32_t)r32(w + 0x14) < 0; r->entered = r8(w + 0x7e2) != 0;
    return r->x > -10 && r->x < 300 && r->y > -10 && r->y < 200 && r->state >= 0 && r->state <= 11;
}
static int rift_open(const Rift *r) { return (r->state == 9 || r->state == 10) && !r->entered; }

/* ---- cannons. Fuse cannon (gimmick Manager +0x60, +4 count, +8 Cannon*[4]):
 * +0x20 x, +0x24 y (bottom; its mouth is 2 higher), +0xa58 state 0 waiting,
 * 1 Kirby inside, 3 fuse burnt down / about to fire, 4 firing. Light the fuse
 * with fire and get in before it burns down; it shoots Kirby to a secret
 * place. Barrel cannon (gimmick::cannonblow, +0x68, +4 count, +8 CannonBlow*[32]):
 * +0x10 x, +0x14 y, +0x24 launch kind (1 = press a button to fire, else it fires
 * by itself). Kirby goes in by touching it. ---- */
typedef struct { uint32_t addr; float x, y; int state, manual, fuse; float angle; } Cannon;
static int read_cannons(uint32_t comp, Cannon *out, int cap) {
    uint32_t gm = r32(comp + 0xb4);
    if (!is_ptr(gm)) return 0;
    int k = 0;
    for (int fuse = 1; fuse >= 0; fuse--) {
        uint32_t m = r32(gm + (fuse ? 0x60 : 0x68)), n = is_ptr(m) ? r32(m + 4) : 0;
        if (n > 32) continue;
        for (uint32_t i = 0; i < n && k < cap; i++) {
            uint32_t c = r32(m + 8 + 4 * i);
            if (!is_ptr(c)) continue;
            float x = rf32(c + (fuse ? 0x20 : 0x10)), y = rf32(c + (fuse ? 0x24 : 0x14));
            if (!(x > -10 && x < 300 && y > -10 && y < 200)) continue;
            out[k].addr = c; out[k].x = x; out[k].y = fuse ? y + 1.0f : y; out[k].fuse = fuse;
            out[k].state = (int)r32(c + (fuse ? 0xa58 : 0xa5c)); out[k].manual = !fuse && r32(c + 0x24) == 1;
            out[k].angle = fuse ? 0 : rf32(c + 0x1e4); k++;
        }
    }
    return k;
}

/* ---- route: breadth-first distances from the target through open cells ---- */
static int16_t g_dist[GW * GH];
static int g_route_ok;
static int passable(int x, int y) {
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return 0;
    int s = g_shape[x + y * GW];
    if (s == 1) return 0;
    if (g_flag[x + y * GW] & 0x10) return 0;
    return 1;
}
static void route_build(int tx, int ty) {
    static int queue[GW * GH];
    for (int i = 0; i < GW * GH; i++) g_dist[i] = -1;
    g_route_ok = 0;
    if (tx < 0 || ty < 0 || tx >= g_w || ty >= g_h) return;
    int qh = 0, qt = 0;
    g_dist[tx + ty * GW] = 0; queue[qt++] = tx + ty * GW;
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    while (qh < qt) {
        int c = queue[qh++], x = c % GW, y = c / GW;
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (!passable(nx, ny) || g_dist[nx + ny * GW] >= 0) continue;
            g_dist[nx + ny * GW] = (int16_t)(g_dist[c] + 1);
            queue[qt++] = nx + ny * GW;
        }
    }
    g_route_ok = 1;
}
/* Follow the route from (x, y) for n steps, sideways moves first. Returns
 * how far it rose (+) or fell (-), and the sideways step direction. */
static int route_follow(int x, int y, int n, int *side, int *total) {
    *side = 0;
    if (!g_route_ok || x < 0 || y < 0 || x >= g_w || y >= g_h) return 0;
    if (g_dist[x + y * GW] < 0) {
        /* Kirby's own cell may be odd (inside a slope): try the cell above */
        if (y + 1 < g_h && g_dist[x + (y + 1) * GW] >= 0) y++;
        else return 0;
    }
    *total = g_dist[x + y * GW];
    int y0 = y;
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    for (int i = 0; i < n; i++) {
        int d = g_dist[x + y * GW], bk = -1;
        if (d <= 0) break;
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= g_w || ny >= g_h) continue;
            if (g_dist[nx + ny * GW] == d - 1) { bk = k; break; }   /* sideways first */
        }
        if (bk < 0) break;
        if (!*side && dx[bk]) *side = dx[bk];
        x += dx[bk]; y += dy[bk];
    }
    return y - y0;
}

/* ---- state ---- */
static uint32_t s_comp, s_hero;
static float s_x, s_y, s_px, s_py;
static int s_have, s_air, s_air_ms, s_face = 1;
static DWORD s_area_t;
static int s_area_w, s_area_h;
static float s_spawn_x, s_spawn_y;
static Ground s_last_ground;
static int s_hp_quarter = 4, s_ability = -1, s_dead, s_riding;
int g_riding;
static DWORD s_next_items;
/* ---- thrown things (shots, balls, apples, bombs, Magolor's magic balls, falling
 * rocks): followed all the time while they fly, so you hear them coming. Each of
 * the 3 nearest ticks from where it is: side = pan, height = pitch (one semitone
 * a tile), faster and louder as it gets close. Coming toward Kirby: a sharp tick
 * with a little hiss; moving away: a soft, lower tick. Without an ability, one
 * that reaches the front of Kirby's mouth plays the in-reach draw-in: inhale now. ---- */
static int creature_kind(uint32_t kind);
static int thrown_kind(uint32_t kind) {
    if (kind < 96 || creature_kind(kind)) return 0;
    const char *n = enemy_name(kind);
    return strcmp(n, "Magolor") && strcmp(n, "Landia") && strcmp(n, "defeated boss") && strncmp(n, "enemy ", 6);
}
typedef struct { uint32_t addr; float d; DWORD next; int reach, coming; } Thrown;
static Thrown s_thr[3]; static int s_nthr;
/* Kirby's own spat star (and other shots of his) are in the same list: anything first
 * seen within a tile and a half of Kirby is his, and is never tracked. The list holds
 * only objects alive in the last poll, so a reused address starts fresh. */
typedef struct { uint32_t addr; int own; } Seen;
static const char *thing_name(uint32_t kind);
static Seen s_seen[80], s_seen2[80]; static int s_nseen, s_nseen2;
static int thrown_own(const Enemy *q) {
    int own = -1;
    for (int i = 0; i < s_nseen; i++) if (s_seen[i].addr == q->addr) own = s_seen[i].own;
    if (own < 0) {
        /* weapons carry their team (0 = Kirby's side, 1 = enemies); enemy-list objects don't */
        own = q->kind >= 1000 ? q->id == 0 : fabsf(q->x - s_x) < 1.5f && fabsf(q->y - s_y) < 1.5f;
        if (q->kind >= 1000) CUE("cue weapon new %s (%u) %s team %u dx %.1f dy %.1f", thing_name(q->kind), q->kind - 1000, own ? "own" : "foe", q->id, q->x - s_x, q->y - s_y);
    }
    if (s_nseen2 < 80) { s_seen2[s_nseen2].addr = q->addr; s_seen2[s_nseen2].own = own; s_nseen2++; }
    return own;
}
static int thrown_tracked(uint32_t addr) { for (int i = 0; i < s_nthr; i++) if (s_thr[i].addr == addr) return 1; return 0; }
void snd_thrown_ex(int coming, int hazard, float dy, float gain, float pan);
void snd_thrown(int coming, float dy, float gain, float pan) { snd_thrown_ex(coming, 0, dy, gain, pan); }
/* hazard: something dangerous you can't inhale (fire orbs, falling rocks, crystals): a low buzzy tick */
void snd_thrown_ex(int coming, int hazard, float dy, float gain, float pan) {
    float m = powf(2.0f, (dy > 12 ? 12 : dy < -12 ? -12 : dy) / 12.0f);
    if (hazard) {
        if (coming) { tone(220 * m, 200 * m, 0.05f, gain * 0.9f, pan, W_SAW); tone(900, 500, 0.02f, gain * 0.4f, pan, W_NOISE); }
        else tone(180 * m, 170 * m, 0.04f, gain * 0.4f, pan, W_TRI);
        return;
    }
    if (coming) {
        tone(2600, 1200, 0.012f, gain * 0.6f, pan, W_NOISE);
        tone(740 * m, 700 * m, 0.04f, gain, pan, W_SQUARE);
    } else tone(560 * m, 540 * m, 0.035f, gain * 0.4f, pan, W_TRI);
}
/* Weapons (scn::step::weapon::Manager at Component +0xc0; MutableArray<Weapon*,128>: +0x90
 * count, +0x94 pointers). Weapon +0 Component, +0xc kind (Desc +0), +0x14 dead, +0x2c Location* (x, y). Most enemy and boss shots
 * live here, not in the enemy list (+0x14 byte set = dead; kind 3 = AirBall checked live): Water Galboros's water, Shotzo bullets, Hot Head fire,
 * Landia's fireballs, the Lor's oars, Magolor's needles. Kirby's own shots are here too
 * (team 0; thrown_own drops them). Returned as Enemy with kind = 1000 + weapon kind, id = team.
 * Seen live: 15 Shotzo bullet, 45/46/51 in the Water Galboros fight (51 = its water). */
static int read_weapons(uint32_t comp, Enemy *out, int cap) {
    uint32_t wm = r32(comp + 0xc0);
    if (!is_ptr(wm)) return 0;
    uint32_t n = r32(wm + 0x90);
    if (n > 128) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t w = r32(wm + 0x94 + 4 * i);
        if (!is_ptr(w)) continue;
        if (r8(w + 0x14)) continue;   /* dead, waiting to be freed */
        uint32_t loc = r32(w + 0x2c), kind = r32(w + 0xc);   /* ScopedPtr at +0x28 keeps its pointer at +4 */
        if (!is_ptr(loc) || kind > 400) continue;
        float x = rf32(loc), y = rf32(loc + 4);
        if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
        /* team: ObjCollLite (+0x5c) builds an ocoll::Owner at +0xc whose +0x18 is the team number */
        uint32_t ocl = r32(w + 0x5c);
        out[k].addr = w; out[k].kind = 1000 + kind; out[k].id = is_ptr(ocl) ? r32(ocl + 0x24) : 0xff; out[k].x = x; out[k].y = y; k++;
    }
    return k;
}
static const char *thing_name(uint32_t kind) {
    static char b[24];
    if (kind < 1000) return enemy_name(kind);
    snprintf(b, sizeof b, "shot %u", kind - 1000); return b;   /* kind names unknown (the step/weapon string order is not the Kind order) */
}
/* Weapons are things to dodge (low buzzy tick, no draw-in); so are hazard enemies. */
static int thing_hazard(uint32_t kind) { return kind >= 1000 ? 1 : enemy_sig(kind).hazard; }
static void thrown_poll(uint32_t comp, DWORD now) {
    Enemy e[80]; int n = read_enemies(comp, e, 48);
    n += read_weapons(comp, e + n, 80 - n);
    int pick[3], np = 0; float pd[3];
    s_nseen2 = 0;
    for (int i = 0; i < n; i++) {
        if ((e[i].kind < 1000 && !thrown_kind(e[i].kind)) || thrown_own(&e[i])) continue;
        float dx = e[i].x - s_x, dy = e[i].y - s_y;
        if (fabsf(dx) > 12 || fabsf(dy) > 8) continue;
        float d = sqrtf(dx * dx + dy * dy);
        int at = np < 3 ? np++ : (d < pd[2] ? 2 : -1);
        if (at < 0) continue;
        pick[at] = i; pd[at] = d;
        for (int j = at; j > 0 && pd[j] < pd[j - 1]; j--) { float t = pd[j]; pd[j] = pd[j - 1]; pd[j - 1] = t; int q = pick[j]; pick[j] = pick[j - 1]; pick[j - 1] = q; }
    }
    Thrown nt[3];
    for (int k = 0; k < np; k++) {
        Enemy *q = &e[pick[k]];
        float dx = q->x - s_x, dy = q->y - s_y, d = pd[k];
        Thrown t = { q->addr, d, now + 40 * k, 0, 1 };
        for (int j = 0; j < s_nthr; j++) if (s_thr[j].addr == q->addr) t = s_thr[j];
        /* closer or farther than at the last tick; tiny wobbles keep the last answer */
        if (d < t.d - 0.08f) { t.coming = 1; t.d = d; } else if (d > t.d + 0.08f) { t.coming = 0; t.d = d; }
        int coming = t.coming;
        if ((int)(now - t.next) >= 0) {
            float gain = 0.12f + 0.20f * (1 - fminf(d, 12) / 12);
            if (d > 0.8f) snd_thrown_ex(coming, thing_hazard(q->kind), dy, gain, fmaxf(-1, fminf(1, dx / 7)) * 0.8f);   /* on top of Kirby: the draw-in says it */
            t.next = now + 130 + (DWORD)(d * 35);
            if (now % 1000 < 20 || d < 2) CUE("cue thrown %s (%u) dx %.1f dy %.1f %s", thing_name(q->kind), q->kind, dx, dy, coming ? "coming" : "away");
        }
        float ahead = dx * s_face;
        int in = s_ability <= 0 && coming && !thing_hazard(q->kind) && ahead > 0.2f && ahead < 3.5f && fabsf(dy) < 1.3f;
        static DWORD s_reach_t;   /* a volley of balls arriving together: one draw-in, not one each */
        if (in && !t.reach && now - s_reach_t > 600) { s_reach_t = now; snd_in_reach(dx / 6.0f); CUE("cue in reach %s ahead %.1f dy %.1f (thrown)", thing_name(q->kind), ahead, dy); }
        t.reach = in; t.addr = q->addr;
        nt[k] = t;
    }
    s_nthr = np; for (int k = 0; k < np; k++) s_thr[k] = nt[k];
    memcpy(s_seen, s_seen2, sizeof s_seen); s_nseen = s_nseen2;
}

static DWORD s_next_radar, s_next_route, s_next_door, s_next_plat, s_next_spike, s_next_edge, s_next_knock, s_next_pit;
static int s_edge_col = -9999;
static int s_route_dir;            /* last spoken/played vertical guidance */
static Door s_target; static int s_have_target;
static int s_at_door, s_at_hub_door = -1;
static DWORD s_next_hub, s_next_warp, s_next_ship, s_next_boss;
static uint32_t s_boss_seen[8]; static int s_boss_quarter[8], s_nboss_seen;
static uint32_t s_boss_told;   /* kinds named and described in this area (Landia splits into four) */
static uint32_t s_last_boss_kind = 0xffffffff;
static int s_at_ship_door = -1;
static int s_under_warp;
static uint32_t s_rift_addr, s_told_cannon, s_cannon_in;
static int s_rift_open = -1, s_rift_told, s_at_rift;
static DWORD s_next_rift, s_next_cannon, s_next_side_door, s_next_aim;
static int s_in_cannon;   /* Kirby sits in a barrel cannon (set by the cannon block, used by the ground cues) */
static DWORD s_next_ladder; static int s_ladder_told_x = -1, s_ladder_told_y = -1, s_rung_row = -999;
int input_dir(void);               /* input.c: -1 left, 1 right, 0 none */

static void cam_reset(void);
static void camera_poll(DWORD now);
static void new_area(void) {
    cam_reset();
    s_area_t = GetTickCount(); s_area_w = g_w; s_area_h = g_h;
    /* bosses are named once per map: Magolor warps Kirby around his arena, which looks
     * like a new area here (a jump of more than 3 tiles) */
    static int s_boss_kind_area = -1;
    int ak = s_comp ? area_kind(s_comp) : -1;
    if (ak != s_boss_kind_area) { s_boss_kind_area = ak; s_nboss_seen = 0; s_boss_told = 0; }
    s_spawn_x = s_x; s_spawn_y = s_y;
    s_have_target = 0; s_route_dir = 0; s_ladder_told_x = -1; s_rung_row = -999; s_edge_col = -9999; s_at_door = 0; objects_reset(); gimmicks_reset();
    s_rift_addr = 0; s_rift_open = -1; s_rift_told = 0; s_at_rift = 0; s_told_cannon = 0; s_cannon_in = 0;
    hum_all_off();
    log_line("area %dx%d (map kind %d), Kirby at %.2f %.2f", g_w, g_h, area_kind(s_comp), s_x, s_y);
    areas_enter(s_comp, g_w, g_h);
}

/* Choose the exit: a door that is not the one Kirby came in by, farthest
 * from where he came in. */
static void pick_target(void) {
    Door d[20]; int n = read_doors(s_comp, d, 20);
    int best = -1; float bd = -1;
    /* a labelled stage door: the goal door or the one onward, else the way back out of a side room */
    for (int want = 0; want < 2 && best < 0; want++)
        for (int i = 0; i < n; i++)
            if (want == 0 ? (d[i].role == 0 || d[i].role == 1) : d[i].role == 3) { best = i; break; }
    if (best >= 0) { s_have_target = 1; s_target = d[best]; return; }
    for (int i = 0; i < n; i++) {
        float ds = fabsf(d[i].x - s_spawn_x) + fabsf(d[i].y - s_spawn_y);
        if (ds < 3.0f && n > 1) continue;
        if (ds > bd) { bd = ds; best = i; }
    }
    s_have_target = best >= 0;
    if (s_have_target) s_target = d[best];
}

void speak_dir(char *buf, size_t cap, float dx, float dy) {
    int ax = (int)lroundf(fabsf(dx)), ay = (int)lroundf(fabsf(dy));
    char hx[48] = "", hy[48] = "";
    if (ax) snprintf(hx, sizeof hx, "%d %s", ax, dx > 0 ? "right" : "left");
    if (ay) snprintf(hy, sizeof hy, "%d %s", ay, dy > 0 ? "up" : "down");
    if (ax && ay) snprintf(buf, cap, "%s, %s", hx, hy);
    else if (ax) snprintf(buf, cap, "%s", hx);
    else if (ay) snprintf(buf, cap, "%s", hy);
    else snprintf(buf, cap, "right here");
}

/* Ladder column containing (x, y): its bottom and top rows. */
static void ladder_span(int x, int y, int *bot, int *top) {
    int b = y, t = y;
    while (b > 0 && is_ladder(x, b - 1)) b--;
    while (t + 1 < g_h && is_ladder(x, t + 1)) t++;
    *bot = b; *top = t;
}
static void ladder_poll(DWORD now, int climbing) {
    int cx = (int)floorf(s_x), fy = (int)floorf(s_y + 0.3f);
    /* Kirby at a ladder: in its column, from its foot up to standing on its top */
    int at = -1, bot = 0, top = 0;
    for (int r = fy - 1; r <= fy + 1; r++)
        if (is_ladder(cx, r)) { at = cx; ladder_span(cx, r, &bot, &top); break; }
    if (at >= 0) {
        int on_top = fy > top, below_top = fy <= top;
        if (at != s_ladder_told_x || top != s_ladder_told_y) {
            s_ladder_told_x = at; s_ladder_told_y = top;
            int down = on_top ? top - bot + 1 : fy - bot, up = below_top ? top + 1 - fy : 0;
            CUE("cue at ladder %d rows %d-%d (up %d down %d)", at, bot, top, up, down);
            snd_ladder(up > 0 ? 1 : -1, 0.28f, 0);
            if (on_top) speakf(0, "Ladder down, %d tiles. Hold down to climb down", down);
            else if (down > 1) speakf(0, "Ladder, %d up, %d down. Hold up or down to climb", up, down);
            else speakf(0, "Ladder up, %d tiles. Hold up to climb", up);
        }
        /* climbing: a soft rung tick for every tile, higher as Kirby goes up */
        if (climbing && fy != s_rung_row && fy >= bot && fy <= top + 1) { if (s_rung_row != -999) snd_rung_at(0, fy - bot); }
        s_rung_row = fy;
        s_next_ladder = now + 1500;
        return;
    }
    s_rung_row = -999;
    if (abs(cx - s_ladder_told_x) > 1) s_ladder_told_x = -1;
    if ((int)(now - s_next_ladder) < 0) return;
    /* nearest ladder within 6 tiles sideways, 3 rows up or down */
    int best = 99, bx = 0, by = 0;
    for (int dx = -6; dx <= 6; dx++)
        for (int dy = -3; dy <= 3; dy++)
            if (is_ladder(cx + dx, fy + dy)) {
                int d = abs(dx) * 2 + abs(dy);
                if (d < best) { best = d; bx = cx + dx; by = fy + dy; }
            }
    if (best == 99) return;
    int b, t; ladder_span(bx, by, &b, &t);
    float dxf = bx + 0.5f - s_x;
    int dir = t >= fy + 1 ? (b < fy - 1 ? 0 : 1) : -1;
    float pan = dxf > 0.3f ? 0.8f : dxf < -0.3f ? -0.8f : 0;
    pan *= fminf(1, fabsf(dxf) / 3);
    float gain = 0.10f + 0.16f * (1 - fminf(fabsf(dxf), 6) / 6);
    CUE("cue ladder near dx %.1f rows %d-%d dir %d", dxf, b, t, dir);
    snd_ladder(dir, gain, pan);
    s_next_ladder = now + 700 + (DWORD)(fabsf(dxf) * 220);
}

/* dev: walk Kirby along waypoints from KirbyAccess\walk.txt ("x y" per line), a
 * small step each frame, so the game's swept collision never sees a jump (stage sweep) */
static float s_wp[1024][2]; static int s_nwp, s_iwp, s_wp_stuck, s_god;
static void walk_poll(uint32_t hero) {
    if (s_iwp >= s_nwp) return;
    uint32_t loc = r32(hero + 0xa0);
    if (!is_ptr(loc)) return;
    float x = rf32(loc), y = rf32(loc + 4), tx = s_wp[s_iwp][0], ty = s_wp[s_iwp][1];
    float dx = tx - x, dy = ty - y, d = sqrtf(dx * dx + dy * dy);
    if (d < 0.05f) { if (++s_iwp >= s_nwp) log_line("walk done at %.2f %.2f", x, y); s_wp_stuck = 0; return; }
    /* stuck: no closer to this waypoint for 1.5 s (the game keeps pushing Kirby back) */
    static float best; static int best_wp = -1;
    if (best_wp != s_iwp || d < best - 0.05f) { best = d; best_wp = s_iwp; s_wp_stuck = 0; }
    else if (++s_wp_stuck > 90) { log_line("walk stuck at %.2f %.2f (wp %d of %d, to %.2f %.2f)", x, y, s_iwp, s_nwp, tx, ty); s_iwp = s_nwp; best_wp = -1; return; }
    float st = d < 0.3f ? d : 0.3f;
    union { float f; uint32_t u; } a = { x + dx / d * st }, b = { y + dy / d * st };
    guest_w32(loc, a.u); guest_w32(loc + 4, b.u);
}
static void walk_load(void) {
    wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%lswalk.txt", g_mod_dir);
    FILE *f = _wfopen(p, L"r"); s_nwp = s_iwp = s_wp_stuck = 0;
    if (!f) return;
    while (s_nwp < 1024 && fscanf(f, "%f %f", &s_wp[s_nwp][0], &s_wp[s_nwp][1]) == 2) s_nwp++;
    fclose(f);
    log_line("walk %d points", s_nwp);
}

/* ---- the Landia chase (DD stage 5, Kirby riding Landia, the screen scrolls right).
 * Gap guide: the first wall column within 10 tiles ahead that blocks Kirby's rows;
 * a reference beep (Kirby's height) then a beep pitched 2 semitones per tile at the
 * nearest gap at least 2 tiles tall; same pitch = lined up. Faster when closer.
 * A wall with no gap: a low buzz (shoot the switch to open it). Aim: a soft tick
 * when an enemy, the boss or a switch is straight ahead within 14 tiles. ---- */
static float s_gap_pan = 0.25f;   /* left of centre = ease back (too steep to make at the scroll speed) */
static void snd_gap(float semis, float dist, int lined, int wave) {
    float pan = s_gap_pan, m = powf(2.0f, (semis > 18 ? 18 : semis < -18 ? -18 : semis) / 12.0f), g = 0.16f + 0.14f * (1 - (dist > 16 ? 16 : dist) / 16);
    if (wave == W_SQUARE) g *= 0.6f;
    tone(660, 660, 0.05f, g * 0.7f, pan, W_TRI);
    if (lined) { tone_at(0.09f, 990, 990, 0.05f, g, pan, wave); tone_at(0.15f, 1320, 1320, 0.06f, g * 0.8f, pan, wave); }
    else tone_at(0.11f, 660 * m, 660 * m, 0.07f, g, pan, wave);
}
static void snd_gap_closed(float dist) {
    float g = 0.18f + 0.16f * (1 - dist / 10);
    tone(110, 104, 0.18f, g, 0.3f, W_SAW); tone_at(0, 220, 208, 0.18f, g * 0.3f, 0.3f, W_SQUARE);
}
static void snd_aim(float pan) { tone(1760, 1760, 0.018f, 0.13f, pan, W_SINE); tone_at(0, 3520, 3520, 0.01f, 0.05f, pan, W_SINE); }
/* Crusher jaws (the last two sections of the chase): move groups that keep opening
 * (|offset| growing) and closing on their own, a top and a bottom half with the
 * same x range (13 tiles long, open about 4 tiles for under a second, about every
 * 2.3 s; the two jaws in a row take turns). Shut, a jaw still leaves small hollow
 * pockets where Kirby is safe: the way through is pocket to pocket, waiting while
 * it is shut. A swoosh when the jaw that matters starts to open, a thud when it
 * starts to close. */
typedef struct { int x0, x1, y0, y1, jaw; } Pocket;      /* cells x0..x1, y0..y1; jaw = its base x */
static Pocket s_pk[16]; static int s_npk, s_jaw_x0 = 99999, s_jaw_x1 = -1, s_rel_jaw = -1;
static struct { int x0, x1; DWORD open_t; } s_jaw[8]; static int s_njaw;   /* jaws with pockets; open_t = last swoosh */
static int s_tried[16], s_ntried;                                         /* base columns already looked at */
static uint32_t s_jaw_lm;
/* Pockets of the jaw whose groups start at column bx: empty cells inside the
 * shut shape of all its groups, joined into pieces. */
static void jaw_pockets(uint32_t lm, int bx) {
    static uint8_t solid[64 * 32];
    int x0 = 99999, x1 = -1, y0 = 99999, y1 = -1;
    memset(solid, 0, sizeof solid);
    for (int i = 0; i < 64; i++) {
        uint32_t gp = r32(lm + 0x20 + 4 * i);
        if (!is_ptr(gp)) break;
        if (r8(gp + 0x10) != 1 || (int)(r32(gp + 4) >> 16) != bx) continue;
        uint32_t n = r32(gp + 0x44), cells = r32(gp + 0x54);
        if (n == 0 || n > 2000 || !is_ptr(cells)) continue;
        const uint8_t *c = guest_ptr(cells, n * 8);
        if (!c) continue;
        int by = (int)(r32(gp + 4) & 0xffff);
        for (uint32_t j = 0; j < n; j++, c += 8) {
            if (c[0] != 1 || c[3] == 0 || c[1] >= 64 || by + c[2] >= 32) continue;
            solid[c[1] + (by + c[2]) * 64] = 1;
            if (bx + c[1] < x0) x0 = bx + c[1];
            if (bx + c[1] > x1) x1 = bx + c[1];
            if (by + c[2] < y0) y0 = by + c[2];
            if (by + c[2] > y1) y1 = by + c[2];
        }
    }
    if (x1 < 0) return;
    int found = 0;
    static uint8_t seen[64 * 32]; memset(seen, 0, sizeof seen);
    for (int y = y0 + 1; y < y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i0 = (x - bx) + y * 64;
            if (solid[i0] || seen[i0] || s_npk >= 16) continue;
            /* flood one piece inside the box (a piece touching the top or bottom is open sky, not a pocket) */
            int st[256], sp = 0, px0 = x, px1 = x, py0 = y, py1 = y, edge = 0, cnt = 0;
            st[sp++] = i0; seen[i0] = 1;
            while (sp) {
                int i = st[--sp], cx = i % 64 + bx, cy = i / 64; cnt++;
                px0 = cx < px0 ? cx : px0; px1 = cx > px1 ? cx : px1; py0 = cy < py0 ? cy : py0; py1 = cy > py1 ? cy : py1;
                static const int ddx[4] = { 1, -1, 0, 0 }, ddy[4] = { 0, 0, 1, -1 };
                for (int d = 0; d < 4; d++) {
                    int nx = cx + ddx[d], ny = cy + ddy[d];
                    if (nx < x0 || nx > x1) continue;
                    if (ny <= y0 || ny >= y1) { edge = 1; continue; }
                    int ni = (nx - bx) + ny * 64;
                    if (solid[ni] || seen[ni] || sp >= 255) continue;
                    seen[ni] = 1; st[sp++] = ni;
                }
            }
            /* switch gates bounce too; their notches open on the gate's near side, a jaw's pockets never do */
            if (edge || cnt > 40 || px0 == x0) continue;
            found++;
            Pocket p = { px0, px1, py0, py1, bx };
            int k = s_npk++;
            while (k > 0 && s_pk[k - 1].x0 > p.x0) { s_pk[k] = s_pk[k - 1]; k--; }
            s_pk[k] = p;
            log_line("jaw %d pocket x %d-%d y %d-%d", bx, px0, px1, py0, py1);
        }
    if (found && s_njaw < 8) {
        s_jaw[s_njaw].x0 = x0; s_jaw[s_njaw].x1 = x1; s_jaw[s_njaw].open_t = 0; s_njaw++;
        if (x0 < s_jaw_x0) s_jaw_x0 = x0;
        if (x1 > s_jaw_x1) s_jaw_x1 = x1;
    }
}
static int jaw_index(int bx) { for (int j = 0; j < s_njaw; j++) if (bx >= s_jaw[j].x0 && bx <= s_jaw[j].x1) return j; return -1; }
static void jaws_poll(uint32_t comp, DWORD now) {
    static float s_off[64]; static int s_dir[64], s_rev[64]; static DWORD s_told, s_last_ev, s_area; static uint32_t s_lm;
    uint32_t lm = r32(comp + 0xb0);
    if (!is_ptr(lm)) return;
    if (lm != s_lm || s_area != s_area_t) {   /* the next area can reuse the same manager */
        s_lm = lm; s_area = s_area_t; memset(s_off, 0, sizeof s_off); memset(s_dir, 0, sizeof s_dir); memset(s_rev, 0, sizeof s_rev);
        s_npk = 0; s_njaw = 0; s_ntried = 0; s_jaw_x0 = 99999; s_jaw_x1 = -1; s_jaw_lm = lm; s_told = 0;
    }
    for (int i = 0; i < 64; i++) {
        uint32_t gp = r32(lm + 0x20 + 4 * i);
        if (!is_ptr(gp)) break;
        if (r8(gp + 0x10) != 1) continue;
        float m = fabsf(rf32(gp + 0x20)), d = m - s_off[i];
        int dir = d > 0.005f ? 1 : d < -0.005f ? -1 : 0;
        s_off[i] = m;
        if (!dir) continue;
        int was = s_dir[i]; s_dir[i] = dir;
        if (!was || was == dir) continue;
        if (s_rev[i] < 9) s_rev[i]++;
        if (s_rev[i] < 4) continue;   /* switch gates open once (and may bounce); jaws keep turning */
        int bx = (int)(r32(gp + 4) >> 16);
        if (s_rev[i] == 4) {          /* first time seen turning for sure: find its pockets (once per jaw) */
            int known = 0;
            for (int t = 0; t < s_ntried; t++) if (s_tried[t] == bx) known = 1;
            if (!known && s_ntried < 16) { s_tried[s_ntried++] = bx; jaw_pockets(lm, bx); }
        }
        int ji = jaw_index(bx);
        if (ji < 0) continue;                 /* a gate, not a jaw */
        if (dir > 0 && now - s_jaw[ji].open_t > 400) s_jaw[ji].open_t = now;
        if (now - s_last_ev < 150) continue;
        float gx = (float)bx, gw = (float)(r32(gp + 8) >> 16);
        float dx = gx - s_x; if (s_x > gx) dx = s_x < gx + gw ? 0 : gx + gw - s_x;
        /* only the jaw Kirby has to get through next */
        if (s_rel_jaw >= 0 ? jaw_index(s_rel_jaw) != ji : (dx < -2 || dx > 18)) continue;
        s_last_ev = now;
        float g = 0.22f + 0.2f * (1 - (dx < 0 ? 0 : dx) / 18), pan = dx / 18;
        if (dir > 0) { tone(500, 1800, 0.22f, g * 0.6f, pan, W_NOISE); tone(330, 660, 0.2f, g * 0.6f, pan, W_TRI); }
        else { tone(90, 55, 0.16f, g * 1.2f, pan, W_SINE); tone(1200, 300, 0.03f, g * 0.5f, pan, W_NOISE); }
        CUE("cue jaws %d %s at %.1f ahead", bx, dir > 0 ? "opening" : "closing", dx);
        if (!s_told) { s_told = now; speak(L"Crusher jaws ahead. When shut they still leave small safe pockets. A soft bell leads to the next pocket. Wait in a pocket while they are shut, holding back a little, since the screen keeps moving: the bell moves to the left when you drift. At the swoosh, fly right into the next one. A thud means they are closing", 0); }
    }
}
/* Pocket guide: while Kirby is at the jaws, lead to the next safe pocket (or out
 * the far end) instead of to gaps. Returns 1 when it handled the guide. */
static int jaws_guide(DWORD now, float ky) {
    static DWORD s_next, s_in_t; static int s_in_last = -1;
    /* the stretch of jaws Kirby is at: from 8 tiles before one to just past it (jaws in a row join up) */
    int a0 = 99999, a1 = -1;
    for (int j = 0; j < s_njaw; j++)
        if (s_x > s_jaw[j].x0 - 8 && s_x < s_jaw[j].x1 + 1.5f) { if (s_jaw[j].x0 < a0) a0 = s_jaw[j].x0; if (s_jaw[j].x1 > a1) a1 = s_jaw[j].x1; }
    for (int grow = 1; grow && a1 >= 0; ) {
        grow = 0;
        for (int j = 0; j < s_njaw; j++) if (s_jaw[j].x1 > a1 && s_jaw[j].x0 <= a1 + 3) { a1 = s_jaw[j].x1; grow = 1; }
    }
    if (!s_npk || a1 < 0) { s_rel_jaw = -1; s_in_last = -1; return 0; }
    int in = -1, next = -1;
    for (int i = 0; i < s_npk; i++)
        if (s_x > s_pk[i].x0 - 0.2f && s_x < s_pk[i].x1 + 0.7f && ky > s_pk[i].y0 - 0.2f && ky < s_pk[i].y1 + 1.2f) in = i;
    for (int i = 0; i < s_npk && next < 0; i++)
        if (s_pk[i].x0 > s_x + 0.5f && s_pk[i].x0 <= a1 && (in < 0 || s_pk[i].x0 > s_pk[in].x1 + 1)) next = i;
    if (in != s_in_last) {
        if (in >= 0) { s_in_t = now; s_next = now; tone(1046, 1568, 0.08f, 0.16f, 0, W_TRI); tone_at(0.09f, 1568, 1568, 0.12f, 0.1f, 0, W_SINE); CUE("cue jaws in pocket %d", in); }
        else CUE("cue jaws left pocket");
        s_in_last = in;
    }
    /* the jaw that matters: the one holding the next pocket, else the one Kirby is in */
    s_rel_jaw = next >= 0 ? s_pk[next].jaw : in >= 0 ? s_pk[in].jaw : -1;
    if (s_rel_jaw < 0) for (int i = 0; i < s_npk; i++) if (s_pk[i].jaw <= s_x + 0.5f) s_rel_jaw = s_pk[i].jaw;
    if ((int)(now - s_next) < 0) return 1;
    float tx, ty;
    /* in a pocket: stay until the jaw ahead swooshes open again */
    int ji = jaw_index(s_rel_jaw), hold = in >= 0 && ji >= 0 && s_jaw[ji].open_t < s_in_t + 100;
    /* the screen keeps scrolling: holding still drifts Kirby into the pocket's far wall, so
     * the hold point is the pocket's near end and the bell pans back toward it */
    if (hold) { tx = s_pk[in].x0 + 1.0f; ty = (s_pk[in].y0 + s_pk[in].y1 + 1) * 0.5f; }
    else if (next >= 0) { tx = s_pk[next].x0 + 0.5f; ty = (s_pk[next].y0 + s_pk[next].y1 + 1) * 0.5f; }
    else { tx = a1 + 2.0f; ty = ky; }   /* last pocket: straight out the far end */
    float dist = tx - s_x, best = ty - ky; int lined = fabsf(best) < 0.4f;
    float pan = hold ? (dist > 0.8f ? 0.8f : dist < -0.8f ? -0.8f : dist) : 0.25f;   /* hold: left = ease back */
    /* a soft bell, two tones like the gap guide: the second is the pocket's height */
    float m = powf(2.0f, (best * 2 > 18 ? 18 : best * 2 < -18 ? -18 : best * 2) / 12.0f), g = 0.12f + 0.12f * (1 - (dist > 16 ? 16 : dist) / 16);
    tone(1175, 1175, 0.05f, g * 0.6f, pan, W_SINE);
    if (lined) tone_at(0.1f, 1760, 1760, 0.09f, g, pan, W_SINE);
    else tone_at(0.11f, 1175 * m, 1175 * m, 0.08f, g, pan, W_SINE);
    CUE("cue jaws next %s %.1f ahead at %.1f (%+.1f)%s", hold ? "hold" : next >= 0 ? "pocket" : "exit", dist, ty, best, lined ? " lined up" : "");
    s_next = hold ? now + 300 : now + (DWORD)(350 + (dist > 0 ? dist : 0) * 40) + (lined ? 350 : 0);
    return 1;
}

void snd_chase_demo(int k) {
    switch (k) {
    case 0: s_gap_pan = 0.6f; snd_gap(4, 4, 0, W_TRI); s_gap_pan = 0.25f; break;
    case 1: s_gap_pan = -0.6f; snd_gap(-6, 4, 0, W_TRI); s_gap_pan = 0.25f; break;
    case 2: snd_gap(3, 6, 0, W_SQUARE); break;
    case 3: snd_gap_closed(3); break;
    case 4: snd_aim(0.4f); break;
    case 5: tone(1175, 1175, 0.05f, 0.14f, 0.25f, W_SINE); tone_at(0.11f, 1480, 1480, 0.08f, 0.22f, 0.25f, W_SINE); break;
    case 6: tone(1046, 1568, 0.08f, 0.16f, 0, W_TRI); tone_at(0.09f, 1568, 1568, 0.12f, 0.1f, 0, W_SINE); break;
    case 7: tone(500, 1800, 0.22f, 0.25f, 0.3f, W_NOISE); tone(330, 660, 0.2f, 0.25f, 0.3f, W_TRI); break;
    default: tone(90, 55, 0.16f, 0.5f, 0.3f, W_SINE); tone(1200, 300, 0.03f, 0.2f, 0.3f, W_NOISE); break;
    }
}
/* flying: a slope cell is as good as a wall */
static inline int sh_solid(int x, int y) { return is_wall(x, y) || is_slope(shape_at(x, y)); }
static void shooter_poll(uint32_t comp, DWORD now) {
    jaws_poll(comp, now);
    static DWORD s_next_gap, s_next_aim, s_closed_told;
    float ky = s_y + 0.5f;
    int cx = (int)floorf(s_x), r0 = (int)floorf(s_y + 0.1f), r1 = (int)floorf(s_y + 0.9f);
    /* a switch not hit yet ahead: lead to its height first (shoot it from afar,
     * the gate opens late) */
    float sw[16]; int ns = objects_open_switches(comp, sw, 8), si = -1;
    for (int i = 0; i < ns; i++) { float dx = sw[2 * i] - s_x; if (dx > 0.3f && dx < 16 && (si < 0 || dx < sw[2 * si] - s_x)) si = i; }
    /* only once there's a clear line to it at its height (Landia's height there and Kirby's
     * column too): until then the way-through guide keeps leading, e.g. out of a narrow passage */
    if (si >= 0) {
        int ra = (int)floorf(sw[2 * si + 1] + 0.02f), rb = (int)floorf(sw[2 * si + 1] + 0.95f), x1 = (int)floorf(sw[2 * si]);
        for (int x = cx; x < x1 && si >= 0; x++) if (sh_solid(x, ra) || sh_solid(x, rb)) si = -1;
    }
    if (si >= 0) {
        static float s_told_sx = -99;
        if (fabsf(sw[2 * si] - s_told_sx) > 1) { s_told_sx = sw[2 * si]; speak(L"Switch ahead. Line up with it and shoot", 0); }
        if ((int)(now - s_next_gap) >= 0) {
            float dx = sw[2 * si] - s_x, best = sw[2 * si + 1] - s_y; int lined = fabsf(best) < 0.4f;
            snd_gap(best * 2.0f, dx, lined, W_SQUARE);
            CUE("cue gap switch %.1f ahead at %.1f (%+.1f)%s", dx, sw[2 * si + 1], best, lined ? " lined up" : "");
            s_next_gap = now + (DWORD)(300 + dx * 40) + (lined ? 300 : 0);
        }
    } else if (jaws_guide(now, ky)) {
    } else if ((int)(now - s_next_gap) >= 0) {
        int wall_c = -1;
        /* only rows open where Kirby is count (the corridor's own floor and ceiling don't) */
        int b0 = sh_solid(cx, r0) ? -1 : r0, b1 = sh_solid(cx, r1) ? -1 : r1;
        for (int i = 1; i <= 10 && wall_c < 0; i++) if ((b0 >= 0 && sh_solid(cx + i, b0)) || (b1 >= 0 && sh_solid(cx + i, b1))) wall_c = cx + i;
        if (wall_c < 0) s_next_gap = now + 250;
        else {
            float dist = (float)(wall_c - cx), best = 99, gy = 0, pan = 0.25f;
            /* the way through: cheapest path from Kirby over the live grid (gates as they are
             * now; 6 columns back to 25 on, a step up or down costing 3 so it keeps level unless
             * it must climb or dive) to the furthest point reachable. Dead ends and U-turns in the
             * twisting tunnels come out right, and a shut gate further on doesn't hide the way up
             * to it. The bell follows the path 3 steps on: pitch = its height, pan = which way
             * sideways (right: go on; centre or left: hold back, the screen carries Landia on
             * about 3.3 tiles a second, and holding back moves it back about 5.7). Checked offline
             * with tools/simguide.py on the chase's map. */
            enum { PW = 32 };
            static int16_t pd[PW * GH], pp[PW * GH]; static int pq[PW * GH], path[PW * GH]; static uint8_t inq[PW * GH];
            int x0 = cx - 6 > 0 ? cx - 6 : 0, xw = g_w - x0 < PW ? g_w - x0 : PW, qh = 0, qt = 0, reach = -1, end = -1;
            for (int i = 0; i < PW * GH; i++) pd[i] = -1;
            memset(inq, 0, sizeof inq);
            int sy = (int)floorf(ky);
            if (sy < 0 || sy >= g_h || sh_solid(cx, sy)) sy = !sh_solid(cx, r0) ? r0 : !sh_solid(cx, r1) ? r1 : -1;
            if (sy >= 0 && sy < g_h && cx - x0 < xw) {
                int st = (cx - x0) + sy * PW;
                pd[st] = 0; pp[st] = -1; pq[qt++] = st; inq[st] = 1;
                static const int ndx[4] = { 1, 0, 0, -1 }, ndy[4] = { 0, 1, -1, 0 };
                while (qh != qt) {
                    int i = pq[qh]; qh = (qh + 1) % (PW * GH); inq[i] = 0;
                    int qx = i % PW, qy = i / PW;
                    for (int d = 0; d < 4; d++) {
                        int nx = qx + ndx[d], ny = qy + ndy[d], ni = nx + ny * PW, c = pd[i] + (ndy[d] ? 3 : 1);
                        if (nx < 0 || nx >= xw || ny < 0 || ny >= g_h || sh_solid(x0 + nx, ny) || is_spike(x0 + nx, ny)) continue;
                        if (pd[ni] >= 0 && pd[ni] <= c) continue;
                        pd[ni] = (int16_t)c; pp[ni] = (int16_t)i;
                        if (!inq[ni]) { inq[ni] = 1; pq[qt] = ni; qt = (qt + 1) % (PW * GH); }
                    }
                }
                for (int i = 0; i < PW * GH; i++)
                    if (pd[i] >= 0 && (i % PW > reach || (i % PW == reach && pd[i] < pd[end]))) { reach = i % PW; end = i; }
            }
            /* reachable only up to the wall in front: shut */
            if (end >= 0 && x0 + reach > wall_c) {
                int np = 0;
                for (int i = end; i >= 0 && np < PW * GH; i = pp[i]) path[np++] = i;
                for (int a = 0, b = np - 1; a < b; a++, b--) { int t = path[a]; path[a] = path[b]; path[b] = t; }   /* Kirby first */
                int wp = path[np - 1 < 3 ? np - 1 : 3];
                gy = wp / PW + 0.5f; best = gy - ky;
                pan = (x0 + wp % PW + 0.5f - s_x) / 2.0f;
                /* where the path steps on to the right after a climb or dive, that height must be
                 * reached before Landia's front gets into the next column at the scroll speed:
                 * if it can't, hold back (stop at a U-turn, where the pan already points back) */
                for (int k = 0; k + 1 < np; k++) {
                    int qx = x0 + path[k] % PW, nx = x0 + path[k + 1] % PW, qy = path[k] / PW;
                    if (nx < qx || qx - cx > 8) break;
                    if (nx > qx && nx > cx) {
                        float avail = (nx - 0.45f - s_x) / 3.3f;
                        if (fabsf(qy + 0.5f - ky) / 7.0f > (avail > 0 ? avail : 0) + 0.05f && pan > -0.6f) pan = -0.6f;
                    }
                }
                pan = pan > 0.8f ? 0.8f : pan < -0.8f ? -0.8f : pan;
            }
            if (best > 90) {
                snd_gap_closed(dist); CUE("cue gap closed col %d", wall_c);
                if (ns && now - s_closed_told > 8000) { s_closed_told = now; speak(L"Closed wall ahead. Shoot the switch to open it", 0); }
            } else {
                int lined = fabsf(best) < 0.3f;
                static DWORD s_back_told;
                if (pan < -0.3f && s_back_told != s_area_t) { s_back_told = s_area_t; speak(L"Beeps from the left: hold back", 0); }
                s_gap_pan = pan;
                snd_gap(best * 2.0f, dist, lined, W_TRI);
                s_gap_pan = 0.25f;
                CUE("cue gap col %d at %.1f (%+.1f) pan %.1f%s", wall_c, gy, best, pan, lined ? " lined up" : "");
            }
            s_next_gap = now + 350 + (best < 90 && fabsf(best) < 0.3f && pan > 0.2f ? 300 : 0);   /* lined up and going on: calmer */
        }
    }
    if ((int)(now - s_next_aim) >= 0) {
        float bx = 99; int found = 0;
        Enemy e[20]; int n = read_enemies(comp, e, 20);
        /* straight ahead the way Landia faces (in the Lor Starcutter fight the ship is often behind) */
        float f = s_face < 0 ? -1.0f : 1.0f;
        for (int i = 0; i < n; i++) { float dx = (e[i].x - s_x) * f; if (dx > 0.3f && dx < 14 && fabsf(e[i].y - s_y) < 0.8f && dx < bx) { bx = dx; found = 1; } }
        BossPos bp[4]; int nb = merge_boss_parts(bp, read_bosses(comp, bp, 4));
        for (int i = 0; i < nb; i++) { float dx = (bp[i].x - s_x) * f; if (dx > 0.3f && dx < 16 && fabsf(bp[i].y - s_y) < 1.6f && dx < bx) { bx = dx; found = 2; } }
        for (int i = 0; i < ns; i++) { float dx = (sw[2 * i] - s_x) * f; if (dx > 0.3f && dx < 14 && fabsf(sw[2 * i + 1] - s_y) < 0.7f && dx < bx) { bx = dx; found = 3; } }
        if (found) { snd_aim(f * bx / 14.0f); CUE("cue aim %s %.1f ahead", found == 1 ? "enemy" : found == 2 ? "boss" : "switch", bx); s_next_aim = now + 450; }
        else s_next_aim = now + 120;
    }
}

void play_poll(void) {
    DWORD now = GetTickCount();
    if (g_movie_playing || g_learn_open) { if (s_have) { if (!g_learn_open) hum_all_off(); s_have = 0; } return; }
    uint32_t comp = scene_component();
    uint32_t hero = comp ? scene_hero(0) : 0;
    float x, y;
    if (!comp || !hero || !hero_pos(hero, &x, &y) || !grid_refresh(comp)) {
        if (s_have) { hum_all_off(); s_have = 0; }
        return;
    }
    s_comp = comp; s_hero = hero;
    walk_poll(hero);
    if (s_god) { uint32_t hp = r32(hero + 0x138); if (is_ptr(hp) && r32(hp + 4) > 0 && r32(hp + 4) < 1000) guest_w32(hp, r32(hp + 4)); }
    int fresh = !s_have;
    s_px = s_have ? s_x : x; s_py = s_have ? s_y : y;
    s_x = x; s_y = y; s_have = 1;
    float vx = s_x - s_px, vy = s_y - s_py;
    if (fresh || g_w != s_area_w || g_h != s_area_h || fabsf(vx) > 3 || fabsf(vy) > 3) new_area();
    if (vx > 0.01f) s_face = 1; else if (vx < -0.01f) s_face = -1;
    int in = input_dir();
    if (in) s_face = in;
    camera_poll(now);

    /* ---- health, ability, death ---- */
    uint32_t hp = r32(hero + 0x138), am = r32(hero + 0x120), dd = r32(hero + 0x148);
    if (is_ptr(hp)) {
        uint32_t cur = r32(hp), max = r32(hp + 4);
        if (max > 0 && cur <= max) {
            int q = cur == 0 ? 0 : (int)((cur * 4 + max - 1) / max);
            if (q < s_hp_quarter && cur > 0) speakf(1, "Health %d percent%s", (int)(cur * 100 / max), q <= 1 ? ", low!" : "");
            s_hp_quarter = q;
        }
    }
    if (is_ptr(dd)) {
        int dead = r8(dd + 4) != 0;
        static DWORD s_down_t;
        if (dead && !s_dead && now - s_down_t > 3000) { s_down_t = now; speak(L"Kirby is down!", 1); hum_all_off(); }
        s_dead = dead;
    }
    if (is_ptr(am)) {
        int ab = (int)r32(am + 0x18);
        if (ab >= 0 && ab < 64) {
            if (s_ability >= 0 && ab != s_ability) {
                if (ab == 0) { if (s_ability != 24) speak(L"Ability lost", 1); }   /* not when Landia flies off */
                else { speakf(1, "%s ability.", ability_name((uint32_t)ab)); speak_utf8(ability_summary((uint32_t)ab), 0); }
                snd_ability(ab != 0);
            }
            s_ability = ab;
        }
    }
    if (s_dead) return;

    /* ---- ground and landing tone (not while riding Landia: that is a flying shooter) ---- */
    g_riding = s_riding = s_ability == 24;
    int gnd = g_ground_on && !s_riding;
    Ground g = ground_below(s_x, s_y);
    float drop = g.pit ? 99 : s_y - g.h;
    /* walking down a slope leaves tiny gaps under the feet: only count
     * clear air, or a small gap that lasts */
    /* Hero +0x12c ScopedPtr<StateChecker> (object at +0x130), +0x3d = on a ladder (StateChecker::isLadder) */
    uint32_t sc = r32(hero + 0x130);
    int climbing = is_ptr(sc) && r8(sc + 0x3d) != 0;
    int air = drop > 0.08f && !climbing && !is_water((int)floorf(s_x), (int)floorf(s_y + 0.5f));
    if (air) s_air_ms += 16; else s_air_ms = 0;
    air = air && (drop > 0.3f || s_air_ms > 250);
    if (gnd && air && s_air_ms > 100 && !s_in_cannon) {   /* inside a cannon: the aim guide speaks instead */
        if (g.pit) {
            hum_set(0, 0, 0, 0, W_SINE);
            if ((int)(now - s_next_pit) >= 0) {
                CUE("cue pit below at %.2f %.2f", s_x, s_y); snd_pit();
                s_next_pit = now + 450;
            }
        } else {
            snd_landing_tone(drop, g.platform, g.spike);
            /* a new platform came under Kirby */
            if (!s_last_ground.pit && fabsf(g.h - s_last_ground.h) > 0.6f && g.h > s_last_ground.h)
                { CUE("cue platform under (%.2f, was %.2f)", g.h, s_last_ground.h); snd_platform_under(); }
            else if (s_last_ground.pit) snd_ground_after_pit();
        }
    } else {
        hum_set(0, 0, 0, 0, W_SINE);
        if (s_air && !air && gnd) CUE("cue landed at %.2f %.2f", s_x, s_y);
        if (s_air && !air && gnd) snd_land(g.platform);
    }
    s_air = air && s_air_ms > 100;
    /* standing on a thin platform while the way goes down: hold down to drop through */
    int on_ladder_top = !air && g.platform && is_ladder((int)floorf(s_x), (int)lroundf(g.h) - 1);
    if (!air && g.platform && s_route_dir < 0 && !s_riding && g_route_on && !on_ladder_top) {
        static DWORD s_next_thin, s_told_t; static int s_told_row = -999;
        if ((int)(now - s_next_thin) >= 0) { CUE("cue thin platform, way down"); snd_drop_through(); s_next_thin = now + 2500; }
        int row = (int)lroundf(g.h);
        if (row != s_told_row || now - s_told_t > 30000) { s_told_row = row; s_told_t = now; speak(L"Thin platform. Hold down to drop through", 0); }
    }
    s_last_ground = g;

    /* ---- platforms above ---- */
    if (gnd && (int)(now - s_next_plat) >= 0) {
        int cx = (int)floorf(s_x), cy = (int)floorf(s_y + 1.0f);
        for (int i = 0; i < 5; i++) {
            int yy = cy + i;
            if (is_wall(cx, yy)) break;
            int s = shape_at(cx, yy);
            if (is_platform(s)) {
                float semis = (float)(i + 1);
                CUE("cue platform above %d", i + 1);
                snd_platform_above((int)semis);
                s_next_plat = now + 1100;
                break;
            }
        }
    }

    /* ---- ladders ---- */
    if (gnd) ladder_poll(now, climbing);

    /* ---- edges ahead (on the ground, moving) ---- */
    if (gnd && !air && (fabsf(vx) > 0.01f || in)) {
        int col = (int)floorf(s_x + s_face * 1.0f);
        if (col != s_edge_col && (int)(now - s_next_edge) >= 0) {
            s_edge_col = col;
            Ground a = ground_below(col + 0.5f, s_y + 1.2f);
            float pan = s_face * 0.5f;
            if (is_wall(col, (int)floorf(s_y + 0.2f))) { /* walls have their own hum */ }
            else if (a.pit) { CUE("cue pit ahead col %d", col); snd_edge_pit(pan); s_next_edge = now + 600; }
            else if (a.spike) { CUE("cue spikes ahead col %d", col); snd_edge_spikes(pan); s_next_edge = now + 600; }
            else if (s_y - a.h > 1.4f) { CUE("cue drop ahead col %d (%.1f)", col, s_y - a.h); snd_drop(pan); s_next_edge = now + 500; }
            else if (a.h - s_y > 0.9f) { CUE("cue step up ahead col %d (%.1f)", col, a.h - s_y); snd_step_up(pan); s_next_edge = now + 500; }
        }
    }

    /* ---- walls ---- */
    if (g_walls_on) {
        const float R = 5.0f;
        float dl = wall_dist(s_x, s_y, -1, R), dr = wall_dist(s_x, s_y, 1, R);
        /* wall_dist can return a little more than R: only count walls within R */
        float hl = wall_height(s_x, s_y, -1, R), hr = wall_height(s_x, s_y, 1, R);
        /* the wall behind Kirby a few dB softer, so a corridor isn't two equal hums */
        float bl = s_face < 0 ? 1.0f : 0.75f, br = s_face > 0 ? 1.0f : 0.75f;
        snd_wall_h(-1, (dl < 0 || dl >= R) ? 0 : (1 - dl / R) * bl, hl);
        snd_wall_h(1, (dr < 0 || dr >= R) ? 0 : (1 - dr / R) * br, hr);
        int cxa = (int)floorf(s_x - 0.35f), cxb = (int)floorf(s_x + 0.35f), ceil_d = -1;
        for (int i = 1; i <= 6; i++)
            if (is_wall(cxa, (int)floorf(s_y + 0.5f) + i) || is_wall(cxb, (int)floorf(s_y + 0.5f) + i)) { ceil_d = i; break; }
        snd_ceiling(ceil_d);
        /* knock: pushing into a wall */
        if (in && (int)(now - s_next_knock) >= 0) {
            float d = in < 0 ? dl : dr;
            if (d >= 0 && d < 0.15f && fabsf(vx) < 0.005f) {
                CUE("cue knock %d", in); snd_knock(in * 0.8f); s_next_knock = now + 350;
                /* bumping into something Kirby can get over: say how */
                float h = in < 0 ? hl : hr;
                static DWORD s_told_t; static int s_told_col = -9999;
                int col = (int)floorf(s_x) + in;
                if (h > 0 && h < 90 && !s_riding && (col != s_told_col || now - s_told_t > 6000)) {
                    s_told_col = col; s_told_t = now;
                    int hi = (int)ceilf(h - 0.05f);
                    if (h <= 2.2f) speakf(0, "Step, %d high. Jump", hi);
                    else speakf(0, "Wall, %d high. Jump and float over", hi);
                }
            }
        }
    } else { hum_set(1, 196, 0, -1, W_SINE); hum_set(2, 196, 0, 1, W_SINE); snd_ceiling(-1); }

    /* ---- riding Landia (the Lor Starcutter chase): where the gap in a wall ahead is,
     * and a tick when something to shoot is straight ahead ---- */
    if (s_riding) shooter_poll(comp, now);

    /* ---- spikes nearby ---- */
    if ((int)(now - s_next_spike) >= 0) {
        int cx = (int)floorf(s_x), cy = (int)floorf(s_y + 0.5f), found = 0;
        float bd = 99, bx = 0, by = 0;
        for (int yy = cy - 4; yy <= cy + 4; yy++) for (int xx = cx - 4; xx <= cx + 4; xx++)
            if (is_spike(xx, yy)) { float d = fabsf(xx + 0.5f - s_x) + fabsf(yy + 0.5f - s_y); if (d < bd) { bd = d; bx = xx + 0.5f - s_x; by = yy + 0.5f - s_y; found = 1; } }
        if (found) { snd_spikes(by, 0.12f + 0.2f * (1 - bd / 9), bx / 4); s_next_spike = now + 700; }
        else s_next_spike = now + 300;
    }

    /* ---- how many enemies on screen: said when more come in (as in Castle Crashers Access) ---- */
    if (g_count_on) {
        static int s_prev = -1, s_said = 0; static DWORD s_changed, s_said_t;
        Enemy e[20]; int n = read_enemies(comp, e, 20), c = 0;
        for (int i = 0; i < n; i++)
            if (creature_kind(e[i].kind) && fabsf(e[i].x - s_x) < 11 && fabsf(e[i].y - s_y) < 7) c++;
        if (now - s_area_t < 1000) { s_prev = c; s_said = c; s_changed = now; }
        if (c != s_prev) { s_prev = c; s_changed = now; }
        if (now - s_changed > 600 && c != s_said) {
            if (c > s_said && now - s_said_t > 2000) { speakf(0, "%d %s", c, c == 1 ? "enemy" : "enemies"); s_said_t = now; s_said = c; }
            else if (c < s_said) s_said = c;
        }
    }

    /* ---- in reach to inhale: an enemy in front of Kirby, close enough, at his height
     * (only without an ability, when X inhales) ---- */
    if (g_radar_on && s_ability <= 0) {
        static uint32_t s_in_reach[8]; static int s_nreach;
        uint32_t now_in[8]; int nn = 0;
        Enemy e[20]; int n = read_enemies(comp, e, 20);
        for (int i = 0; i < n && nn < 8; i++) {
            if (!creature_kind(e[i].kind)) continue;
            const char *nm = enemy_name(e[i].kind);
            if (!strcmp(nm, "Armor Waddle Dee") || !strcmp(nm, "Puppet Waddle Dee") || !strcmp(nm, "Coconut")) continue;
            float ahead = (e[i].x - s_x) * s_face, dy = e[i].y - s_y;
            if (ahead < 0.2f || ahead > 3.2f || fabsf(dy) > 1.3f) continue;
            now_in[nn++] = e[i].addr;
            int was = 0;
            for (int q = 0; q < s_nreach; q++) if (s_in_reach[q] == e[i].addr) was = 1;
            if (!was) { CUE("cue in reach %s ahead %.1f dy %.1f", nm, ahead, dy); snd_in_reach((e[i].x - s_x) / 6.0f); }
        }
        s_nreach = nn; for (int q = 0; q < nn; q++) s_in_reach[q] = now_in[q];
    }

    /* ---- thrown things, followed while they fly ---- */
    if (g_radar_on) thrown_poll(comp, now);

    /* ---- enemy radar ---- */
    if (g_radar_on && (int)(now - s_next_radar) >= 0) {
        Enemy e[20]; int n = read_enemies(comp, e, 20);
        int order[20], m = 0;
        float nearest = 99;
        for (int i = 0; i < n; i++) {
            float dx = e[i].x - s_x, dy = e[i].y - s_y;
            if (fabsf(dx) > 14 || fabsf(dy) > 9) continue;
            if (thrown_kind(e[i].kind)) continue;   /* thrown things have their own ticks (and Kirby's own star none) */
            order[m++] = i;
        }
        for (int i = 0; i < m; i++) for (int j = i + 1; j < m; j++) {
            float di = fabsf(e[order[i]].x - s_x) + fabsf(e[order[i]].y - s_y), dj = fabsf(e[order[j]].x - s_x) + fabsf(e[order[j]].y - s_y);
            if (dj < di) { int t = order[i]; order[i] = order[j]; order[j] = t; }
        }
        if (m > 4) m = 4;   /* with the click and the sparkle, four fit in a second */
        for (int i = 0; i < m; i++) {
            Enemy *q = &e[order[i]];
            float dx = q->x - s_x, dy = q->y - s_y, d = sqrtf(dx * dx + dy * dy);
            if (d < nearest) nearest = d;
            float semis = dy > 12 ? 12 : dy < -12 ? -12 : dy;
            float gain = 0.42f * (1.0f - d / 18.0f); if (gain < 0.07f) gain = 0.07f;
            play_enemy(q->kind, i * 0.25f, semis, gain, dx / 9.0f);
            CUE("cue enemy %s dx %.1f dy %.1f", enemy_name(q->kind), dx, dy);
        }
        s_next_radar = now + (nearest < 3 ? 550 : 1000);
    }

    /* ---- bosses: name when they appear, health by quarters, a drum from where they are ---- */
    {
        BossPos bp[4]; int n = read_bosses(comp, bp, 4);
        n = merge_boss_parts(bp, n);
        for (int i = 0; i < n; i++) {
            int slot = -1;
            for (int j = 0; j < s_nboss_seen; j++) if (s_boss_seen[j] == bp[i].addr) slot = j;
            if (slot < 0 && s_nboss_seen < 8) {
                slot = s_nboss_seen++; s_boss_seen[slot] = bp[i].addr; s_boss_quarter[slot] = 4;
                uint32_t bit = bp[i].kind < 32 ? 1u << bp[i].kind : 0;
                for (uint32_t k = 0; k < 22; k++)   /* kinds that share a name (two Lor Starcutters, two Magolors) */
                    if ((s_boss_told & (1u << k)) && !strcmp(boss_name(k), boss_name(bp[i].kind)) && !strcmp(boss_desc(k), boss_desc(bp[i].kind))) s_boss_told |= bit;
                if (!(s_boss_told & bit)) {
                    s_boss_told |= bit;
                    speakf(0, "%s%s! %s", boss_is_mid(bp[i].kind) ? "Mid-boss: " : "Boss: ", boss_name(bp[i].kind), boss_desc(bp[i].kind));
                    speech_protect((DWORD)(strlen(boss_desc(bp[i].kind)) * 65 + 2000));   /* about 15 characters a second */
                }
                s_last_boss_kind = bp[i].kind;
            }
            if (slot >= 0 && bp[i].maxhp > 0 && bp[i].hp >= 0 && bp[i].hp <= bp[i].maxhp) {
                int q = bp[i].hp == 0 ? 0 : (bp[i].hp * 4 + bp[i].maxhp - 1) / bp[i].maxhp;
                if (q < s_boss_quarter[slot]) {
                    if (q == 0) speakf(1, "%s defeated!", boss_name(bp[i].kind));
                    else speakf(1, "%s health %d percent", boss_name(bp[i].kind), (bp[i].hp * 100 + bp[i].maxhp - 1) / bp[i].maxhp);
                }
                s_boss_quarter[slot] = q;
            }
        }
        if (n && g_radar_on && (int)(now - s_next_boss) >= 0) {
            float nearest = 99;
            for (int i = 0; i < n; i++) {
                float dx = bp[i].x - s_x, dy = bp[i].y - s_y, d = sqrtf(dx * dx + dy * dy);
                if (d < nearest) nearest = d;
                snd_boss(boss_is_mid(bp[i].kind), dy, 0.45f, dx / 8.0f);
                CUE("cue boss %s dx %.1f dy %.1f hp %d/%d", boss_name(bp[i].kind), dx, dy, bp[i].hp, bp[i].maxhp);
            }
            s_next_boss = now + (nearest < 4 ? 800 : nearest < 8 ? 1000 : 1400);
        }
        if (!n && s_nboss_seen && now - s_area_t > 2000) { /* keep the list until the area changes */ }
    }

    areas_poll();

    /* ---- keys and other carried things, locked gates, blocks, water, ability stars ---- */
    objects_poll(comp, s_x, s_y, now);
    gimmicks_poll(comp, s_x, s_y, s_ability, now);

    /* ---- items ---- */
    if (g_radar_on && (int)(now - s_next_items) >= 0) {
        ItemPos it[32]; int n = read_items(comp, it, 32), played = 0;
        for (int pass = 0; pass < 4; pass++) {
            int bi = -1; float bd = 1e9f;
            for (int i = 0; i < n; i++) {
                if (it[i].kind == 99) continue;
                float dx = it[i].x - s_x, dy = it[i].y - s_y, d = fabsf(dx) + fabsf(dy);
                if (fabsf(dx) > 10 || fabsf(dy) > 7) continue;
                if (item_is_minor(it[i].kind) && d > 5) continue;
                if (d < bd) { bd = d; bi = i; }
            }
            if (bi < 0) break;
            float dx = it[bi].x - s_x, dy = it[bi].y - s_y;
            float semis = dy > 12 ? 12 : dy < -12 ? -12 : dy;
            float gain = 0.35f * (1.0f - bd / 18.0f); if (gain < 0.06f) gain = 0.06f;
            item_sound(it[bi].kind, played * 0.18f, semis, gain, dx / 8.0f);
            CUE("cue item %s dx %.1f dy %.1f", k_item_names[it[bi].kind], dx, dy);
            it[bi].kind = 99; played++;
        }
        s_next_items = now + 1300;
    }

    /* ---- right under an item: a bling as Kirby lines up with it (again every 1.2 s while he stays) ---- */
    if (g_radar_on) {
        static int s_under = 0; static DWORD s_under_t;
        ItemPos it[32]; int n = read_items(comp, it, 32), under = 0; float uy = 0;
        for (int i = 0; i < n; i++) {
            if (it[i].kind == 10) continue;   /* the Warp Star has its own "above you" */
            float dx = it[i].x - s_x, dy = it[i].y - s_y;
            if (fabsf(dx) < (s_under ? 0.65f : 0.45f) && dy > 1.0f && dy < 10.0f && (!under || dy < uy)) { under = 1; uy = dy; }
        }
        if (under && (!s_under || (int)(now - s_under_t) >= 1200)) {
            snd_under_item(uy);
            s_under_t = now;
            CUE("cue under item dy %.1f", uy);
        }
        s_under = under;
    }

    /* ---- level hub stage doors ---- */
    {
        HubDoor hd[10]; int n = read_hub_doors(comp, hd, 10), at = -1;
        int closest = -1; float nd = 1e9f;
        for (int i = 0; i < n; i++) {
            float dx = hd[i].x - s_x, dy = hd[i].y - s_y;
            if (fabsf(dx) < 0.4f && fabsf(dy) < 0.8f && !air) at = i;
            float d = fabsf(dx) + fabsf(dy);
            if (d < nd) { nd = d; closest = i; }
        }
        if (at >= 0 && at != s_at_hub_door) {
            speakf(1, "Stage %d door%s, press up", hd[at].stage + 1, hd[at].active ? "" : " (locked)");
        }
        s_at_hub_door = at;
        if (closest >= 0 && nd < 10 && at < 0 && (int)(now - s_next_hub) >= 0) {
            float dx = hd[closest].x - s_x, dy = hd[closest].y - s_y;
            float semis = dy > 12 ? 12 : dy < -12 ? -12 : dy;
            snd_hub_door(semis, dx / 6);
            CUE("cue hub door %d dx %.1f dy %.1f", hd[closest].stage + 1, dx, dy);
            s_next_hub = now + 1800;
        }
    }

    /* ---- doors on the Lor Starcutter ---- */
    {
        ShipDoor sd[20]; int n = read_ship_doors(comp, sd, 20), at = -1, closest = -1;
        float nd = 1e9f;
        for (int i = 0; i < n; i++) {
            float dx = sd[i].x - s_x, dy = sd[i].y - s_y;
            if (fabsf(dx) < 0.5f && fabsf(dy) < 0.8f && !air) at = i;
            float d = fabsf(dx) + fabsf(dy);
            if (sd[i].open && d < nd) { nd = d; closest = i; }   /* the bell only for doors you can use */
        }
        if (at >= 0 && at != s_at_ship_door) {
            if (sd[at].open) speakf(1, "%s, press up", ship_door_name(&sd[at]));
            else speakf(1, "%s, locked", ship_door_name(&sd[at]));
        }
        s_at_ship_door = at;
        if (closest >= 0 && nd < 10 && at < 0 && (int)(now - s_next_ship) >= 0) {
            float dx = sd[closest].x - s_x, dy = sd[closest].y - s_y;
            snd_exit_door(dy > 12 ? 12 : dy < -12 ? -12 : dy, dx / 6);
            CUE("cue ship door dx %.1f dy %.1f", dx, dy);
            s_next_ship = now + 1800;
        }
    }

    /* ---- warp star ---- */
    {
        float wx, wy;
        if (read_warp_star(comp, &wx, &wy)) {
            float dx = wx - s_x, dy = wy - s_y;
            int under = fabsf(dx) < 1.0f && dy > 0 && dy < 6;
            if (under && !s_under_warp) speak(L"Warp Star above you, jump into it", 1);
            s_under_warp = under;
            if (fabsf(dx) < 12 && fabsf(dy) < 8 && (int)(now - s_next_warp) >= 0) {
                float semis = dy > 12 ? 12 : dy < -12 ? -12 : dy;
                snd_warp_star(semis, dx / 6);
                CUE("cue warp star dx %.1f dy %.1f", dx, dy);
                s_next_warp = now + 2000;
            }
        }
    }

    /* ---- dimensional rift ---- */
    {
        Rift r;
        if (read_rift(comp, &r)) {
            float dx = r.x - s_x, dy = r.y - (s_y + 0.5f);
            int open = rift_open(&r), super = s_ability >= 25 && s_ability <= 29;
            char where[64]; speak_dir(where, sizeof where, dx, dy);
            if (r.addr != s_rift_addr) { s_rift_addr = r.addr; s_rift_open = -1; s_rift_told = 0; }
            if (open && s_rift_open == 0) {
                /* it has just opened */
                if (r.out) speakf(1, "The way out is open: a rift, %s. Jump into it", where);
                else speakf(1, "A rift opened, %s! Jump into it to go to Another Dimension", where);
                s_rift_told |= 2; s_next_rift = now;
                CUE("cue rift opened dx %.1f dy %.1f", dx, dy);
            } else if (open && !(s_rift_told & 2) && fabsf(dx) < 14 && fabsf(dy) < 9) {
                /* open from the start (back from Another Dimension, or the way out of it) */
                speakf(0, r.out ? "A rift, %s: the way out. Jump into it" : "A rift, %s. Jump into it to go to Another Dimension", where);
                s_rift_told |= 2;
            } else if (!open && r.state == 0 && super && !(s_rift_told & 1) && !r.entered) {
                speakf(0, "A rift is hidden %s. Smash the scenery there with your Super Ability to open it", where);
                s_rift_told |= 1;
            }
            s_rift_open = open;
            int at = open && fabsf(dx) < 1.2f && fabsf(dy) < 1.5f;
            if (at && !s_at_rift) speak(L"Rift right here, jump", 1);
            s_at_rift = at;
            /* the sound: always when open (it is the way on), the hidden spot only while Kirby has a Super Ability */
            int hear = open ? fabsf(dx) < 40 && fabsf(dy) < 16 : (r.state == 0 && super && !r.entered && fabsf(dx) < 60);
            if (g_radar_on && hear && (int)(now - s_next_rift) >= 0) {
                float d = fabsf(dx) + fabsf(dy), gain = 0.42f * (1.0f - d / 60.0f); if (gain < 0.12f) gain = 0.12f;
                snd_rift(dy > 12 ? 12 : dy < -12 ? -12 : dy, gain, dx / 10.0f, !open);
                CUE("cue rift %s dx %.1f dy %.1f", open ? "open" : "hidden", dx, dy);
                s_next_rift = now + (d < 6 ? 1100 : 1700);
            }
        }
    }

    /* ---- cannons ---- */
    {
        Cannon c[36]; int n = read_cannons(comp, c, 36), best = -1; float bd = 1e9f;
        for (int i = 0; i < n; i++) {
            float dx = c[i].x - s_x, dy = c[i].y - s_y, d = fabsf(dx) + fabsf(dy);
            if (c[i].fuse) {
                if (c[i].state == 1 && s_cannon_in != c[i].addr) { s_cannon_in = c[i].addr; speak(L"In the cannon. Wait for the fuse to burn down", 1); }
                if (fabsf(dx) < 12 && fabsf(dy) < 8 && s_told_cannon != c[i].addr && c[i].state == 0) {
                    s_told_cannon = c[i].addr;
                    char where[64]; speak_dir(where, sizeof where, dx, dy);
                    speakf(0, "Fuse cannon, %s. Light its fuse with a fire ability, then get into the cannon before the flame reaches it. It shoots you somewhere secret", where);
                }
            } else if (fabsf(dx) < 2.5f && fabsf(dy) < 2.5f && s_told_cannon != c[i].addr) {
                s_told_cannon = c[i].addr;
                speak(c[i].manual ? L"Barrel cannon. Jump in, then press a button to fire" : L"Barrel cannon. Jump in and it fires you", 0);
            }
            /* only the nearest barrel is heard (some rooms have 30) */
            if (!c[i].fuse && fabsf(dx) < 10 && fabsf(dy) < 8 && d < bd) { bd = d; best = i; }
        }
        /* Inside a barrel cannon you fire yourself (state 1 = Kirby in): follow its aim through the
         * live grid (moving walls included) and say when the way is clear. CannonBlow +0x1e4 = aim in
         * degrees, +0xa5c state (0 wait, 1 Kirby enters, 2 waiting to be fired, 3 turning, then fires). */
        s_in_cannon = 0;
        for (int i = 0; i < n; i++) {
            /* 1 = just entered, 2 = waiting to be fired (manual), 3 = turning before it fires */
            if (c[i].fuse || c[i].state < 1 || c[i].state > 3 || fabsf(c[i].x - s_x) > 1.5f || fabsf(c[i].y - s_y) > 1.5f) continue;
            s_in_cannon = 1;
            static int s_aim_blocked_seen, s_aim_prev; static DWORD s_aim_open_t, s_aim_enter_t;
            if (s_cannon_in != c[i].addr) {
                s_cannon_in = c[i].addr; s_aim_blocked_seen = 0; s_aim_prev = -1; s_aim_enter_t = now;
                if (c[i].manual) speak(L"In the cannon. Steady high beeps: the way is clear, press a button to fire. Low ticks: something is in the way. When moving walls open, a bright chime plays: fire right away", 1);
            }
            if (!c[i].manual || (int)(now - s_next_aim) < 0) break;
            float a = c[i].angle * 0.0174533f, ux = -sinf(a), uy = cosf(a);   /* 0 = up, counter-clockwise (checked live) */
            float free = 0;
            for (float t = 1.5f; t < 30; t += 0.25f) {
                int cx = (int)floorf(c[i].x + ux * t), cy = (int)floorf(c[i].y + uy * t);
                if (cx < 0 || cy < 0 || cx >= g_w || cy >= g_h) { free = 30; break; }
                if (shape_at(cx, cy) == 1) break;
                free = t;
            }
            int clear = free >= 8;
            const char *what;
            if (!clear) {                       /* blocked: low tick, higher the farther the block */
                s_aim_blocked_seen = 1;
                tone(180 + free * 30, 160 + free * 30, 0.05f, 0.25f, 0, W_SQUARE); what = "blocked";
            } else if (!s_aim_blocked_seen && now - s_aim_enter_t < 2500) {   /* listening first: walls may be moving */
                tone(880, 880, 0.02f, 0.08f, 0, W_SINE); what = "checking";
            } else if (!s_aim_blocked_seen) {   /* clear the whole time: steady high beeps, fire any time */
                tone(1320, 1320, 0.04f, 0.22f, 0, W_TRI); tone_at(0.06f, 1760, 1760, 0.05f, 0.22f, 0, W_TRI); what = "clear";
            } else if (s_aim_prev == 0) {       /* moving walls just opened: fire now (tested: Kirby gets through) */
                s_aim_open_t = now;
                static const float nn[3] = { 1047, 1568, 2093 };
                for (int q = 0; q < 3; q++) tone_at(q * 0.05f, nn[q], nn[q], q == 2 ? 0.14f : 0.05f, 0.32f, 0, W_TRI);
                what = "open now";
            } else {                            /* still open, but late: walls further down may close on Kirby */
                tone(2093, 2093, 0.02f, 0.08f, 0, W_SINE); what = now - s_aim_open_t < 400 ? "open" : "open late";
            }
            s_aim_prev = clear;
            CUE("cue cannon aim %.1f free %.1f %s", c[i].angle, free, what);
            s_next_aim = now + (clear ? 250 : 200);
            break;
        }
        if (g_radar_on && (int)(now - s_next_cannon) >= 0) {
            for (int i = 0; i < n; i++) {
                float dx = c[i].x - s_x, dy = c[i].y - s_y, d = fabsf(dx) + fabsf(dy);
                if (c[i].fuse ? fabsf(dx) >= 30 || fabsf(dy) >= 14 || c[i].state >= 1 : i != best || s_in_cannon) continue;
                float gain = (c[i].fuse ? 0.45f : 0.3f) * (1.0f - d / 44.0f); if (gain < 0.1f) gain = 0.1f;
                float semis = dy > 12 ? 12 : dy < -12 ? -12 : dy;
                if (c[i].fuse) snd_cannon(semis, gain, dx / 10.0f); else snd_barrel(semis, gain, dx / 8.0f);
                CUE("cue %s dx %.1f dy %.1f", c[i].fuse ? "cannon" : "barrel", dx, dy);
            }
            s_next_cannon = now + 1900;
        }
    }

    /* ---- route to the exit ---- */
    if (g_route_on && !s_riding && (int)(now - s_next_route) >= 0) {
        s_next_route = now + 350;
        if (!s_have_target || now - s_area_t < 1500) pick_target();
        if (s_have_target) {
            route_build((int)floorf(s_target.x), (int)floorf(s_target.y + 0.5f));
            int side, total = 0;
            int rise = route_follow((int)floorf(s_x), (int)floorf(s_y + 0.5f), 6, &side, &total);
            int want = rise >= 2 ? 1 : rise <= -2 ? -1 : 0;
            static DWORD next_cue;
            if (want && (want != s_route_dir || (int)(now - next_cue) >= 0)) {
                CUE("cue route %s (rise %d, side %d, dist %d)", want > 0 ? "up" : "down", rise, side, total);
                snd_route(want, side * 0.4f);
                next_cue = now + 1600;
            }
            s_route_dir = want;
            /* the door itself */
            float ddx = s_target.x - s_x, ddy = s_target.y - s_y;
            if (fabsf(ddx) < 10 && fabsf(ddy) < 6 && (int)(now - s_next_door) >= 0) {
                float semis = ddy > 12 ? 12 : ddy < -12 ? -12 : ddy;
                CUE("cue door dx %.1f dy %.1f", ddx, ddy);
                snd_exit_door(semis, ddx / 6);
                s_next_door = now + 1800;
            }
        }
        /* every door: where it leads when Kirby stands at it; side doors have their own softer bell */
        Door d[20]; int n = read_doors(s_comp, d, 20), at_any = 0;
        for (int i = 0; i < n; i++) {
            float ddx = d[i].x - s_x, ddy = d[i].y - s_y;
            int target = s_have_target && fabsf(d[i].x - s_target.x) < 0.1f && fabsf(d[i].y - s_target.y) < 0.1f;
            if (!target && fabsf(ddx) < 10 && fabsf(ddy) < 6 && (int)(now - s_next_side_door) >= 0) {
                float semis = ddy > 12 ? 12 : ddy < -12 ? -12 : ddy;
                CUE("cue side door dx %.1f dy %.1f %s", ddx, ddy, d[i].label ? d[i].label : "?");
                snd_side_door(semis, ddx / 6);
                s_next_side_door = now + 2600;
            }
            if (fabsf(ddx) < 0.35f && fabsf(ddy) < 0.6f && !air) {
                at_any = 1;
                if (s_at_door != i + 1) {
                    if (d[i].label) speakf(1, "%s. Press up", d[i].label); else speak(L"Door, press up", 1);
                }
                s_at_door = i + 1;
            }
        }
        if (!at_any) s_at_door = 0;
    }
}

/* ---- spoken reports (keys) ---- */
void play_say_enemies(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    Enemy e[20]; int n = read_enemies(s_comp, e, 20);
    char buf[1024] = ""; int k = 0;
    for (int pass = 0; pass < n; pass++) {
        int bi = -1; float bd = 1e9f;
        for (int i = 0; i < n; i++) {
            if (e[i].addr == 0) continue;
            float d = fabsf(e[i].x - s_x) + fabsf(e[i].y - s_y);
            if (d < bd) { bd = d; bi = i; }
        }
        if (bi < 0 || k >= 6) break;
        char where[64]; speak_dir(where, sizeof where, e[bi].x - s_x, e[bi].y - s_y);
        Sig s = enemy_sig(e[bi].kind);
        size_t L = strlen(buf);
        snprintf(buf + L, sizeof buf - L, "%s%s%s, %s", k ? ". " : "", enemy_name(e[bi].kind), s.hazard ? " (obstacle)" : "", where);
        e[bi].addr = 0; k++;
    }
    BossPos bp[4]; int nb = merge_boss_parts(bp, read_bosses(s_comp, bp, 4));
    for (int i = 0; i < nb; i++) {
        char where[64]; speak_dir(where, sizeof where, bp[i].x - s_x, bp[i].y - s_y);
        size_t L = strlen(buf);
        if (bp[i].maxhp > 0) snprintf(buf + L, sizeof buf - L, "%s%s, %s, health %d percent", k ? ". " : "", boss_name(bp[i].kind), where, (bp[i].hp * 100 + bp[i].maxhp - 1) / bp[i].maxhp);
        else snprintf(buf + L, sizeof buf - L, "%s%s, %s", k ? ". " : "", boss_name(bp[i].kind), where);
        k++;
    }
    if (!k) speak(L"No enemies nearby", 1); else speak_utf8(buf, 1);
}

/* Rift and cannons for F4 and Shift+F2: appended to buf, returns how many. */
static int say_rift_cannons(char *buf, size_t cap, int cannons) {
    size_t L = strlen(buf); int k = 0; char where[64]; Rift r;
    if (read_rift(s_comp, &r) && (rift_open(&r) || (r.state == 0 && !r.entered && s_ability >= 25 && s_ability <= 29))) {
        speak_dir(where, sizeof where, r.x - s_x, r.y - (s_y + 0.5f));
        L += snprintf(buf + L, cap - L, "%s%s, %s", L ? ". " : "", !rift_open(&r) ? "Hidden rift, open it with your Super Ability" : r.out ? "Rift, the way out" : "Rift to Another Dimension", where);
        k++;
    }
    if (!cannons) return k;
    Cannon c[36]; int n = read_cannons(s_comp, c, 36), nb = 0, bi = -1; float bd = 1e9f;
    for (int i = 0; i < n && L < cap - 80; i++) {
        float d = fabsf(c[i].x - s_x) + fabsf(c[i].y - s_y);
        if (!c[i].fuse) { nb++; if (d < bd) { bd = d; bi = i; } continue; }
        speak_dir(where, sizeof where, c[i].x - s_x, c[i].y - s_y);
        L += snprintf(buf + L, cap - L, "%sfuse cannon, %s", L ? ". " : "", where); k++;
    }
    if (bi >= 0 && L < cap - 80) {
        speak_dir(where, sizeof where, c[bi].x - s_x, c[bi].y - s_y);
        L += snprintf(buf + L, cap - L, "%s%d barrel cannon%s, nearest %s", L ? ". " : "", nb, nb == 1 ? "" : "s", where); k++;
    }
    return k;
}

void play_say_items(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    ItemPos it[32]; int n = read_items(s_comp, it, 32);
    char buf[1024] = ""; int k = 0, minor = 0;
    for (int pass = 0; pass < n && k < 6; pass++) {
        int bi = -1; float bd = 1e9f;
        for (int i = 0; i < n; i++) {
            if (it[i].kind == 99) continue;
            if (item_is_minor(it[i].kind)) continue;
            float d = fabsf(it[i].x - s_x) + fabsf(it[i].y - s_y);
            if (d < bd) { bd = d; bi = i; }
        }
        if (bi < 0) break;
        char where[64]; speak_dir(where, sizeof where, it[bi].x - s_x, it[bi].y - s_y);
        size_t L = strlen(buf);
        if (it[bi].ab > 0 && it[bi].ab < 24) snprintf(buf + L, sizeof buf - L, "%s%s ability star, %s", k ? ". " : "", ability_name(it[bi].ab), where);
        else snprintf(buf + L, sizeof buf - L, "%s%s, %s", k ? ". " : "", k_item_names[it[bi].kind], where);
        it[bi].kind = 99; k++;
    }
    for (int i = 0; i < n; i++) if (it[i].kind != 99 && item_is_minor(it[i].kind)) minor++;
    if (minor) { size_t L = strlen(buf); snprintf(buf + L, sizeof buf - L, "%s%d point star%s", k ? ". " : "", minor, minor == 1 ? "" : "s"); }
    k += objects_describe(s_comp, s_x, s_y, buf, sizeof buf);
    k += gimmicks_describe(s_x, s_y, s_ability, buf, sizeof buf);
    k += say_rift_cannons(buf, sizeof buf, 1);
    if (!k && !minor) speak(L"No items nearby", 1); else speak_utf8(buf, 1);
}

void play_say_status(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    char buf[256];
    uint32_t hp = r32(s_hero + 0x138);
    int pct = is_ptr(hp) && r32(hp + 4) ? (int)(r32(hp) * 100 / r32(hp + 4)) : -1;
    Ground g = ground_below(s_x, s_y);
    const char *where = is_water((int)floorf(s_x), (int)floorf(s_y + 0.5f)) ? "in water" :
                        (s_y - g.h > 0.08f || g.pit) ? "in the air" : g.platform ? "on a platform" : "on the ground";
    wchar_t lives[16]; char extra[64] = "";
    if (text_hud("RestPlayerPanelN", "Count", lives, 16)) snprintf(extra, sizeof extra, " %d lives.", _wtoi(lives));
    snprintf(buf, sizeof buf, "Health %d percent. %s. Kirby is %s.%s", pct, s_ability > 0 ? ability_name((uint32_t)s_ability) : "No ability", where, extra);
    speak_utf8(buf, 1);
}

void play_say_route(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    HubDoor hd[10]; int nh = read_hub_doors(s_comp, hd, 10);
    if (nh) {   /* a level hub: list the stage doors */
        char buf[512] = ""; size_t L = 0;
        for (int i = 0; i < nh && L < sizeof buf - 64; i++) {
            char where[64]; speak_dir(where, sizeof where, hd[i].x - s_x, hd[i].y - s_y);
            L += snprintf(buf + L, sizeof buf - L, "%sStage %d door%s, %s", i ? ". " : "", hd[i].stage + 1, hd[i].active ? "" : " (locked)", where);
        }
        speak_utf8(buf, 1);
        return;
    }
    pick_target();
    if (!s_have_target) {
        /* no exit door here (the Lor Starcutter, a door room): say where you can go */
        char buf[512] = ""; size_t L = 0; char where[64];
        float wx, wy;
        if (read_warp_star(s_comp, &wx, &wy)) {
            speak_dir(where, sizeof where, wx - s_x, wy - s_y);
            L += snprintf(buf + L, sizeof buf - L, "Warp Star to the World Map, %s, floating: jump into it. ", where);
        }
        ShipDoor sd[20]; int n = read_ship_doors(s_comp, sd, 20);
        for (int i = 0; i < n && L < sizeof buf - 64; i++) {
            speak_dir(where, sizeof where, sd[i].x - s_x, sd[i].y - s_y);
            L += snprintf(buf + L, sizeof buf - L, "%s%s, %s. ", ship_door_name(&sd[i]), sd[i].open ? "" : " (locked)", where);
        }
        if (say_rift_cannons(buf, sizeof buf, 0)) L = strlen(buf);
        if (!L) snprintf(buf, sizeof buf, "No exit door found in this area");
        speak_utf8(buf, 1);
        return;
    }
    route_build((int)floorf(s_target.x), (int)floorf(s_target.y + 0.5f));
    int side, total = 0;
    int rise = route_follow((int)floorf(s_x), (int)floorf(s_y + 0.5f), 6, &side, &total);
    char where[64], buf[1024];
    speak_dir(where, sizeof where, s_target.x - s_x, s_target.y - s_y);
    const char *how = rise >= 2 ? "The way goes up here" : rise <= -2 ? "The way goes down here" :
                      side > 0 ? "The way goes right" : side < 0 ? "The way goes left" : "";
    snprintf(buf, sizeof buf, "%s, %s. %s.", s_target.label ? s_target.label : "Exit door", where, how);
    Door od[20]; int on = read_doors(s_comp, od, 20);
    for (int i = 0; i < on; i++) {
        if (fabsf(od[i].x - s_target.x) < 0.1f && fabsf(od[i].y - s_target.y) < 0.1f) continue;
        char w2[64]; speak_dir(w2, sizeof w2, od[i].x - s_x, od[i].y - s_y);
        size_t L = strlen(buf); snprintf(buf + L, sizeof buf - L, " %s, %s.", od[i].label ? od[i].label : "Another door", w2);
    }
    char extra[192] = "";
    if (say_rift_cannons(extra, sizeof extra, 0)) { size_t L = strlen(buf); snprintf(buf + L, sizeof buf - L, " Also: %s.", extra); }
    speak_utf8(buf, 1);
}

void play_say_surroundings(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    char buf[512]; size_t L = 0;
    float dl = wall_dist(s_x, s_y, -1, 8), dr = wall_dist(s_x, s_y, 1, 8);
    for (int side = -1; side <= 1; side += 2) {
        float d = side < 0 ? dl : dr, h = wall_height(s_x, s_y, side, 8);
        const char *sn = side < 0 ? "left" : "right";
        if (d < 0) L += snprintf(buf + L, sizeof buf - L, "No wall %s. ", sn);
        else if (h > 0 && h <= 2.2f) L += snprintf(buf + L, sizeof buf - L, "Step %d high, %d %s: jump. ", (int)ceilf(h - 0.05f), (int)lroundf(d), sn);
        else if (h > 0 && h < 90) L += snprintf(buf + L, sizeof buf - L, "Wall %d high, %d %s: jump and float over. ", (int)ceilf(h - 0.05f), (int)lroundf(d), sn);
        else L += snprintf(buf + L, sizeof buf - L, "Wall %d %s. ", (int)lroundf(d), sn);
    }
    /* ground ahead, column by column */
    float prev = s_y; int told = 0;
    for (int i = 1; i <= 8 && L < sizeof buf - 80; i++) {
        int col = (int)floorf(s_x + s_face * i);
        if (is_wall(col, (int)floorf(s_y + 0.2f))) break;
        Ground a = ground_below(col + 0.5f, prev + 2.5f);
        if (a.pit) { L += snprintf(buf + L, sizeof buf - L, "Pit %d ahead. ", i); told = 1; break; }
        if (fabsf(a.h - prev) > 0.9f) {
            L += snprintf(buf + L, sizeof buf - L, "%s %d at %d ahead. ", a.h > prev ? "Step up" : "Drop", (int)lroundf(fabsf(a.h - prev)), i);
            told = 1; prev = a.h;
        }
        if (a.spike) { L += snprintf(buf + L, sizeof buf - L, "Spikes %d ahead. ", i); told = 1; }
    }
    if (!told) L += snprintf(buf + L, sizeof buf - L, "Level ground ahead. ");
    int cx = (int)floorf(s_x);
    for (int i = 1; i <= 6; i++) {
        int s = shape_at(cx, (int)floorf(s_y + 0.5f) + i);
        if (s == 1) { snprintf(buf + L, sizeof buf - L, "Ceiling %d above.", i); break; }
        if (is_platform(s)) { snprintf(buf + L, sizeof buf - L, "Platform %d above.", i); break; }
    }
    L = strlen(buf); if (L < sizeof buf - 2) { buf[L++] = ' '; buf[L] = 0; }
    objects_blocks_describe(s_comp, s_x, s_y, buf, sizeof buf);
    speak_utf8(buf, 1);
}

void play_cmd(const char *line) {
    if (!strcmp(line, "sigtest")) { play_sig(enemy_sig(2), 0, 0, 0.3f, 0); log_line("sigtest queued"); }
    if (!strcmp(line, "humtest")) { hum_set(5, 300, 0.2f, 0, W_SINE); log_line("humtest"); }
    if (!strcmp(line, "humoff")) hum_set(5, 300, 0, 0, W_SINE);
    if (!strcmp(line, "cues on")) g_cue_log = 1;
    if (!strcmp(line, "cues off")) g_cue_log = 0;
    if (!strcmp(line, "walk")) walk_load();
    if (!strcmp(line, "grid")) {   /* dev: the grid the cues use (blocks and locked gates solid) to KirbyAccess\grid.txt */
        wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%lsgrid.txt", g_mod_dir);
        FILE *f = _wfopen(p, L"w");
        if (f) {
            fprintf(f, "%dx%d\n", g_w, g_h);
            for (int y = g_h - 1; y >= 0; y--) {
                fprintf(f, "%3d ", y);
                for (int x = 0; x < g_w; x++) fputc(g_block[x + y * GW] ? 'b' : is_wall(x, y) || is_slope(shape_at(x, y)) ? '#' : is_spike(x, y) ? '^' : is_platform(shape_at(x, y)) ? '=' : '.', f);
                fputc('\n', f);
            }
            fclose(f); log_line("grid written");
        }
    }
    if (!strcmp(line, "walkstop")) s_nwp = s_iwp = 0;
    if (!strcmp(line, "god on")) s_god = 1;   /* dev: keep Kirby's health full (stage sweep) */
    if (!strcmp(line, "god off")) s_god = 0;
    if (!strcmp(line, "riftopen")) {   /* dev: open the area's rift without a Super Ability (stage sweep) */
        Rift r; if (s_comp && read_rift(s_comp, &r)) { guest_w32(r.addr + 0x7e8, 10); log_line("rift at %.2f %.2f opened (was %d)", r.x, r.y, r.state); }
    }
    if (!strcmp(line, "objs")) {
        BossPos bp[4]; int nb = s_comp ? read_bosses(s_comp, bp, 4) : 0;
        for (int i = 0; i < nb; i++) log_line("boss %u %s at %.2f %.2f hp %d/%d", bp[i].kind, boss_name(bp[i].kind), bp[i].x, bp[i].y, bp[i].hp, bp[i].maxhp);
        Enemy e[20]; int n = s_comp ? read_enemies(s_comp, e, 20) : 0;
        for (int i = 0; i < n; i++) log_line("enemy %u %s at %.2f %.2f", e[i].kind, enemy_name(e[i].kind), e[i].x, e[i].y);
        HubDoor hd[10]; int nh = s_comp ? read_hub_doors(s_comp, hd, 10) : 0;
        for (int i = 0; i < nh; i++) log_line("hub door stage %d active %d at %.2f %.2f", hd[i].stage, hd[i].active, hd[i].x, hd[i].y);
        Door d[20]; int nd = s_comp ? read_doors(s_comp, d, 20) : 0;
        for (int i = 0; i < nd; i++) log_line("door kind %d at %.2f %.2f role %d %s", d[i].kind, d[i].x, d[i].y, d[i].role, d[i].label ? d[i].label : "-");
        { float wx, wy; if (s_comp && read_warp_star(s_comp, &wx, &wy)) log_line("warpstar at %.2f %.2f", wx, wy); }
        { int lv, st, ar; if (s_comp && area_place(s_comp, &lv, &st, &ar)) log_line("place %d %d %d", lv, st, ar); }
        { uint32_t sc = s_hero ? r32(s_hero + 0x130) : 0; log_line("kirby %.2f %.2f target %d %.2f %.2f ladder %d", s_x, s_y, s_have_target, s_target.x, s_target.y, is_ptr(sc) ? r8(sc + 0x3d) : -1); }
    }
}

void play_enemy_sound(uint32_t kind, float delay, float semis, float gain, float pan) { play_enemy(kind, delay, semis, gain, pan); }
int enemy_is_hazard(uint32_t kind) { return enemy_sig(kind).hazard; }
void play_item_sound(uint32_t kind, float delay, float semis, float gain, float pan) { item_sound(kind, delay, semis, gain, pan); }

/* Ctrl+F2: what the boss of this fight (or the last one) does. */
void play_say_boss(void) {
    if (s_last_boss_kind == 0xffffffff) { speak(L"No boss fight yet", 1); return; }
    speakf(1, "%s. %s", boss_name(s_last_boss_kind), boss_desc(s_last_boss_kind));
}

/* Shift+F3: every move of the ability Kirby has, from the game's own move list. */
void play_say_moves(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    uint32_t ab = s_ability > 0 ? (uint32_t)s_ability : 0;
    char buf[2048];
    snprintf(buf, sizeof buf, "%s moves. %s", ab ? ability_name(ab) : "Kirby's", ability_moves(ab));
    speak_utf8(buf, 1);
}

/* Words for an enemy's sound, from its settings: "a high bright trill". */
void enemy_sound_words(uint32_t kind, char *buf, size_t cap) {
    Sig g = enemy_sig(kind);
    const char *pitch = g.f < 180 ? "very low" : g.f < 330 ? "low" : g.f < 650 ? "middle" : g.f < 1200 ? "high" : "very high";
    static const char *const wave[] = { "soft", "hollow", "bright", "buzzy", "hissing" };
    static const char *const pat[] = { "blip", "double blip", "trill", "rising slide", "falling slide", "long tone", "thud" };
    snprintf(buf, cap, "A %s %s %s%s", pitch, wave[g.wave % 5], pat[g.pat % 7], "");
}

/* Shift+F5: where Kirby is */
void play_say_where(void) {
    if (!s_have) { speak(L"Not in a stage", 1); return; }
    areas_say(s_comp, g_w, g_h);
}

/* Ctrl+F5 hints on/off, Ctrl+Shift+F5 this stage's hint */
void play_hint_key(int shift) { areas_hint_key(s_have ? s_comp : 0, shift); }

#include "camera.inc"
