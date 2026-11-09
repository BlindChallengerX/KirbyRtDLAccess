Kirby Access 0.8.8

A screen reader and audio mod for Kirby's Return to Dream Land on PC, using
the Kirby Wii Recomp port (KirbyRecomp.exe, by ExpansionPak / ModernGekko).

- Menus, text, story scenes and stage information are spoken through your
  screen reader using Prism. If no screen reader is running, the Windows
  voice is used.
- Gameplay is told with sounds: the ground and platforms under Kirby, walls,
  enemies, items, blocks, hazards, and which way the stage goes.
- A tile camera lets you look around Kirby one tile at a time, to learn the
  shape of the platforms near you.

The mod is still being tested. Please tell me what sounds wrong, what is too
loud or too busy, and what is missing.


CONTENTS

  1. Start here
  2. What you need and installing
  3. Game controls
  4. Mod keys
  5. Learn Sounds
  6. Tile camera
  7. What speaks
  8. Gameplay sounds
  9. Story and cutscenes
  10. Known limits
  11. How it works


1. START HERE

The mod uses many sounds, and each one means something. You will get much
more out of it if you learn them before you play:

  Open Learn Sounds with F11 (or hold the right stick click for a second)
  and go through it. Every sound plays, then you hear what it means and what
  to do about it.

Start with the first categories (how the sounds work, ground and platforms,
walls, finding the way). Come back to the rest when you meet something new.
Learn Sounds is explained in section 5.

Three more things to know:
- Press F1 at any time to hear the mod keys.
- Don't press Escape: the port closes the game on Escape.
- Every sound uses the same code: left or right tells you which side of
  Kirby the thing is on, and a higher pitch means it is higher up. One step
  of pitch is one tile, about Kirby's own size.

There is also a stage guide in its own file, "Kirby Stage Guide.txt". You
don't need to read it, and it spoils the stages. But it can help if you get
stuck, or wonder what you should be looking for: keys, switches, Energy
Spheres, Super Ability rifts and how to beat each boss.


2. WHAT YOU NEED AND INSTALLING

You need:
- Your own copy of Kirby's Return to Dream Land, USA version (SUKE01), as a
  .wbfs or .iso file.
- An internet connection the first time, to download the game program.

To install:
1. Unzip this zip into a folder of its own, for example Documents\Kirby.
2. Start "Play Kirby Access.exe" in that folder.

The first time, Play Kirby Access asks to download Kirby Wii Recomp, the PC
port the mod runs on (by ExpansionPak and ModernGekko, about 42 MB, from
its official GitHub release). Press Yes. You hear a short rising tick for
every tenth of the download, then a message when it is installed. The mod
checks that the file is exactly the version it was tested with. Kirby Wii
Recomp is a test release itself and has its own bugs.

If you already have Kirby Wii Recomp, you can instead copy "Play Kirby
Access.exe" and the KirbyAccess folder into its folder, next to
KirbyRecomp.exe. Either way, always start "Play Kirby Access.exe" instead of
KirbyWiiRecomp.exe.

The first time, Play Kirby Access sets the game up for you. It looks for your
disc image (a .wbfs or .iso with "Kirby" in its name) in the same folder and
in your Downloads folder, tells you before it starts, and asks the port to
copy the game from it. This takes a minute or two. It also lets the port set
up your controller, and turns off the frames-per-second count in the window
title so your screen reader doesn't keep reading it. After that, the game
starts straight away.

A few seconds after the window opens you should hear "Kirby Access loaded.
Press F1 for keys." If something goes wrong, the KirbyAccess folder has two
logs, launch.log and KirbyAccess.log. Please send me both.


3. GAME CONTROLS

