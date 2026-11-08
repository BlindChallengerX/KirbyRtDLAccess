/*
 * Learn Sounds: a menu that plays every sound the mod makes and explains what
 * it means, what the thing is and how to deal with it.
 *
 * Keyboard: F11 opens and closes it. Up and down arrows move through the
 * sounds, left and right change category, Enter or Space plays the sound
 * again, Backspace (or F11) closes. Not Escape: the port quits on Escape. On a controller: hold the right stick
 * click for a second to open; the right stick moves (up and down sounds,
 * left and right categories), a right stick click plays again, a left stick
 * click closes. Pause the game first (plus / Start) so Kirby stays put.
 *
 * Gameplay sounds are silent while it is open.
 */
#include "access.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bossdesc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <xinput.h>

void snd_ability(int got); void snd_pit(void); void snd_platform_under(void); void snd_ground_after_pit(void);
void snd_land(int platform); void snd_platform_above(int up); void snd_edge_pit(float pan); void snd_edge_spikes(float pan);
void snd_drop(float pan); void snd_step_up(float pan); void snd_knock(float pan); void snd_spikes(float dy, float gain, float pan);
void snd_hub_door(float dy, float pan); void snd_warp_star(float dy, float pan); void snd_route(int up, float pan);
void snd_exit_door(float dy, float pan); void snd_landing_tone(float drop, int platform, int spike);
void snd_wall(int side, float closeness); void snd_ceiling(int dist); void snd_wall_h(int side, float closeness, float height);
void play_enemy_sound(uint32_t kind, float delay, float semis, float gain, float pan);
void snd_boss(int mid, float dy, float gain, float pan);
void play_item_sound(uint32_t kind, float delay, float semis, float gain, float pan);

volatile int g_learn_open;

/* demo kinds */
enum { D_NONE, D_ENEMY, D_ITEM, D_FN, D_LAND_SOLID, D_LAND_PLAT, D_LAND_SPIKE, D_WALL_L, D_WALL_R, D_CEIL, D_PITCH, D_STEP_R, D_MIDWALL_R };
typedef struct { const char *name; const char *desc; int demo; int arg; void (*fn)(void); } Entry;
typedef struct { const char *name; const Entry *e; int n; } Category;

static void f_pit(void) { snd_pit(); }
static void f_in_reach(void) { snd_in_reach(0.4f); }
static void f_drop_through(void) { snd_drop_through(); }
static void f_under_item(void) { snd_under_item(2); }
static void f_plat_under(void) { snd_platform_under(); }
static void f_after_pit(void) { snd_ground_after_pit(); }
static void f_land(void) { snd_land(0); }
static void f_plat_above(void) { snd_platform_above(2); }
void snd_side_door(float dy, float pan); void snd_hazard(float dy, float gain, float pan); void snd_work(float dy, float gain, float pan);
static void f_hazard(void) { snd_hazard(0, 0.26f, 0.5f); }
void snd_pillar(float dy, float gain, float pan); static void f_pillar(void) { snd_pillar(0, 0.4f, 0.5f); }
static void f_work(void) { snd_work(0, 0.22f, -0.5f); }
static void f_dark(void) { for (int i = 0; i < 6; i++) tone_at(i * 0.22f, 75 + i * 6, 75 + i * 6, 0.2f, 0.12f + i * 0.04f, -0.7f, W_SAW); }
static void f_side_door(void) { snd_side_door(0, 0.4f); }
void snd_ladder(int dir, float gain, float pan); void snd_rung_at(float delay, int height);
static void f_ladder_up(void) { snd_ladder(1, 0.26f, 0.6f); }
static void f_ladder_down(void) { snd_ladder(-1, 0.26f, -0.6f); }
static void f_rungs(void) { for (int i = 0; i < 4; i++) snd_rung_at(0.3f * i, i * 2); }
static void f_drop(void) { snd_drop(0.5f); }
static void f_step(void) { snd_step_up(0.5f); }
static void f_edge_pit(void) { snd_edge_pit(0.5f); }
static void f_edge_spikes(void) { snd_edge_spikes(0.5f); }
static void f_knock(void) { snd_knock(0.8f); }
static void f_spikes(void) { snd_spikes(-1, 0.3f, 0.5f); }
static void f_up(void) { snd_route(1, 0.4f); }
static void f_down(void) { snd_route(-1, 0.4f); }
static void f_exit(void) { snd_exit_door(0, 0.5f); }
static void f_hub(void) { snd_hub_door(0, 0.5f); }
static void f_warp(void) { snd_warp_star(4, 0); }
void snd_rift(float dy, float gain, float pan, int hidden); void snd_cannon(float dy, float gain, float pan); void snd_barrel(float dy, float gain, float pan);
static void f_rift(void) { snd_rift(0, 0.42f, 0, 0); }
static void f_rift_hidden(void) { snd_rift(0, 0.42f, 0, 1); }
static void f_cannon(void) { snd_cannon(0, 0.45f, 0); }
static void f_barrel(void) { snd_barrel(0, 0.35f, 0); }
/* the aim guide inside a cannon you fire: low ticks (blocked), then the bright "now" chime, then late ticks */
static void f_cannon_aim(void) {
    for (int i = 0; i < 4; i++) tone_at(i * 0.2f, 270, 250, 0.05f, 0.25f, 0, W_SQUARE);
    static const float nn[3] = { 1047, 1568, 2093 };
    for (int q = 0; q < 3; q++) tone_at(0.85f + q * 0.05f, nn[q], nn[q], q == 2 ? 0.14f : 0.05f, 0.32f, 0, W_TRI);
    for (int i = 0; i < 3; i++) tone_at(1.3f + i * 0.25f, 2093, 2093, 0.02f, 0.08f, 0, W_SINE);
}
static void f_cannon_clear(void) { for (int i = 0; i < 3; i++) { tone_at(i * 0.25f, 1320, 1320, 0.04f, 0.22f, 0, W_TRI); tone_at(i * 0.25f + 0.06f, 1760, 1760, 0.05f, 0.22f, 0, W_TRI); } }
static void f_ability(void) { snd_ability(1); }
static void f_ability_lost(void) { snd_ability(0); }
static void f_boss(void) { snd_boss(0, 0, 0.45f, 0.5f); }
/* a thrown thing flying in from the right at Kirby's height, then in reach, then a miss going away left */
void snd_thrown(int coming, float dy, float gain, float pan); void snd_in_reach(float pan);
static void f_thrown(void) {
    float t = 0;
    for (int i = 0; i < 9; i++) { float d = 8 - i; tone_offset(t); snd_thrown(1, 0, 0.12f + 0.20f * (1 - d / 12), d / 7 * 0.8f); t += (130 + d * 35) / 1000.0f; }
    tone_offset(t); snd_in_reach(0.1f);
    t += 0.5f;
    for (int i = 0; i < 4; i++) { float d = 1 + i * 1.5f; tone_offset(t); snd_thrown(0, 0, 0.12f + 0.20f * (1 - d / 12), -d / 7 * 0.8f); t += (130 + d * 35) / 1000.0f; }
    tone_offset(0);
}
static void f_midboss(void) { snd_boss(1, 0, 0.45f, 0.5f); }
void snd_boss_rush(float dy, float gain, float pan); static void f_rush(void) { snd_boss_rush(0, 0.3f, 0.6f); }
static void f_key(void) { snd_key(0.5f); }
static void f_carry(void) { snd_carry_item(0.5f); }
static void f_gate(void) { snd_locked_gate(0.5f); }
static void f_switch(void) { snd_switch(0.5f); }
static void f_switch_on(void) { snd_switch_done(0.5f); }
static void f_chase(int k) { snd_chase_demo(k); }
static void f_ch_gap(void) { f_chase(0); }
static void f_ch_back(void) { f_chase(1); }
static void f_ch_switch(void) { f_chase(2); }
static void f_ch_closed(void) { f_chase(3); }
static void f_ch_aim(void) { f_chase(4); }
static void f_ch_pocket(void) { f_chase(5); }
static void f_ch_in(void) { f_chase(6); }
static void f_ch_open(void) { f_chase(7); }
static void f_ch_shut(void) { f_chase(8); }
static void f_b_star(void) { snd_block(1, 0.5f); }
static void f_b_rock(void) { snd_block(8, 0.5f); }
static void f_b_bomb(void) { snd_block(2, 0.5f); }
static void f_b_hard(void) { snd_block(3, 0.5f); }
static void f_b_ice(void) { snd_block(4, 0.5f); }
static void f_b_fire(void) { snd_block(5, 0.5f); }
static void f_b_crumble(void) { snd_block(6, 0.5f); }
static void f_b_heavy(void) { snd_block(7, 0.5f); }
static void f_w_in(void) { snd_water_in(); }
static void f_w_out(void) { snd_water_out(); }
static void f_w_bubble(void) { snd_water_bubble(1); snd_water_bubble(1); }

