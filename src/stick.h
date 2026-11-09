/* One stick as a single direction for the tile camera (the stronger axis).
 * Pushing past 22000 starts a direction, it lasts until the stick drops under
 * 14000. A flicked stick springs back past the centre for a moment; that
 * bounce (straight to the other side, or the other way within 80 ms of
 * letting go) is ignored
 * until the stick is back in the middle, so every flick moves exactly one
 * tile. Shared by pad.c and tools/sticktest.c. */
#ifndef STICK_H
#define STICK_H
#include <stdlib.h>
typedef struct { int dx, dy, ldx, ldy, blocked; unsigned rel_t; } Stick;
static void stick_step(Stick *s, int x, int y, unsigned now, int *odx, int *ody) {
    int on = (s->dx || s->dy || s->blocked) ? 14000 : 22000;
    int dx = 0, dy = 0;
    if (abs(x) > on || abs(y) > on) { if (abs(x) >= abs(y)) dx = x > 0 ? 1 : -1; else dy = y > 0 ? 1 : -1; }
    if (!dx && !dy) {
        if (s->dx || s->dy) { s->ldx = s->dx; s->ldy = s->dy; s->rel_t = now; }
        s->blocked = 0;
    } else if ((s->dx || s->dy) && dx == -s->dx && dy == -s->dy) {   /* straight through the middle: the spring-back */
        s->ldx = s->dx; s->ldy = s->dy; s->rel_t = now; s->blocked = 1;
    } else if (!s->dx && !s->dy && !s->blocked && (s->ldx || s->ldy) && dx == -s->ldx && dy == -s->ldy && now - s->rel_t < 80)
        s->blocked = 1;   /* the other way just after letting go: also the spring-back */
    if (s->blocked) dx = dy = 0;
    s->dx = dx; s->dy = dy;
    *odx = dx; *ody = dy;
}
#endif
