/*
 * Stage gimmicks the other cues don't cover, from two places:
 *
 *   - the dark wall of Another Dimension (gimmick Manager +0xb8 ->
 *     ScopedPtr, +4 darkwall::DarkWall*: +0xc variation 0 comes from the
 *     left, 2 from the right, 1/3 up/down; +0x10/+0x14 front position, from
 *     darkwall::Manager::getPos and gimmick::Util::mfIsInDarkWall): a low
 *     throbbing rumble from its side, louder as it closes in;
 *   - fixed things placed in the area's map file (files/map/step/levelN/
 *     stageM/StepNN.dat, gimmick section, see tools/stageaudit.py): hazards
 *     (icicles, crushers, lava geysers, spiked bars, rolling balls, falling
 *     pillars, attacking statues), things to work (hang switches, stakes,
 *     ropes, piñatas, crystal walls, shutters), currents, and Super Ability
 *     targets. Only where they start: moving ones are not followed.
 *
 * Hazards rattle and things to work chime when within reach, the nearest
 * of each kind; each is named once per area when Kirby comes near (the
 * description the first time in a session). Shift+F2 lists them.
 */
#include "access.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void speak_dir(char *buf, size_t n, float dx, float dy);
int area_place(uint32_t comp, int *lv, int *st, int *ar);
const wchar_t *msg_game_dir(void);
extern volatile int g_cue_log;
#define CUE(...) do { if (g_cue_log) log_line(__VA_ARGS__); } while (0)