static const Entry k_basics[] = {
    { "Left and right", "Every sound that points at something comes from its side: in your left ear when it is to Kirby's left, in your right ear when it is to the right, in both when it is right above or below him.", D_ENEMY, 2, NULL },
    { "Higher and lower", "Pitch tells height. The same sound is played level with Kirby, then 4 tiles above him, then 4 tiles below. It goes up one step for each tile something is above Kirby, and down when it is below. A tile is about Kirby's own size.", D_PITCH, 0, NULL },
    { "Enemy or obstacle", "Every enemy's sound starts with a short click, like this Waddle Dee. A click means something you can fight or inhale. Obstacles you can't beat, like Gordo, items and blocks never have the click. Enemies that give a copy ability when swallowed end with a tiny two-note sparkle; listen to Waddle Doo for one. When more enemies come onto the screen you hear how many, for example \"2 enemies\"; F9 turns that on or off.", D_ENEMY, 2, NULL },
    { "Enemy with an ability", "The click, the enemy's own sound, then a tiny two-note sparkle: swallowing this one gives a copy ability. This is a Waddle Doo, which gives Beam.", D_ENEMY, 16, NULL },
    { "In reach to inhale", "A soft breathy draw-in: an enemy has come close enough in front of Kirby, at his height, to inhale it now. It plays once as the enemy comes into reach, and only while Kirby has no ability, when X inhales.", D_FN, 0, f_in_reach },
    { "New ability", "A rising chime: Kirby just got a copy ability. Its name is spoken.", D_FN, 0, f_ability },
    { "Ability lost", "A falling chime: Kirby lost his copy ability, for example after being hit.", D_FN, 0, f_ability_lost },
};
static const Entry k_ground[] = {
    { "Landing tone over solid ground", "While Kirby is in the air, a quiet, low steady tone plays for the ground right under him, soft enough that Kirby's own float sound stays clear. This plain tone means solid ground. It goes up one step for every tile closer the ground gets, so you can count the steps: here it climbs from far below to right under his feet.", D_LAND_SOLID, 0, NULL },
    { "Landing tone over a platform", "A softer, hollow tone: the thing under Kirby is a thin platform he can land on, or jump up through from below. Pressing down on a platform drops through it.", D_LAND_PLAT, 0, NULL },
    { "Thin platform, way down", "Two wooden knocks, the second lower: Kirby is standing on a thin platform and the way to the exit goes down. Hold down to drop through it. The first time you also hear \"Thin platform. Hold down to drop through\".", D_FN, 0, f_drop_through },
    { "Landing tone over spikes", "A buzzy tone: there are spikes under Kirby. Do not land there; float on to safe ground.", D_LAND_SPIKE, 0, NULL },
    { "Pit below", "A low double pulse that repeats while there is nothing under Kirby at all: a bottomless pit. Keep floating (press jump again and again) and move on until the landing tone comes back.", D_FN, 0, f_pit },
    { "Platform comes under you", "One firm wooden knock: while in the air, a higher platform has just come under Kirby. Stop floating and you will land on it.", D_FN, 0, f_plat_under },
    { "Ground again after a pit", "A lower two-note chime: there is ground under Kirby again after a pit.", D_FN, 0, f_after_pit },
    { "Landing", "A soft thud when Kirby lands.", D_FN, 0, f_land },
    { "Platform above", "Two quick wooden knocks: a platform is right above Kirby, up to 5 tiles. Higher knocks mean it is higher up. Jump straight up to reach it; platforms let you jump up through them.", D_FN, 0, f_plat_above },
    { "Ladder going up", "Two metal clinks, the second higher, from the ladder's side: a ladder is near, up to 6 tiles, and it climbs up from Kirby's height. Faster and louder when it is closer. Walk to it until you hear \"Ladder up\", then hold up to climb.", D_FN, 0, f_ladder_up },
    { "Ladder going down", "Two metal clinks, the second lower: a ladder near Kirby that goes down from here, usually under a thin platform. Stand on top of it until you hear \"Ladder down\", then hold down to climb down.", D_FN, 0, f_ladder_down },
    { "Climbing a ladder", "A soft metal tick for every tile Kirby climbs, higher as he goes up.", D_FN, 0, f_rungs },
    { "Drop ahead", "A falling slide while walking: the ground ahead drops down. You can walk off or float down.", D_FN, 0, f_drop },
    { "Step up ahead", "A rising slide: the ground ahead steps up, too high to walk onto. Jump.", D_FN, 0, f_step },
    { "Pit ahead", "A falling buzz: there is a pit just ahead. Jump and float across it.", D_FN, 0, f_edge_pit },
    { "Spikes ahead", "A flat buzz: spikes just ahead on the ground. Jump over them; touching them hurts.", D_FN, 0, f_edge_spikes },
    { "Spikes nearby", "A short buzz from where spikes are in the stage, within 4 tiles. Higher when they are above Kirby, like on a ceiling. Keep away from them.", D_FN, 0, f_spikes },
};
static const Entry k_walls[] = {
    { "Wall on the left", "A steady low hum in the left ear: a wall is near on Kirby's left. The closer the wall, the louder the hum (up to 5 tiles away). The wall behind Kirby is a little softer than the one he faces. Steps and slopes he can walk onto do not hum.", D_WALL_L, 0, NULL },
    { "Wall on the right", "The same steady hum in the right ear: a wall on Kirby's right. Steady means a real wall, too high to get over.", D_WALL_R, 0, NULL },
    { "Step you can jump onto", "A brighter hum that ticks: the wall on that side is only 1 or 2 tiles high. Jump onto it and carry on. Bumping into it says \"Step, 1 high. Jump\".", D_STEP_R, 0, NULL },
    { "Wall you can float over", "A hum that slowly throbs: a wall from 3 to 12 tiles high. Jump, then keep pressing jump to float up and over it. Bumping into it says how high it is.", D_MIDWALL_R, 0, NULL },
    { "Ceiling", "A soft rush of air in the middle: there is a ceiling above Kirby, up to 6 tiles. Louder and brighter when it is lower. Close up you can't jump or float up here.", D_CEIL, 0, NULL },
    { "Bumping into a wall", "A low knock from the side you are pushing: Kirby is walking into a wall. Jump or float over it, or go back.", D_FN, 0, f_knock },
};
static const Entry k_way[] = {
    { "The way goes up", "Three rising notes: the way to the exit goes up from here. Jump and float up. They come from the side the way continues to.", D_FN, 0, f_up },
    { "The way goes down", "Three falling notes: the way to the exit goes down from here, for example down a shaft or off a ledge. Nothing plays while the way just goes straight on. F4 says where the exit is.", D_FN, 0, f_down },
    { "Exit door", "A soft bell from the door that leads on, to the next area or the goal, when it is within 10 tiles. Walk to it until you hear where it leads, for example \"Door to the next area, area 2 of 4. Press up\", then press up to go through.", D_FN, 0, f_exit },
    { "Side door", "A lower, softer bell falling a little: another door, to a side room or back out of one. Standing at it says where it leads and what is in a side room, like an Energy Sphere or a 1-Up. F4 lists every door in the area.", D_FN, 0, f_side_door },
    { "Danger here", "A dry rattle with a hiss, from its side, higher when it is above Kirby: a hazard placed in the stage, like icicles, a crusher, a lava geyser, a spiked bar, or rolling boulders. It is named when Kirby comes near, with how to get past it the first time. These are where the hazards start: rolling and moving ones travel from there.", D_FN, 0, f_hazard },
    { "Falling pillar", "A heavy stone thud with rubble clattering after it: a stone pillar that topples over when Kirby comes close. Rush past it, or step back and let it fall first.", D_FN, 0, f_pillar },
    { "Something to work", "A bright two-note pluck, up a step: something to use is near, like a hang switch to jump up and grab, a stake to pound down, a rope to cut, or a piñata. It is named with what to do when Kirby comes near. Shift F2 lists them.", D_FN, 0, f_work },
    { "Darkness chasing", "A low throbbing rumble from one side: in Another Dimension, a wall of darkness closes in from that side. It grows louder and throbs faster as it gets closer. Keep moving away from it; it hurts Kirby if it catches him.", D_FN, 0, f_dark },
    { "Stage door", "In a level's door room, a bell from the nearest stage door. Standing at one, you hear its stage number. Press up to enter. Locked doors are said as locked.", D_FN, 0, f_hub },
    { "Rift to Another Dimension", "A swirling rush of air around a low wobbling tone: a rift, a hole in space you jump into. In the stages with a Super Ability, smashing the scenery in the right spot opens one; you hear \"A rift opened\" and where. Jump into it to go to Another Dimension, a dark copy of the stage where a monster chases you; an Energy Sphere waits at its end. A second rift there is the way back out. The sound plays from up to 40 tiles away and comes faster when you are close; \"Rift right here, jump\" when you touch it. F4 and Shift F2 say where it is.", D_FN, 0, f_rift },
    { "Hidden rift", "The rift sound, but muffled and quieter: while Kirby has a Super Ability, this is the spot where a rift is hidden. Go there and smash the scenery with the Super Ability, and the rift opens.", D_FN, 0, f_rift_hidden },
    { "Fuse cannon", "A low boom with a metal ring: a cannon with a fuse. Light the fuse with a fire ability, then hurry into the cannon before the flame reaches it, and it shoots Kirby to a secret place. You hear \"In the cannon\" once you are inside.", D_FN, 0, f_cannon },
    { "Barrel cannon", "A hollow barrel thump with a puff: the nearest barrel cannon. Jump into it and it shoots Kirby onward; some fire by themselves, others when you press a button, which is said when you come close. Shift F2 says how many there are.", D_FN, 0, f_barrel },
    { "In a cannon: way clear", "Inside a barrel cannon you fire yourself, the mod follows its aim. Steady high double beeps: nothing is in the way, press a button to fire whenever you like. (For the first two seconds a soft tick plays while it checks for moving walls.)", D_FN, 0, f_cannon_clear },
    { "In a cannon: moving walls", "Low ticks: something is in the cannon's way, often crushing walls. Wait. When the walls open, a bright rising chime plays: fire right away. Soft high ticks after it mean the walls are still open but it's late, and lower walls may close on Kirby: wait for the next chime. This plays the ticks, the chime, then the late ticks.", D_FN, 0, f_cannon_aim },
    { "Warp Star", "On the Lor Starcutter, a rising sweep from the Warp Star to the world map. It floats above the floor: stand under it and jump into it.", D_FN, 0, f_warp },
};
static const Entry k_items[] = {
    { "Food", "A warm bloop. Food restores some of Kirby's health. Walk into it to eat it.", D_ITEM, 3, NULL },
    { "Maxim Tomato", "The same warm bloop: a Maxim Tomato restores all of Kirby's health.", D_ITEM, 2, NULL },
    { "Energy Sphere", "A sparkling chime. Energy Spheres are the gears Magolor needs to repair his ship. Many are hidden; collecting enough opens rooms on the Lor Starcutter with sub-games and challenges.", D_ITEM, 4, NULL },
    { "1-Up", "A rising two-note fanfare: an extra life.", D_ITEM, 1, NULL },
    { "Copy ability star", "A bright two-note ping: a star holding a copy ability. Touch or inhale it to get that ability.", D_ITEM, 8, NULL },
    { "Super ability star", "The same ping: a glowing star with a Super Ability, very strong for a short time. Shake (the triggers) to use it, and use it to smash big obstacles.", D_ITEM, 12, NULL },
    { "Invincible Candy", "A rising whistle. Eating it makes Kirby invincible for a while: run into enemies to defeat them.", D_ITEM, 5, NULL },
    { "Warp Star item", "A long rising sweep: a Warp Star. Get on it to fly to the next place.", D_ITEM, 10, NULL },
    { "Point star", "A tiny ting, only when one is within 5 tiles. Collect 100 point stars for an extra life.", D_ITEM, 0, NULL },
    { "Right under an item", "A bright bell bling, in the middle: Kirby is lined up right under an item above him. Jump or float straight up to grab it. It rings again every second or so while you stay under it, and is a little lower the higher the item is.", D_FN, 0, f_under_item },
};
static const Entry k_enemies[] = {
    { "Waddle Dee", "A soft boop. A small round walker that strolls back and forth. Harmless unless you bump into it. Inhale it or hit it. It gives no copy ability.", D_ENEMY, 2, NULL },
    { "Waddle Doo", "A double boop. A one-eyed Waddle Dee that shoots a crackling beam from its eye. Get close between beams and inhale it for the Beam ability.", D_ENEMY, 16, NULL },
    { "Bronto Burt", "A fluttering trill. A small winged flyer that flies in waves and sometimes swoops at Kirby. Inhale it or hit it. No ability.", D_ENEMY, 1, NULL },
    { "Sir Kibble", "A sharp metal double click. A little armored knight who throws a boomerang blade that comes back to him. Duck or jump the blade and inhale him for the Cutter ability.", D_ENEMY, 0, NULL },
    { "Blade Knight", "A falling square tone. A knight with a sword who slashes when you come close. Inhale him for the Sword ability.", D_ENEMY, 3, NULL },
    { "Hot Head", "A hiss. It breathes fire in front of itself. Approach between breaths, or from above, and inhale it for the Fire ability.", D_ENEMY, 5, NULL },
    { "Sparky", "An electric buzz. A blob that hops and sends out sparks all around itself. Wait until it stops sparking, then inhale it for the Spark ability.", D_ENEMY, 18, NULL },
    { "Chilly", "A high bright double ping. A little snowman that blows freezing air around itself. Inhale it for the Ice ability.", D_ENEMY, 51, NULL },
    { "Knuckle Joe", "A low double square tone. A fighter who punches and kicks up close. Inhale him for the Fighter ability.", D_ENEMY, 6, NULL },
    { "Poppy Bros Jr", "A bright square blip. A little jester who throws bombs. Inhale him for the Bomb ability.", D_ENEMY, 30, NULL },
    { "Leafan", "A falling soft tone. A leafy creature. Inhale it for the Leaf ability.", D_ENEMY, 15, NULL },
    { "Whippy", "A soft blip. It swings a whip. Inhale it for the Whip ability.", D_ENEMY, 12, NULL },
    { "Needlous", "A buzzy double blip. A spiky creature: touching its spikes hurts. Inhale it for the Needle ability.", D_ENEMY, 19, NULL },
    { "Rocky", "A heavy thud. A rock creature that can drop down on Kirby from above. Inhale it for the Stone ability.", D_ENEMY, 26, NULL },
    { "Flamer", "A crackling trill. A ball of fire that circles along floors and walls. Inhale it for the Fire ability.", D_ENEMY, 23, NULL },
    { "Twister", "A whirling trill. A spinning enemy. Inhale it for the Tornado ability.", D_ENEMY, 29, NULL },
    { "Noddy", "A slow falling tone. A sleepy creature. Swallowing it gives Sleep, which only makes Kirby fall asleep for a while, so you may want to spit it out instead (press attack while it is in your mouth).", D_ENEMY, 31, NULL },
    { "Starman", "A rising bright tone. A caped jumper. Inhale him for the Hi-Jump ability.", D_ENEMY, 28, NULL },
    { "Cappy", "A rising soft slide. A mushroom with a big cap. The first inhale pulls off its cap; inhale again to swallow it. No ability.", D_ENEMY, 4, NULL },
    { "Scarfy", "A bright buzzy blip. A floating face. If you try to inhale it, it turns angry and chases you, so attack it instead. No ability.", D_ENEMY, 21, NULL },
    { "Broom Hatter", "A swishing trill. It sweeps the ground back and forth. Harmless to inhale. No ability.", D_ENEMY, 20, NULL },
    { "Pacto", "A heavy snap. A plant mouth that bites. Keep out of its reach.", D_ENEMY, 13, NULL },
    { "Blipper", "A bubbly trill. A goggled fish that swims in water and can jump out at you.", D_ENEMY, 33, NULL },
    { "Glunk", "A low soft double tone. A sea anemone that sits in water and shoots at you.", D_ENEMY, 36, NULL },
    { "Gordo (obstacle)", "A harsh low buzz. A ball of spikes that can't be defeated or inhaled. It may stay still or move along a path. Go around it, and wait for moving ones to pass.", D_ENEMY, 10, NULL },
    { "Shotzo (obstacle)", "A harsh double square tone. A cannon that can't be defeated. It fires cannonballs; move out of its line of fire.", D_ENEMY, 14, NULL },
    { "Spiky plant (obstacle)", "A harsh buzz. A thorny plant that hurts to touch. Jump over it or go around.", D_ENEMY, 11, NULL },
    { "Rolling ball (obstacle)", "A long rumbling noise. A big ball rolling along the ground. Jump over it or get out of its path.", D_ENEMY, 38, NULL },
    { "Falling rock (obstacle)", "A long rumbling noise. A rock falling or rolling down. Move out of the way.", D_ENEMY, 143, NULL },
    { "Boss", "A heavy, low double drum beat from where the boss is, using the same left, right and pitch code. It beats faster the closer the boss is. When a boss appears you hear its name and what it does (Control F2 says that again; the Bosses category has them all); its health is said at each quarter, and \"defeated\" at the end. When it charges at Kirby you hear a whoosh (the next entry), and its thrown attacks tick like \"Something thrown at you\". F2 says where the boss is and its health.", D_FN, 0, f_boss },
    { "Boss rushing at you", "A fast rising whoosh with a low growl, from the boss's side: a boss is charging, diving or rolling toward Kirby right now, faster than the drum can tell you. Jump, float up or move away at once.", D_FN, 0, f_rush },
    { "Something thrown at you", "Shots, balls, apples, bombs, magic balls, falling rocks and stars to inhale tick all the time while they fly, from where they are: left or right, higher when above Kirby. The ticks get faster and louder as the thing gets close. A sharp tick with a little hiss means it is coming toward Kirby; a soft, lower tick means it is moving away. Without an ability, when one reaches the front of Kirby's mouth the breathy draw-in plays: press X to inhale it, then turn to the boss and press X again to spit it back. Dangerous things you can't inhale, like fire orbs, falling rocks and crystals, tick with a low buzz instead. This plays one flying in from the right, reaching Kirby, then one going away to the left.", D_FN, 0, f_thrown },
    { "Mid-boss", "A metal clang instead of a drum: a mid-boss, a big enemy partway through a stage, like Bonkers or Kibble Blade. Many give a copy ability when beaten and inhaled.", D_FN, 0, f_midboss },
    { "Other enemies", "Every other kind of enemy has its own fixed sound too, made from its name, so it always sounds the same. Press F2 in a stage to hear the names of the enemies near you.", D_ENEMY, 47, NULL },
};
static const Entry k_more_enemies[] = {   /* "*": the sound is described from its settings */
    { "Puppet Waddle Dee", "*A big paper puppet of a Waddle Dee with three Waddle Dees inside. It slowly walks into you or stands blocking the way. No ability.", D_ENEMY, 7, NULL },
    { "Armor Waddle Dee", "*A big robot shell shaped like a Waddle Dee, with hammer arms and three Waddle Dees inside. It is tough; keep attacking it. No ability.", D_ENEMY, 8, NULL },
    { "Como", "*A spider that drops down from the ceiling on a thread when Kirby comes near. No ability.", D_ENEMY, 9, NULL },
    { "Bouncy Sis", "*A rare, bigger Bouncy with a bow. She just bounces on Kirby. No ability.", D_ENEMY, 37, NULL },
    { "Nruff", "*A boar that charges along the ground. Jump over it or inhale it. No ability.", D_ENEMY, 22, NULL },
    { "Tick", "*A white blob with a red spike on its head. It stays in place and stabs at Kirby from above, often hanging from ceilings. Inhale it for Needle.", D_ENEMY, 24, NULL },
    { "Water Galbo", "*A blue creature that spits a jet of water skimming along the ground. Jump over the jet, then inhale it for Water.", D_ENEMY, 25, NULL },
    { "Bulby", "*A rare bulb plant that hides underground, then pops up and shoots spores when you come near. Inhale it for Leaf.", D_ENEMY, 27, NULL },
    { "Cerulean", "*A flyer that moves in short slow bursts along a set pattern. No ability.", D_ENEMY, 32, NULL },
    { "Gigatzo (obstacle)", "*A big cannon that fires giant flaming shots straight ahead. It can't be beaten or inhaled; stay out of its line of fire.", D_ENEMY, 34, NULL },
    { "Coldtzo (obstacle)", "*A big cannon like Gigatzo that fires icy shots. It can't be beaten; stay out of its line of fire.", D_ENEMY, 73, NULL },
    { "Volttzo (obstacle)", "*A big cannon like Gigatzo that fires electric shots. It can't be beaten; stay out of its line of fire.", D_ENEMY, 57, NULL },
    { "Babut", "*A bat that lurks in caves, especially dark ones. It grabs Kirby and chews on him; press buttons quickly to break free. No ability.", D_ENEMY, 35, NULL },
    { "Mumbies", "*A round, bandaged, one-eyed floater that either chases Kirby or holds still. No ability.", D_ENEMY, 39, NULL },
    { "Kabu", "*A stone head with empty eyes. Mostly in the way; it can be defeated. No ability.", D_ENEMY, 41, NULL },
    { "Mopoo", "*A little snow cloud launched out of a hole, flying straight or in an arc. No ability.", D_ENEMY, 43, NULL },
    { "Foley", "*A small flyer with leaf propellers that hovers above, then drops and explodes when Kirby passes underneath. Inhale it for Bomb.", D_ENEMY, 44, NULL },
    { "Jackle", "*A flying wheel with blades that throws cutters in straight lines. Inhale it for Cutter.", D_ENEMY, 45, NULL },
    { "Bowby", "*A floating bow that shoots arrows when Kirby comes close. No ability.", D_ENEMY, 46, NULL },
    { "Owgulf", "*A red owl that flies back and forth throwing feather needles down. Inhale it for Wing.", D_ENEMY, 47, NULL },
    { "Degout", "*A tough metal relative of Mumbies. It waits until attacked or sneaks up behind Kirby; the red Hunter Degout chases anyone carrying a key. No ability.", D_ENEMY, 48, NULL },
    { "Shelt", "*A small enemy in a pointed metal shell that charges at Kirby, jumping over obstacles. No ability.", D_ENEMY, 49, NULL },
    { "Lanzer", "*A round knight with a trident. Inhale him for Spear.", D_ENEMY, 50, NULL },
    { "Moonja", "*A ninja that hops around throwing shuriken and can teleport to ambush Kirby. Inhale it for Ninja.", D_ENEMY, 52, NULL },
    { "Pluid", "*A water drop with red eyes that hangs from the ceiling and drops onto Kirby. Inhale it for Water.", D_ENEMY, 53, NULL },
    { "Gemra", "*A floating orb in a jester hat that attacks with a spinning beam. Inhale it for Beam.", D_ENEMY, 54, NULL },
    { "Wapod", "*A ghost that rises out of a vase. Beating the ghost only brings another; break the vase to stop it. No ability.", D_ENEMY, 55, NULL },
    { "Barbar (obstacle)", "*A huge eel that hides in holes in the walls under water and lunges as you swim past. It can't be hurt; avoid it.", D_ENEMY, 58, NULL },
    { "Iron Barbar (obstacle)", "*Like Barbar, but it lunges out of pools of lava. Avoid it.", D_ENEMY, 77, NULL },
    { "Barracu", "*A barracuda in the water that bites onto Kirby; press directions quickly to break free. No ability.", D_ENEMY, 59, NULL },
    { "Squishy", "*A squid that swims in the water; on land it can't move well. No ability.", D_ENEMY, 60, NULL },
    { "Clayn", "*A blob that looks like a lump until you come close, then puffs up and hurts anyone touching it. No ability.", D_ENEMY, 61, NULL },
    { "Pierce", "*A flying bee with a spear. Inhale it for Spear.", D_ENEMY, 62, NULL },
    { "Searches", "*A floating mine with an angry face. It pulses and explodes when something comes close. Inhale it for Crash, a one-time screen-clearing blast.", D_ENEMY, 63, NULL },
    { "Walky", "*A rare walking microphone that hurts with musical notes. Inhale it for Mike.", D_ENEMY, 64, NULL },
    { "Elec", "*A green shelled creature that rolls along walls, then floats off and rams Kirby with electricity. Inhale it for Spark.", D_ENEMY, 65, NULL },
    { "Galbo", "*A spiky dragon that stays in place and breathes fire. Inhale it for Fire.", D_ENEMY, 69, NULL },
    { "Snowl", "*A snowy owl that zips back and forth leaving a trail of ice, sometimes dropping ice. Inhale it for Ice.", D_ENEMY, 70, NULL },
    { "Coconut", "*Coconuts on the beach of Onion Ocean stage 1. Big ones explode when dropped or hit; a Super Inhale can eat them. No ability.", D_ENEMY, 74, NULL },
    { "Craby", "*Walks like a crab on the sea floor and shoots piston claws from its head. No ability.", D_ENEMY, 75, NULL },
    { "Walf", "*A little turtle like a floating toy, bobbing on the water. It only hurts on touch. No ability.", D_ENEMY, 78, NULL },
    { "Darpa", "*A small purple butterfly-like creature on Halcandra that only rams into Kirby. Dippa and Dupa are its relatives. No ability.", D_ENEMY, 81, NULL },
    { "Leaper", "*A relative of Darpa found in Dangerous Dinner. No ability.", D_ENEMY, 84, NULL },
};