The port maps a Wii Remote held sideways onto an Xbox controller:

  D-pad          move. Up enters doors, down crouches and slides.
  A              jump (the Wii Remote's 2 button); hold or press again to float
  X              inhale, attack, use your copy ability (the 1 button)
  Left bumper    the Wii Remote's A button (Energy Spheres on the world map)
  Right bumper   the Wii Remote's B button
  Triggers       shake the remote (Super Abilities, dropping an ability)
  Back           minus: drop your ability; hold for Quick Quit in multiplayer
  Start          plus: pause menu

The left stick is not used; move with the d-pad. The mod uses the right
stick for the tile camera, and the left stick too while Kirby is in the air.


4. MOD KEYS

Information keys:
  F1         keys help
  F2         enemies nearby, nearest first, with where they are
  Shift+F2   items nearby (food, Energy Spheres, 1-Ups, copy ability stars
             by name), keys and other things to carry, locked gates
  Ctrl+F2    what the boss you are fighting does, and how to beat it
             (in a boss fight, F2 says this too while the game is paused)
  F3         health, copy ability, lives, and whether Kirby is on the
             ground, on a platform, in the air or in water
  Shift+F3   every move of Kirby's current ability, with its buttons
  F4         the way to the exit door. In a level's door room: the stage
             doors. On the Lor Starcutter: the Warp Star to the World Map
             and the ship's doors (Copy Ability rooms and sub-game rooms are
             named, and say "locked" until you have the Energy Spheres they
             need)
  F5         walls left and right, the ground ahead (drops, step ups, pits,
             spikes), a ceiling or platform above, the nearest breakable
             blocks, and under water how far up the surface is
  Shift+F5   where you are: the level, stage and area, or what this room is
  Ctrl+Shift+F5  the stage hint again
  F12        repeat the last thing said

Settings:
  Ctrl+F5    stage hints on or off (remembered for next time)
  F6         wall hums on or off
  Shift+F6   enemy and item sounds on or off
  Ctrl+F6    route tones on or off
  F7, F8     mod sounds quieter, louder
  F9         enemy count on or off ("2 enemies" when more come on screen)

Menus and camera:
  F11        Learn Sounds (section 5)
  I J K L    tile camera up, left, down, right (section 6)
  U          say the camera's tile
  O          camera back to Kirby

On the controller:
  Left stick click            enemies (like F2)
  Right stick click           the way to the exit (like F4)
  Both stick clicks together  status (like F3)
  Hold right stick click      open Learn Sounds
  Push the right stick        tile camera
  Push the left stick         tile camera, while Kirby is in the air
While the tile camera is away from Kirby, the stick clicks work the camera
instead (section 6).


5. LEARN SOUNDS

