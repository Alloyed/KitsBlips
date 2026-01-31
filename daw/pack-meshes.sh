#!/bin/bash

find assets -maxdepth 1 -name '*.glb' ! -name '*.packed.glb' -print0 |
while IFS= read -r -d '' file; do
  #gltfpack -v -cc -tc -mi -i "$file" -o "${file%.glb}.packed.glb"
  gltfpack -v -i "$file" -o "${file%.glb}.packed.glb"
done