/* Copy abilities: the summary, then the game's move list. Filled in once. */
static void f_ability_get(void) { snd_ability(1); }
static Entry k_abil[29];
static char k_abil_name[29][40];
static void fill_abilities(void) {
    static int done; if (done) return; done = 1;
    for (int i = 0; i < 29; i++) {
        uint32_t k = (uint32_t)i + 1;
        snprintf(k_abil_name[i], sizeof k_abil_name[i], "%s%s", ability_name(k), k >= 25 ? ", Super Ability" : "");
        size_t n = strlen(ability_summary(k)) + strlen(ability_moves(k)) + 16;
        char *d = (char *)malloc(n);
        if (d) snprintf(d, n, "%s%s%s", ability_summary(k), ability_moves(k)[0] ? " Moves: " : "", ability_moves(k));
        k_abil[i].name = k_abil_name[i]; k_abil[i].desc = d ? d : ability_summary(k);
        k_abil[i].demo = D_FN; k_abil[i].arg = 0; k_abil[i].fn = f_ability_get;
    }
}
static const Entry k_things[] = {
    { "Key", "A little two-note jingle from where a key lies. Press X to pick it up, then walk it into a locked gate to open it. Don't press X again while carrying it, that throws it. While carrying it Kirby can only jump once, and if you hold it too long it breaks. You hear \"Carrying key\" when you pick it up.", D_FN, 0, f_key },
    { "Locked gate", "A heavy clunk and a rattle from where a locked gate is. It also hums like a wall, because Kirby can't get past it. When you carry a key the gate sounds more often and from farther away, so you can follow it. Walk into the gate with the key to open it: \"Gate unlocked!\".", D_FN, 0, f_gate },
    { "Switch", "A click and a higher clack, like flicking a lever, from where a switch is waiting to be hit. Higher means it is above Kirby, lower below. Hit it with an attack to turn it on; switches open doors or raise bridges. Some are boxed in by blocks you must break first, which is said when you come near. Shift F2 lists switches.", D_FN, 0, f_switch },
    { "Switch on", "A rising four-note chime and \"Switch on!\": a switch was hit and something in the area changed, like a bridge rising.", D_FN, 0, f_switch_on },
    { "Landia chase: way through", "While riding Landia through Another Dimension: two triangle beeps. The second is the height of the way through the walls ahead: higher means fly up, lower fly down, three rising notes mean you are at the right height. The beeps come from the right while the way goes on ahead.", D_FN, 0, f_ch_gap },
    { "Landia chase: hold back", "The same beeps from the left: the way turns or drops too steeply to make at the speed the screen carries you, or doubles back. Hold left to slow down or fly back, until the beeps come from the right again.", D_FN, 0, f_ch_back },
    { "Landia chase: switch ahead", "Square beeps: a switch that opens the next gate. Line up with it, the same high-low code, and shoot it. The gate opens late, so shoot from afar.", D_FN, 0, f_ch_switch },
    { "Landia chase: closed wall", "A low buzz: the wall ahead is shut. Shoot the switch for it.", D_FN, 0, f_ch_closed },
    { "Landia chase: aim", "A tiny high tick: an enemy, the switch or the boss is straight ahead. Shoot.", D_FN, 0, f_ch_aim },
    { "Crusher jaws: next pocket", "Soft sine bells at the crusher jaws: the second bell is the height of the next safe pocket. When shut, the jaws still leave small hollow pockets.", D_FN, 0, f_ch_pocket },
    { "Crusher jaws: in a pocket", "A bright little chime: Kirby is inside a safe pocket. Wait there while the jaws are shut, holding back a little, since the screen keeps moving: if you drift, the bells move to the left.", D_FN, 0, f_ch_in },
    { "Crusher jaws: opening", "A rising swoosh: the jaw you must get through next is opening. Fly right into the next pocket now.", D_FN, 0, f_ch_open },
    { "Crusher jaws: closing", "A low thud: the jaws are closing. Be in a pocket.", D_FN, 0, f_ch_shut },
    { "Other things to carry", "A wooden double knock: something Kirby can pick up and carry, like a Crackler, which fires blasts that break metal blocks, or a balloon bomb or candle. When you stand next to one its name is said. Shift F2 lists them.", D_FN, 0, f_carry },
    { "Star block", "A wooden tock-tock from the nearest star block. Star blocks break when you attack them, or spit a star at them. They often hide food or block a passage.", D_FN, 0, f_b_star },
    { "Rock block", "A lower tock-tock: a grey rock block. Break it with an attack like a star block.", D_FN, 0, f_b_rock },
    { "Bomb block", "A fuse hiss and a tock: a bomb block. Hit it and it blows up a whole chain of blocks connected to it, often opening a new way.", D_FN, 0, f_b_bomb },
    { "Metal block", "A metal clank: a hard block. Ordinary attacks don't break it; use a strong ability like Stone or Hammer, a Crackler, or a Super Ability.", D_FN, 0, f_b_hard },
    { "Ice or snow block", "A glassy tinkle: ice or snow. Melt it with a fire attack.", D_FN, 0, f_b_ice },
    { "Burnable block", "A crackle: a block that burns away with fire.", D_FN, 0, f_b_fire },
    { "Crumbling block", "A low rattle: a block that falls away soon after you stand on it. Keep moving.", D_FN, 0, f_b_crumble },
    { "Heavy block", "A deep stone thud: a big heavy block, usually broken only by a Super Ability.", D_FN, 0, f_b_heavy },
    { "Into the water", "A falling splash and \"In water\". In water Kirby swims: up and down move freely. Every two seconds a bubble plays; the higher the bubble, the closer the surface. F5 says how far up the surface is.", D_FN, 0, f_w_in },
    { "Out of the water", "A rising splash and \"Out of water\".", D_FN, 0, f_w_out },
    { "Under water", "Bubbles every two seconds while Kirby is under water. A high bubble means the surface is close above; a low one means you are deep.", D_FN, 0, f_w_bubble },
};
static const Entry k_bosses[] = {   /* the drum or clang, then what the fight is like */
    { "Whispy Woods, boss of Cookie Country", BD_WHISPY, D_FN, 0, f_boss },
    { "Mr. Dooter, boss of Raisin Ruins", BD_DOOTER, D_FN, 0, f_boss },
    { "Fatty Puffer, boss of Onion Ocean", BD_FATTY_PUFFER, D_FN, 0, f_boss },
    { "Goriath, boss of White Wafers", BD_GORIATH, D_FN, 0, f_boss },
    { "Grand Doomer, boss of Nutty Noon", BD_GRAND_DOOMER, D_FN, 0, f_boss },
    { "Metal General, boss of Egg Engines", BD_METAL_GENERAL, D_FN, 0, f_boss },
    { "HR-D3, Egg Engines in Extra Mode", BD_HRD3, D_FN, 0, f_boss },
    { "Landia, boss of Dangerous Dinner", BD_LANDIA, D_FN, 0, f_boss },
    { "The Lor Starcutter and Magolor", BD_LOR, D_FN, 0, f_boss },
    { "Magolor", BD_MAGOLOR, D_FN, 0, f_boss },
    { "Magolor with the Master Crown", BD_MAGOLOR_CROWN, D_FN, 0, f_boss },
    { "Galacta Knight, The True Arena", BD_GALACTA, D_FN, 0, f_boss },
    { "Bonkers, mid-boss", BD_BONKERS, D_FN, 0, f_midboss },
    { "Gigant Edge, mid-boss", BD_GIGANT_EDGE, D_FN, 0, f_midboss },
    { "Kibble Blade, mid-boss", BD_KIBBLE_BLADE, D_FN, 0, f_midboss },
    { "King Doo, mid-boss", BD_KING_DOO, D_FN, 0, f_midboss },
    { "Water Galboros, mid-boss", BD_WATER_GALBOROS, D_FN, 0, f_midboss },
    { "Dubior, mid-boss", BD_DUBIOR, D_FN, 0, f_midboss },
    { "Moundo, mid-boss", BD_MOUNDO, D_FN, 0, f_midboss },
    { "Sphere Doomer, mid-boss", BD_SPHERE_DOOMER, D_FN, 0, f_midboss },
};
#define N(a) (int)(sizeof a / sizeof *a)
void snd_camera_demo(int k);
static void f_cam0(void) { snd_camera_demo(0); }  static void f_cam1(void) { snd_camera_demo(1); }
static void f_cam2(void) { snd_camera_demo(2); }  static void f_cam3(void) { snd_camera_demo(3); }
static void f_cam4(void) { snd_camera_demo(4); }  static void f_cam6(void) { snd_camera_demo(6); }
static void f_cam7(void) { snd_camera_demo(7); }  static void f_cam8(void) { snd_camera_demo(8); }
static void f_cam9(void) { snd_camera_demo(9); }  static void f_cam11(void) { snd_camera_demo(11); }
static void f_camhome(void) { snd_camera_demo(-1); }
static void f_campitch(void) { snd_camera_demo(6); tone_at(0.35f, 560 * 1.4983f, 560 * 1.4983f * 0.82f, 0.045f, 0.38f, 0, W_SINE); tone_at(0.7f, 560 * 0.6674f, 560 * 0.6674f * 0.82f, 0.045f, 0.38f, 0, W_SINE); }
static const Entry k_camera[] = {
    { "Using the tile camera", "The tile camera looks at the tiles around Kirby, one at a time, so you can learn the shape of the place: where platforms are, how far a drop goes, what blocks are in the way. Push the right stick, or press I J K L on the keyboard, and the camera moves one tile that way and plays the sound of what is there; hold it to keep moving. The camera follows Kirby. Press the left stick click, or U, to hear the tile in words, with how far it is from Kirby and, for an open tile, how far down the ground is. The right stick click, or O, brings it back to Kirby, and it comes back by itself after 8 seconds. Like the other sounds, the side tells you left or right of Kirby, and the pitch goes up a step for each tile above him.", D_FN, 0, f_campitch },
    { "Open tile", "A soft high tick: nothing is here, Kirby could move or float through.", D_FN, 0, f_cam0 },
    { "Water", "A bubble: open water.", D_FN, 0, f_cam1 },
    { "Solid", "The low knock, like bumping a wall: solid ground or wall.", D_FN, 0, f_cam2 },
    { "Slope up to the right", "The knock, then a rising slide: a sloped floor that climbs going right.", D_FN, 0, f_cam3 },
    { "Slope down to the right", "The knock, then a falling slide: a sloped floor that goes down going right.", D_FN, 0, f_cam4 },
    { "Thin platform", "A wooden knock: a thin platform. Kirby can stand on it, jump up through it from below, and hold down to drop through.", D_FN, 0, f_cam6 },
    { "Ladder", "A metal clink: a ladder.", D_FN, 0, f_cam7 },
    { "Spikes", "A flat buzz: spikes. Do not touch.", D_FN, 0, f_cam8 },
    { "Blocks", "Breakable blocks play their own sounds, the same ones you hear when they are near: wooden tock-tock for star blocks, a metal clank for metal blocks, a fuse hiss for bomb blocks, and so on. A block the mod can't name plays a knock and a tock, like this.", D_FN, 0, f_cam9 },
    { "Edge of the map", "A dull low thump: the camera has gone past the edge of the area.", D_FN, 0, f_cam11 },
    { "Things on the tile", "After the tile sound, anything standing on the tile plays its own sound, the same one you hear in play: a door's bell, an item, an enemy, a switch (like this), a key or other thing to carry, a rift, a cannon, a hazard or something to work. The left stick click, or U, names it, and says whether a switch is already on or boxed in by blocks. Hazards and things to work are found within a tile of where the stage puts them.", D_FN, 0, f_switch },
    { "Back on Kirby", "A soft rising chirp after the tile sound: the camera is on Kirby's own tile again. A falling chirp means it jumped back to Kirby by itself or by the right stick click.", D_FN, 0, f_camhome },
};
static const Category k_cats[] = {
    { "How the sounds work", k_basics, N(k_basics) },
    { "Ground and platforms", k_ground, N(k_ground) },
    { "Walls", k_walls, N(k_walls) },
    { "Tile camera", k_camera, N(k_camera) },
    { "Finding the way", k_way, N(k_way) },
    { "Items", k_items, N(k_items) },
    { "Enemies and obstacles", k_enemies, N(k_enemies) },
    { "More enemies", k_more_enemies, N(k_more_enemies) },
    { "Keys, blocks and water", k_things, N(k_things) },
    { "Copy abilities", k_abil, 29 },
    { "Bosses", k_bosses, N(k_bosses) },
};