Press F11, or hold the right stick click for a second, to open a menu with
every sound the mod makes. Each one plays, then you hear its name and what it
means: what the thing is and how to deal with it. Enemies also tell you what
they do, how to beat or avoid them, which copy ability they give, and where
they are found (taken from the game's own stage maps). There are categories
for copy abilities and for bosses too, so you can read up before you choose
an ability or start a fight.

Pause the game first (Start) so Kirby stays put. Gameplay sounds are quiet
while the menu is open.

  Up and down arrows     next or previous sound (controller: right stick up
                         and down)
  Left and right arrows  next or previous category (right stick left and
                         right)
  Enter or Space         play the sound again (right stick click)
  Backspace or F11       close (left stick click)

The categories are: how the sounds work, ground and platforms, walls, tile
camera, finding the way, items, enemies and obstacles, more enemies, keys,
blocks and water, copy abilities, and bosses.


6. TILE CAMERA

The tile camera lets you look at the tiles around Kirby one at a time, like
the camera in the audiogame BK3. Use it to learn the shape of a place: where
the platforms are, how high a ledge is, how far a drop goes, or what is on
the other side of a gap.

Push the right stick (or press I, J, K or L) and the camera moves one tile
up, left, down or right from Kirby and plays what is there. Hold it to keep
moving. The camera moves with Kirby, so it always stays near him.

While Kirby is in the air (floating, jumping or falling), the left stick
moves the camera as well. Float in place with the d-pad and look around
with the left stick, so you can find the next platform before you drop.

  Left stick click or U    say the tile in words, for example "3 right,
                           2 up, thin platform". For an open tile it also
                           says how far down the ground is: "open, ground
                           2 below".
  Right stick click or O   bring the camera back to Kirby. It also comes back
                           by itself after 8 seconds.

While the camera is on Kirby, the stick clicks do their usual jobs.

The tile sounds:
  soft high tick        open air
  bubble                water
  low knock             solid ground or wall
  knock, rising slide   slope going up to the right
  knock, falling slide  slope going down to the right
  wooden knock          thin platform you can jump up through
  metal clink           ladder
  buzz                  spikes
  block sounds          breakable blocks (star, metal, bomb, ice and so on),
                        the same sounds as in play
  gate sound            locked gate
  dull thump            past the edge of the area
  rising chirp          you moved the camera back onto Kirby's tile
  falling chirp         the camera jumped back to Kirby (right stick click,
                        O, or after 8 seconds)

Anything standing on the tile plays its own sound right after the tile
sound, the same sound you hear in play: a door, an item, an enemy, a switch,
a key or other thing to carry, a rift, a cannon, a hazard (icicles,
crushers, falling pillars and so on) or something to work (hang switches,
stakes, ropes). U or the left stick click names it, and tells you whether a
switch is already on or boxed in by blocks.

As with every other sound, left or right tells you which side of Kirby the
tile is on, and the pitch goes up one step for each tile above him. The Tile
camera category in Learn Sounds plays all of these.


7. WHAT SPEAKS

Menus and text
- Text on screen when it appears: titles, menus, tips, level and stage
  names, messages. Button pictures in text are read as button names ("Hold
  down minus"). A few are not known yet and are read as "button 12" and so
  on; please tell me which button each one is.
- The menu item you move to, with its place, for example "File 2 Empty,
  2 of 3" or "Return to Game, 1 of 3". Menu buttons that are only pictures
  are named too: Ninja Dojo, Scope Shot, The Arena and The True Arena.
- The screens before the title: the Wii wrist strap warning and "Hold the
  Wii Remote sideways" (press any button to go on), then the title screen
  (press A).

Where you are
- The first time you enter a level's door room, the Lor Starcutter's deck or
  inside, a Copy Ability room or the Arena: a short description of the place
  and what to do there.
- When a stage starts: its name ("Cookie Country, stage 1 of 4"), then a
  short hint: what the stage is like, the enemies and dangers to expect, and
  what to look for (keys and locked gates, switches, cannons, Super
  Abilities and rifts). Shift+F5 says where you are again, Ctrl+Shift+F5 the
  hint, Ctrl+F5 turns hints off.

Doors and ladders
- In a level's door room: "Stage 1 door, press up" when you stand at a door,
  and "(locked)" for doors you can't open yet. On the Lor Starcutter: "Warp
  Star above you, jump into it" under the star that goes to the world map.
- In a stage, at any door: where it leads, then "Press up". For example
  "Door to the next area, area 2 of 4", "Goal door: the end of the stage",
  "Door to a side room: an Energy Sphere and two enemies", "Door out, to
  area 3 of 5". F4 lists every door in the area.
- Ladders: "Ladder up, 5 tiles. Hold up to climb", or "Ladder down" when
  Kirby stands on top of one.

Kirby
- Health at each quarter ("Health 47 percent"), "low!" below a quarter, and
  "Kirby is down!".
- A new copy ability ("Fire ability"), followed by a short explanation of
  what it does and its main moves, and "Ability lost". Shift+F3 reads every
  move of the ability, taken from the game's own pause screen, with the
  buttons on your controller: X is the Wii Remote's 1 button, A its 2, left
  bumper its A button, the triggers shake it, Back is minus. "Dash" means
  press left or right twice quickly.

Hazards
- The first time Kirby comes near one in an area: icicles, crushers, lava
  geysers, spiked bars, rolling boulders, falling pillars, hang switches,
  stakes, ropes, piñatas, currents and wind, with what to do. Shift+F2 lists
  them.


8. GAMEPLAY SOUNDS

All of these are in Learn Sounds (F11), where you can hear them one by one.
Remember the code: left or right is the side, and the pitch goes up one step
for each tile the thing is above Kirby, down when it is below.

Ground and platforms
  Landing tone   while Kirby is in the air (jumping, floating, falling), a
                 steady tone plays for the ground right under him. The closer
                 the ground, the higher the tone. A plain tone means solid
                 ground, a softer hollow tone a platform you can jump up
                 through, and a buzzy tone spikes.
  Pit            a low double pulse, repeating, while there is nothing under
                 Kirby at all. Don't drop: float on.
  New platform   a bright two-note chime when, in the air, a higher platform
                 comes under Kirby: you can land on it now.
  Landing        a soft thud when Kirby lands.
  Drop through   two soft falling notes while Kirby stands on a thin platform
                 and the way to the exit goes down: hold down to drop through
                 it ("Thin platform. Hold down to drop through" the first
                 time). Down on solid ground only ducks.
  Platform above two soft high notes when a platform is right above Kirby (up
                 to 5 tiles): you can jump up onto it. Higher notes mean it is
                 higher up.
  Ladder         two metal clinks from the ladder's side when one is within 6
                 tiles: the second higher when it goes up from here, lower
                 when it goes down. Faster and louder when closer. A soft tick
                 for every tile while climbing.
  Edges ahead    while walking: a falling slide when the ground ahead drops
                 down, a rising slide when it steps up (jump), a buzz before a
                 pit, and a flat buzz before spikes.

Walls
  A steady hum on the left or right when a wall is near on that side (up to
  5 tiles), louder the closer it is. The hum tells you how high the wall is:
  - a higher, brighter hum: a step of 1 or 2 tiles you can just jump onto;
  - a middle hum: a wall up to 12 tiles high you can jump and float over;
  - the low hum: a real wall.
  A higher hum in the middle means a ceiling right above Kirby. Walking into
  a step or wall you can get over says so ("Step, 1 high. Jump"), and F5
  gives the heights too. A low knock plays when you walk into a wall. Steps
  and slopes Kirby can walk onto are not walls.

Which way to go (route tones)
  The mod works out the way to the area's exit door. When the way goes up
  from here you hear three rising notes; when it goes down, three falling
  notes. They come from the side the way continues to. Nothing plays while
  the way just goes straight on. When the door onward (or the goal door) is
  within 10 tiles, it rings a soft bell from where it is; other doors, to
  side rooms or back, ring a lower, softer bell. F4 says where the door is,
  which way to go, and the other doors.

Enemies and obstacles
  Every enemy's sound starts with a short click: a click always means
  something you can fight or inhale. Enemies that give a copy ability when
  swallowed end with a tiny two-note sparkle. Obstacles you can't beat,
  items and blocks never click.
  About once a second every enemy within about 14 tiles gives its own sound
  from where it is, nearest first (up to five). Each kind of enemy always
  sounds the same, so you can learn them: for example a Waddle Dee is a soft
  boop, a Waddle Doo a double boop, a Bronto Burt a fluttering trill, a Sir
  Kibble a sharp metal double click, a Blade Knight a falling square tone, a
  Hot Head a hiss, a Sparky an electric buzz. Within 3 tiles the sounds come
  faster.
  When an enemy you can inhale is within reach in front of Kirby (about 3
  tiles, at his height), a soft breathy draw-in plays once. This only
  happens while Kirby has no ability, when X inhales.
  Things you can't defeat, like Gordo, Shotzo, spiky plants and falling
  rocks, give a harsh low buzz. Spikes in the ground buzz from where they
  are when they are within 4 tiles.
  When more enemies come onto the screen you hear how many ("2 enemies"); F9
  turns that off. F2 names everything.

Items
  About every second the nearest items give their sound: food and Maxim
  Tomatoes a warm bloop, Energy Spheres a sparkling chime, 1-Ups a rising
  two-note fanfare, copy ability stars a bright two-note ping, Invincible
  Candy a rising whistle, a Warp Star a long rising sweep. Point stars give a
  tiny ting, only within 5 tiles.
  When Kirby stands right under an item, a bright bell "bling" plays in the
  middle, and again about every second while he stays lined up: jump or
  float straight up to grab it.
  Copy ability stars, on stands or lying in stages, are named when Kirby is
  next to one ("Sword ability star").

Keys and things to carry
  A key gives a little two-note jingle from where it lies. Pick it up and
  you hear "Carrying key". Kirby can only jump once while carrying it, and a
  key held too long breaks. Locked gates give a heavy clunk and a rattle;
  while you carry a key they sound more often and from farther away. Walk
  into the gate with the key: "Gate unlocked!". Other things you can carry,
  like a Crackler, give a wooden double knock, and their name is said when
  you stand next to one.

Breakable blocks
  About every one and a half seconds, the nearest block of each kind within
  about 7 tiles sounds from where it is:
  - star block: a wooden tock-tock
  - rock block: a lower tock-tock
  - bomb block: a fuse hiss and a tock (hit it to blow up the chain of
    blocks joined to it)
  - metal block: a clank (needs a strong attack)
  - ice or snow: a glassy tinkle (melt it with fire)
  - burnable block: a crackle
  - crumbling block: a low rattle (it falls away under you)
  - heavy block: a deep thud (usually needs a Super Ability)
  Blocks count as walls and floor for all the other sounds.

Water
  A falling splash and "In water" when Kirby goes in, a rising splash and
  "Out of water" when he comes out. Under water a bubble plays every two
  seconds: the higher it is, the closer the surface. F5 says how many tiles
  up the surface is.

Bosses and mid-bosses
  When a boss fight starts you hear its name ("Boss: Goriath!", "Mid-boss:
  Kibble Blade!") and a short description: what it is, its attacks, and how
  to hurt it, for example which of its attacks you can inhale and spit back.
  If you press a keyboard key while it is being read, your screen reader
  stops talking; the mod notices and reads it again from where it was cut
  (twice at most). Controller buttons don't cut it.
  To plan a fight, pause the game (plus / Start). In a boss fight the mod
  then says "F2 or left stick click: how to beat ..."; press either one and
  the whole description is read while the game waits. Ctrl+F2 says it again
  at any time.
  During the fight a boss gives a heavy double drum beat from where it is; a
  mid-boss gives a lighter metal clang. The beats come faster when it is
  close. When a boss charges, dives or rolls toward Kirby, a fast rising
  whoosh with a low growl plays from its side at once: get out of the way.
  Its health is read at each quarter ("Goriath health 70 percent"),
  and "defeated!" at the end. F2 names the boss with its health.
  Some arenas start away from the boss (Whispy Woods's starts high above
  it). There the way guide and F4 lead to the boss until you are near its
  height.

Things thrown at you
  Shots, balls, apples, bombs, magic balls, falling rocks and the stars a
  boss leaves for you to inhale tick all the time while they fly, from where
  they are: left or right, higher when above Kirby. The ticks get faster and
  louder as the thing gets close. A sharp tick with a little hiss means it is
  coming toward Kirby; a soft, lower tick means it is moving away. Without an
  ability, when one reaches the front of Kirby's mouth a breathy draw-in
  plays: press X to inhale it, then face the boss and press X to spit it back.
  Dangerous things you can't inhale tick with a low buzz instead: enemy
  shots like Water Galboros's water and Shotzo's cannonballs, Halcandra's
  fire orbs, falling rocks, and the crystals in the Landia chase.

Barrel cannons
  The nearest barrel cannon gives a hollow thump. Inside one you fire
  yourself, the mod follows the cannon's aim: steady high double beeps mean
  the way is clear, so fire when you like. Low ticks mean something is in the
  way, like crushing walls. When moving walls open, a bright rising chime
  plays: fire right away. Soft high ticks after the chime mean you're late:
  wait for the next chime. (In Dangerous Dinner stage 3, a bot fired on the
  chime and got past the crushing walls.)

Switches
  A switch waiting to be hit clicks and clacks from where it is, up to 30
  tiles away, even when it's hidden behind walls or blocks. Closer
  switches click more often. When you come near one, its place is said, and
  whether blocks box it in.

Hazards, things to work, and Another Dimension
  A dry rattle with a hiss comes from where a hazard is (icicles, crushers,
  lava geysers, spiked bars, rolling boulders). A falling pillar sounds like
  rock: a heavy stone thud with rubble clattering after it. Things to use
  (hang switches, stakes, ropes, piñatas) give a bright two-note pluck.
  Moving ones travel as they move. In Another Dimension a low
  throbbing rumble comes from the side the wall of darkness is closing in
  from, louder and faster as it gets near.


9. STORY AND CUTSCENES

- Dialogue: what characters say in text boxes, like Magolor on the Lor
  Starcutter, is read as a whole line as soon as it starts to appear, not
  letter by letter. Press the button to go on as usual; the "Next" prompt is
  not read.
- Cutscenes: the story scenes are movies without words. When one starts you
  hear "Cutscene:" and its title, then a description of what happens, timed
  to the pictures. Every movie has its own description: the opening, each
  ship part coming back, the Lor Starcutter reborn, Halcandra, Magolor's
  explanation, both final battles, the ending and Kirby's dance. When
  Magolor explains himself after the battle with Landia, his subtitles are
  read between the descriptions. If you skip a cutscene, the description
  stops.
- The Theater (on the main menu once you have finished the game) plays any
  of them again. Moving through its list reads the movie you are on.


10. KNOWN LIMITS

- Levels 1 to 6 were swept by a test bot with a completed save: door labels,
  ladders, hazards and boss names were checked where the bot could get to
  (65 areas). It can't carry keys or solve switch puzzles, so areas behind
  those were checked from the stage files only. Dangerous Dinner was not
  swept. The darkness rumble in Another Dimension has not been heard in a
  run yet.
- A test bot fought the bosses in The Arena by the mod's sounds (with the
  Cutter ability) and beat all of them except Grand Doomer, which it reached
  twice but couldn't finish. Every hit Kirby took was checked against what
  the mod was playing: nearly all came from the boss itself or from a shot
  that ticks. The fights haven't been heard by a player yet.
- A key hasn't been carried to a gate in a test yet. Tell me if "Carrying
  key" or "Gate unlocked!" doesn't come.
- The tile camera was tested with the keyboard. The right stick hasn't been
  tested on a real controller yet, and rifts and cannons haven't been looked
  at with the camera in a test yet.
- In Dangerous Dinner, the Key Dees, power stakes, burner boxes and light
  blocks are new and haven't been met in a test run yet.
- Some carried things (balloon bomb, candle, Stomper Boot, Prism Shield) are
  named but have no explanation yet.
- The Lor Starcutter's sub-games have no special sounds yet. Scope Shot's
  own menu doesn't say which item is chosen.
- The world map is read by its text: the level you move to is named when its
  sign appears. There are no extra sounds for the map itself.
- Some pictures of buttons in text are read as "button" and a number.


11. HOW IT WORKS (for the curious)

The port runs the game's code with Dolphin's machinery, and keeps the Wii's
memory in the game process. Play Kirby Access starts KirbyRecomp.exe and loads
KirbyAccess.dll into it. The mod reads the game's memory using the names in
the game disc's own link map (iuk.MAP), and never changes the game, except
for one invisible trick to tell which text is on screen (see the source).
Nothing in the port folder is replaced.

Source: KirbyAccess-src (build.sh); notes on the memory layout are in
NOTES.md. Uses MinHook (BSD 2-clause) and Prism (MPL 2.0); licences are in
the licenses folder.
