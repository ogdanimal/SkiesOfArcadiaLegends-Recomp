#!/usr/bin/env bash
# The app icon art is by Reckoner (credited in README.md and release/CREDITS.md).
# The branded app's launcher icon from one picture of the logo (any size, transparent background
# best): release/android-icon/ (Android resources: legacy mipmap-*/skies_launcher.png on a rounded
# square, and an adaptive icon whose foreground keeps the logo inside the 66/108 safe zone) and
# release/skies.ico (the Windows release's exe icon, 16-256 px), plus drawable-xhdpi/skies_banner.png
# (the app's banner, from release/launcher-background.png). Needs ImageMagick.
# usage: tools/make-skies-icon.sh LOGO.png [BACKGROUND=#121A2C]
set -eu
cd "$(dirname "$0")/.."
LOGO=$1 BG=${2:-#121A2C}
OUT=release/android-icon
rm -rf $OUT
mkdir -p $OUT/mipmap-anydpi-v26 $OUT/values
# Trim the transparent margin so the logo itself is what gets fitted.
SRC=$(mktemp --suffix=.png); convert "$LOGO" -trim +repage -background none "$SRC"
for d in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
  dpi=${d%%:*} px=${d##*:}; mkdir -p $OUT/mipmap-$dpi
  r=$((px * 18 / 100)); inner=$((px * 84 / 100))
  # Legacy icon: the logo on a rounded square of the background colour.
  convert -size ${px}x${px} xc:none -fill "$BG" -draw "roundrectangle 0,0,$((px-1)),$((px-1)),$r,$r" \
    \( "$SRC" -resize ${inner}x${inner} \) -gravity center -composite $OUT/mipmap-$dpi/skies_launcher.png
  # Adaptive foreground: 108 dp canvas, logo within the central 66 dp (a little less, 62).
  fg=$((px * 108 / 48)); logo=$((fg * 62 / 108))
  convert -size ${fg}x${fg} xc:none \( "$SRC" -resize ${logo}x${logo} \) -gravity center -composite \
    $OUT/mipmap-$dpi/skies_launcher_foreground.png
done
cat > $OUT/mipmap-anydpi-v26/skies_launcher.xml <<XML
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/skies_launcher_background"/>
    <foreground android:drawable="@mipmap/skies_launcher_foreground"/>
</adaptive-icon>
XML
cat > $OUT/values/skies_launcher.xml <<XML
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <color name="skies_launcher_background">$BG</color>
</resources>
XML
# TV-style banner (app-level android:banner; launchers such as the Retroid's Radikal show it in their
# app drawer): the launcher art cropped 16:9 from the right, a navy fade on the left, the logo on it.
ART=release/launcher-background.png
if [ -f "$ART" ]; then
  mkdir -p $OUT/drawable-xhdpi
  T=$(mktemp -d)
  convert "$ART" -gravity east -crop "$(identify -format '%[fx:int(h*16/9)]x%h' "$ART")+0+0" +repage \
    -resize 640x360! $T/art.png
  convert -size 360x640 gradient:"${BG}EB"-"${BG}00" -rotate -90 $T/fade.png
  convert "$SRC" -resize 280x $T/logo.png
  convert $T/art.png $T/fade.png -composite $T/logo.png -gravity west -geometry +28+0 -composite \
    $OUT/drawable-xhdpi/skies_banner.png
  rm -rf $T
fi
convert "$SRC" -background none -gravity center -resize 256x256 -extent 256x256 \
  -define icon:auto-resize=256,128,64,48,32,16 release/skies.ico
rm -f "$SRC"
find $OUT -type f | sort; ls -la release/skies.ico
