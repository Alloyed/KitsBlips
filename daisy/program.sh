#!/bin/bash

OCD_SCRIPTS_DIR="/C/Program Files/DaisyToolchain/openocd/scripts"
TARGET=target/stm32h7x.cfg
PGM_DEVICE=interface/stlink.cfg

function program_jtag() {
    openocd -s "$OCD_SCRIPTS_DIR" -f "$PGM_DEVICE" -f "$TARGET" -c "program $1 verify reset exit"
}

function program_usb() {
    DAISY_PID="df11"
    STM_PID="df11"
    INTERNAL_ADDRESS="0x08000000"
    USBPID="$STM_PID"
    FLASH_ADDRESS="$INTERNAL_ADDRESS"

    dfu-util -a 0 -s "$FLASH_ADDRESS:leave" -D "$1" -d ",0483:$USBPID"
}

program_jtag $1