enum { G_HAZARD, G_WORK, G_FLOW, G_SUPER, G_INFO };   /* G_INFO: named once when near, no repeating sound (it has its own elsewhere) */
static const struct { int kind, cat; const char *name, *desc; } k_kinds[] = {
    { 0x2e, G_HAZARD, "icicles", "Icicles hang from the ceiling here and drop when Kirby passes under them. Hurry past, or let them fall first." },
    { 0x3f, G_HAZARD, "crusher", "A press slams down here. Go under it right after it lifts." },
    { 0x44, G_HAZARD, "lava geyser", "Lava spouts up from the ground here now and then. Cross when it dies down." },
    { 0x2f, G_HAZARD, "spiked bar", "A bar covered in spikes moves here. Watch its timing and get past while it is away." },
    { 0x35, G_HAZARD, "rolling boulders", "Boulders roll through here. Jump or float over them, or keep out of their way." },
    { 0x45, G_HAZARD, "rolling ice balls", "Big balls of ice roll through here. Jump or float over them." },
    { 0x36, G_HAZARD, "rolling iron balls", "Heavy iron balls roll through here. Jump or float over them." },
    { 0x5b, G_HAZARD, "rolling lava balls", "Balls of lava roll through here. Jump or float over them." },
    { 0x01, G_HAZARD, "falling pillar", "A pillar topples over when Kirby comes close. Rush past or step back while it falls." },
    { 0x1c, G_HAZARD, "statue", "A statue here attacks when Kirby is near. Break it or slip past between attacks." },
    { 0x3c, G_HAZARD, "biting statue", "A statue that bites. Keep clear of its mouth, or break it." },
    { 0x3b, G_HAZARD, "moving ground", "Blocks of ground move here and can squash Kirby. Keep clear of the gaps they close." },
    { 0x20, G_WORK, "hang switch", "A handle hangs from above here. Jump up into it: Kirby grabs it and his weight pulls it down, which opens something nearby." },
    { 0x0e, G_WORK, "stake", "A stake sticks up from the ground. Pound it down with a heavy attack from above, like Stone or Hammer." },
    { 0x48, G_WORK, "big stake", "A big stake in the ground. It needs a strong pounding attack, like Hammer or Stone." },
    { 0x0a, G_WORK, "rope", "A rope holds something up here. Cut it or burn it, with Sword, Cutter or Fire, and what it holds drops down." },
    { 0x1d, G_WORK, "rope", "A rope holds something here. Cut it or burn it, and what it holds drops down." },
    { 0x47, G_WORK, "piñata", "A piñata. Hit it until it breaks open and treats fall out." },
    { 0x14, G_WORK, "crystal wall", "A crystal wall that only lets Kirby through while he carries the invisible stone." },
    { 0x1e, G_WORK, "shutter gate", "A gate that a switch nearby opens." },
    { 0x2d, G_FLOW, "water current", "A strong current pushes Kirby here." },
    { 0x2a, G_FLOW, "wind", "A strong wind blows here and pushes Kirby along." },
    { 0x27, G_SUPER, "giant stake", "A giant stake: pound it with a Super Ability." },
    { 0x28, G_SUPER, "giant screw", "A giant screw: turn it with a Super Ability." },
    { 0x30, G_SUPER, "huge wall", "A huge wall: smash it with a Super Ability." },
    { 0x33, G_SUPER, "huge bell wall", "A huge wall with a bell: smash it with a Super Ability." },
    { 0x37, G_SUPER, "totem pole", "A totem pole: smash it with a Super Ability." },
    { 0x39, G_SUPER, "sand castle", "A sand castle: smash it with a Super Ability." },
    { 0x4a, G_SUPER, "giant snowman", "A giant snowman: smash it with a Super Ability." },
    { 0x46, G_SUPER, "ice pins", "Giant bowling pins of ice: roll into them with Snow Bowl to knock them down." },
    { 0x57, G_SUPER, "rock pillar", "A rock pillar: smash it with a Super Ability." },
    { 0x58, G_SUPER, "ice volcano", "An ice volcano: smash it with a Super Ability." },
    { 0x55, G_SUPER, "volcano", "A volcano: smash it with a Super Ability." },
    { 0x23, G_SUPER, "super beam switch", "A big switch: hit it with a Super Ability." },
    { 0x4f, G_INFO, "fire orbs", "Orbs of fire float and swing around here. Each one ticks with a low buzz from where it is, faster as it gets close: wait for it to swing away, then go." },
    { 0x54, G_WORK, "Key Dee", "A Waddle Dee carrying a key. Beat it, or follow it, to get the key, then carry the key to the locked gate." },
    { 0x4d, G_WORK, "power stake", "A stake that sends power along a wire. Pound it down with Grand Hammer to connect the circuit." },
    { 0x50, G_WORK, "burner box", "A burner box. Knock it down; knocking down every one in the room opens the way." },
    { 0x53, G_HAZARD, "fire machine arm", "An arm of a fire-breathing machine. Keep clear of its flames." },
    { 0x1f, G_INFO, "light blocks", "Blocks here only become solid near light, like the candle's glow. Carry the candle close before you step onto them." },
};
#define NKINDS ((int)(sizeof k_kinds / sizeof *k_kinds))

typedef struct { float x, y; int ki; } Gmk;
static Gmk s_g[96];
static int s_n, s_loaded = -1;          /* map file loaded for this place id */
static uint8_t s_told[96], s_desc_told[NKINDS];
static DWORD s_next_hazard, s_next_work, s_next_flow;
static int s_dark_told, s_flow_in = -1;

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static void load(int lv, int st, int ar) {
    s_n = 0;
    const wchar_t *g = msg_game_dir();
    if (!g || !g[0]) return;
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls/files/map/step/level%d/stage%d/Step%02d.dat", g, lv + 1, st, ar);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { log_line("gimmicks: no map file L%d s%d a%d", lv + 1, st, ar); return; }
    DWORD size = GetFileSize(h, NULL), got = 0;
    uint8_t *b = size < (8u << 20) ? malloc(size) : NULL;
    if (b && ReadFile(h, b, size, &got, NULL) && got == size && size > 0x40) {
        uint32_t gs = be32(b + 0x30), arr = gs + 8 <= size ? be32(b + gs + 4) : 0;
        uint32_t n = arr + 4 <= size ? be32(b + arr) : 0;
        for (uint32_t i = 0; i < n && s_n < 96; i++) {
            const uint8_t *e = b + arr + 4 + i * 0x40;
            if (e + 0x40 > b + size) break;
            int kind = (int)be32(e);
            for (int k = 0; k < NKINDS; k++)
                if (k_kinds[k].kind == kind) {
                    s_g[s_n].x = (int32_t)be32(e + 4) / 16.0f; s_g[s_n].y = (int32_t)be32(e + 8) / 16.0f; s_g[s_n].ki = k;
                    s_n++; break;
                }
        }
        log_line("gimmicks: L%d s%d a%d: %d placed", lv + 1, st, ar, s_n);
    }
    free(b);
    CloseHandle(h);
}

