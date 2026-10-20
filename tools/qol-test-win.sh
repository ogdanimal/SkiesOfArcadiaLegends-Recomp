#!/usr/bin/env bash
TOOLS=$(dirname "$(readlink -f "$0")")  # before any cd
# QOL option checks on Windows (patches/game/qol.c + VideoCommon/SkiesMenu.cpp), with the dev runner.
# usage: tools/qol-test-win.sh GAME MODULE TEST [KEY=VALUE ...]
#   GAME / MODULE as Windows paths under C:\dev\SkiesRecomp (a DOL with qol.c and its module)
#   TEST: rewards     wins the first battle (02-first-battle) by tapping A; prints Vyse's EXP, the
#                     party's gold and Vyse's magic EXP afterwards (unpatched game: 3, 5, 2)
#         encounters  flies forward on the world map (10-worldmap-android) for up to 60 s; prints
#                     when a battle starts (game mode 7)
#         invincible  first battle with [Skies] Invincible: lowest HP of Vyse, MP and Spirit
#         speed       deck battle (16-deck-battle-ws10): game frames per second (frame counter
#                     0x803475C0) before and after a Z tap
#         field       field speed (ws26): STATE (default 10-worldmap-android, flying forward; also
#                     01-opening-cutscene, 03-valuan-ship-field) at [Skies] FieldSpeed 1-3 set live:
#                     game frames per second and the emulation speed; then FieldSpeed after a Z tap
#                     (the game's: unchanged) and after Z+X (the next speed)
#         ship        Baltor ship battle (17b-ship-battle-baltor-commands, made with ws15) with
#                     [Skies] Invincible=1 (pass it): qol_ship_live (0x80002BA8, ws17 on), the ship's
#                     HP and the map through the battle and into a second one; the flag should be 0 on
#                     load (an older state), set from the first phase, 0 after the results
#   KEY=VALUE: [Skies] settings for the run, e.g. Exp=400 Gold=400 MagicExp=400 Encounters=0
#   RUNNER: the runner to start (default the dev build's; the release one is
#   lib\ModernGekko\build-release\skies-run.exe, which takes only the pinned DOL)
set -u
cd "${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}"
GAME=$1 MODULE=$2 TEST=$3; shift 3
U=build/user-qoltest A=build/auto-qoltest
rm -rf "$U" "$A"; mkdir -p "$A/commands"; cp -r build/user-bt "$U"
{ echo; echo "[Skies]"; for kv in "$@"; do echo "${kv%%=*} = ${kv#*=}"; done; } >> "$U/Config/Dolphin.ini"
cmd.exe /c "${RUNNER:-lib\\ModernGekko\\build\\moderngekko-run.exe} --game $GAME --module $MODULE --user-dir build\\user-qoltest --automation-dir build\\auto-qoltest > build\\auto-qoltest\\run.log 2>&1" &
D="python3 $TOOLS/drive.py"
d() { AUTO=$A timeout 120 $D "$@" 2>/dev/null | grep -v "^$"; }
word() { d peek:$1:4 | awk '{print strtonum("0x"$2$3$4$5)}'; }
byte() { d peek:$1:1 | awk '{print strtonum("0x"$2)}'; }
d wait:6 >/dev/null
case $TEST in
rewards)
  d load:build/savestates/02-first-battle.sav wait:3 >/dev/null
  for i in $(seq 1 90); do
    d tap:a:4 wait:0.4 >/dev/null
    [ $((i % 10)) = 0 ] && [ "$(word 0x8030B818)" != 0 ] && break
  done
  d wait:3 >/dev/null
  echo "Vyse EXP $(word 0x8030B818)  gold $(word 0x8030BB40)  battle data exp/gold/magic $(word 0x80EC0670)/$(word 0x80EC0674)/$(byte 0x80EC0641)"
  d peek:0x8030B838:0x18
  d peek:0x80002EA0:0x20 ;;
