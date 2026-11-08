/*
 * What each copy ability does.
 *
 *   - ability_summary: a short "what it's for and how to use it", the mod's
 *     own words (checked against the move lists), said after "Sword ability"
 *     when Kirby gets one;
 *   - ability_moves: every move with its buttons, from the game's own pause
 *     screen pages (moves.inc, generated from Pause.msbt), read by Shift+F3
 *     and in Learn Sounds.
 *
 * Index = AbilityManager kind (hero +0x120, +0x18), as k_ability_names in
 * play.c. Buttons are named as on an Xbox pad with the port's controller
 * profile: X is the Wii Remote's 1, A its 2, left bumper its A, the
 * triggers shake it, Back is minus.
 */
#include "access.h"
#include <stdio.h>

#include "moves.inc"

static const char *const k_summary[30] = {
    /*  0 */ "No ability. X inhales an enemy or object; press X again to spit it out as a star, or down to swallow it and copy its ability. Left bumper guards.",
    /*  1 Sword */ "Press X to slash. Hold X and release for a Spin Slash. At full health your slashes also fire a sword beam. In the air, down plus X stabs downward. A good all-round close-range ability.",
    /*  2 Cutter */ "X throws a cutter blade that flies ahead and comes back like a boomerang. Hold X and release for a bigger Hyper Boomerang. Next to an enemy, tap X repeatedly for Final Cutter. Good from a distance, and it cuts ropes.",
    /*  3 Leaf */ "X throws leaves ahead. Hold X to surround Kirby with spinning leaves. Hold left bumper to hide in a pile of leaves, where most attacks can't touch you. Up plus X is an uppercut.",
    /*  4 Whip */ "X lashes a whip forward with long reach; tap it quickly for many slaps. Hold X and release to grab an enemy, then throw it with a direction.",
    /*  5 Fire */ "X breathes fire ahead; aim it up or down with the d-pad. Dash, then X, to fly forward as a fireball. Fire burns burnable blocks and melts ice.",
    /*  6 Needle */ "X sticks out spikes all around Kirby, hurting anything that touches him. Up plus X makes giant spikes. Good against enemies that come close.",
    /*  7 Beam */ "X swings a beam whip that passes through walls. Hold X and release to fire a big Wave Beam. Next to an enemy, up, forward or down plus X captures it.",
    /*  8 Spark */ "X makes a burst of electricity all around Kirby. Shake with a trigger to charge up; then X fires an arrow, a laser or a big wave, depending on how long you charged. Up plus X calls a thunderbolt above.",
    /*  9 Stone */ "X turns Kirby into a heavy stone: while he is stone, enemies can't hurt him, and dropping as stone crushes what's below. Press X again to change back. Up plus X is a stone uppercut.",
    /* 10 Sleep */ "Kirby falls asleep and can't move for a while. Shake with a trigger to wake up early, then press Back to drop the ability.",
    /* 11 Parasol */ "X swings the parasol; hold X to hold it up as a shield. While it is open Kirby drifts down slowly. Next to an enemy, up or forward plus X throws it.",
    /* 12 Water */ "X sends a wave along the ground. Dash to surf along, and dash then X for a surf attack. Up plus X raises a geyser under Kirby.",
    /* 13 Hi-Jump */ "X launches Kirby high up, hurting anything he hits. Hold X for an even higher jump. In the air, down plus X dives down.",
    /* 14 Tornado */ "X turns Kirby into a tornado that whirls along hitting enemies. Shake with a trigger during it for a Hyper Tornado.",
    /* 15 Bomb */ "Hold X to pull out a bomb, release X to throw it. Down plus X sets a bomb on the ground, and dash then X rolls one. Bombs break blocks too.",
    /* 16 Spear */ "X thrusts a spear forward; tap quickly for a flurry. Dash then X throws spears. Hold X and release to spin the spear like a helicopter and fly. Up plus X thrusts upward.",
    /* 17 Hammer */ "X swings a big, very strong hammer. Hold X and release for three hits. Up plus X is a flaming upward swing. In the air, X spins the hammer around.",
    /* 18 Ice */ "X breathes ice that freezes enemies; walk into a frozen enemy to kick it into others. Left bumper makes an ice block that guards you. Dash to skate.",
    /* 19 Wing */ "X shoots feathers. Dash then X dives head first. A flaps the wings: Kirby flies easily. In the air, down plus X dive-bombs.",
    /* 20 Ninja */ "X throws knives. Hold X to slash an enemy right next to you. In the air, hold left or right against a wall to cling to it, and A to jump off. Down plus X makes a smoke screen.",
    /* 21 Fighter */ "X punches; tap it for rapid jabs. Down plus hold X charges a Force Blast, an energy ball you throw. Up plus hold X is a Rising Break uppercut. Next to an enemy, up or forward plus X grabs and throws it.",
    /* 22 Crash */ "One use only. Press X for a huge explosion that hits every enemy on the screen; then the ability is gone.",
    /* 23 Mike */ "Press X up to three times: each time Kirby sings louder, hitting every enemy on the screen. After the third the ability is gone.",
    /* 24 Landia */ "You are riding Landia. X breathes stars; hold X and release for a bigger burst. A does a Spiral Dragon spin.",
    /* 25 Ultra Sword */ "A Super Ability, for a short time. Press X to swing a gigantic sword that smashes enemies and the scenery. Use it to break huge obstacles and to open the rift to another dimension.",
    /* 26 Monster Flame */ "A Super Ability, for a short time. Press X to send out a huge fire dragon that burns everything in its path; steer it with the d-pad.",
    /* 27 Flare Beam */ "A Super Ability, for a short time. Press X to send out a huge ball of energy; steer it with the d-pad, and A lets it go.",
    /* 28 Grand Hammer */ "A Super Ability, for a short time. Press X to slam down a giant hammer; shake with a trigger during the swing to make it bigger, and again for bigger still.",
    /* 29 Snow Bowl */ "A Super Ability, for a short time. Press X to become a giant snowball that rolls along flattening everything; A jumps.",
};

const char *ability_summary(uint32_t k) { return k < 30 ? k_summary[k] : ""; }
const char *ability_moves(uint32_t k) { return k < 30 ? k_moves[k] : ""; }
