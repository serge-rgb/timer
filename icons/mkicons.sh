if [ ! -d timer.iconset ]; then
   mkdir timer.iconset
fi
pushd timer.iconset
convert -alpha on -background none -resize 16x16\! ../TimerIcon.png icon_16x16.png
convert -alpha on -background none -resize 32x32\! ../TimerIcon.png icon_16x16@2x.png
convert -alpha on -background none -resize 32x32\! ../TimerIcon.png icon_32x32.png
convert -alpha on -background none -resize 64x64\! ../TimerIcon.png icon_32x32@2x.png
convert -alpha on -background none -resize 128x128\! ../TimerIcon.png icon_128x128.png
convert -alpha on -background none -resize 256x256\! ../TimerIcon.png icon_128x128@2x.png
convert -alpha on -background none -resize 256x256\! ../TimerIcon.png icon_256x256.png
convert -alpha on -background none -resize 512x512\! ../TimerIcon.png icon_256x256@2x.png
convert -alpha on -background none -resize 512x512\! ../TimerIcon.png icon_512x512.png
convert -alpha on -background none -resize 1024x1024\! ../TimerIcon.png icon_512x512@2x.png
popd
iconutil -c icns timer.iconset