invincible)
  # Vyse's battle record (lbl_80309DE4[0]): HP +0x14, max +0x18, MP +0xFE / +0xFF; Spirit in the
  # battle data (lbl_80347390): +0x8 / +0xA, max +0x6. Lowest HP seen while tapping A through the
  # first battle.
  d load:build/savestates/${STATE:-02-first-battle}.sav wait:2 >/dev/null
  low=99999
  for i in $(seq 1 ${TAPS:-60}); do
    d tap:a:4 wait:0.4 >/dev/null
    [ "$(word 0x803475CC)" = 7 ] || break
    u=$(d peek:0x80309DE4:4 | awk '{print $2$3$4$5}')
    hp=$(word 0x$(printf '%08X' $((0x$u + 0x14)))); max=$(word 0x$(printf '%08X' $((0x$u + 0x18))))
    [ "$hp" -lt "$low" ] && low=$hp
    [ $((i % 2)) = 0 ] && { b=$(d peek:0x80347390:4 | awk '{print $2$3$4$5}'); echo "tap $i: HP $hp/$max  MP $(byte 0x$(printf '%08X' $((0x$u + 0xFE))))/$(byte 0x$(printf '%08X' $((0x$u + 0xFF))))  Spirit $(d peek:0x$b:0xC | awk '{print strtonum("0x"$10$11)"/"strtonum("0x"$8$9)}')"; }
  done
  echo "lowest HP seen: $low  (Vyse EXP after: $(word 0x8030B818))" ;;
encounters)
  d load:build/savestates/10-worldmap-android.sav wait:3 >/dev/null
  start=$(date +%s)
  while [ $(( $(date +%s) - start )) -lt 60 ]; do
    d hold:main_y=1:20 >/dev/null
    m=$(word 0x803475CC)
    [ "$m" = 7 ] && { echo "battle after $(( $(date +%s) - start )) s, last frames-since-battle $last"; break; }
    last=$(word 0x80346D28)
  done
  [ "$m" = 7 ] || echo "no battle in 60 s (frames since battle: $(word 0x80346D28))"
  d peek:0x80002EA0:0x20 ;;
ship)
  # The state was made with Spirit at 0, and a state from before ws17 has the flag at 0, so
  # Invincible's refill waits for the first phase: Focus first (the stick turns the command ring),
  # then each cycle opens Attack, moves to the second cannon (the first is often greyed: reloading
  # or too dear) and confirms. The battle ends in ~30 cycles at 1x, but how depends on where the
  # ring lines up: 2026-10-12 one run won (map 205, Vyse EXP 231), one fled (map 90, EXP 25). What
  # counts is the flag (1 in battle, 0 after its end) and the ship's HP (10000 throughout).
  d load:build/savestates/${STATE:-17b-ship-battle-baltor-commands}.sav wait:4 >/dev/null
  echo "after load: live $(byte 0x80002BA8)  map $(word 0x80311AC4)  mode $(word 0x803475CC)"
  for try in 1 2 3 4 5 6 7 8; do
    d tap:b:4 wait:0.6 hold:main_y=-1:8 wait:0.6 tap:a:4 wait:0.6 >/dev/null
    for k in 1 2 3 4 5 6; do d tap:a:4 wait:0.5 >/dev/null; done
    [ "$(byte 0x80002BA8)" != 0 ] && { echo "Focus played a phase (try $try): live $(byte 0x80002BA8)"; break; }
  done
  low=99999
  for i in $(seq 1 ${CYCLES:-60}); do
    d tap:a:4 wait:0.5 hold:main_y=-1:6 wait:0.3 tap:a:4 wait:0.5 tap:a:4 wait:0.5 tap:a:4 wait:0.5 >/dev/null
    map=$(word 0x80311AC4); live=$(byte 0x80002BA8)
    if [ "$map" -lt 500 ] || [ "$map" -gt 599 ]; then
      d wait:3 >/dev/null
      echo "left the battle at cycle $i: map $(word 0x80311AC4)  live $(byte 0x80002BA8)  Vyse EXP $(word 0x8030B818)"
      break
    fi
    data=$(d peek:0x8034727C:4 | awk '{print $2$3$4$5}')
    ship=$(d peek:0x$(printf '%08X' $((0x$data + 0x20))):4 | awk '{print $2$3$4$5}')
    hp=$(word 0x$(printf '%08X' $((0x$ship + 0x18)))); max=$(word 0x$(printf '%08X' $((0x$ship + 0x14))))
    [ -n "$hp" ] && [ "$hp" -gt 0 ] && [ "$hp" -lt "$low" ] && low=$hp
    [ $((i % 5)) = 0 ] && echo "cycle $i: live $live  map $map  mode $(word 0x803475CC)  ship HP $hp/$max"
  done
  echo "lowest ship HP seen in battle: $low"
  # A second battle (the party has the ship now): the freed data's pointer is still set, the flag not
  echo "before a second battle: data $(d peek:0x8034727C:4 | awk '{print $2$3$4$5}')  live $(byte 0x80002BA8)"
  d poke:0x80305CF0:$(printf 'me500a.sct\0\0' | xxd -p) poke:0x80311AEC:0000000f wait:3 >/dev/null
  echo "second battle: map $(word 0x80311AC4)  data $(d peek:0x8034727C:4 | awk '{print $2$3$4$5}')  live $(byte 0x80002BA8)" ;;
