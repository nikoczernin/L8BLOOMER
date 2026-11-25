# Built with a little help

This plugin uses **JUCE 9.0.5** by Raw Material Software Limited, under its AGPLv3 option. The full details are in [JUCE’s license](JUCE-LICENSE.md) and the project’s [AGPLv3 license](../LICENSE.md).

JUCE brings along some other libraries for things like graphics and audio plugin support. Their credits and license texts are kept in `licenses/`. There’s also a [full dependency list](JUCE.spdx.json) from JUCE, including optional bits this plugin doesn’t use.

Each release has a **source-with-JUCE** download with JUCE and its original notices included. You can also [browse JUCE 9.0.5 on GitHub](https://github.com/juce-framework/JUCE/tree/9.0.5).

The build script checks the JUCE download against this SHA-256:

```text
d92207f51ba728d3d195b6d5e93ecd049717a915fc250ff6844c32ea7272315a
```

JUCE’s JPEG support is based in part on the work of the Independent JPEG Group. Its full notice is in `licenses/modules/juce_graphics/image_formats/jpglib/README`.
