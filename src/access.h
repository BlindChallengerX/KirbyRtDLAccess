/* Shared declarations for Kirby Access (Kirby's Return to Dream Land, Kirby Wii Recomp). */
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#define MOD_VERSION "0.8.6"

extern wchar_t g_game_dir[MAX_PATH];   /* folder of KirbyRecomp.exe, with trailing backslash */
extern wchar_t g_mod_dir[MAX_PATH];    /* g_game_dir + "KirbyAccess\" */

void log_line(const char *fmt, ...);
char *to_utf8(const wchar_t *w);       /* malloc'd; caller frees */
wchar_t *from_utf8(const char *s);     /* malloc'd; caller frees */
int game_has_focus(void);

/* speech.c */
void speech_init(void);
void speak(const wchar_t *text, int interrupt);
void speak_utf8(const char *text, int interrupt);
void speakf(int interrupt, const char *fmt, ...);
void speak_repeat(void);
void speech_protect(DWORD ms);   /* queue interrupting game speech for ms (0 = stop) */
void speech_stop(void);

/* guest.c: Wii memory.
 *
 * The port (ModernGekko, module ABI 2) runs the game as recompiled chunks
 * that all take a CPUState. Its ram / exram pointers are MEM1 (24 MB at
 * 0x80000000) and MEM2 (64 MB at 0x90000000), stored big-endian as on the
 * Wii. The CPUState is caught from the module's dispatch function. */
int  guest_init(void);                         /* hooks dispatch; 1 when done */
int  guest_ready(void);                        /* CPUState caught and memory known */
const uint8_t *guest_ptr(uint32_t a, uint32_t n);   /* host pointer, or NULL if outside RAM */
uint8_t  r8(uint32_t a);
uint16_t r16(uint32_t a);
uint32_t r32(uint32_t a);
float    rf32(uint32_t a);
void     guest_w32(uint32_t a, uint32_t v);          /* only for the text visibility marks */
static inline int16_t s16(uint32_t a) { return (int16_t)r16(a); }
static inline int32_t s32(uint32_t a) { return (int32_t)r32(a); }
static inline int is_ptr(uint32_t a) { return (a >= 0x80000000u && a < 0x81800000u) || (a >= 0x90000000u && a < 0x94000000u); }
int  guest_str(uint32_t a, char *out, int cap);             /* NUL-terminated byte string */
int  guest_wstr(uint32_t a, wchar_t *out, int cap);         /* NUL-terminated big-endian UTF-16 */
uint32_t guest_pc(void);
void guest_dump(const wchar_t *dir);                        /* writes mem1.bin and mem2.bin */
extern volatile uint32_t g_dispatch_count;
typedef void (*guest_hook_fn)(const uint32_t *gpr);    /* gpr[3] = first argument */
void guest_on_entry(uint32_t addr, guest_hook_fn fn);   /* call fn when a guest function is entered from another chunk */
void guest_watch(uint32_t addr, int count);             /* dev: log registers at entry */
void guest_hist(int on);                                /* dev: histogram of entered addresses */

/* audio.c: tones and hums mixed on our own waveOut stream */
void audio_init(void);
/* W_WARM: sine with a few harmonics, so a low hum is still heard on small speakers */
enum { W_SINE, W_SQUARE, W_TRI, W_SAW, W_NOISE, W_WARM };
/* A one-shot tone gliding f0 -> f1. pan -1 left .. 1 right. */
void tone(float f0, float f1, float seconds, float gain, float pan, int wave);
/* A tone after a delay (for little melodies). */
void tone_at(float delay, float f0, float f1, float seconds, float gain, float pan, int wave);
void tone_offset(float d);   /* start offset for the next tones (reset to 0 after) */
/* Continuous hums: slot 0..7. gain 0 fades it out. */
void hum_set(int slot, float freq, float gain, float pan, int wave);
/* A hum with texture: pulse_hz > 0 makes it throb (pluck 0, smooth) or tick
 * (pluck 1, a short accent each beat) with depth 0..1; lp_hz > 0 low-passes it
 * (for soft filtered noise). */
void hum_set_ex(int slot, float freq, float gain, float pan, int wave, float pulse_hz, float depth, int pluck, float lp_hz);
void hum_all_off(void);
extern volatile float g_master;

/* scene.c: the running stage */
uint32_t scene_component(void);        /* stage Component, 0 when no stage runs */
void scene_forget(void);
uint32_t scene_hero(int i);             /* hero i (0 = player 1), or 0 */
int hero_pos(uint32_t hero, float *x, float *y);
typedef struct { uint8_t shape, flags, extra, attr, outside; } LandCell;
int land_size(int *w, int *h);
int land_cell(int x, int y, LandCell *out);   /* outside the map reads as solid */

/* text.c: on-screen text */
void text_poll(void);
int  text_hud(const char *panel, const char *box, wchar_t *out, int cap);
extern volatile int g_text_on, g_text_log;

/* msg.c: the game's message files */
void msg_init(void);
int  msg_normalise(wchar_t *s);
int  msg_match(const wchar_t *start, const wchar_t **full);
const wchar_t *msg_by_label(const char *label);

/* movie.c: story movies */
void movie_poll(void);
extern volatile int g_movie_playing;

/* learn.c: Learn Sounds menu */
void learn_toggle(void);
void learn_replay(void);
int  learn_poll(void);
extern volatile int g_learn_open;

/* game.c */
void game_init(void);
void game_poll(void);
void game_key(int vk, int shift, int ctrl);

/* tools.c: development commands from KirbyAccess\cmd.txt */
void tools_poll(void);

/* objects.c */
void objects_reset(void);
void gimmicks_reset(void);
void gimmicks_poll(uint32_t comp, float px, float py, int ability, DWORD now);
int gimmicks_describe(float px, float py, int ability, char *buf, size_t cap);
void objects_poll(uint32_t comp, float px, float py, DWORD now);
extern int g_riding;   /* riding Landia: the Another Dimension chase (play.c) */
int objects_describe(uint32_t comp, float px, float py, char *buf, size_t cap);
int objects_blocks_describe(uint32_t comp, float px, float py, char *buf, size_t cap);
void snd_key(float pan); void snd_carry_item(float pan); void snd_locked_gate(float pan);
void snd_switch(float pan); void snd_switch_done(float pan);
void snd_chase_demo(int k);   /* Learn Sounds: the Landia chase cues */
void snd_block(int cat, float pan); void snd_water_in(void); void snd_water_out(void); void snd_water_bubble(int depth);

/* movie.c: the strap and title screens */
void startup_poll(void);

/* abilities.c */
const char *ability_summary(uint32_t k);
const char *ability_moves(uint32_t k);
const char *ability_name(uint32_t kind);
void enemy_sound_words(uint32_t kind, char *buf, size_t cap);
extern volatile int g_count_on;
void snd_drop_through(void);
void snd_under_item(float dy);
int enemy_is_creature(uint32_t kind);

/* areas.c */
int area_kind(uint32_t comp);
void areas_enter(uint32_t comp, int w, int h);
void areas_poll(void);
void areas_say(uint32_t comp, int w, int h);
void areas_hint_key(uint32_t comp, int shift);
void snd_in_reach(float pan);
int objects_open_switches(uint32_t comp, float *xy, int cap);
uint32_t enemy_gives_ability(uint32_t kind);
