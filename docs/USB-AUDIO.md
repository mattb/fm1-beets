# USB audio routing

Beets exposes six PCM16 capture channels over its class-compliant USB interface:

| Channels | Names | Signal |
| --- | --- | --- |
| 1–2 | Master L, Master R | Existing mix, including BLEND, MASTER and mono/split routing |
| 3–4 | Deck A L, Deck A R | Deck A after its effects and mute envelope, before BLEND and MASTER |
| 5–6 | Deck B L, Deck B R | Deck B after its effects and mute envelope, before BLEND and MASTER |

Each deck currently produces mono audio, duplicated into its pair. Stems use the
engine's existing 0.9 headroom. Moving BLEND fully to one deck leaves the other
available on its separate recording channels. Muting a deck silences its stem.
Turning MASTER down silences the speaker and master pair while stems keep playing.
Host applications may show channel numbers instead of the supplied names.

USB capture runs at the engine's native 44.1 kHz. USB playback from the computer
to the FM1 is disabled. Stereo analog output keeps its existing mono/split option
and MASTER behavior.

## Platform contract

The pinned fm1-base fork adds an opt-in `usb_audio_capture_channels` list in `fm1.toml`.
An app supplies `APP_USB_CAPTURE` and an `app_usb_capture` callback to fill the capture
buffer after rendering. Beets renders its engine once, retaining the deck samples
for this callback. It does not run separate engines for USB and analog output.

Existing firmware can keep its TOML and `app_render` unchanged: default capture
remains the traditional stereo mix with the existing MASTER/full-level policy.
The default descriptors and packet PCM are tested directly against upstream.
Additional named channels require explicit configuration. See the base's
`include/plat.h` and `examples/usb-audio` for the API
and a diagnostic that forces analog output to zero.

The platform also supports independently named USB playback channels through
`usb_audio_playback_channels` and `APP_USB_PLAYBACK`. Its receive callback runs
before rendering, giving the app raw audio to process; ordinary firmware keeps the
existing stereo mix. Beets currently exposes capture only and does not record or
monitor USB input. The base diagnostic tests both directions with silent analog
output.

## Repeating hardware tests

`tools/usb-audio-validation/README.md` covers dependencies and compilation
of the CoreAudio helpers. The diagnostic checks channel ordering, tone amplitude,
isolation, alignment and continuity. It verifies the physical stream format and
USB endpoint sampling rate during recording. A requested WAV rate alone may hide
host resampling.

With Beets installed, run `tools/smoke-usb-stems.py` using the project Python venv.
It requires those compiled helpers in that validation directory. The script forces MASTER
to zero before playing, records each deck and mute/blend/split combination, then
restores controls and leaves transport stopped before releasing MASTER to the pot.
It never saves settings. It does not verify the analog signal electrically.

Hardware results and coverage limits belong in [VERIFICATION.md](VERIFICATION.md).
