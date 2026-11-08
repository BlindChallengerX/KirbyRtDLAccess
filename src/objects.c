/*
 * Things in a stage besides enemies and items (play.c calls objects_poll
 * every frame Kirby is in a stage):
 *
 *   - carried items (scn::step::carryitem, Component +0xc4: Manager +0x90
 *     count, +0x94 CarryItem*[16]; item +0x14 BinCarryItemKind, +0x5c the
 *     one carrying it (0 when lying), +0x118 Location*): keys, Cracklers,
 *     balloon bombs, candles...
 *   - locked gates (gimmick::lockshutter, gimmick Manager +0x120: +4 count,
 *     +8 LockShutter*[8]; gate +0xc x, +0x10 y of its foot, 3 tiles tall,
 *     +0x240 state 0 locked, 1 opening, 2 open)
 *   - breakable blocks (gimmick::block, gimmick Manager +0x48 BlockManager:
 *     +0x3144 Block*[x + y * 288] on the land grid; Block +8 BinGmkBlockKind
 *     = index - 1 in the model table at 0x807aec88)
 *   - water (land cell flag 0x08): in and out, depth, the way up
 *   - copy ability stars on stands (item kind 8, +0x18 the ability), named
 *     when Kirby is next to one
 *
 * Coordinates are tiles, y up, as in play.c.
 */
#include "access.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* from play.c */
void speak_dir(char *buf, size_t n, float dx, float dy);
int land_is_water(int x, int y);
const char *ability_name(uint32_t kind);
extern volatile int g_radar_on;
extern volatile int g_cue_log;
#define CUE(...) do { if (g_cue_log) log_line(__VA_ARGS__); } while (0)

/* ---------------- carried items ---------------- */
static const char *const k_carry_names[10] = {
    "ability star", "key", "Maxim Tomato box", "candle", "Crackler", "invisibility stone",
    "balloon bomb", "Stomper Boot", "Prism Shield", "ship part",
};
static const char *const k_carry_desc[10] = {
    "", "Walk it into the locked gate to open it. Don't press X again while carrying it, that throws it. Kirby can only jump once while carrying it, and if you hold it too long it breaks.",
    "", "", "It fires blasts that break metal blocks and hurt enemies.", "", "", "", "", "",
};
typedef struct { uint32_t addr, kind, user; float x, y; } Carry;
static int read_carry(uint32_t comp, Carry *out, int cap) {
    uint32_t cm = r32(comp + 0xc4);
    if (!is_ptr(cm)) return 0;
    uint32_t n = r32(cm + 0x90);
    if (n > 16) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t c = r32(cm + 0x94 + 4 * i);
        if (!is_ptr(c)) continue;
        uint32_t kind = r32(c + 0x14), loc = r32(c + 0x118);
        if (kind >= 10 || !is_ptr(loc)) continue;
        float x = rf32(loc), y = rf32(loc + 4);
        if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
        out[k].addr = c; out[k].kind = kind; out[k].user = r32(c + 0x5c); out[k].x = x; out[k].y = y; k++;
    }
    return k;
}
/* A key: a little two-note jingle. Anything else you can carry: a wooden double knock. */
static void snd_carry(uint32_t kind, float semis, float gain, float pan) {
    float m = powf(2.0f, semis / 12.0f);
    if (kind == 1) {
        tone(1568 * m, 1568 * m, 0.06f, gain, pan, W_SQUARE);
        tone_at(0.08f, 2093 * m, 2093 * m, 0.09f, gain, pan, W_SQUARE);
        tone_at(0.08f, 4186 * m, 4186 * m, 0.05f, gain * 0.3f, pan, W_SINE);
    } else {
        tone(523 * m, 500 * m, 0.05f, gain, pan, W_TRI);
        tone_at(0.1f, 659 * m, 630 * m, 0.05f, gain, pan, W_TRI);
    }
}
void snd_key(float pan) { snd_carry(1, 0, 0.4f, pan); }
void snd_carry_item(float pan) { snd_carry(4, 0, 0.4f, pan); }

