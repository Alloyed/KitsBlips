### CLAP

To create VSTs, instead of using an existing mega-library I'm trying out writing directly to the CLAP plugin api and taking advantage of `clap-wrapper`, which is an upstream package to turn clap plugins into VSTs. We'll see how that goes!

```bash
$ mkdir -p clap/build && cd clap/build
$ cmake ..
$ cmake --build .
```