static int s_cat, s_idx;
static int s_demo;              /* running steady-sound demo */
static DWORD s_demo_t0;

static void stop_demo(void) { hum_all_off(); s_demo = D_NONE; }

static void play_entry(void) {
    const Entry *e = &k_cats[s_cat].e[s_idx];
    stop_demo();
    switch (e->demo) {
    case D_ENEMY: play_enemy_sound((uint32_t)e->arg, 0, 0, 0.35f, 0); break;
    case D_ITEM: play_item_sound((uint32_t)e->arg, 0, 0, 0.35f, 0); break;
    case D_FN: e->fn(); break;
    case D_PITCH:
        play_enemy_sound(2, 0, 0, 0.35f, 0); play_enemy_sound(2, 0.5f, 4, 0.35f, 0); play_enemy_sound(2, 1.0f, -4, 0.35f, 0);
        break;
    default: s_demo = e->demo; s_demo_t0 = GetTickCount(); break;
    }
}

#include "enemywhere.inc"
/* An entry's text; enemies also say where they are found (from the stage maps). */
static const char *full_desc(const Entry *e) {
    static char buf[2048];
    if (e->demo != D_ENEMY || e->arg < 0 || e->arg >= (int)(sizeof k_enemy_where / sizeof *k_enemy_where)) return e->desc;
    const char *where = strncmp(e->name, "Other", 5) ? k_enemy_where[e->arg] : "";
    if (e->desc[0] == '*') {
        char snd[96]; enemy_sound_words((uint32_t)e->arg, snd, sizeof snd);
        snprintf(buf, sizeof buf, "%s. %s %s", snd, e->desc + 1, where);
    } else snprintf(buf, sizeof buf, "%s %s", e->desc, where);
    return buf;
}

