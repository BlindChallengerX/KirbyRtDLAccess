/*
 * Story movies for Kirby Access.
 *
 * The story scenes are pre-rendered movies (the .mo files in files/mov) played by
 * scn::movie::SceneMovie (vtable 0x80770050; +0x3f0 = mov::MovieKind, the
 * order of MOVIE_INFO_TABLE). They have no text, except Magolor's speech in
 * "Let Me Explain Everything", which the game draws as subtitles straight
 * onto the movie (SUBTITLES table in Subtitle.o: start frame, label in
 * Movie.msbt).
 *
 * When a movie starts, the mod says its title (the Theater's name for it)
 * and then a description of what happens, timed to the pictures (written
 * from frames of each movie captured in the Theater; seconds from when the
 * movie really starts playing). The subtitles are real layout text, so the
 * text reader speaks them as they appear.
 */
#include "access.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define VT_SCENEMOVIE 0x80770050u

typedef struct { float t; const char *text; } Cue;

static const Cue k_opening[] = {   /* seconds after the movie starts playing */
    { 1, "Out in space, among the stars, shines Planet Popstar." },
    { 10, "A sunny morning in Dream Land, with a rainbow over the hills and forests." },
    { 14, "Kirby runs across the meadow carrying a huge strawberry shortcake. King Dedede is chasing him, wanting the cake for himself." },
    { 19, "They race past Meta Knight, who is reading a book under a tree, with Bandana Waddle Dee hurrying after them." },
    { 24, "Suddenly a swirling rift tears open high in the sky, and everyone stops and looks up." },
    { 32, "A ship bursts out of it in a blinding flash: a flying sailing ship with wings, oars and a mast. It is badly damaged." },
    { 41, "Pieces break off, the wings, the oars and more, and fly away inside bubbles in every direction." },
    { 50, "The ship plunges down over the forest. Kirby and his friends run after it." },
    { 57, "It has crashed beyond the trees, trailing smoke. The great ship lies in a clearing." },
    { 65, "As the friends arrive, a door in its side opens with a burst of light." },
    { 73, "Inside, the ship is all white and shining. They follow a small hooded figure up to the bridge." },
    { 81, "The figure works the controls in front of a big screen, but the screen shows only static." },
    { 96, "The screen flickers on: a picture of the ship, with red crosses where its parts are missing." },
    { 101, "The captain turns to them: Magolor, a small round figure in a blue hood and white cloak, with big yellow eyes. He explains what happened, and takes Kirby's hands, asking for help." },
    { 124, "The screen shows Planet Popstar, with the five missing parts scattered across it." },
    { 129, "Kirby, King Dedede, Meta Knight and Bandana Waddle Dee all agree to help." },
    { 134, "Kirby dashes out of the ship, and the four friends set off across the fields to find the missing parts." },
    { 143, "The ship rests on the hill, looking out over Dream Land." },
};
static const Cue k_oars[] = {
    { 1, "The recovered oars float in a glowing bubble." },
    { 4, "A burst of light, and the oars are fitted back onto the Lor Starcutter." },
    { 7, "The ship glides through a deep blue sky, its oars working again." },
    { 14, "It swoops low over the green hills of Dream Land, oars spread beneath it." },
};
static const Cue k_rwing[] = {
    { 1, "The right wing glows inside a bubble." },
    { 4, "A flash of light, and the wing is back on the ship." },
    { 7, "The Lor Starcutter flies with its right wing restored, swooping and climbing." },
    { 13, "It soars over Dream Land." },
};
static const Cue k_lwing[] = {
    { 1, "The left wing, marked with a yellow star, glows inside a bubble." },
    { 4, "A flash of light, and it joins the ship." },
    { 7, "With both wings back, the Lor Starcutter shoots up through a rushing tunnel of light." },
    { 14, "It flies over Dream Land, beating its white wings." },
};
static const Cue k_emblem[] = {
    { 1, "The star emblem glows inside a bubble." },
    { 4, "A flash, and the emblem is fixed to the bow." },
    { 7, "The ship races through a dark swirling tunnel." },
    { 13, "It flies out over Dream Land with the star emblem shining on its bow." },
};
static const Cue k_level5[] = {
    { 1, "The last part, the mast and sail, glows inside a bubble." },
    { 4, "A brilliant white flash." },
    { 7, "Light pours down as the mast settles into place. The Lor Starcutter is whole again: sail, wings, oars and emblem." },
    { 14, "The ship spreads its wings in a shower of sparkles and flies off, gleaming." },
    { 20, "The finished ship sails over Dream Land, ready for its journey." },
};
static const Cue k_halcandra[] = {
    { 1, "On the bridge, the big screen shows the fully repaired ship." },
    { 7, "A map appears, and a course is plotted from Planet Popstar to a distant world." },
    { 18, "The Lor Starcutter launches straight up, oars spread, and climbs through the clouds." },
    { 25, "A swirling rift opens in space. The ship bursts through into a strange dimension of grid lines and crystals." },
    { 33, "Floating volcanic islands appear: the world of Halcandra. The ship comes in to land." },
    { 42, "A huge four-headed dragon rises up, a crown on its central head. This is Landia." },
    { 47, "Landia breathes fire. A blast hits near the ship, and the Lor is struck and falls past a volcano, over a land of lava." },
    { 57, "Landia spreads its wings and roars. A white flash." },
    { 67, "On the bridge, everyone is knocked over. The screen shows only static, then the damage to the ship, then Landia with warning marks." },
    { 86, "Magolor turns to the friends and pleads for their help once more." },
    { 99, "Kirby nods with a big smile. Bandana Waddle Dee looks determined." },
    { 107, "All of them turn to face Landia on the screen." },
};
/* Magolor's lines in this one are on-screen subtitles: the text reader speaks them. */
static const Cue k_ambition[] = {
    { 1, "Landia lies defeated. It has split apart into four small red dragons, and the Master Crown has fallen to the ground." },
    { 7, "Kirby, King Dedede, Meta Knight and Bandana Waddle Dee cheer." },
    { 11, "Magolor floats down from the swirling sky." },
    { 23, "He picks up the crown, and it glows." },
    { 40, "He puts on the Master Crown. Power surges through him in lightning and light." },
    { 50, "The friends stare up in shock." },
    { 56, "Pictures from the past: the Lor Starcutter fleeing through space to Popstar." },
    { 75, "Magolor transforms: a red cloak, glowing floating hands, and the crown on his head. He laughs." },
    { 82, "He opens a star-shaped portal leading to Popstar." },
    { 95, "The friends look at each other." },
    { 103, "A small red dragon, one of the four parts of Landia, comes to Kirby's side." },
    { 110, "The four little dragons gather, ready to help. The friends climb on." },
    { 125, "Riding the dragons, they fly into the star portal after Magolor." },
};
static const Cue k_final[] = {
    { 1, "Kirby, King Dedede, Meta Knight and Bandana Waddle Dee each ride one of Landia's four little dragons through a purple dimension." },
    { 7, "Ahead is Magolor, wearing the Master Crown, with a red cloak and spiked glowing hands floating beside him." },
    { 14, "He fires twin glowing rings and opens pink black holes. The dragons weave between them." },
    { 28, "Kirby dives toward a shining surface. Get ready to fight." },
};
static const Cue k_final_ex[] = {
    { 1, "The four friends ride Landia's little dragons through a purple dimension." },
    { 6, "Magolor appears before them, the Master Crown on his head, spiked glowing hands at his sides." },
    { 11, "Kirby flies at him. Magolor fires blasts of light from both hands." },
    { 21, "Meta Knight, Dedede and Bandana Dee charge in. A pink explosion fills the sky." },
    { 32, "Kirby dives down toward a glowing surface." },
    { 39, "Magolor gathers his power. His body burns white, the crown blazing." },
    { 46, "A blinding flash, and Magolor rises again, darker and stronger. Get ready to fight." },
};
static const Cue k_ending[] = {
    { 3, "The four friends stand in the dimension, Magolor beaten." },
    { 11, "A star-shaped portal opens, showing Planet Popstar." },
    { 22, "The floor cracks and the dimension starts to shatter. The four little dragons fly to them." },
    { 36, "The Lor Starcutter swoops in." },
    { 42, "The floor gives way. Kirby falls, eyes shut, and a little dragon catches him." },
    { 54, "Everyone rides the dragons through the shattering dimension beside the Lor, toward a bright light." },
    { 72, "They fly through the star portal, into white." },
    { 89, "The sky of Dream Land. Kirby lies asleep in a meadow, a butterfly resting on him. He wakes up." },
    { 103, "Bandana Waddle Dee has landed head first, and sits up rubbing his head. Meta Knight lies on the grass." },
    { 114, "King Dedede is stuck head first in the ground. He pops out, dazed, a flower and a butterfly on his head." },
    { 126, "Kirby smiles, and the friends gather together." },
    { 135, "The Lor Starcutter sails overhead, flashes into a star-shaped portal and is gone." },
    { 146, "Everyone waves goodbye happily." },
    { 153, "A last look over Dream Land, and the storybook page turns." },
};
static const Cue k_credits[] = { { 1, "The staff credits roll." } };
static const Cue k_dance[] = {
    { 1, "Spotlights sweep over a theater full of Kirby's friends and enemies." },
    { 6, "On stage, Kirby pops out of a drum wearing a top hat, with Meta Knight, King Dedede, Bandana Waddle Dee and a yellow Kirby beside him." },
    { 14, "They dance together as confetti falls. The game is complete." },
};

