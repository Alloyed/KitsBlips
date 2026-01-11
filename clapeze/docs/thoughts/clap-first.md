# Why go clap-first?

when I tell musician friends that my main plugin format is CLAP, they're understandably confused. for them, VST is _the_ plugin format. how could another format with less support be worthwhile, especially given that my favorite DAW, renoise, doesn't support it?

### licensing

There was news on this front _incredibly_ recently, but it was at the forefront of my mind when making this decision, so it's important to mention: Steinberg is the sole corporate owner of the VST format, and their stewardship has made it hard to use VST in open-source for a really long time.

VST2 plugins could only be made by personally requesting the headers from steinberg: no open-source plugins could distribute them directly. Right now, (even though there are still older DAWs that _only_ support VST2), they will not send you these headers anymore. there's no legal way to make a new VST2 plugin, in 2026.

VST3 plugins (until incredibly recently) followed a model of "ask for a commercial license/use the existing license under the GPL". While this wouldn't have been a problem per-se (I could have just made clapeze GPL to use that version of the license), it's still a headache! How could you make an automatic build system that knows whether or not the user has one of those commercial licenses or not?

Now, as of writing, this has been Fixed(tm): recent VST3 headers are available under the MIT license. But it wasn't like that when i made the decision :p

### Closed development model

VST is stewarded by a closed, corporate entity. clap is an open source project, with development happening in the open through github issues and pull requests. If I have a problem or a suggestion, I have much more confidence I can communicate with the developers of the format to discuss it with clap than with VST, and that's valuable to me.

### clap is a superset of VST (for right now)

Because clap is built to be used basically everywhere VST is currently used right now, it has a lot of focus on making sure there's feature parity with VST3. the clap-wrapper project, maintained by the clap devs, ensures this by hand translating each feature. This means I can reasonably suspect that if I do something with the clap api, I'm not "missing out" by not looking into the VST api and seeing what it does instead.

This could easily change in the future: maybe VST continues to develop, but clap becomes moribund. If so, that sucks, but wrapped VST3s I make right now should continue to work up until hosts stop supporting VST3 entirely (which is possible! look what happened to VST2).

## takeaway

Summing up, It's very much an emotional decision, rather than an informed take from working with VST very directly. But even on that emotional level, it gives me the warm fuzzies to try the New Good Idea, and be a (small) piece of what makes it sink or swim :)
