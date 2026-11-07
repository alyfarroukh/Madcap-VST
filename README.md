# Madcap

A granular voice instrument for **Windows and macOS**: load or record a voice, cut it, slice
it and scatter it into a pitched granular cloud that follows the chords you play. Or let the
**image sweeper** turn a picture into music. Both feed an arpeggiator and a re-orderable FX
chain (drive, compressor, tape delay, ambient reverb).

Desktop port of **Madcap Vita** (PS Vita), inspired by the Chase Bliss / Hologram Microcosm
and MOOD MkII.

| Format | Windows | macOS |
| --- | --- | --- |
| VST3 | yes | yes (universal: Apple Silicon + Intel) |
| AU | – | yes (universal) |
| Standalone app | yes | yes (universal) |

Instrument, stereo out, MIDI in. The only audio input is for recording (sidechain in a DAW,
microphone in the Standalone app).

---

## Getting the builds (no local toolchain needed)

Every push runs the GitHub Actions workflow in `.github/workflows/build.yml`:

1. Push this folder to a GitHub repository (or run the workflow by hand: **Actions → Build
   Madcap → Run workflow**).
2. When the run is green, open it and download the artifacts at the bottom of the page:
   * **Madcap-Windows**: `Madcap-Windows-VST3.zip`, `Madcap-Windows-Standalone.zip`
   * **Madcap-macOS**: `Madcap-macOS-VST3.zip`, `Madcap-macOS-AU.zip`, `Madcap-macOS-Standalone.zip`
   * **pluginval-logs-…**: the validation reports

The workflow has three parts:

* **DSP tests:** the framework-free engine and its Catch2 tests on Linux (with
  AddressSanitizer + UBSan), Windows and macOS.
* **Plugin builds:** Windows (Visual Studio, x64) and macOS (Xcode, universal
  `arm64;x86_64`, ad-hoc signed).
* **Validation:** pluginval at **strictness level 5** on the VST3 (both platforms) and the
  AU (macOS), plus Apple's `auval`. A failing validation fails the build.

## Installing

### Windows
Unzip `Madcap-Windows-VST3.zip` and copy the **`Madcap.vst3`** folder to

```
C:\Program Files\Common Files\VST3\
```

Rescan plugins in your DAW. The Standalone app (`Madcap.exe`) runs from anywhere.

### macOS
Unzip, then copy:

* `Madcap.vst3` → `~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/` for all users)
* `Madcap.component` → `~/Library/Audio/Plug-Ins/Components/` (or `/Library/Audio/Plug-Ins/Components/`)
* `Madcap.app` → `/Applications/`