typedef struct { const char *title_label; const char *fallback; const Cue *cues; int n; } MovieInfo;
#define N(a) (int)(sizeof a / sizeof *a)
static const MovieInfo k_movies[14] = {
    { "Movie_Opening", "A Visitor from Afar", k_opening, N(k_opening) },
    { "Movie_Level1", "Oars Restored!", k_oars, N(k_oars) },
    { "Movie_Level2", "Right Wing Restored!", k_rwing, N(k_rwing) },
    { "Movie_Level3", "Left Wing Restored!", k_lwing, N(k_lwing) },
    { "Movie_Level4", "Emblem Restored!", k_emblem, N(k_emblem) },
    { "Movie_Level5", "The Lor Starcutter Reborn", k_level5, N(k_level5) },
    { "Movie_Halcandra", "Halcandra: A New Enemy", k_halcandra, N(k_halcandra) },
    { "Movie_MahoroaAmbition", "Let Me Explain Everything", k_ambition, N(k_ambition) },
    { "Movie_DecisiveBattle", "Final Battle", k_final, N(k_final) },
    { "Movie_DecisiveBattle2", "Final Battle EX", k_final_ex, N(k_final_ex) },
    { "Movie_Ending", "Each to His Own World", k_ending, N(k_ending) },
    { "Movie_StaffCredit", "Staff Credits", k_credits, N(k_credits) },
    { "Movie_StaffCreditExtra", "Special Staff Credits", k_credits, N(k_credits) },
    { "Movie_ClearDance", "Kirby Master!", k_dance, N(k_dance) },
};