/* ---------------- locked gates ---------------- */
typedef struct { uint32_t addr; float x, y; int state; } Gate;
static int read_gates(uint32_t comp, Gate *out, int cap) {
    uint32_t gm = r32(comp + 0xb4), lm = is_ptr(gm) ? r32(gm + 0x120) : 0;
    if (!is_ptr(lm)) return 0;
    uint32_t n = r32(lm + 4);
    if (n > 8) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < cap; i++) {
        uint32_t g = r32(lm + 8 + 4 * i);
        if (!is_ptr(g)) continue;
        float x = rf32(g + 0xc), y = rf32(g + 0x10);
        if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
        out[k].addr = g; out[k].x = x; out[k].y = y + 1.5f; out[k].state = (int)r32(g + 0x240); k++;
    }
    return k;
}
/* A locked gate: a low heavy clunk, then a rattle. */
static void snd_gate(float semis, float gain, float pan) {
    float m = powf(2.0f, semis / 12.0f);
    tone(147 * m, 110 * m, 0.12f, gain, pan, W_SQUARE);
    tone_at(0.15f, 220 * m, 220 * m, 0.03f, gain * 0.6f, pan, W_NOISE);
    tone_at(0.21f, 220 * m, 220 * m, 0.03f, gain * 0.6f, pan, W_NOISE);
}
void snd_locked_gate(float pan) { snd_gate(0, 0.45f, pan); }

/* ---------------- blocks ---------------- */
enum { B_NONE, B_STAR, B_BOMB, B_HARD, B_ICE, B_FIRE, B_CRUMBLE, B_HEAVY, B_ROCK, B_NCAT };
static const char *const k_block_names[B_NCAT] = {
    "", "star block", "bomb block", "metal block", "ice block", "burnable block", "crumbling block", "heavy block", "rock block",
};
static int block_cat(uint32_t kind) {
    switch (kind) {
    case 0: return B_STAR;                       /* BlockStar */
    case 30: return B_ROCK;                      /* BlockChip: grey rock, breaks like a star block */
    case 18: return B_BOMB;                      /* BlockBomb (19-21 are the chain it blows up) */
    case 34: case 41: case 53: return B_HARD;    /* BlockHard, BlockHardBig */
    case 36: case 37: case 38: case 39: case 40: return B_ICE;   /* BlockIce, BlockSnow, BlockGIce */
    case 32: case 35: case 54: return B_FIRE;    /* BlockFire, BlockFireSuper, BlockFireL */
    case 33: case 47: return B_CRUMBLE;          /* BlockCrumble, BlockCrumbleFactory */
    case 2: case 4: case 5: case 22: case 24: case 25: case 26: case 27: case 28: case 31: return B_HEAVY;
    }
    return B_NONE;
}
static uint32_t block_at(uint32_t bm, int x, int y) {
    if (x < 0 || y < 0 || x >= 288 || y >= 160) return 0;
    uint32_t b = r32(bm + 0x3144 + 4 * (uint32_t)(x + y * 288));
    return is_ptr(b) ? b : 0;
}
/* The sound of each kind of block. */
static void snd_block_cat(int cat, float delay, float semis, float gain, float pan) {
    float m = powf(2.0f, semis / 12.0f);
    switch (cat) {
    case B_STAR: case B_ROCK:     /* a wooden tock-tock (rock a little lower) */
        if (cat == B_ROCK) m *= 0.75f;
        tone_at(delay, 1100 * m, 900 * m, 0.035f, gain, pan, W_TRI);
        tone_at(delay + 0.09f, 1100 * m, 900 * m, 0.035f, gain, pan, W_TRI); break;
    case B_BOMB:     /* a fuse hiss and a tock */
        tone_at(delay, 3000 * m, 3000 * m, 0.18f, gain * 0.35f, pan, W_NOISE);
        tone_at(delay + 0.2f, 700 * m, 500 * m, 0.06f, gain, pan, W_SQUARE); break;
    case B_HARD:     /* a metal clank */
        tone_at(delay, 1245 * m, 1245 * m, 0.12f, gain * 0.6f, pan, W_SQUARE);
        tone_at(delay, 1867 * m, 1867 * m, 0.10f, gain * 0.4f, pan, W_SINE); break;
    case B_ICE:      /* a glassy tinkle */
        tone_at(delay, 2637 * m, 2637 * m, 0.05f, gain * 0.6f, pan, W_SINE);
        tone_at(delay + 0.06f, 3136 * m, 3136 * m, 0.05f, gain * 0.6f, pan, W_SINE);
        tone_at(delay + 0.12f, 3951 * m, 3951 * m, 0.06f, gain * 0.5f, pan, W_SINE); break;
    case B_FIRE:     /* a crackle */
        for (int i = 0; i < 4; i++) tone_at(delay + i * 0.05f, 1500 * m, 1500 * m, 0.02f, gain * 0.5f, pan, W_NOISE);
        break;
    case B_CRUMBLE:  /* a low rattle */
        for (int i = 0; i < 3; i++) tone_at(delay + i * 0.06f, 260 * m, 220 * m, 0.03f, gain, pan, W_SQUARE);
        break;
    case B_HEAVY:    /* a deep stone thud */
        tone_at(delay, 110 * m, 70 * m, 0.16f, gain * 1.2f, pan, W_SINE);
        tone_at(delay, 400 * m, 400 * m, 0.04f, gain * 0.3f, pan, W_NOISE); break;
    }
}
void snd_block(int cat, float pan) { snd_block_cat(cat, 0, 0, 0.4f, pan); }
/* Tile camera (play.c): the kind of breakable block standing on a cell, 0 when none. */
int objects_block_cat_at(uint32_t comp, int x, int y) {
    uint32_t gm = r32(comp + 0xb4), bm = is_ptr(gm) ? r32(gm + 0x48) : 0;
    if (!is_ptr(bm)) return 0;
    uint32_t b = block_at(bm, x, y);
    return b ? block_cat(r32(b + 8)) : 0;
}
const char *objects_block_name(int cat) { return cat > 0 && cat < B_NCAT ? k_block_names[cat] : "block"; }
void objects_block_sound(int cat, float delay, float semis, float gain, float pan) { snd_block_cat(cat, delay, semis, gain, pan); }
typedef struct { int cat; float x, y, d; } BlockNear;
/* The nearest block of each kind within the box around Kirby. */
static int near_blocks(uint32_t comp, float px, float py, int rx, int ry, BlockNear *out) {
    uint32_t gm = r32(comp + 0xb4), bm = is_ptr(gm) ? r32(gm + 0x48) : 0;
    int n = 0;
    if (!is_ptr(bm)) return 0;
    BlockNear best[B_NCAT]; for (int c = 0; c < B_NCAT; c++) best[c].d = 1e9f;
    int cx = (int)floorf(px), cy = (int)floorf(py + 0.5f);
    for (int y = cy - ry; y <= cy + ry; y++)
        for (int x = cx - rx; x <= cx + rx; x++) {
            uint32_t b = block_at(bm, x, y);
            if (!b) continue;
            int cat = block_cat(r32(b + 8));
            if (!cat) continue;
            float dx = x + 0.5f - px, dy = y + 0.5f - (py + 0.5f), d = fabsf(dx) + fabsf(dy);
            if (d < best[cat].d) { best[cat].d = d; best[cat].x = x + 0.5f; best[cat].y = y + 0.5f; best[cat].cat = cat; }
        }
    for (int c = 1; c < B_NCAT; c++) if (best[c].d < 1e8f) out[n++] = best[c];
    return n;
}

