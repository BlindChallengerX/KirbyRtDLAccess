#!/bin/sh
# Builds Kirby Access with the 64-bit w64devkit. Output in out/:
#   out/Play Kirby Access.exe        launcher (goes next to KirbyRecomp.exe)
#   out/KirbyAccess/KirbyAccess.dll  the mod
set -e
cd "$(dirname "$0")"
# Put w64devkit (64-bit) on PATH first, or set W64DEVKIT to its folder.
[ -n "$W64DEVKIT" ] && export PATH="$W64DEVKIT/bin:$PATH"
MH=third_party/minhook
mkdir -p out/KirbyAccess
gcc -O2 -Wall -s -mwindows -municode -o "out/Play Kirby Access.exe" src/launch.c -static-libgcc -lwininet -lbcrypt
gcc -O2 -Wall -Wno-unused-function -Wno-format-truncation -shared -s -o out/KirbyAccess/KirbyAccess.dll \
  src/dllmain.c src/speech.c src/guest.c src/audio.c src/game.c src/scene.c src/play.c src/objects.c src/gimmicks.c src/abilities.c src/areas.c src/text.c src/msg.c src/movie.c src/learn.c src/pad.c src/tools.c $EXTRA \
  $MH/src/hook.c $MH/src/buffer.c $MH/src/trampoline.c $MH/src/hde/hde64.c \
  -I$MH/include -Ithird_party/prism -Isrc -static-libgcc -lole32 -luuid -lsapi -lwinmm -lshell32 -lxinput1_4
cp -u third_party/prism/prism.dll out/KirbyAccess/
echo built
