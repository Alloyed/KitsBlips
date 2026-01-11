# Why no GUI integration?

clapeze does _not_ attempt to integrate a GUI library or or provide much gui specific code at all: there is `BaseGuiFeature`, but it more just forwards the exact API clap provides onto the user.

this is for a few reasons:

### GUI is diverse

to put it simply, there are a lot of different visions of what good plugin UI is, and most of them require some level of panache and stylization beyond what a traditional native UI toolkit or web framework thinks about.

the current approach most developers have, which seems to be either shapes rendering directly using a vector graphics library, or a set of png flip books, are uninspiring and flat to me. So I want to explore alternate graphics and interaction technologies, and deeper integration is Worse for swapping those in and out.

### the clap API is Bad(tm)

`CLAP_EXT_GUI` is built to match the existing capabilities the VST API, which is very old, so I can't give it too much shit. but it has a few design misfeatures IMO:

- it leaves interaction between the plugin and host entirely up to the OS windowing API
- OS windowing APIs have lots of small details that are different in between them. in practice, your only hope is to write platform specific integrations that take all that into account.
- stepping back? I'm not sure why we want windows(the OS concept). each DAW has native instruments and effects, and very few do floating windows. windows are hard to multitask with or get an overall view of a signal chain. Browsers and mobile devices don't have windows at all!

the more I think about it, the more I want to see vendor and host-specific experimentation. the most comfy plugins for Ableton are the ones with max4live frontends. I would unironically relish the opportunity to make an "ok" GUI for the DAWs I don't use, but a perfect GUI for the DAWs I do prioritize.

### Do I always need all of that?

this is more related to small, focused plugins. often times, it feels like I'm pulling in multiple MBs of dependencies and art to draw 4 knobs, and it often feels like an unwarranted jump in complexity.

## takeaway

all of this, combined, leaves me Unsatisfied with the UI choices I have right now, despite caring about it a lot. so. your problem now >:p have fun!