The builds are **ad-hoc signed, not notarized**, so macOS quarantines them when they're
downloaded. Clear the quarantine flag once after copying:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Madcap.vst3
```

```bash
xattr -cr ~/Library/Audio/Plug-Ins/Components/Madcap.component
```

```bash
xattr -cr /Applications/Madcap.app
```

For Logic, also reset the AU cache if Madcap doesn't show up:

```bash
killall -9 AudioComponentRegistrar; auval -a | grep Madcap
```

Logic lists it under **AU Instruments → Madcap Audio → Madcap**.

---

## Using Madcap

The top bar shows the six tabs, the input (green dot = SAMPLE, amber = IMAGE), the tempo,
the current chord, the presets (◀ name ▶ SAVE), UNDO / REDO, **RND** (randomize), the
level meter and the big **PLAY / STOP** transport button.

* **PLAY / STOP** stops everything (grains, arp and FX tails). Press it again to restart.
  With *Follow host transport* on (SAMPLE tab), stopping the host stops Madcap too.
* **Undo / redo:** Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z (or Ctrl+Y). Ctrl/Cmd+1…6 switches tabs.
* Drop an audio file or a picture anywhere on the window to load it.
* Knobs: drag (Shift = fine), mouse wheel, double-click resets, arrow keys when focused.
  The value appears in a bubble while you touch a knob; hover for a tooltip.
* The window is resizable (75–200 %) and stays sharp at any size.

### MIDI
* **DEGREE mode** (default): a held note picks the **chord degree** nearest to it in
  KEY / SCALE. Hold several notes for a cluster chord. The note's octave sets OCTAVE
  (C4 = 0).
* **ROOT mode:** the played note becomes the chord root directly (chromatic).
* **LATCH on:** the last chord keeps playing when you let go. **LATCH off:** silent until you
  play, like a normal synth.
* Velocity scales the grain and arp level (VELOCITY amount on the PLAY tab).
* The mod wheel goes to an assignable destination (MOD WHEEL, default Spray). Pitch bend is
  ±2 semitones on everything.

### 1 · SAMPLE
* Waveform with orange **IN / OUT** flags. Drag them, or swipe across the waveform to draw a
  new cut. Double-click selects the whole sample. **CROP** makes the cut permanent.
* **LOAD:** WAV, AIFF, FLAC, MP3, OGG (or drop a file). Mixed to mono, resampled to the
  engine rate, up to 60 s. **DEMO** brings back the built-in sung "ahh" on C.
* **SAMPLE ON** (top left) makes the sample the live input. Only one input plays at a time,
  so the IMAGE tab greys out (and vice versa). The SAMPLE / IMAGE switch is on both tabs.
* **REC / STOP** records from the **RECORD INPUT**: in the Standalone app, pick the audio device
  and the input channel your mic or guitar is plugged into (or open **Audio settings**). In a
  DAW, enable Madcap's sidechain and route your mic / guitar track to it. Choose all inputs, input 1
  or input 2, and watch the level meter. The take becomes the new sample.
* **PLAY** auditions the cut dry (also while stopped), with **LOOP**.
* **SLICES** 1 / 2 / 4 / 8 / 16, **GRID** (equal) or **HITS** (transients inside the cut).
  Tap or drag across the slice strip, or use SLICE / SPAN, to choose where the grains read.
* **ROOT:** the note the sample was sung on. **INPUT:** SAMPLE or IMAGE.
* **Tempo:** follows the host by default (TEMPO HOST / MANUAL plus a BPM field). The
  Standalone app shows a **TAP** pad and the BPM field (30–300).

### 2 · IMAGE (image sweeper)
* A scan line sweeps left to right across the picture and each column becomes sound.
  Click and hold the picture to grab and scrub the line.
* **IMAGE ON** (top left) makes the image sweeper the live input (the SAMPLE tab greys out).
* ◀ ▶ step through four built-in Bauhaus posters. **LOAD IMAGE** takes PNG, JPG, BMP or GIF
  of any size. The **whole picture** is used at its own aspect ratio, never cropped, and the
  analysis grid follows its resolution (up to 512 × 256 cells).
* **TERRAIN** turns the picture into a 3D wireframe landscape (brightness = height, or
  darkness with LISTEN TO DARK) that rolls through the fixed scan line (orange front edge).
* **SOUND SOURCE:** NOISE (band-passed noise), NOTES (PolyBLEP oscillators) or both.
* **MODE:**
  * SPECTRAL: each band of rows is a scale note, so bright areas sound as chords.
  * BRIGHT: one voice that follows where the brightness sits.
  * HEIGHT: every edge the line crosses plays a note at its height.
* **KEY / SCALE / LOW / RANGE** set the image's own scale. The resulting note count is shown.
* **STEPS:** 0 = smooth (held, sliding tones), or any number of steps per sweep from 1 to 128,
  each a separate note. The step grid is drawn on the picture. **VOICES**, **WAVE**,
  **DECAY**, **SWEEP** time (1–60 s), **SYNC** (step = 16th note, locked to the host bar),
  **LISTEN TO** light or dark (posters default to DARK, photos to LIGHT), **LEVEL**, **DRY**.
* Harmony applies: the PLAY-tab interval stacks companions on every image note, and the
  chord degree shifts the melody up the scale. **HOLD** freezes the 6 s live buffer that
  the grains and arp read from.

### 3 · GRAIN
**GRAINS** on/off, **MODE** (Cloud, Mosaic, Arp, Strum, Tunnel, Glide), **HOLD**, interpolation,
and the knobs SIZE, DENSITY, POSITION, SPRAY, SCAN, REVERSE, STEREO, OCT JUMP, DETUNE, GLIDE,
TONE (LP ↔ HP) and LEVEL. The grain field shows the grains moving over the waveform or the
live buffer.

### 4 · ARP
**ARP** on/off, **PATTERN** (Up, Down, Up/Dn, Rand, Stab), **RATE** (1/4 … 1/32, synced to the
host bar), a 16-step lane (click or drag to mute steps; LENGTH; ALL / RANDOM / CLEAR), OCT,
GATE, SWING, ATTACK, RELEASE, START, WALK, GRAINS UNDER and LEVEL. The orbit display shows the
chord notes around a ring, with the note that's playing lit.

### 5 · FX
DRIVE (Tape / Fuzz / Crush, 2× / 4× oversampling), COMP, DELAY (tape, free or synced,
ping-pong, wow/flutter) and REVERB (8-line modulated FDN, decay 0.8 s → 60 s → INFINITE,
width 0–150 %). Move a module with ◀ ▶: the order on screen is the order of the chain, also
available as the automatable **FX Order** parameter.

### 6 · PLAY
The grain field XY pad (X = position, Y = spray), KEY / SCALE / OCT, VOICING, MASTER (−40…+18 dB, followed by a look-ahead limiter
so it never clips), the interval strip, and seven chord pads I–VII. Click a pad to select it;
Shift-click adds it to a cluster. Pads light up when MIDI selects them.

### Presets
14 factory presets (Choir Cloud, Glass Mosaic, Tunnel Drone, Strum Pad, Arp Shimmer, Glide
Choir, Infinite Haze, Tape Crush Pulse, Stab Arp, Sixteenth Walk, Image Pent Melody, Height
Sketch, Spectral Noise Field, Image Grain Cloud). They're also exposed to the host as
programs. **SAVE** writes a user preset, including its sample and picture, to:

* Windows: `%APPDATA%\Madcap\Presets\`
* macOS: `~/Library/Application Support/Madcap/Presets/`

**RND** randomizes every page whose **LOCK** (bottom right of each page) is off.

### Projects
The DAW project stores every parameter, the sample (embedded as FLAC), the picture (PNG), the
cut, the slice selection, the arp lane, the randomize locks and the window size. A project
opens identically on another machine.

---

## Building locally (optional)

Requirements: CMake ≥ 3.22, a C++20 compiler (Visual Studio 2022, Xcode 14+, or GCC 11+ /
Clang 14+ for the DSP tests). JUCE 8.0.15 and Catch2 are fetched automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --config Release --target Madcap_All
```

