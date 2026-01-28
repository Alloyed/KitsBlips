#!/bin/bash
set -euo pipefail
set -o xtrace

# shortcut for configuring and building everything all at once. Walk away for a bit!
git submodule update --init

export CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Debug -G Ninja"

cmake -S kitdsp -B kitdsp/build $CMAKE_FLAGS
cmake -S kitdsp-gpl -B kitdsp-gpl/build $CMAKE_FLAGS
cmake -S vcvrack -B vcvrack/build $CMAKE_FLAGS
cmake -S kitgui -B kitgui/build $CMAKE_FLAGS
cmake -S clapeze -B clapeze/build $CMAKE_FLAGS
cmake -S daw -B daw/build $CMAKE_FLAGS
cmake -S daisy -B daisy/build -DCMAKE_BUILD_TYPE=MinSizeRel

./rebuild-all.sh
