
/* ---- tile camera (after BK3's camera): move a look point tile by tile
 * around Kirby and hear what each tile is. Right stick on a controller
 * (a push = one tile, held = repeats; the left stick too while Kirby is in
 * the air, so he can float and look around), or I J K L on the keyboard. The
 * point follows Kirby; it goes back to him with R3 / O, or by itself after
 * 8 seconds untouched. L3 / U says the tile in words while it is away.
 *
 * Channels: timbre = what the tile is (the same family as the mod's other
 * cues: wall knock = solid, wood knock = thin platform, metal clink =
 * ladder, buzz = spikes, the block sounds), ear = which side of Kirby the
 * tile is, pitch = one semitone per tile above Kirby (lower below).
 * Included at the end of play.c. ---- */
int objects_block_cat_at(uint32_t comp, int x, int y);
const char *objects_block_name(int cat);
void objects_block_sound(int cat, float delay, float semis, float gain, float pan);
void pad_right_stick(int *dx, int *dy);
void pad_left_stick(int *dx, int *dy);
int objects_cam_at(uint32_t comp, int x, int y, int play, float semis, float pan, char *name, size_t cap);
int gimmicks_cam_at(int x, int y, int ability, int play, float semis, float pan, char *name, size_t cap);

static int s_cam_ox, s_cam_oy, s_cam_sx, s_cam_sy;
static DWORD s_cam_last, s_cam_next;
#define CAM_IDLE_MS 8000
#define CAM_RANGE 40

static void cam_reset(void) { s_cam_ox = s_cam_oy = 0; }
int kirby_in_air(void) { return s_have && s_air; }
int camera_away(void) { return s_have && (s_cam_ox || s_cam_oy); }
static void cam_kirby_cell(int *x, int *y) { *x = (int)floorf(s_x); *y = (int)floorf(s_y + 0.1f); }

static void wood_knock_p(float delay, float f, float gain, float pan) {
    tone_at(delay, f, f * 0.82f, 0.045f, gain, pan, W_SINE);
    tone_at(delay, f * 2.76f, f * 2.3f, 0.025f, gain * 0.3f, pan, W_SINE);
    tone_at(delay, 3200, 1600, 0.008f, gain * 0.2f, pan, W_NOISE);
}

/* What the tile is, for both sound and words. */
enum { CT_AIR, CT_WATER, CT_SOLID, CT_SLOPE_UP, CT_SLOPE_DOWN, CT_CEIL_SLOPE, CT_PLATFORM, CT_LADDER, CT_SPIKES, CT_BLOCK, CT_GATE, CT_EDGE, CT_N };
static const char *const k_cam_names[CT_N] = { "open", "water", "solid", "slope up to the right", "slope down to the right", "solid, sloped underneath",
    "thin platform", "ladder", "spikes", "block", "locked gate", "edge of the map" };

static int cam_kind(int x, int y, int *cat) {
    *cat = 0;
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return CT_EDGE;
    int i = x + y * GW, s = g_shape[i];
    if (g_block[i] == 2) return CT_GATE;
    if (g_block[i] == 1) { *cat = s_have ? objects_block_cat_at(s_comp, x, y) : 0; return CT_BLOCK; }
    if (is_spike(x, y)) return CT_SPIKES;
    if (is_ladder(x, y)) return CT_LADDER;
    if (s == 1) return CT_SOLID;
    if (s >= 14 && s <= 25) return CT_CEIL_SLOPE;
    if (is_slope(s)) return k_top_r[s] > k_top_l[s] ? CT_SLOPE_UP : CT_SLOPE_DOWN;
    if (is_platform(s)) return CT_PLATFORM;
    return is_water(x, y) ? CT_WATER : CT_AIR;
}

/* The tile's own sound; semis = pitch step, pan = side. Also used by Learn Sounds. */
static void cam_kind_sound(int k, int cat, float st, float pan) {
    float m = semi(st);
    switch (k) {
    case CT_AIR:        /* open: a soft high tick (BK3: empty is high, ground is low) */
        tone(1100 * m, 1100 * m, 0.03f, 0.10f, pan, W_TRI); break;
    case CT_WATER:      /* open water: a bubble */
        tone(450 * m, 900 * m, 0.06f, 0.20f, pan, W_SINE); tone_at(0.07f, 600 * m, 1200 * m, 0.05f, 0.14f, pan, W_SINE); break;
    case CT_SOLID: case CT_CEIL_SLOPE:   /* solid: the low wall knock */
        tone(150 * m, 110 * m, 0.07f, 0.5f, pan, W_SQUARE); break;
    case CT_SLOPE_UP: case CT_SLOPE_DOWN:   /* the knock, then a glide the way the floor climbs (rising = up to the right) */
        tone(150 * m, 110 * m, 0.05f, 0.4f, pan, W_SQUARE);
        tone_at(0.06f, (k == CT_SLOPE_UP ? 400 : 600) * m, (k == CT_SLOPE_UP ? 600 : 400) * m, 0.09f, 0.22f, pan, W_TRI); break;
    case CT_PLATFORM:   /* thin platform: the wood knock */
        wood_knock_p(0, 560 * m, 0.38f, pan); break;
    case CT_LADDER:
        rung_clink(0, 784 * m, 0.24f, pan); break;
    case CT_SPIKES:
        tone(220 * m, 220 * m, 0.12f, 0.30f, pan, W_SAW); break;
    case CT_BLOCK:
        if (cat) objects_block_sound(cat, 0, st, 0.4f, pan);
        else { tone(150 * m, 110 * m, 0.05f, 0.4f, pan, W_SQUARE); tone_at(0.07f, 1000 * m, 850 * m, 0.035f, 0.3f, pan, W_TRI); }
        break;
    case CT_GATE: snd_locked_gate(pan); break;
    case CT_EDGE:       /* past the edge of the map: a dull low thump */
        tone(90, 60, 0.12f, 0.45f, pan, W_SINE); tone(400, 200, 0.05f, 0.12f, pan, W_NOISE); break;
    }
}
void snd_camera_demo(int k) { if (k >= 0 && k < CT_N) cam_kind_sound(k, 0, 0, 0); else tone(880, 1320, 0.07f, 0.16f, 0, W_SINE); }

