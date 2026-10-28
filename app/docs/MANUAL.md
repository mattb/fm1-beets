# Playing Beets

Beets is a two-deck performance loop slicer. Each break has eight equal slices.
With the five probabilities at zero it plays a straight loop; turn the knobs to
repeat, skip, reverse, stutter or switch loops on each eighth-note.

## First groove

Power on the browser emulator, then press **PLAY**. Deck A plays the first demo
break at 160 BPM. Deck B is muted at boot: **GLO / MUTE B** brings it in. MASTER
sets output volume and PRESETS blends the two decks.

The same controls work on the FM-1. Browser labels show the Beets function above
its physical button name. Hardware installation has not yet been verified.

## Both decks under your hands

The left eight white keys always control A slices 1–8. The right eight always
control B slices 1–8. This does not depend on which deck is focused.

- Tap a key to cue that slice on the next eighth-note. When stopped, it auditions
  one slice immediately (the deck must be unmuted).
- Hold two white keys in one deck to set an inclusive range between them.
- **LFO / LOCK** locks the focused deck to its current slice; press again to open it.
- **HOME / FULL** returns to the full 1–8 range. Slice eligibility masks remain as set.
- **REC / RE-CUE** cues both decks to their range starts on the next eighth-note,
  preserving the clock. Stop/start PLAY to restart the clock from the downbeat.

## Focus is only for editing

**OCT−** focuses A; **OCT+** focuses B. Both lanes remain visible on the display.
Black keys 1–8 select the eight original Beets demo loops; 9–10 select the two Amen
loops. Black key 11 is unassigned. Selection is quantized while running.

| Knob | Function |
| --- | --- |
| SELECT | Global tempo, 60–220 BPM |
| PRESETS | Global A/B blend |
| ALGORITHM | Loop-switch probability |
| KNOB 1 | Repeat probability |
| KNOB 2 | Skip probability |
| KNOB 3 | Reverse probability |
| KNOB 4 | Stutter probability |

Probability knobs move in 5% steps. Repeat means repeat a slice, not sequence
backwards. Reverse changes sample playback direction. Each event is sampled
independently; every bar returns to the first eligible slice of the chosen range.
A manual loop selection takes priority over random switching at that boundary.

## Performance buttons

**SEL / MUTE A** and **GLO / MUTE B** independently mute the two decks.
**FX / BYPASS** bypasses the focused deck's random mutations without clearing its
probabilities. Held effects are still explicit overrides.

Hold **ENV** to force reverse or **ARP** to force a short stutter. These changes
apply at the next slice boundary. The effect remains attached to the deck selected
when the button went down, even if focus changes while it is held. Release to end.

**SEQ / NEXT** enables automatic loop advance when the slice range wraps.

Hold **EDIT / MASK** and tap white keys to toggle which slices can be chosen;
hold EDIT and tap black keys to toggle which loops can be chosen by switching or
auto-advance. At least one eligible slice and loop are retained, so the engine
cannot get stuck. Normal manual loop selection re-enables that loop.

## Save

Stop playback, then press **SAVE**. Tempo, blend, deck focus, samples, ranges,
probabilities, mutes, bypass, masks and auto-advance are restored on next boot.
Transport always boots stopped; held performance gestures are not saved. During
playback SAVE reports **STOP TO SAVE**, avoiding a flash-write interruption.
The browser keeps these saved objects in its own local storage. Factory reset in
the browser asks for a second click before clearing the emulator's saved setup.

## Browser controls

Click/touch keys and buttons. Drag knobs up/down or scroll them. Focus a knob and
use arrows for one detent or Page Up/Down for five. Shift-click latches a key so a
single mouse can hold a two-key range; click it again to release.

When the panel has focus, `P` starts/stops, `Z`/`X` focus A/B, `QWERTYUI` play A,
`ASDFGHJK` play B, `1…0` choose loops, and Escape restores full range. Keyboard
shortcuts can be disabled below the instrument.

## Sound and scope

All ten bundled recordings are the actual original Beets demos, credited to
vcvcvc_val. Originals are preserved in the repository. To fit internal flash,
the shared engine uses 22.05 kHz mono μ-law data, interpolated to 44.1 kHz output.
Tempo changes pitch as in rate-based sample playback; this is not time stretching.

The port retains the two-lane slicing and probability performance core. It does
not include Norns/Softcut, arbitrary sample loading, slice metadata editing,
filter/resonance/pan pages, arc or crow integration. USB audio output from the
instrument is compiled in; Bluetooth and Wi-Fi are omitted to keep memory free.
MIDI notes 36–43 trigger slices (channel 1 A, channel 2 B); MIDI start/stop are
handled. MIDI clock sync and external Web MIDI routing are not implemented.
Firmware installation, USB MIDI and bounded processing measurements have been
verified. Physical audio quality and control feel still need hands-on validation;
see the [current verification record](../../docs/VERIFICATION.md) for scope and evidence.
