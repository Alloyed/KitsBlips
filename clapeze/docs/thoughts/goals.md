# clapeze design goals

- clapeze (as the name might imply) is **clap-first**. it's implemented in terms of the clap C API, but VST and AU support are provided via optional "wrappers" from the clapfirst project.
- clapeze is **easy, more than it is simple or fast**. a feature complete plugin can be built in a couple hundred lines of C++, and we do that by assuming your plugin is also "easy", and working toward defaults that make sense for that.
- clapeze is **low-scope**. it's to build clap plugins and that's it. large dependencies, like GUI libraries, should not be directly included.

all of this adds up to a reasonably thin layer, and honestly, that's the part I find most exciting. you don't need JUCE! you can build your own framework, with your own opinions, about as easily as I built mine, and still get the benefits of good cross platform and cross host support.