static int door_on(const Door *d, int x, int y) { int dy = (int)floorf(d->y + 0.01f); return (int)floorf(d->x) == x && y >= dy && y <= dy + 1; }
static int thing_on(float tx, float ty, int x, int y) { return (int)floorf(tx) == x && (int)floorf(ty + 0.3f) == y; }

/* Everything on the tile, in this order: door, item, enemy, switch or thing to
 * carry, rift or cannon, hazard or thing to work. With buf == NULL their own
 * sounds play one after another from delay d; otherwise their names are added
 * to buf (", name"). */
static void cam_add(char *buf, size_t cap, const char *name) { size_t L = strlen(buf); snprintf(buf + L, cap - L, ", %s", name); }
static void cam_things(int x, int y, float st, float pan, float d, char *buf, size_t cap) {
    int play = buf == NULL;
    float m = semi(st);
    char nm[200];
    Door dr[20]; int nd = read_doors(s_comp, dr, 20);
    for (int i = 0; i < nd; i++)
        if (door_on(&dr[i], x, y)) {
            if (play) {
                float f = (dr[i].role == 0 || dr[i].role == 1) ? 1319 : 988;
                tone_at(d, f * m, f * m, 0.25f, 0.16f, pan, W_SINE); tone_at(d + 0.12f, f * 1.26f * m, f * 1.26f * m, 0.25f, 0.1f, pan, W_SINE);
                d += 0.3f;
            } else cam_add(buf, cap, dr[i].label ? dr[i].label : "door");
            break;
        }
    ItemPos it[32]; int ni = read_items(s_comp, it, 32);
    for (int i = 0; i < ni; i++)
        if (thing_on(it[i].x, it[i].y, x, y)) {
            if (play) { item_sound(it[i].kind, d, st, 0.3f, pan); d += 0.18f; } else cam_add(buf, cap, k_item_names[it[i].kind]);
            break;
        }
    Enemy en[20]; int ne = read_enemies(s_comp, en, 20);
    for (int i = 0; i < ne; i++)
        if (thing_on(en[i].x, en[i].y, x, y)) {
            if (play) { play_enemy(en[i].kind, d, st, 0.3f, pan); d += 0.2f; } else cam_add(buf, cap, enemy_name(en[i].kind));
            break;
        }
    if (play) tone_offset(d);
    if (objects_cam_at(s_comp, x, y, play, st, pan, nm, sizeof nm)) { if (play) { d += 0.22f; tone_offset(d); } else cam_add(buf, cap, nm); }
    int super = s_ability >= 25 && s_ability <= 29;
    Rift r;
    if (read_rift(s_comp, &r) && fabsf(r.x - (x + 0.5f)) < 1.5f && fabsf(r.y - (y + 0.5f)) < 1.5f) {
        int open = rift_open(&r), hidden = !open && r.state == 0 && super && !r.entered;
        if (open || hidden) {
            if (play) { snd_rift(st, 0.35f, pan, hidden); d += 0.45f; tone_offset(d); }
            else cam_add(buf, cap, hidden ? "hidden rift, smash the scenery here with your Super Ability" : r.out ? "rift, the way out, jump into it" : "rift to Another Dimension, jump into it");
        }
    }
    Cannon c[36]; int nc = read_cannons(s_comp, c, 36);
    for (int i = 0; i < nc; i++)
        if (fabsf(c[i].x - (x + 0.5f)) < 1.0f && fabsf(c[i].y - (y + 0.5f)) < 1.2f) {
            if (play) { if (c[i].fuse) snd_cannon(st, 0.35f, pan); else snd_barrel(st, 0.35f, pan); d += 0.2f; tone_offset(d); }
            else cam_add(buf, cap, c[i].fuse ? "fuse cannon, light the fuse with fire and get in" : c[i].manual ? "barrel cannon, jump in, then press a button to fire" : "barrel cannon, jump in and it fires you");
            break;
        }
    if (gimmicks_cam_at(x, y, s_ability, play, st, pan, nm, sizeof nm) && !play) cam_add(buf, cap, nm);
    if (play) tone_offset(0);
}

