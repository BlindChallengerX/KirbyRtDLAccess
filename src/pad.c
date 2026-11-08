/*
 * Input for Kirby Access, read alongside the game (nothing is taken from it):
 *   - which way the player is pushing (keyboard arrows, or an XInput pad's
 *     d-pad / left stick), for the wall knock and the facing used by the
 *     "ahead" cues;
 *   - mod keys on the pad: the stick clicks, which the Wii Remote profile
 *     does not use. L3 = enemies (F2), R3 = way to the exit (F4),
 *     L3 + R3 together = status (F3). Acted on when released. Holding R3
 *     for a second opens Learn Sounds; in it R3 plays again, L3 closes.
 *   - the right stick (also unused by the port) moves the tile camera
 *     (play.c, camera.inc); while the camera is away from Kirby, L3 says the
 *     tile and R3 brings the camera back.
 */
#include "access.h"
#include <xinput.h>
#include <stdlib.h>

#define B_L3 0x0040
#define B_R3 0x0080

static WORD s_buttons;
static SHORT s_lx, s_ly, s_rx, s_ry;
int kirby_in_air(void);
int camera_away(void); void camera_say(void); void camera_home(void);

/* A stick as one direction (the stronger axis), with a little hysteresis. */
static void stick_dir(int rx, int ry, int *st, int *dx, int *dy) {
    int on = (st[0] || st[1]) ? 14000 : 22000;
    st[0] = st[1] = 0;
    if (abs(rx) > on || abs(ry) > on) { if (abs(rx) >= abs(ry)) st[0] = rx > 0 ? 1 : -1; else st[1] = ry > 0 ? 1 : -1; }
    *dx = st[0]; *dy = st[1];
}
void pad_right_stick(int *dx, int *dy) { static int st[2]; stick_dir(s_rx, s_ry, st, dx, dy); }
void pad_left_stick(int *dx, int *dy) { static int st[2]; stick_dir(s_lx, s_ly, st, dx, dy); }

int input_dir(void) {
    int d = 0;
    if (game_has_focus()) {
        if (GetAsyncKeyState(VK_LEFT) < 0) d -= 1;
        if (GetAsyncKeyState(VK_RIGHT) < 0) d += 1;
    }
    if (!d) {
        if (s_buttons & XINPUT_GAMEPAD_DPAD_LEFT) d = -1;
        else if (s_buttons & XINPUT_GAMEPAD_DPAD_RIGHT) d = 1;
        else if (s_lx < -16000) d = -1;
        else if (s_lx > 16000) d = 1;
    }
    return d;
}

void pad_poll(void) {
    static int held, both;
    XINPUT_STATE st;
    WORD b = 0;
    for (DWORD i = 0; i < 4; i++) {
        if (XInputGetState(i, &st) == ERROR_SUCCESS) { b = st.Gamepad.wButtons; s_lx = st.Gamepad.sThumbLX; s_ly = st.Gamepad.sThumbLY; s_rx = st.Gamepad.sThumbRX; s_ry = st.Gamepad.sThumbRY; break; }
    }
    s_buttons = b;
    static DWORD r3_down; static int long_done;
    int now = b & (B_L3 | B_R3);
    if ((now & B_R3) && !(held & B_R3)) { r3_down = GetTickCount(); long_done = 0; }
    /* holding the right stick click for a second opens Learn Sounds */
    if ((now & B_R3) && !(now & B_L3) && !long_done && !g_learn_open && GetTickCount() - r3_down > 1000) { long_done = 1; learn_toggle(); }
    if (now) { held |= now; if ((held & (B_L3 | B_R3)) == (B_L3 | B_R3)) both = 1; return; }
    if (held) {
        if (long_done) { /* that was the long press */ }
        else if (g_learn_open) {
            if (held & B_L3) learn_toggle();
            else if (held & B_R3) learn_replay();
        }
        else if (!both && camera_away()) { if (held & B_L3) camera_say(); else camera_home(); }
        else if (both) game_key(VK_F3, 0, 0);
        else if (held & B_L3) game_key(VK_F2, 0, 0);
        else if (held & B_R3) game_key(VK_F4, 0, 0);
        held = 0; both = 0; long_done = 0;
    }
}
