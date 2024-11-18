#!/bin/sh

OCD_SCRIPTS_DIR="/C/Program Files/DaisyToolchain/openocd/scripts"
TARGET=target/stm32h7x.cfg
PGM_DEVICE=interface/stlink.cfg

openocd -s "$OCD_SCRIPTS_DIR" -f "$PGM_DEVICE" -f "$TARGET" -c "program $1 verify reset exit"