static void say_entry(int with_cat) {
    const Category *c = &k_cats[s_cat];
    const Entry *e = &c->e[s_idx];
    static char buf[4096];
    if (with_cat) snprintf(buf, sizeof buf, "%s, %d sounds. %s, %d of %d. %s", c->name, c->n, e->name, s_idx + 1, c->n, full_desc(e));
    else snprintf(buf, sizeof buf, "%s, %d of %d. %s", e->name, s_idx + 1, c->n, full_desc(e));
    speak_utf8(buf, 1);
    play_entry();
}

void learn_replay(void) { if (g_learn_open) play_entry(); }

void learn_toggle(void) {
    fill_abilities();
    g_learn_open = !g_learn_open;
    stop_demo();
    if (g_learn_open) {
        speak(L"Learn sounds. Up and down arrows: sounds. Left and right: categories. Enter: play again. Backspace: close. "
              L"On a controller, the right stick moves, right stick click plays again, left stick click closes.", 1);
        static char buf[4096]; const Category *c = &k_cats[s_cat];
        snprintf(buf, sizeof buf, "%s, %d sounds. %s, %d of %d. %s", c->name, c->n, c->e[s_idx].name, s_idx + 1, c->n, full_desc(&c->e[s_idx]));
        speak_utf8(buf, 0);
        play_entry();
    } else speak(L"Learn sounds closed", 1);
}

