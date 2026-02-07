# Notes on individual hosts

Generally speaking you can debug individual hosts by turning off process isolation and attaching to the host/audio process for the host. Visual Studio uses the Ctrl-Alt-p shortcut for this, and on linux you can use this script (I named mine gdb-attach)

```bash
#!/bin/bash

if [[ "$(cat /proc/sys/kernel/yama/ptrace_scope)" != "0" ]]; then
  sudo sh -c 'echo 0 > /proc/sys/kernel/yama/ptrace_scope'
fi

gdb -p "$(pidof "$1")"
```

## Bitwig

process name: `BitwigAudioEngine-X64-AVX2`

- bitwig requires a state feature; so if you don't actually need state use noopStateFeature
- bitwig will automatically save an init patch and re-use it on later uses of a plugin with the same id. consider it a test of your migration system :p while in development you may want to make state management unconditionally true

### Bitwig+Linux

- sometimes, when using the "With Bitwig" option for process isolation, I get a crash in bitwig's gpu code? scary

## Reaper

process name: `reaper`

### Reaper+Linux

- reaper on linux has absolutely no concept of high dpi. I need to look more into this

## clap-host (VST)

process name: `clap-host`

## clap-wrapper (VST)

Surprise! this is a host of a sort.
