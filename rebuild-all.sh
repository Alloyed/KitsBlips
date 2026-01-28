#!/bin/bash
set -euo pipefail
set -o xtrace

# shortcut for building everything all at once. assumes you've already done your configuring
cmake --build kitdsp/build
cmake --build kitdsp-gpl/build

cmake --build vcvrack/build

cmake --build kitgui/build
cmake --build clapeze/build
cmake --build daw/build

cmake --build daisy/build

# projects with unit tests
ctest --test-dir kitdsp/build
ctest --test-dir kitdsp-gpl/build
ctest --test-dir clapeze/build
ctest --test-dir daw/build
