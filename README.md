# L8BLOOMER

An auto-swell plugin for Logic Pro. Pick a note and let the volume rise, a filter open up, or the sound drift into a metallic frequency shift. It listens for pick attacks, so you don’t need silence between notes.

**[Download L8BLOOMER 1.3.1 for macOS](https://github.com/nikoczernin/L8BLOOMER/releases/download/v1.3.1/L8BLOOMER-1.3.1-macOS-universal.zip)** · [All releases](https://github.com/nikoczernin/L8BLOOMER/releases) · [Controls and manual](docs/MANUAL.md)

![L8BLOOMER interface](docs/interface.png)

## Get it into Logic

Works on **Intel and Apple Silicon Macs** running **macOS 11 or later**, with a compatible version of Logic Pro. You can use it on mono or stereo tracks.

1. Download and unzip **`L8BLOOMER-1.3.1-macOS-universal.zip`** using the link above.
2. Quit Logic Pro.
3. In Finder, choose **Go → Go to Folder…** (`Shift–Command–G`) and enter:
   ```text
   ~/Library/Audio/Plug-Ins/Components
   ```
   Create the `Components` folder if it’s missing.
4. Copy the whole **`L8BLOOMER.component`** into that folder. If you’re updating, keep your old copy somewhere outside `Components` as a backup.
5. Reopen Logic and insert **Audio FX → Audio Units → Niko Audio → L8BLOOMER**.

There’s also an **`Install.command`** helper if you’d rather let it handle the copying and backups. In Terminal, type `bash `, drag `Install.command` into the window, and press Return.

macOS may block the download because it’s locally signed and hasn’t been notarized by Apple. If that happens, or Logic can’t find it, the [install guide](docs/INSTALL.md) walks you through it.

## Give it a go

Start with **Volume**, **Direction IN**, **Attack 500 ms**, and **Sensitivity 50%**. Each pick the detector catches starts a fresh sweep, even over a ringing note or amp noise. Put delay and reverb after it to preserve their tails.

- Volume, high-pass, low-pass, and frequency-shift modes, plus bypass.
- Attack from 1 ms to 20 seconds, transient sensitivity, and one-way IN/OUT sweeps.
- Cutoff and resonance; frequency-shift mode instead offers FROM/TO endpoints (±5000 Hz).
- Input, output, master mix, and adjustable minimum retrigger time.
- Optional low-cut/high-cut filtering of the trigger detector only.
- Double-click a knob to reset it, or type in a value. Settings save with your session, and you can automate them.

Every mode reports zero fixed processing latency, and bypass leaves your audio untouched. A little of the pick can get through while the detector reacts. The frequency shifter also changes phase, even at 0 Hz, so use bypass when you want the original sound.

The [manual](docs/MANUAL.md) explains the controls, and the [test notes](docs/VALIDATION.md) cover what’s been checked.

## Build it yourself

Install Apple’s **Xcode Command Line Tools** (`xcode-select --install`) and **CMake 3.22+** ([CMake downloads](https://cmake.org/download/), or `brew install cmake` if you use Homebrew).

```sh
git clone https://github.com/nikoczernin/L8BLOOMER.git
cd L8BLOOMER
bash scripts/build.sh
bash scripts/install.sh
auval -v aufx SlBl Niko -strict
```

The script downloads **JUCE 9.0.5**, checks the download, builds for **Apple Silicon and Intel**, and runs the tests. Your plugin ends up in `dist/L8BLOOMER.component`. If you already have JUCE 9.0.5, set `JUCE_DIR=/absolute/path/to/JUCE-9.0.5` before the build command. You can adjust the number of parallel build jobs with `CMAKE_BUILD_PARALLEL_LEVEL` (default 6).

You can also grab **source-with-JUCE.tar.gz** from the release page. It has the matching source and JUCE already packed together. Unpack it and run `bash scripts/build.sh`—you’ll still need Apple’s Command Line Tools and CMake.

To make your own release package, run `bash scripts/package-release.sh` after building and committing your changes. It puts the plugin, installer, source, JUCE, and download checksums in `dist/releases/`.

## Use it, tweak it, share it

This is open source under [AGPLv3](LICENSE.md). You’re welcome to use it, change it, and share it. If you pass the plugin on, including a version you’ve changed, make its source available under the same license too. That’s the short version; the linked license has the full terms.

Built with JUCE and a few other libraries. Their [credits and licenses are here](ThirdParty/README.md).

© 2026 Niko Czernin.