static uint32_t s_scene;
static int s_kind = -1, s_next_cue;
static DWORD s_start, s_last_scan;
static int s_playing;           /* state +0x3ec: 0 ready, 1 buffering, 2 playing, 3 ending */
volatile int g_movie_playing;

static uint32_t find_scene(void) {
    const uint8_t *m = guest_ptr(0x80000000, 4);
    if (!m) return 0;
    const uint8_t pat[4] = { 0x80, 0x77, 0x00, 0x50 };
    for (uint32_t off = 0x008f4000; off + 0x400 <= 0x01800000; off += 4) {
        if (m[off] != 0x80 || memcmp(m + off, pat, 4)) continue;
        uint32_t k = r32(0x80000000 + off + 0x3f0);
        if (k < 14) return 0x80000000 + off;
    }
    return 0;
}

void movie_poll(void) {
    DWORD now = GetTickCount();
    if (s_scene) {
        if (r32(s_scene) != VT_SCENEMOVIE || r32(s_scene + 0x3f0) != (uint32_t)s_kind) {
            /* over or skipped: stop talking about it */
            log_line("movie %d ended", s_kind);
            if (s_next_cue < k_movies[s_kind].n) speech_stop();
            s_scene = 0; s_kind = -1; g_movie_playing = 0;
            return;
        }
    } else {
        if (now - s_last_scan < 500) return;
        s_last_scan = now;
        uint32_t sc = find_scene();
        if (!sc) return;
        s_scene = sc; s_kind = (int)r32(sc + 0x3f0); s_start = now; s_next_cue = 0; s_playing = 0;
        g_movie_playing = 1;
        hum_all_off();
        const wchar_t *title = msg_by_label(k_movies[s_kind].title_label);
        char buf[160];
        if (title) { char *u = to_utf8(title); snprintf(buf, sizeof buf, "Cutscene: %s.", u ? u : ""); free(u); }
        else snprintf(buf, sizeof buf, "Cutscene: %s.", k_movies[s_kind].fallback);
        log_line("movie %d starts", s_kind);
        speak_utf8(buf, 1);
    }
    /* the clock starts when the movie really plays (after loading) */
    if (!s_playing) {
        if (r32(s_scene + 0x3ec) < 2) return;
        s_playing = 1; s_start = now;
        log_line("movie %d playing", s_kind);
    }
    float t = (now - s_start) / 1000.0f;
    const MovieInfo *mi = &k_movies[s_kind];
    while (s_next_cue < mi->n && t >= mi->cues[s_next_cue].t) speak_utf8(mi->cues[s_next_cue++].text, 0);
}

