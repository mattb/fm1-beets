# Beets app

The portable Beets instrument lives here. See the [project README](../README.md)
for builds and the [player manual](docs/MANUAL.md) for controls.

- `app.c` / `app.h`: FM-1 controls, screen, MIDI input and persistent settings.
- `dsp/engine.c`: two-deck sample playback and probabilistic slice selection.
- `fm1.toml`: all-target build configuration and sample generation.
- `web/emu/`: browser panel with the same physical control mapping.
- `tests/`: scripted native-host scenarios and screen captures.

Build output belongs in the ignored `build/` directory.
