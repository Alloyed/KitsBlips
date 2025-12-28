This is the monorepo for all kitsblips VCV Rack plugins.

# Building

Building on windows requires mingw-64, see [official docs](https://vcvrack.com/manual/Building#Windows) for details. you will need to run cmake from a mingw shell.

```bash
$ mkdir -p vcvrack/build && cd vcvrack/build
$ cmake ..
$ cmake --build .

$ cmake --install . --prefix dist
$ cp -r dist/* ~/.local/share/Rack2/plugins/lin-x64/
# or
$ cp -r dist/* $LOCALAPPDATA/Rack2/plugins-win-x64/
```

## SVGs
to prep, do:
```
cp -r ../panels/*.svg res/ && ./exportsvgs.py res
```