/* ---- the screens before the file menu ----
 * They are pictures, not text, so the mod says them:
 *   scn::strap::SceneStrap (vtable 0x807d22d0), +0x334 state: 0-2 the Wii
 *   wrist strap warning (2 waits for a button), 4-6 "Hold the Wii Remote
 *   sideways" (6 waits), 7-8 leaving. Any button but Home goes on
 *   (isButtonReceived, mask 0x1f1f).
 *   scn::title::SceneTitle (vtable 0x807d2b38): the intro, then the logo
 *   and "Press 2".
 * Scenes are found the same way as the movie scene. */
#define VT_STRAP 0x807d22d0u
#define VT_TITLE 0x807d2b38u
static uint32_t find_vt(uint32_t vt) {
    const uint8_t *m = guest_ptr(0x80000000, 4);
    if (!m) return 0;
    const uint8_t pat[4] = { vt >> 24, (vt >> 16) & 0xff, (vt >> 8) & 0xff, vt & 0xff };
    for (uint32_t off = 0x008f4000; off + 0x400 <= 0x01800000; off += 4)
        if (m[off] == pat[0] && !memcmp(m + off, pat, 4)) return 0x80000000 + off;
    return 0;
}
void startup_poll(void) {
    static uint32_t s_strap, s_title;
    static int s_said_strap, s_said_side, s_said_title;
    static DWORD s_last;
    DWORD now = GetTickCount();
    if (s_strap && r32(s_strap) != VT_STRAP) s_strap = 0;
    if (s_title && r32(s_title) != VT_TITLE) { s_title = 0; s_said_title = 0; }
    if (!s_strap && !s_title) {
        if (now - s_last < 400) return;
        s_last = now;
        /* every scene so far is built at 0x809ed000; search all of MEM1
         * only in the first minute and a half after starting */
        static DWORD s_boot; if (!s_boot) s_boot = now;
        uint32_t vt = r32(0x809ed000);
        if (vt == VT_STRAP) s_strap = 0x809ed000; else if (vt == VT_TITLE) s_title = 0x809ed000;
        else if (now - s_boot < 90000) { s_strap = find_vt(VT_STRAP); if (!s_strap) s_title = find_vt(VT_TITLE); }
        if (!s_strap && !s_title) return;
    }
    if (s_strap) {
        uint32_t st = r32(s_strap + 0x334);
        if (st <= 2 && !s_said_strap) {
            s_said_strap = 1;
            speak(L"Wii safety screen: Put on and tighten the wrist strap. Hold the Wii Remote securely and do not let go of it. "
                  L"Use the Wii Remote Jacket. Allow adequate room around you during game play. Press any button to continue.", 0);
        } else if (st >= 4 && st <= 6 && !s_said_side) {
            s_said_side = 1;
            speak(L"Hold the Wii Remote sideways. Press any button to continue.", 1);
        }
    } else if (s_title && !s_said_title) {
        s_said_title = 1;
        speak(L"Kirby's Return to Dream Land. Title screen. Press 2 to start: the A button on a controller.", 1);
    }
}
