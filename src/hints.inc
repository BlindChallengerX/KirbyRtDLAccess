/* Stage hints: what to look for and what to expect, spoken after the stage's name when a
 * stage starts (Ctrl+F5 turns them on or off, Ctrl+Shift+F5 repeats). Written from what each
 * stage map holds (enemies, gimmicks and items in map/step/levelN/stageM, see tools/enemymap.py
 * and tools/gmkmap.py) and the stages' layout in the game. Index [level][stage folder - 1]. */
static const char *const k_stage_hints[7][6] = {
    { /* Cookie Country */
        "Hint: an easy first stage of grassy hills, five areas. Expect Waddle Dees, Bronto Burts flying in waves, and Sir Kibble, Blade Knight and Hot Head, who give Cutter, Sword and Fire. Look for: a key in area 2; carry it to the locked gate to reach an Energy Sphere. In area 3 a huge glowing Blade Knight gives the Ultra Sword Super Ability: swing it at the dark crack in the air to open a rift into Another Dimension, with more Energy Spheres at its end.",
        "Hint: forests and trees, seven areas. Expect Como spiders that drop down on threads, Bouncy hopping on springs, and Leafan, who gives Leaf. Some platforms hang from ropes. Look for: a Warp Star ride between areas, and in area 4 a switch that raises a bridge over a bottomless pit: hit it with an attack, and don't fall in while carrying the Crackler. Energy Spheres are often just off the main path.",
        "Hint: rolling hills and caves, five areas. Expect lots of Shotzo cannons: they can't be beaten, so time your moves between their shots. Bouncy, Blade Knight and Hot Head are here too. Look for: switches that open shutter gates, and two locked gates that need a key. Shift F2 lists keys, switches and gates nearby.",
        "Hint: a long stage through woods and an icy cave, seven areas. Expect many Como spiders, Bowby birds swooping down, and icicles that fall from the ceiling when you pass under them: keep moving. Look for: switches, a glowing giant enemy that gives a Super Ability, and a rift into Another Dimension with more Energy Spheres.",
        "Hint: the boss stage. Copy ability stars wait near the start: take one if you have no ability. Then Whispy Woods, a giant tree that drops apples: inhale an apple and spit it back. Control F2 tells you how to beat him. After the fight, jump into the Warp Star.",
        NULL,
    },
    { /* Raisin Ruins */
        "Hint: a sandy desert, eight areas. Expect Nruff boars charging along the ground, Flamer circling the walls, Pacto snapping out of the ground, and rolling boulders. Look for: fuse lines and a fuse cannon. Light the fuse with Fire, then get inside the cannon before the spark reaches it, and it shoots you to a hidden place.",
        "Hint: ancient ruins with pools of water, ten areas. Expect Blipper fish and Glunk anemones that spit bubbles in the water, Kabu stones that slide at you, and Spiky plants. Lots of stone statues stand around the ruins. Look for: a pull switch, a Warp Star ride, a glowing giant enemy that gives a Super Ability, and a rift into Another Dimension with Energy Spheres at its end.",
        "Hint: desert ruins, six areas. Expect Cerulean, Starman, Twister tornadoes and Mumbies, ghosts that drift toward you. Look for: many barrel cannons. Step in, and the cannon fires you onward, by itself or when you press the attack button. Energy Spheres sit along the cannon routes.",
        "Hint: a long temple, ten areas. Expect many Shotzo cannons, Gigatzo, Babut bats swooping down, and Mumbies. Look for: ropes holding platforms or doors: cut them with Sword, Cutter or Fire. There are a locked gate that needs a key, a switch that resets after a moment, and a rift into Another Dimension with more Energy Spheres.",
        "Hint: the boss stage. Copy ability stars wait near the start. Then Mr. Dooter, a magician who throws skulls and bombs: inhale them and spit them back. Control F2 tells you how to beat him. Afterwards, jump into the Warp Star.",
        NULL,
    },
    { /* Onion Ocean */
        "Hint: a sunny beach and the sea, ten areas. Expect Coconut trees that drop coconuts, Glunks, Cappys, Gordos that can't be beaten, and Squishy squids under water. Look for: sand castles that only a Super Ability can smash, many switches, a key and a locked gate, a Warp Star ride, a glowing giant enemy with a Super Ability, and a rift into Another Dimension.",
        "Hint: mostly under water, six areas. Expect Blipper fish, Craby crabs, Barbar and Walf, Glunks and Squishys. Under water, Kirby swims freely; the bubble sound gets higher near the surface. Look for: a pull switch, and Energy Spheres in the side tunnels.",
        "Hint: a sea cave with strong water currents, ten areas. Expect many Gordos that can't be beaten, Barracu fish and Glunks. Currents push Kirby along: swim against them or let them carry you. Look for: biting statues, area switches that change the water, icicles, a glowing giant enemy with a Super Ability, and a rift into Another Dimension.",
        "Hint: a sunken place full of gates, eight areas. Expect Blipper fish, Barbar, Barracu, Squishy and Craby, and spiked bars that hurt on touch. Look for: many switches that open and close shutter gates, some only for a moment; and a key with a locked gate. Shift F2 lists switches and gates nearby.",
        "Hint: the boss stage. Copy ability stars wait near the start. Then Fatty Puffer, a huge pufferfish that rolls and pounds the ground. Control F2 tells you how to beat him. Afterwards, jump into the Warp Star.",
        NULL,
    },
    { /* White Wafers */
        "Hint: snowy fields, five areas. Expect Nruff boars, Chilly snowmen who give Ice, Spiky plants, Bouncy and Knuckle Joe, who gives Fighter. Look for: lots of barrel cannons that fire you across gaps, icicles, and pull switches.",
        "Hint: a long snowy mountain, nine areas. Expect Mopoo, Cappy, Rocky and Snowl. Many icicles fall from the ceiling. Look for: long fuse lines and a fuse cannon: light the fuse with Fire and hurry into the cannon. Five locked gates need keys, stakes can be pounded down, and there is a rift into Another Dimension.",
        "Hint: an icy lake, partly under water, six areas. Expect Blipper fish, Coldtzo, Craby and Squishy. Rolling ice balls come down the slopes: jump over them. Water currents push Kirby. Look for: switches, two locked gates with keys, and a pull switch.",
        "Hint: a frozen cave, seven areas. Expect many Broom Hatters, Pluid, Cerulean, Gordos and Chilly. Moving platforms, icicles and spiked bars; strong wind blows in places. Look for: pinatas to break for items, many pull and area switches, two locked gates, barrel cannons and a rift into Another Dimension.",
        "Hint: the last snowy stage, six areas. Expect lots of Scarfy, Wapod pots that drop ghosts, Gemra and Lanzer. Look for: four locked gates with keys, switches and barrel cannons. Energy Spheres are behind the locked gates.",
        "Hint: the boss stage. Copy ability stars wait near the start. Then Goriath, a big white ape that punches and throws snowballs. Control F2 tells you how to beat him. Afterwards, jump into the Warp Star.",
    },
    { /* Nutty Noon */
        "Hint: high in the sky among clouds, nine areas. Expect Cerulean, Foley leaves that fall and explode, Jackle blades, Owgulf and Mopoo. Look for: many platforms hanging from ropes that you cut to drop them, iron balls, wind that lifts Kirby, and a Warp Star ride.",
        "Hint: a stage in the clouds, eight areas. Expect lots of Scarfy, Mumbies and Shelt. Look for: many pull switches and shutter gates, barrel cannons, stakes to pound, a glowing giant enemy that gives a Super Ability, special switches for that Super Ability, and a rift into Another Dimension.",
        "Hint: windy cliffs, eight areas. Expect Pacto, Bronto Burts, Shotzo cannons, Babut bats, Volttzo and Starman. Strong wind blows Kirby around, and you cross many rope lines. Look for: switches and shutter gates, a key with a locked gate, and a barrel cannon.",
        "Hint: a sky stage of moving land, eight areas. Expect Gordos, Shotzo cannons, Shelt and Pacto. Moving platforms carry you along. Look for: strike boxes, stakes to pound, and walls, bells and screws that only a Super Ability can break, then a rift into Another Dimension.",
        "Hint: a stage of many small rooms joined by doors, more than twenty areas, with very few enemies. Each room is a short puzzle or path. Look for: doors with F4, and Energy Spheres spread over many rooms, more than in other stages. Switches and stakes open some of the ways.",
        "Hint: the boss stage. Copy ability stars wait near the start. Then Grand Doomer, the fiery leader of the Sphere Doomers. Crouch under its dives. Control F2 tells you how to beat it. Afterwards, jump into the Warp Star.",
    },
    { /* Egg Engines */
        "Hint: a mechanical factory, eleven areas. Expect lots of Volttzo, Bowby, Gordos and Wapod pots. Watch for presses that crush from above and machines that shoot bombs. Look for: long fuse lines and a fuse cannon, many barrel cannons, switches, a glowing giant enemy with a Super Ability, and a rift into Another Dimension.",
        "Hint: a cold machine stage, thirteen areas. Expect Degout, Scarfy, Searches and Gordos. Many icicles, and spiked bars that hurt on touch. Look for: switches, a pull switch, and a key with a locked gate.",
        "Hint: a factory of beams and gates, eight areas. Expect Waddle Dees, Degout, Blipper and Shotzo cannons. Crushing presses and falling poles; wind in places. Look for: many special switches and shutter gates, barrel cannons, and a rift into Another Dimension.",
        "Hint: a flooded factory, seven areas, much of it under water. Expect many Glunks, Barbar, Clayn and Babut. Strong water currents push Kirby, and spiked bars hurt on touch. Look for: a key with a locked gate.",
        "Hint: a long machine fortress, eight areas, full of Waddle Dees and Armor Waddle Dees. Look for: pull switches and shutter gates, barrel cannons, spiked bars, and a key with a locked gate.",
        "Hint: the boss stage. Copy ability stars wait near the start. Then Metal General, a metal knight robot with a laser sword. Control F2 tells you how to beat it. Afterwards, jump into the Warp Star.",
    },
    { /* Dangerous Dinner */
        "Hint: a volcano with lava, nine areas. Expect Flamer, Gigatzo, Volttzo, Iron Barbar and Armor Waddle Dees. Lava geysers shoot up and lava balls roll: keep clear of them. Look for: four locked gates with keys, pinatas, many switches, a glowing giant enemy with a Super Ability, and a rift into Another Dimension.",
        "Hint: a fiery machine stage, ten areas. Expect Bronto Burts, Degout, Moonja, Jackle and Searches. Moving platforms, lava geysers and spiked bars. Look for: power sources and circuit blocks, many switches, barrel cannons and a rift into Another Dimension.",
        "Hint: the longest stage, fourteen areas,. Expect Waddle Dees, Bowby, Owgulf, Iron Barbar and Knuckle Joe. Look for: area switches, rock pillars, barrel cannons, a key with a locked gate, a Warp Star ride, a glowing giant enemy with a Super Ability, and a rift into Another Dimension.",
        "Hint: the boss stage. Copy ability stars wait near the start: take a strong one. Then Landia, a great four-headed dragon. Control F2 tells you how to beat it.",
        "Hint: Another Dimension. Kirby rides the dragon Landia and flies through it, shooting. Attack to fire; Darpa, Dippa, Dupa and Leaper come at you in waves. Then the Lor Starcutter itself attacks. Control F2 tells you how to beat it.",
        "Hint: the final battle with Magolor. Copy ability stars are spread around the arena. Control F2 tells you how to beat him. When the Ultra Sword appears, swing it.",
    },
};