speed)
  d load:build/savestates/16-deck-battle-ws10.sav wait:3 >/dev/null
  rate() { a=$(word 0x803475C0); s1=$(date +%s.%N); d wait:5 >/dev/null; b=$(word 0x803475C0); s2=$(date +%s.%N)
           echo "game frames/s $(echo "($b - $a) / ($s2 - $s1)" | bc -l | cut -c1-5)  mode $(word 0x803475CC)  speed setting $(word 0x80002EB4)  $(grep -E '^speed|^fps' $A/status.txt | tr '\n' ' ')"; }
  rate
  d tap:z:3 wait:1 >/dev/null; rate
  d tap:z:3 wait:1 >/dev/null; rate
  d shot:$(pwd)/build/qoltest-speed-a.png wait:0.3 shot:$(pwd)/build/qoltest-speed-b.png >/dev/null
  grep -i "BattleSpeed" "$U/Config/Dolphin.ini" ;;
field)
  d load:build/savestates/${STATE:-10-worldmap-android}.sav wait:3 >/dev/null
  [ "${STATE:-10-worldmap-android}" = 10-worldmap-android ] && d press:main_y=1 >/dev/null
  rate() { a=$(word 0x803475C0); s1=$(date +%s.%N); d speed:5 > $A/speed.out; b=$(word 0x803475C0); s2=$(date +%s.%N)
           echo "game frames/s $(echo "($b - $a) / ($s2 - $s1)" | bc -l | cut -c1-5)  mode $(word 0x803475CC)  map $(word 0x80311AC4)  speed word $(d peek:0x80002EB4:4 | awk '{print $2$3$4$5}')  $(tr '\n' ' ' < $A/speed.out)"; }
  for n in 1 2 3; do
    d config:Dolphin.Skies.FieldSpeed=$n wait:2 >/dev/null; echo -n "FieldSpeed $n: "; rate
  done
  d config:Dolphin.Skies.FieldSpeed=1 release wait:1 tap:z:3 wait:1 >/dev/null
  echo "after a Z tap: $(grep -i "FieldSpeed" "$U/Config/Dolphin.ini")  speed word $(d peek:0x80002EB4:4 | awk '{print $2$3$4$5}')"
  d hold:z=1:6 hold:z=1,x=1:6 wait:1 >/dev/null
  echo "after Z+X: $(grep -i "FieldSpeed" "$U/Config/Dolphin.ini")  speed word $(d peek:0x80002EB4:4 | awk '{print $2$3$4$5}')" ;;
esac
d stop >/dev/null; wait
