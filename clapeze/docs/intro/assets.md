# Assets

clapeze supports bundling assets directly in your plugin binary using miniz[^miniz].

To use it, simply concatenate a .zip file to the end of your plugin:

```bash
zip -r - my-assets-folder >> my-plugin.clap
```

and in code you can write plugins that access your zip file with the `AssetsFeature` class[^src]:

```cpp
class Plugin : public clapeze::BasePlugin {
    [...]
    void Config() override {
        ConfigFeature<clapeze::AssetsFeature>(GetHost());
    }
}
```

To pull a file from the assets directory:

```cpp
const char* path;
BasePlugin& plugin = /*...*/;
auto& assets = clapeze::AssetsFeature::GetFromPlugin<clapeze::AssetsFeature>(plugin);
bool ok = assets.LoadResourceToString(path, out);
```

paths follow the format in your zip file. you can use `unzip -l <thefile>` to see what assets are available, and what path to use to access them.

## How does it work?

super simple: your .clap executable file starts with some kind of header[^elf] to signal to the computer what kind of file it is and what it contains:

![Elf layout, containing a header and several extension](https://upload.wikimedia.org/wikipedia/commons/7/77/Elf-layout--en.svg)

Zip files, by comparison, _end_ with a central directory[^zip] which represents all the data you need to understand the file:

![a diagram of a zip file, containing multiple entries and a central directory at the end](https://upload.wikimedia.org/wikipedia/commons/6/63/ZIP-64_Internal_Layout.svg)

Put simply, given the same file, a computer told to look for the CLAP will look from the start, but a computer told to look for a zip will look from the end. Each format won't see the other. This technique was stolen from the LÖVE[^love] game framework, but works equally well here.

[^miniz]: https://github.com/richgel999/miniz
[^src]: [source](/clapeze/include/clapeze/ext/assets.h)
[^elf]: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#File_layout
[^zip]: https://en.wikipedia.org/wiki/ZIP_(file_format)#Structure
[^love]: https://love2d.org