static void move(int dc, int di) {
    if (dc) { s_cat = (s_cat + dc + N(k_cats)) % N(k_cats); s_idx = 0; say_entry(1); }
    else { s_idx = (s_idx + di + k_cats[s_cat].n) % k_cats[s_cat].n; say_entry(0); }
}

/* Steady-sound demos run for about two seconds. */
static void run_demo(void) {
    if (s_demo == D_NONE) return;
    float t = (GetTickCount() - s_demo_t0) / 1000.0f;
    if (t > 2.2f) { stop_demo(); return; }
    switch (s_demo) {
    case D_LAND_SOLID: case D_LAND_PLAT: case D_LAND_SPIKE:
        snd_landing_tone(8.0f * (1.0f - t / 2.0f > 0 ? 1.0f - t / 2.0f : 0), s_demo == D_LAND_PLAT, s_demo == D_LAND_SPIKE);
        break;
    case D_WALL_L: snd_wall(-1, 0.4f + 0.3f * t); break;
    case D_WALL_R: snd_wall(1, 0.4f + 0.3f * t); break;
    case D_STEP_R: snd_wall_h(1, 0.4f + 0.3f * t, 1); break;
    case D_MIDWALL_R: snd_wall_h(1, 0.4f + 0.3f * t, 5); break;
    case D_CEIL: snd_ceiling(1); break;
    }
}