/* ---------------- switches ---------------- */
/* gimmick::switch_n (gimmick Manager +0x230): three arrays SwitchOnce / SwitchToggle /
 * SwitchReturn at +0, +0x48, +0x90, each +4 count, +8 Switch*[16]. Switch +0x10 x,
 * +0x14 y (bottom of its tile), +0xc74 state 0 off, 1 turning on, 2 on, 3 turning off.
 * Kirby turns one on by hitting it with an attack (searchAttackOn). Found in Cookie
 * Country 2 area 4 (SwitchOnce at 103.5, 9, boxed in by blocks, raises a bridge). */
typedef struct { uint32_t addr; float x, y; int state; } Switch;
static int read_switches(uint32_t comp, Switch *out, int cap) {
    uint32_t gm = r32(comp + 0xb4), sm = is_ptr(gm) ? r32(gm + 0x230) : 0;
    if (!is_ptr(sm)) return 0;
    int k = 0;
    for (uint32_t a = 0; a < 3; a++) {
        uint32_t n = r32(sm + a * 0x48 + 4);
        if (n > 16) continue;
        for (uint32_t i = 0; i < n && k < cap; i++) {
            uint32_t s = r32(sm + a * 0x48 + 8 + 4 * i);
            if (!is_ptr(s)) continue;
            uint32_t vt = r32(s);
            if (vt != 0x807b3230 && vt != 0x807b3280 && vt != 0x807b3258 && vt != 0x807b320c) continue;
            float x = rf32(s + 0x10), y = rf32(s + 0x14);
            if (!(x > -50 && x < 400 && y > -50 && y < 250)) continue;
            out[k].addr = s; out[k].x = x; out[k].y = y; out[k].state = (int)r32(s + 0xc74); k++;
        }
    }
    return k;
}
/* dev "switches": every switch_n switch, raw (no filter on vtable or state) */
void objects_dump_switches(uint32_t comp) {
    uint32_t gm = r32(comp + 0xb4), sm = is_ptr(gm) ? r32(gm + 0x230) : 0;
    if (!is_ptr(sm)) { log_line("switches: no manager"); return; }
    for (uint32_t a = 0; a < 3; a++) {
        uint32_t n = r32(sm + a * 0x48 + 4);
        log_line("switches: array %u count %u", a, n);
        for (uint32_t i = 0; i < n && i < 16; i++) {
            uint32_t s = r32(sm + a * 0x48 + 8 + 4 * i);
            if (!is_ptr(s)) continue;
            log_line("  switch %08x vt %08x at %.2f %.2f state %u  +c70 %08x +c78 %08x +c7c %08x", s, r32(s), rf32(s + 0x10), rf32(s + 0x14), r32(s + 0xc74), r32(s + 0xc70), r32(s + 0xc78), r32(s + 0xc7c));
        }
    }
}
/* Switches not hit yet (x, y pairs), for the shooter aim tick. */
int objects_open_switches(uint32_t comp, float *xy, int cap) {
    Switch sw[16]; int n = read_switches(comp, sw, 16), k = 0;
    for (int i = 0; i < n && k < cap; i++) if (sw[i].state == 0) { xy[2 * k] = sw[i].x; xy[2 * k + 1] = sw[i].y; k++; }
    return k;
}
/* A switch waiting to be hit: a click then a higher clack, like flicking a lever. */
static void snd_switch_off(float semis, float gain, float pan) {
    float m = powf(2.0f, semis / 12.0f);
    tone(2400 * m, 2400 * m, 0.006f, gain * 0.5f, pan, W_NOISE);
    tone(392 * m, 370 * m, 0.03f, gain * 0.8f, pan, W_SQUARE);
    tone_at(0.11f, 2400 * m, 2400 * m, 0.006f, gain * 0.5f, pan, W_NOISE);
    tone_at(0.11f, 587 * m, 560 * m, 0.04f, gain, pan, W_TRI);
}
/* Turned on: a rising chime. */
static void snd_switch_on(float pan) {
    static const float n[4] = { 523, 659, 784, 1047 };
    for (int i = 0; i < 4; i++) tone_at(i * 0.07f, n[i], n[i], i == 3 ? 0.18f : 0.07f, 0.4f, pan, W_TRI);
}
void snd_switch(float pan) { snd_switch_off(0, 0.4f, pan); }
void snd_switch_done(float pan) { snd_switch_on(pan); }
/* What surrounds a switch: "covered by metal blocks" etc., or "". */
static const char *switch_cover(uint32_t comp, const Switch *s) {
    uint32_t gm = r32(comp + 0xb4), bm = is_ptr(gm) ? r32(gm + 0x48) : 0;
    if (!is_ptr(bm)) return "";
    int cx = (int)floorf(s->x), cy = (int)floorf(s->y + 0.5f), any = 0, hard = 0;
    static const int d[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (int i = 0; i < 4; i++) {
        uint32_t b = block_at(bm, cx + d[i][0], cy + d[i][1]);
        if (!b) continue;
        any++; if (block_cat(r32(b + 8)) == B_HARD) hard++;
    }
    if (any < 2) return "";
    return hard ? " It's boxed in by metal blocks: break them first, a Crackler or a strong ability does it." : " It's boxed in by blocks: break them first.";
}

/* Tile camera: a switch, or something lying there to carry (a key, a Crackler),
 * on cell x,y. Plays its sound (pitch semis, side pan) when play is set, and
 * adds its name to name. Returns 1 when something is there. */
int objects_cam_at(uint32_t comp, int x, int y, int play, float semis, float pan, char *name, size_t cap) {
    Switch sw[16]; int n = read_switches(comp, sw, 16);
    for (int i = 0; i < n; i++) {
        if ((int)floorf(sw[i].x) != x || (int)floorf(sw[i].y + 0.5f) != y) continue;
        int on = sw[i].state == 1 || sw[i].state == 2;
        if (play) { if (on) { tone(523, 784, 0.08f, 0.25f, pan, W_TRI); } else snd_switch_off(semis, 0.4f, pan); }
        snprintf(name, cap, on ? "switch, already on" : "switch, hit it with an attack.%s", switch_cover(comp, &sw[i]));
        return 1;
    }
    Carry cs[16]; n = read_carry(comp, cs, 16);
    for (int i = 0; i < n; i++) {
        if (cs[i].user || cs[i].kind == 0) continue;
        if ((int)floorf(cs[i].x) != x || (int)floorf(cs[i].y + 0.3f) != y) continue;
        if (play) snd_carry(cs[i].kind, semis, 0.4f, pan);
        snprintf(name, cap, "%s, press X to pick it up", k_carry_names[cs[i].kind]);
        return 1;
    }
    return 0;
}

/* ---------------- water ---------------- */
/* Into the water: a falling splash. Out: a rising one. */
static void snd_splash(int in) {
    if (in) { tone(1800, 300, 0.25f, 0.35f, 0, W_NOISE); tone(500, 200, 0.2f, 0.25f, 0, W_SINE); }
    else { tone(300, 1800, 0.2f, 0.3f, 0, W_NOISE); tone(250, 600, 0.15f, 0.2f, 0, W_SINE); }
}
void snd_water_in(void) { snd_splash(1); }
void snd_water_out(void) { snd_splash(0); }
/* Under water: a bubble every two seconds; the closer the surface, the higher. */
static void snd_bubble(int depth) {
    float m = powf(2.0f, (6 - (depth > 12 ? 12 : depth)) / 12.0f);
    tone(400 * m, 900 * m, 0.07f, 0.22f, 0, W_SINE);
    tone_at(0.1f, 500 * m, 1100 * m, 0.06f, 0.16f, 0, W_SINE);
}
void snd_water_bubble(int depth) { snd_bubble(depth); }
/* Tiles of water above Kirby's body up to the surface (-1: no surface within 20). */
static int water_depth(float x, float y) {
    int cx = (int)floorf(x), cy = (int)floorf(y + 0.5f);
    for (int i = 0; i < 20; i++) if (!land_is_water(cx, cy + i)) return i;
    return -1;
}

/* ---------------- state ---------------- */
static DWORD s_next_carry, s_next_gate, s_next_block, s_next_bubble, s_next_switch;
static int s_sw_state[16]; static uint32_t s_sw_addr[16], s_told_sw[4]; static int s_told_sw_i;
static uint32_t s_held, s_held_kind = 99;          /* what Kirby is carrying */
static uint32_t s_told_gate, s_told_star, s_told_carry;
static DWORD s_told_star_t, s_told_carry_t;
static int s_gate_state[8]; static uint32_t s_gate_addr[8];
static int s_in_water = -1;

void objects_reset(void) {
    s_held = 0; s_held_kind = 99; s_told_gate = s_told_star = s_told_carry = 0; s_in_water = -1;
    memset(s_gate_addr, 0, sizeof s_gate_addr);
    memset(s_sw_addr, 0, sizeof s_sw_addr); memset(s_told_sw, 0, sizeof s_told_sw);
}

void objects_poll(uint32_t comp, float px, float py, DWORD now) {
    /* ---- water ---- */
    /* bobbing at the surface flips this back and forth: it must hold for a moment */
    int wet = land_is_water((int)floorf(px), (int)floorf(py + 0.5f));
    static int s_wet_pending = -1; static DWORD s_wet_since;
    if (s_in_water < 0) s_in_water = wet;
    if (wet != s_in_water) {
        if (wet != s_wet_pending) { s_wet_pending = wet; s_wet_since = now; }
        else if (now - s_wet_since > 250) {
            s_in_water = wet; s_wet_pending = -1;
            snd_splash(wet);
            speak(wet ? L"In water" : L"Out of water", 0);
            CUE("cue water %s", wet ? "in" : "out");
        }
    } else s_wet_pending = -1;
    if (wet && (int)(now - s_next_bubble) >= 0) {
        int d = water_depth(px, py);
        snd_bubble(d < 0 ? 20 : d);
        s_next_bubble = now + 2000;
    }

    /* ---- carried items ---- */
    Carry cs[16]; int nc = read_carry(comp, cs, 16);
    uint32_t held = 0, held_kind = 99;
    for (int i = 0; i < nc; i++)
        if (cs[i].user && fabsf(cs[i].x - px) < 2.5f && fabsf(cs[i].y - py) < 3) { held = cs[i].addr; held_kind = cs[i].kind; }
    if (held != s_held) {
        if (held) {
            char where[80] = "";
            if (held_kind == 1) {   /* where the gate is */
                Gate gs[8]; int ng = read_gates(comp, gs, 8); float bd = 1e9f; int bi = -1;
                for (int i = 0; i < ng; i++) if (gs[i].state == 0) { float d = fabsf(gs[i].x - px) + fabsf(gs[i].y - py); if (d < bd) { bd = d; bi = i; } }
                if (bi >= 0) { char w2[64]; speak_dir(w2, sizeof w2, gs[bi].x - px, gs[bi].y - (py + 0.5f)); snprintf(where, sizeof where, " The locked gate is %s.", w2); }
            }
            speakf(1, "Carrying %s.%s %s", k_carry_names[held_kind], where, k_carry_desc[held_kind]);
        }
        else if (s_held) {
            int still = 0;
            for (int i = 0; i < nc; i++) if (cs[i].addr == s_held) still = 1;
            if (still) {
                for (int i = 0; i < nc; i++) if (cs[i].addr == s_held) {
                    char w2[64]; speak_dir(w2, sizeof w2, cs[i].x - px, cs[i].y - py);
                    speakf(0, "Dropped %s, %s", k_carry_names[s_held_kind], w2);
                }
            }
            else if (s_held_kind == 1) {
                /* a key that is gone: used on a gate (said below) or broken */
                int opened = 0; Gate gs[8]; int ng = read_gates(comp, gs, 8);
                for (int i = 0; i < ng; i++) if (gs[i].state != 0 && fabsf(gs[i].x - px) < 4) opened = 1;
                if (!opened) speak(L"The key is gone", 0);
            }
        }
        s_held = held; s_held_kind = held_kind;
    }
    /* one lying within reach: name it once */
    for (int i = 0; i < nc; i++) {
        if (cs[i].user || cs[i].kind == 0) continue;
        if (fabsf(cs[i].x - px) < 1.2f && fabsf(cs[i].y - py) < 1.5f && (cs[i].addr != s_told_carry || now - s_told_carry_t > 8000)) {
            s_told_carry = cs[i].addr; s_told_carry_t = now;
            speakf(0, "%s here, press X to pick it up", k_carry_names[cs[i].kind]);
        }
    }
    int any_locked = 0;
    { Gate g2[8]; int n2 = read_gates(comp, g2, 8); for (int i = 0; i < n2; i++) if (g2[i].state == 0) any_locked = 1; }
    if (g_radar_on && (int)(now - s_next_carry) >= 0) {
        for (int i = 0; i < nc; i++) {
            if (cs[i].addr == held || cs[i].kind == 0) continue;
            float dx = cs[i].x - px, dy = cs[i].y - py, d = fabsf(dx) + fabsf(dy);
            /* a key is worth hearing from further away while a locked gate is waiting */
            int wide = cs[i].kind == 1 && any_locked;
            if (fabsf(dx) > (wide ? 40 : 14) || fabsf(dy) > (wide ? 14 : 9)) continue;
            float gain = 0.4f * (1.0f - d / 24.0f); if (gain < 0.08f) gain = 0.08f;
            snd_carry(cs[i].kind, dy > 12 ? 12 : dy < -12 ? -12 : dy, gain, dx / 9.0f);
            CUE("cue carry %s dx %.1f dy %.1f", k_carry_names[cs[i].kind], dx, dy);
        }
        s_next_carry = now + 1400;
    }

    /* ---- locked gates ---- */
    Gate gs[8]; int ng = read_gates(comp, gs, 8);
    for (int i = 0; i < ng; i++) {
        int slot = -1;
        for (int j = 0; j < 8; j++) if (s_gate_addr[j] == gs[i].addr) slot = j;
        if (slot < 0) for (int j = 0; j < 8; j++) if (!s_gate_addr[j]) { slot = j; s_gate_addr[j] = gs[i].addr; s_gate_state[j] = gs[i].state; break; }
        if (slot >= 0) {
            if (s_gate_state[slot] == 0 && gs[i].state != 0) speak(L"Gate unlocked!", 1);
            s_gate_state[slot] = gs[i].state;
        }
        if (gs[i].state != 0) continue;
        float dx = gs[i].x - px, dy = gs[i].y - (py + 0.5f);
        if (fabsf(dx) < 3 && fabsf(dy) < 3 && s_told_gate != gs[i].addr) {
            s_told_gate = gs[i].addr;
            speak(held_kind == 1 ? L"Locked gate. Touch it with the key" : L"Locked gate. It needs a key", 0);
        }
    }
    if (g_radar_on && (int)(now - s_next_gate) >= 0) {
        for (int i = 0; i < ng; i++) {
            if (gs[i].state != 0) continue;
            float dx = gs[i].x - px, dy = gs[i].y - (py + 0.5f);
            if (fabsf(dx) > (held_kind == 1 ? 30 : 12) || fabsf(dy) > 12) continue;
            float d = fabsf(dx) + fabsf(dy), gain = 0.42f * (1.0f - d / 40.0f); if (gain < 0.1f) gain = 0.1f;
            snd_gate(dy > 12 ? 12 : dy < -12 ? -12 : dy, gain, dx / 10.0f);
            CUE("cue gate dx %.1f dy %.1f", dx, dy);
        }
        s_next_gate = now + (held_kind == 1 ? 1100 : 1800);
    }

    /* ---- switches ---- */
    Switch sw[16]; int nsw = read_switches(comp, sw, 16);
    for (int i = 0; i < nsw; i++) {
        int slot = -1;
        for (int j = 0; j < 16; j++) if (s_sw_addr[j] == sw[i].addr) slot = j;
        if (slot < 0) for (int j = 0; j < 16; j++) if (!s_sw_addr[j]) { slot = j; s_sw_addr[j] = sw[i].addr; s_sw_state[j] = sw[i].state; break; }
        float dx = sw[i].x - px, dy = sw[i].y - py;
        if (slot >= 0) {
            int was_on = s_sw_state[slot] == 1 || s_sw_state[slot] == 2, on = sw[i].state == 1 || sw[i].state == 2;
            if (on && !was_on) { snd_switch_on(dx / 10.0f); speak(L"Switch on!", 1); CUE("cue switch on"); }
            else if (!on && was_on) speak(L"Switch off", 0);
            s_sw_state[slot] = sw[i].state;
        }
        if (sw[i].state != 0) continue;
        int told = 0;   /* each switch named once (two in range used to take turns, every frame) */
        for (int j = 0; j < 4; j++) if (s_told_sw[j] == sw[i].addr) told = 1;
        if (fabsf(dx) < 7 && fabsf(dy) < 7 && !told && !g_riding) {
            s_told_sw[s_told_sw_i++ & 3] = sw[i].addr;
            char where[64]; speak_dir(where, sizeof where, dx, dy);
            speakf(0, "Switch, %s. Hit it with an attack.%s", where, switch_cover(comp, &sw[i]));
        }
    }
    if (g_radar_on && (int)(now - s_next_switch) >= 0) {
        for (int i = 0; i < nsw; i++) {
            if (sw[i].state != 0) continue;
            float dx = sw[i].x - px, dy = sw[i].y - py;
            if (fabsf(dx) > 30 || fabsf(dy) > 16) continue;   /* heard through walls and blocks too: hidden switches count */
            float d = fabsf(dx) + fabsf(dy), gain = 0.5f * (1.0f - d / 50.0f); if (gain < 0.16f) gain = 0.16f;
            snd_switch_off(dy > 12 ? 12 : dy < -12 ? -12 : dy, gain, dx / 10.0f);
            CUE("cue switch dx %.1f dy %.1f", dx, dy);
        }
        float nd = 99;
        for (int i = 0; i < nsw; i++) if (sw[i].state == 0) nd = fminf(nd, fabsf(sw[i].x - px) + fabsf(sw[i].y - py));
        s_next_switch = now + (nd < 8 ? 800 : 1200);
    }

    /* ---- blocks: the nearest of each kind ---- */
    if (g_radar_on && (int)(now - s_next_block) >= 0) {
        BlockNear bn[B_NCAT]; int nb = near_blocks(comp, px, py, 7, 5, bn), played = 0;
        for (int i = 0; i < nb && played < 3; i++) {
            float dx = bn[i].x - px, dy = bn[i].y - (py + 0.5f);
            float gain = 0.32f * (1.0f - bn[i].d / 16.0f); if (gain < 0.07f) gain = 0.07f;
            snd_block_cat(bn[i].cat, played * 0.3f, dy, gain, dx / 8.0f);
            CUE("cue block %s dx %.1f dy %.1f", k_block_names[bn[i].cat], dx, dy);
            played++;
        }
        s_next_block = now + 1600;
    }

    /* ---- copy ability stars: name the ability when Kirby is next to one ---- */
    uint32_t im = r32(comp + 0xcc);
    if (is_ptr(im)) {
        uint32_t n = r32(im + 0x94);
        for (uint32_t i = 0; i < n && i < 32; i++) {
            uint32_t it = r32(im + 0x98 + 4 * i);
            if (!is_ptr(it) || r32(it + 0x14) != 8) continue;
            uint32_t loc = r32(it + 0x48), ab = r32(it + 0x18);
            if (!is_ptr(loc) || ab == 0 || ab > 23) continue;
            float dx = rf32(loc) - px, dy = rf32(loc + 4) - py;
            if (fabsf(dx) < 1.5f && fabsf(dy) < 2 && (it != s_told_star || now - s_told_star_t > 6000)) {
                s_told_star = it; s_told_star_t = now;
                speakf(0, "%s ability star", ability_name(ab));
            }
        }
    }
}

/* Shift+F2 adds these. */
int objects_describe(uint32_t comp, float px, float py, char *buf, size_t cap) {
    size_t L = strlen(buf); int k = 0;
    Carry cs[16]; int nc = read_carry(comp, cs, 16);
    for (int i = 0; i < nc; i++) {
        if (cs[i].kind == 0) continue;
        char where[64]; speak_dir(where, sizeof where, cs[i].x - px, cs[i].y - py);
        L += snprintf(buf + L, cap - L, "%s%s%s, %s", L ? ". " : "", cs[i].user ? "carried " : "", k_carry_names[cs[i].kind], where);
        if (L >= cap) return k;
        k++;
    }
    Gate gs[8]; int ng = read_gates(comp, gs, 8);
    for (int i = 0; i < ng; i++) {
        if (gs[i].state != 0) continue;
        char where[64]; speak_dir(where, sizeof where, gs[i].x - px, gs[i].y - (py + 0.5f));
        L += snprintf(buf + L, cap - L, "%slocked gate, %s", L ? ". " : "", where);
        if (L >= cap) return k;
        k++;
    }
    Switch sw[16]; int nsw = read_switches(comp, sw, 16);
    for (int i = 0; i < nsw; i++) {
        char where[64]; speak_dir(where, sizeof where, sw[i].x - px, sw[i].y - py);
        L += snprintf(buf + L, cap - L, "%sswitch%s, %s", L ? ". " : "", sw[i].state ? " (on)" : "", where);
        if (L >= cap) return k;
        k++;
    }
    return k;
}

/* F5 adds the nearest blocks. */
int objects_blocks_describe(uint32_t comp, float px, float py, char *buf, size_t cap) {
    BlockNear bn[B_NCAT]; int nb = near_blocks(comp, px, py, 10, 6, bn);
    size_t L = strlen(buf);
    for (int i = 0; i < nb; i++) {
        char where[64]; speak_dir(where, sizeof where, bn[i].x - px, bn[i].y - (py + 0.5f));
        L += snprintf(buf + L, cap - L, "%s, %s. ", k_block_names[bn[i].cat], where);
        if (L >= cap) break;
    }
    if (s_in_water > 0) {
        int d = water_depth(px, py);
        if (d > 0) L += snprintf(buf + L, cap - L, "Under water, surface %d up. ", d);
        else if (d < 0) L += snprintf(buf + L, cap - L, "Deep under water. ");
    }
    return nb;
}

/* The ability of a copy ability star item, or 0. */
uint32_t item_ability(uint32_t item) { return r32(item + 0x14) == 8 ? r32(item + 0x18) : 0; }
