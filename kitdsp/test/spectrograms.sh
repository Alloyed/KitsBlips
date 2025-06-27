#!/bin/sh

#Create spectrograms of all wav files
for file in test/*.wav; do
    [ -e "$file" ] || continue
    outfile="${file%.*}.png"
    sox "$file" -n spectrogram -o "$outfile"
done