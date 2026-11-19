#!/usr/bin/env bash
# A repeatability sweep of the standalone: every start twice (A and B) with the same presses, then each start's
# errors, whether A and B dumped the same frames, and a contact sheet of A's frames (ImageMagick's montage).
#   tools/sa-sweep.sh STARTS OUT [SECS]
#     STARTS  a file, one start per line: REGION START (as tools/sa-run.sh takes them; # comments). card:N lines
#             take CARD_DIR from the environment.
#     OUT     report.txt, a folder per run, sheets/<name>.png
#     SECS    guest seconds per run (default 40)
# The presses (A, D-pad down, A, right, A, B every 6 s) are SWEEP_PRESS, a pattern repeated over the run
# ("+0.5:A,+1.5:DOWN,..." within one period of SWEEP_PERIOD seconds); a card Continue starts them at 8 s.
# Found nothing on 2026-11-19 over 47 starts (HANDOFF-emulator-free, Sweep).
# SPDX-License-Identifier: GPL-3.0-or-later
set -u
cd "$(dirname "$0")/.."
STARTS=${1:?usage: tools/sa-sweep.sh STARTS OUT [SECS]} OUT=$(realpath -m "${2:?}") SECS=${3:-40}
PATTERN=${SWEEP_PRESS:-0.5:A,1.5:DOWN,2.5:A,3.5:RIGHT,4.5:A,5.5:B} PERIOD=${SWEEP_PERIOD:-6}
mkdir -p "$OUT/sheets"
: > "$OUT/report.txt"
i=0
while read -r region start; do
  [ -z "${region:-}" ] || [[ $region == \#* ]] && continue
  i=$((i + 1))
  name=$(printf "%02d-%s-%s" "$i" "$region" "$(basename "${start%.*}" | tr ':' '-' | cut -c1-40)")
  offset=0
  [[ $start == card:* ]] && offset=8
  presses=$(python3 -c "
import sys
secs,period,offset=float(sys.argv[1]),float(sys.argv[2]),float(sys.argv[3]); pat=sys.argv[4]
out=[]; k=0
while offset+k*period<secs:
    for item in pat.split(','):
        at,name=item.split(':'); a=offset+k*period+float(at.lstrip('+'))
        if a<secs: out.append('+%.2f:%s'%(a,name))
    k+=1
print(','.join(out))" "$SECS" "$PERIOD" "$offset" "$PATTERN")
  for run in A B; do
    SKIES_PRESS="$presses" tools/sa-run.sh "$OUT/$name-$run" "$start" "$region" "$SECS" > /dev/null
  done
  a=$OUT/$name-A b=$OUT/$name-B
  differing=0
  for f in "$a"/frames/*.ppm; do
    [ -f "$f" ] || continue
    g=$b/frames/$(basename "$f")
    [ -f "$g" ] && ! cmp -s "$f" "$g" && differing=$((differing + 1))
  done
  frames=$(ls "$a/frames" | wc -l)
  errors=$(grep -c -i "fatal\|couldn't\|can't\|invalid\|unmapped\|bad access\|abort\|exception" "$a/run.log")
  printf "%s frames=%s errors=%s differing=%s %s\n" "$name" "$frames" "$errors" "$differing" \
    "$(grep '\[host\] speed' "$a/run.log" | tail -1)" | tee -a "$OUT/report.txt"
  if [ "$frames" -gt 0 ] && command -v montage >/dev/null; then
    montage "$a"/frames/*.ppm -tile 8x -geometry 240x180+2+2 -title "$name" "$OUT/sheets/$name.png" 2>/dev/null
  fi
done < "$STARTS"
echo "done: $(grep -c '' "$OUT/report.txt") starts" | tee -a "$OUT/report.txt"