void gimmicks_reset(void) {
    s_loaded = -1; s_n = 0; memset(s_told, 0, sizeof s_told); s_dark_told = 0; s_flow_in = -1;
    hum_set(6, 80, 0, 0, W_SAW);
}

/* hazard: a dry rattle (fast pulses of a low square), pitch = height */
void snd_hazard(float dy, float gain, float pan) {
    float f = 233 * powf(2.0f, (dy > 12 ? 12 : dy < -12 ? -12 : dy) / 12.0f);
    for (int i = 0; i < 4; i++) tone_at(i * 0.035f, f, f * 0.94f, 0.022f, gain, pan, W_SQUARE);
    tone_at(0.0f, 900, 500, 0.12f, gain * 0.5f, pan, W_NOISE);
}
/* falling pillar: a heavy stone thud with rubble clattering after it, pitch = height */
void snd_pillar(float dy, float gain, float pan) {
    float m = powf(2.0f, (dy > 12 ? 12 : dy < -12 ? -12 : dy) / 12.0f);
    tone(200 * m, 100 * m, 0.18f, gain * 0.8f, pan, W_WARM);           /* the block of stone landing */
    tone(330 * m, 160 * m, 0.04f, gain * 0.45f, pan, W_SQUARE);       /* its hard edge */
    tone(1500, 400, 0.06f, gain * 0.35f, pan, W_NOISE);               /* the crack */
    static const float gf[7] = { 620, 410, 530, 300, 460, 350, 260 }, gt[7] = { 0.06f, 0.09f, 0.12f, 0.16f, 0.19f, 0.24f, 0.29f };
    for (int i = 0; i < 7; i++)                                       /* stones skittering, fading */
        tone_at(gt[i], gf[i] * m, gf[i] * 0.8f * m, 0.02f, gain * (0.42f - i * 0.045f), pan, W_SQUARE);
}
/* something to work: a bright two-note pluck, up a fourth */
void snd_work(float dy, float gain, float pan) {
    float f = 880 * powf(2.0f, (dy > 12 ? 12 : dy < -12 ? -12 : dy) / 12.0f);
    tone(f, f, 0.05f, gain, pan, W_TRI);
    tone_at(0.07f, f * 1.335f, f * 1.335f, 0.07f, gain * 0.9f, pan, W_TRI);
}

static int dark_wall(uint32_t comp, float *front, int *dir) {
    uint32_t gm = r32(comp + 0xb4), sp = is_ptr(gm) ? r32(gm + 0xb8) : 0, dw = is_ptr(sp) ? r32(sp + 4) : 0;
    if (!is_ptr(dw)) return 0;
    uint32_t var = r32(dw + 0xc);
    if (var == 0 || var == 2) { *front = rf32(dw + 0x10); *dir = var == 0 ? -1 : 1; return 1; }
    if (var == 1 || var == 3) { *front = rf32(dw + 0x14); *dir = var == 1 ? -2 : 2; return 1; }
    return 0;
}