DSP tests only (no JUCE needed), optionally with sanitizers:

```bash
cmake -S . -B build-tests -DMADCAP_BUILD_PLUGIN=OFF -DMADCAP_SANITIZE=ON
```

```bash
cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
```

Plugins end up in `build/plugin/Madcap_artefacts/Release/{VST3,AU,Standalone}`.

## Source layout

```
dsp/                    framework-free engine (no JUCE): unit-testable
  src/*.c, *.h          the Madcap Vita C engine (engine, grains, slicer, theory, arp, fx,
                        image synth, posters, demo voice), fixed for any rate / block size
  include/madcap/       C++20 wrapper: lock-free source/image hand-off, parameter smoothing,
                        MIDI chord / latch / velocity logic, UI snapshot atomics, resampler
tests/                  Catch2 tests (safety, rates & block sizes, bounds, image harmony, MIDI)
plugin/                 JUCE 8 plugin
  Source/PluginProcessor.*   audio, state (FLAC/PNG), presets, recording, randomize
  Source/SampleStore.* ImageStore.* ImageDecode.*   loading off the audio thread
  Source/ui/            HUD look and feel, ring knobs, displays, pages, top bar
  Resources/fonts/      Rajdhani (SIL Open Font License)
.github/workflows/      CI: tests, builds, pluginval, artifacts
```

### Real-time safety
The audio thread never allocates, locks, logs or touches files. Samples and pictures are
decoded and resampled on a background thread and handed over through single-slot atomic
mailboxes. The replaced buffers go onto a lock-free retire queue that the message thread
frees. The editor reads only atomics, at the display refresh rate.

### What changed from the Vita engine
* Any sample rate (44.1–192 kHz): every buffer (delay, reverb lines, pre-delay, 6 s live
  buffer, limiter look-ahead) is sized at init, and the one-pole coefficients tuned at
  48 kHz are converted for the actual rate.
* Any block size, down to 1 sample: no fixed 256-sample blocks. The image analysis runs on a
  time-based control clock, and smoothing, metering and glides are time-based.
* The looper was removed. Added: MIDI (degree / root modes, latch, velocity, pitch bend,
  mod wheel), host tempo and bar sync, separate grain / image HOLD, cubic grain
  interpolation, 128 grains, drive oversampling, tape wow/flutter, and a tighter look-ahead
  limiter whose latency is reported to the host.

## Licenses
Madcap source: yours. Rajdhani font: SIL Open Font License 1.1 (`plugin/Resources/fonts/Rajdhani-OFL.txt`).
picojpeg and stb_image: public domain. JUCE 8: AGPLv3 / commercial (see juce.com for the
terms that apply to distributing plugins).