/* Returns 1 while the menu is open (the caller then skips gameplay keys). */
int learn_poll(void) {
    if (!g_learn_open) return 0;
    run_demo();
    static int prev[7];
    static const int keys[7] = { VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_SPACE, VK_BACK };
    if (game_has_focus()) {
        for (int i = 0; i < 7; i++) {
            int d = GetAsyncKeyState(keys[i]) < 0;
            if (d && !prev[i]) {
                switch (keys[i]) {
                case VK_UP: move(0, -1); break;
                case VK_DOWN: move(0, 1); break;
                case VK_LEFT: move(-1, 0); break;
                case VK_RIGHT: move(1, 0); break;
                case VK_RETURN: case VK_SPACE: play_entry(); break;
                default: prev[i] = d; learn_toggle(); return 1;     /* closed: stop handling keys */
                }
            }
            prev[i] = d;
        }
    }
    /* right stick on an XInput pad */
    static int stick;
    XINPUT_STATE st;
    for (DWORD p = 0; p < 4; p++) {
        if (XInputGetState(p, &st) != ERROR_SUCCESS) continue;
        int rx = st.Gamepad.sThumbRX, ry = st.Gamepad.sThumbRY, dir = 0;
        if (ry > 20000) dir = 1; else if (ry < -20000) dir = 2; else if (rx < -20000) dir = 3; else if (rx > 20000) dir = 4;
        if (dir && !stick) { if (dir == 1) move(0, -1); else if (dir == 2) move(0, 1); else if (dir == 3) move(-1, 0); else move(1, 0); }
        stick = dir;
        break;
    }
    return 1;
}