void gimmicks_poll(uint32_t comp, float px, float py, int ability, DWORD now) {
    /* ---- Another Dimension's dark wall ---- */
    float front; int dir;
    if (dark_wall(comp, &front, &dir) && isfinite(front)) {
        float d = dir == -1 ? px - front : dir == 1 ? front - px : dir == -2 ? py - front : front - py;
        if (d < 0) d = 0;
        float c = d > 16 ? 0 : 1 - d / 16;
        float pan = dir == -1 ? -0.8f : dir == 1 ? 0.8f : 0;
        hum_set_ex(6, 70 + 40 * c, 0.08f + 0.42f * c * c, pan, W_SAW, 2.0f + 4.0f * c, 0.6f, 0, 260 + 300 * c);
        if (!s_dark_told && d < 30) {
            s_dark_told = 1;
            speak_utf8(dir == -1 ? "The darkness is coming from the left. Keep moving right!" :
                       dir == 1 ? "The darkness is coming from the right. Keep moving left!" :
                       dir == -2 ? "The darkness is rising from below. Keep going up!" : "The darkness is coming down from above. Keep going down!", 0);
        }
        if (now % 2000 < 20) CUE("cue dark wall front %.1f dist %.1f", front, d);
    } else hum_set(6, 80, 0, 0, W_SAW);

    /* ---- placed gimmicks ---- */
    int lv, st, ar;
    if (!area_place(comp, &lv, &st, &ar)) { s_n = 0; return; }
    int id = lv * 1000 + st * 50 + ar;
    if (id != s_loaded) { s_loaded = id; load(lv, st, ar); memset(s_told, 0, sizeof s_told); }
    int super = ability >= 25 && ability <= 29;
    int bh = -1, bw = -1; float dh = 1e9f, dwk = 1e9f;
    int flow = 0;
    for (int i = 0; i < s_n; i++) {
        float dx = s_g[i].x - px, dy = s_g[i].y - py, dist = fabsf(dx) + fabsf(dy) * 0.7f;
        int cat = k_kinds[s_g[i].ki].cat;
        if (cat == G_SUPER && !super) continue;
        if (cat == G_HAZARD && fabsf(dx) < 9 && fabsf(dy) < 7 && dist < dh) { dh = dist; bh = i; }
        if ((cat == G_WORK || cat == G_SUPER) && fabsf(dx) < 10 && fabsf(dy) < 7 && dist < dwk) { dwk = dist; bw = i; }
        if (cat == G_FLOW && fabsf(dx) < 3 && fabsf(dy) < 3) flow = i + 1;
        /* named once per area when close */
        if (!s_told[i] && cat != G_FLOW && fabsf(dx) < (cat == G_HAZARD ? 6 : 4) && fabsf(dy) < 5) {
            s_told[i] = 1;
            for (int j = 0; j < s_n; j++) if (s_g[j].ki == s_g[i].ki && fabsf(s_g[j].x - s_g[i].x) < 8 && fabsf(s_g[j].y - s_g[i].y) < 6) s_told[j] = 1;
            char where[64], buf[400]; speak_dir(where, sizeof where, dx, dy);
            int ki = s_g[i].ki;
            if (!s_desc_told[ki]) { s_desc_told[ki] = 1; snprintf(buf, sizeof buf, "%s, %s. %s", k_kinds[ki].name, where, k_kinds[ki].desc); }
            else snprintf(buf, sizeof buf, "%s, %s", k_kinds[ki].name, where);
            buf[0] = (char)(buf[0] >= 'a' && buf[0] <= 'z' ? buf[0] - 32 : buf[0]);
            CUE("cue gimmick told %s at %.1f %.1f", k_kinds[ki].name, s_g[i].x, s_g[i].y);
            speak_utf8(buf, 0);
        }
    }
    if (bh >= 0 && (int)(now - s_next_hazard) >= 0) {
        float dx = s_g[bh].x - px, dy = s_g[bh].y - py, c = 1 - fminf(fabsf(dx), 9) / 9;
        float pn = fmaxf(-0.8f, fminf(0.8f, dx / 5));
        if (k_kinds[s_g[bh].ki].kind == 0x01) snd_pillar(dy, 0.2f + 0.2f * c, pn);
        else snd_hazard(dy, 0.18f + 0.2f * c, pn);
        CUE("cue hazard %s dx %.1f dy %.1f", k_kinds[s_g[bh].ki].name, dx, dy);
        s_next_hazard = now + 900 + (DWORD)(fabsf(dx) * 110);
    }
    if (bw >= 0 && (int)(now - s_next_work) >= 0) {
        float dx = s_g[bw].x - px, dy = s_g[bw].y - py, c = 1 - fminf(fabsf(dx), 10) / 10;
        snd_work(dy, 0.10f + 0.14f * c, fmaxf(-0.8f, fminf(0.8f, dx / 5)));
        CUE("cue work %s dx %.1f dy %.1f", k_kinds[s_g[bw].ki].name, dx, dy);
        s_next_work = now + 1300 + (DWORD)(fabsf(dx) * 120);
    }
    if (flow != s_flow_in) {
        if (flow && (int)(now - s_next_flow) >= 0) {
            int ki = s_g[flow - 1].ki;
            if (!s_desc_told[ki]) { s_desc_told[ki] = 1; speak_utf8(k_kinds[ki].desc, 0); }
            else { char b[64]; snprintf(b, sizeof b, "%c%s", k_kinds[ki].name[0] - 32, k_kinds[ki].name + 1); speak_utf8(b, 0); }
            s_next_flow = now + 6000;
        }
        s_flow_in = flow;
    }
}

