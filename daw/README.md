# KitsBlips DAW

To create VSTs, instead of using an existing mega-library I'm trying out writing directly to the CLAP plugin api and taking advantage of `clap-wrapper`, which is an upstream package to turn clap plugins into VSTs. We'll see how that goes!

```bash
mkdir -p daw/build && cd daw/build
cmake ..
cmake --build .
```

## Assets

raw gltf files right out of blender aren't great, so we use gltfpack on the result: run

```sh
pack-meshes.sh
```

and also only load gltf files marked .packed.
