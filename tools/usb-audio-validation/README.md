# USB audio hardware validation

These macOS tools verify native 44.1 kHz USB audio. Capture works with Beets;
duplex playback and diagnostic MIDI commands require the muted fm1-base USB
audio diagnostic. Its loopback mode keeps the speaker and headphones silent.
The scripts do not flash firmware, save settings, or change MASTER.

Install dependencies and compile the shared CoreAudio inspector here:

```sh
brew install libusb portaudio
python3 -m venv .venv
.venv/bin/pip install numpy sounddevice mido python-rtmidi pyusb
swiftc coreaudio-inspect.swift -o coreaudio-inspect
```

`coreaudio-inspect` reads physical formats and channel names in both directions.
`usb-descriptors.py --product 'FM1 USB Audio Test' --get-rate` reads the actual
USB descriptors. Capture supports GET_CUR; fixed-rate playback has no sampling
frequency control, so its descriptor and physical CoreAudio format establish
its rate. The probe never claims interfaces or detaches drivers.

For diagnostic loopback, use the exact MIDI port listed by
`mido.get_output_names()`. macOS may retain the older name `X0X FM-1`.

```sh
.venv/bin/python diagnostic-midi.py --port 'X0X FM-1' --mode 2 --mask 63
.venv/bin/python duplex.py --seconds 60 --output out/roundtrip
.venv/bin/python analyze.py out/roundtrip.wav --expected-peak-dbfs -18
```

Duplex uses independent input and output callbacks because macOS exposes the
USB directions as separately clocked audio devices. It records the sent and
received PCM, both physical formats, channel names, USB descriptors and callback
flags. Startup flags in the first half-second remain in the report; steady-state
flags fail the check. The waveform analyzer also excludes that settling period.
Repeat with `--mask 1`, `2`, `4`, `8`, `16`, and `32`, passing the same mask to
`analyze.py`, to check each route. Use `duplex.py --signal alignment` followed by
`analyze.py CAPTURE.wav --mode alignment` for shared-impulse channel alignment.

Run latency measurements only after the functional suite passes on the final
firmware. The mono test sends uniquely spaced impulses on channel 1:

```sh
.venv/bin/python duplex.py --mono-wallclock --blocksize 128 --seconds 10 --output out/latency128
.venv/bin/python analyze-wallclock.py out/latency128
```

Repeat at block size 256 when useful. The report gives pulse count and minimum,
median, 95th percentile and maximum application wallclock delay, using
`perf_counter_ns` at the output handoff and input receipt callbacks. It separately
reports sample-timeline lag and an ADC/DAC timestamp estimate. Independent streams
start at different times, so their sample-index lag includes that offset. Application delay
includes host buffering and scheduling; it does not isolate firmware latency.

For Beets capture tests, use `../smoke-usb-stems.py`; its existing `capture.py`
interface remains supported. Keep the firmware hash, JSON reports and WAV files
together. These tests provide no analog electrical measurement, USB wire trace,
firmware counter proof, or iPad compatibility result.