/* Tile camera: a placed hazard or thing to work within a tile of cell x,y
 * (the map gives one point per object; most are 2-3 tiles big). Super
 * Ability things only while Kirby has one, as everywhere else. */
int gimmicks_cam_at(int x, int y, int ability, int play, float semis, float pan, char *name, size_t cap) {
    int super = ability >= 25 && ability <= 29, bi = -1; float bd = 9;
    for (int i = 0; i < s_n; i++) {
        int cat = k_kinds[s_g[i].ki].cat;
        if (cat == G_SUPER && !super) continue;
        float dx = fabsf(s_g[i].x - (x + 0.5f)), dy = fabsf(s_g[i].y - (y + 0.5f));
        if (dx < 1.5f && dy < 1.5f && dx + dy < bd) { bd = dx + dy; bi = i; }
    }
    if (bi < 0) return 0;
    int cat = k_kinds[s_g[bi].ki].cat;
    if (play) {
        if (k_kinds[s_g[bi].ki].kind == 0x01) snd_pillar(semis, 0.4f, pan);
        else if (cat == G_HAZARD) snd_hazard(semis, 0.38f, pan);
        else if (cat == G_INFO) { }   /* fire orbs have their own ticks */
        else if (cat == G_FLOW) tone(300, 1200, 0.25f, 0.12f, pan, W_NOISE);
        else snd_work(semis, 0.24f, pan);
    }
    snprintf(name, cap, "%s", k_kinds[s_g[bi].ki].name);
    return 1;
}

/* Shift+F2: the placed gimmicks in this area, nearest first (up to 5). */
int gimmicks_describe(float px, float py, int ability, char *buf, size_t cap) {
    int used[96] = { 0 }, k = 0, super = ability >= 25 && ability <= 29;
    for (int pass = 0; pass < 5; pass++) {
        int bi = -1; float bd = 1e9f;
        for (int i = 0; i < s_n; i++) {
            if (used[i] || (k_kinds[s_g[i].ki].cat == G_SUPER && !super)) continue;
            float d = fabsf(s_g[i].x - px) + fabsf(s_g[i].y - py);
            if (d < bd && d < 40) { bd = d; bi = i; }
        }
        if (bi < 0) break;
        for (int j = 0; j < s_n; j++) if (s_g[j].ki == s_g[bi].ki && fabsf(s_g[j].x - s_g[bi].x) < 8 && fabsf(s_g[j].y - s_g[bi].y) < 6) used[j] = 1;
        char where[64]; speak_dir(where, sizeof where, s_g[bi].x - px, s_g[bi].y - py);
        size_t L = strlen(buf);
        snprintf(buf + L, cap - L, "%s%s, %s", L ? ". " : "", k_kinds[s_g[bi].ki].name, where);
        k++;
    }
    return k;
}