static void cam_sound(int x, int y, int dx, int dy) {
    float st = (float)(dy > 12 ? 12 : dy < -12 ? -12 : dy);
    float pan = dx / 8.0f; if (pan > 1) pan = 1; if (pan < -1) pan = -1; pan *= 0.8f;
    int cat, k = cam_kind(x, y, &cat);
    cam_kind_sound(k, cat, st, pan);
    float m = semi(st), d = 0.13f;
    if (k != CT_AIR && k != CT_WATER && k != CT_EDGE && is_water(x, y)) { tone_at(d, 450 * m, 900 * m, 0.05f, 0.14f, pan, W_SINE); d += 0.08f; }
    if (!dx && !dy) { tone_at(d, 880, 1320, 0.07f, 0.16f, 0, W_SINE); d += 0.09f; }   /* back on Kirby */
    cam_things(x, y, st, pan, d, NULL, 0);
}

static void cam_move(int mx, int my) {
    if (!s_have) return;
    int kx, ky; cam_kirby_cell(&kx, &ky);
    int ox = s_cam_ox + mx, oy = s_cam_oy + my, x = kx + ox, y = ky + oy;
    if (abs(ox) > CAM_RANGE || abs(oy) > CAM_RANGE || x < -1 || y < -1 || x > g_w || y > g_h) { snd_knock(0); return; }
    s_cam_ox = ox; s_cam_oy = oy; s_cam_last = GetTickCount();
    cam_sound(x, y, ox, oy);
    int cat;
    CUE("cue camera %d %d (%+d %+d) %s", x, y, ox, oy, k_cam_names[cam_kind(x, y, &cat)]);
}

void camera_home(void) {
    if (!camera_away()) return;
    cam_reset();
    tone(1320, 880, 0.08f, 0.14f, 0, W_SINE);
}

void camera_say(void) {
    if (!s_have) return;
    int kx, ky; cam_kirby_cell(&kx, &ky);
    int x = kx + s_cam_ox, y = ky + s_cam_oy, cat, k = cam_kind(x, y, &cat);
    char buf[700];
    int L, home = !s_cam_ox && !s_cam_oy;
    if (home) L = snprintf(buf, sizeof buf, "Kirby%s", k == CT_WATER ? ", in water" : "");
    else {
        char where[64]; speak_dir(where, sizeof where, (float)s_cam_ox, (float)s_cam_oy);
        L = snprintf(buf, sizeof buf, "%s, %s", where, k == CT_BLOCK && cat ? objects_block_name(cat) : k_cam_names[k]);
    }
    if (k != CT_AIR && k != CT_WATER && k != CT_EDGE && is_water(x, y)) L += snprintf(buf + L, sizeof buf - L, ", under water");
    if (!home && (k == CT_AIR || k == CT_WATER)) {   /* how far down to something to stand on (BK3: checking a drop with the camera) */
        Ground g = ground_below(x + 0.5f, (float)y);
        if (g.pit) L += snprintf(buf + L, sizeof buf - L, ", nothing below");
        else {
            int dn = (int)lroundf((float)y - g.h);
            const char *what = g.spike ? "spikes" : g.platform ? "platform" : "ground";
            if (dn <= 0) L += snprintf(buf + L, sizeof buf - L, ", %s right under it", what);
            else L += snprintf(buf + L, sizeof buf - L, ", %s %d below", what, dn);
        }
    }
    cam_things(x, y, 0, 0, 0, buf, sizeof buf);
    speak_utf8(buf, 1);
}

/* Keyboard: I J K L move, U says, O goes back to Kirby. */
void camera_key(int vk) {
    switch (vk) {
    case 'I': cam_move(0, 1); break;
    case 'K': cam_move(0, -1); break;
    case 'J': cam_move(-1, 0); break;
    case 'L': cam_move(1, 0); break;
    case 'U': camera_say(); break;
    case 'O': camera_home(); break;
    }
}

static void camera_poll(DWORD now) {
    int sx, sy, lx, ly; pad_right_stick(&sx, &sy); pad_left_stick(&lx, &ly);
    if (!sx && !sy && s_air) { sx = lx; sy = ly; }   /* floating: the left stick looks around too */
    if (sx || sy) {
        if (sx != s_cam_sx || sy != s_cam_sy) { cam_move(sx, sy); s_cam_next = now + 350; }
        else if ((int)(now - s_cam_next) >= 0) { cam_move(sx, sy); s_cam_next = now + 170; }
    }
    s_cam_sx = sx; s_cam_sy = sy;
    if (camera_away() && now - s_cam_last > CAM_IDLE_MS) camera_home();
}
