# 125A Chorderator V1 — Musical behavior contract (engineering candidate)
**Branch:** develop/v1.0.0. **Status:** implementation candidate, requires independent tests and Studio One acceptance.

## Chord and scale semantics

- KEY + SCALE define seven allowed **trigger roots** when CHROMATIC ROOTS is OFF. Chord notes are formed from stacked diatonic thirds and fifths across all nine available scales.
- CHROMATIC ROOTS ON permits non-scale triggers. A non-scale root currently produces a major triad rooted at the played MIDI pitch. This is a deliberate, predictable fallback, **not** harmonization into the selected scale.
- NORMAL uses the diatonic third and fifth. SUS2 replaces only the third with a major second (2 semitones); SUS4 replaces it with a perfect fourth (5 semitones). Both **retain** the original fifth quality.
- Therefore CHROMATIC ROOTS OFF does **not** promise that every output note stays inside the key. SUS2/SUS4 may contain non-scale suspension pitches. Diminished and augmented fifths are preserved and reported as sus2(b5), sus2(#5), sus4(b5), sus4(#5). POWER deliberately uses a perfect fifth, even on scale degrees with diminished fifth.
- Classical **melodic minor** is modeled as melodic minor **ascending** in both directions, consistently; no contextual descending scale transformation.

## Four CHARACTER voicings

For root pitch `r`, original third interval `t` and fifth interval `f`:

| Mode | Deterministic OFF | Example C major (MIDI note numbers) |
|---|---|---|
| CLASSIC | `r, r+t, r+f` compact root-position triad | 60,64,67 |
| DARK | `r, r+f, r+t+12`: open fifth, less exposed third in the low register | 60,67,76 |
| WIDE | `r, r+t+12, r+f+12`: upper voices one octave higher | 60,76,79 |
| POWER | `r, r+7, r+12`: fifth and octave, third deliberately omitted | 60,67,72 |

The incoming root is **always the lowest note**, in the exact input octave. If a character profile would exceed MIDI 127, use the closest valid candidate/fallback without emitting invalid notes. These profiles alter note registers, not plugin audio processing or gain.

## VOICE LEADING

- OFF plays the exact character profile regardless of history.
- AUTO chooses a close valid upper-voice arrangement against the **preceding chord**, maintaining the requested character's reference register and fixed played bass. It must never modulate the chord pitch classes.
- The selected voicing for each **absolute input pitch + NORMAL/SUS2/SUS4 modifier** is saved in bounded processor memory. When that exact note/variant recurs, its previously chosen full voicing is reused, even after a different chord and regardless of how many semitones the returning bass drops.
- Memory invalidates whenever KEY, SCALE, CHROMATIC ROOTS, CHARACTER, LEADING, ROOT OCTAVE DOUBLE change, or upon explicit transport/reset lifecycle flush. Thus repeated loops in an uninterrupted passage remain stable. Starting in the middle of a song after a reset may choose a different first voicing because the history before the start position is unavailable. Full host seek reproducibility across arbitrary start positions cannot be guaranteed by a history-dependent algorithm; users may select OFF for fully context-independent playback.
- There are no threshold-based attempts to guess phrase boundaries from a 10-, 11- or 12-semitone bass jump.

## Other MIDI rules

- ROOT OCTAVE DOUBLE adds `r+12` only. It must not follow the highest chord voice to `r+24`. Do not emit duplicate octave if already contained, as in POWER.
- Incoming NoteOn velocity 1–127 must be retained on all newly generated MIDI NoteOns, including repeat events using a stored voicing. MIDI 0 is reserved for note release.
- Overlapping shared output pitches currently use reference-counted **legato** ownership: a repeated attack on a note that is already held does **not** cause a second NoteOn until its ownership fully releases. This is explicit V1 behavior, not proof of a successful re-articulation design. If per-note re-striking is wanted later, it requires its own ownership-safe NoteOff/NoteOn lifecycle design and regression.
- MIDI control keys in this Studio One octave convention: C0=24/NORMAL, D0=26/SUS2, E0=28/SUS4. They are always silent and recognized only on the exact sample where a musical chord begins (held controls do not latch).
- ROOT OCTAVE and variant modifiers are output-only; input NoteOff releases the original note set even when settings change.
- Optional seventh/add9 and adjustable MIDI control-key location from the early **DRAFT** are **not** part of the current implemented V1 GUI; they are not silently presented as working features.

## QA required before release

- Independent golden reference note tables for every scale, tonic and chord degree.
- Golden CHARACTER pitch arrangements with OFF and nontrivial AUTO; repeated four-/eight-note motifs across all characters and octave-double states.
- Velocity 1,19,37,109,127 and resetting musical memory after parameter changes.
- SUS pitch and altered-fifth identity, CHROMATIC/POWER exceptions, NoteOn/Off ownership and overlap.
- Windows VST3 build/Steinberg validator, Plugin Tester, and native Studio One recording/loops/transport/preset recall/100% and 150% GUI.
- A green technical build does not mean musical acceptance.
