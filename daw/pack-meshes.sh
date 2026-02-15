#!/bin/bash

find assets -maxdepth 1 -name '*.glb' ! -name '*.packed.glb' -print0 |
while IFS= read -r -d '' file; do
  # -cc draco mesh compression
  # -tc basis texture compression
  # -kn keep node names
  # -mi use mesh instancing
  # -mm merge instances of the same mesh
  #gltfpack -v -cc -tc -mi -i "$file" -o "${file%.glb}.packed.glb"
  gltfpack -v -tc -i "$file" -o "${file%.glb}.packed.glb"
done
