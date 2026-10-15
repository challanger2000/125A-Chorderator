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

With VOICE LEADING **OFF**, the incoming root is the bass as described in the CHARACTER table. With **AUTO**, the incoming MIDI note specifies the *harmonic root and register*, not necessarily the actual bass. AUTO can play **root position**, **first inversion** (third in bass), or **second inversion** (fifth in bass). Musical identity and GUI chord name always follow the input harmonic root, not the lowest emitted MIDI pitch.

## VOICE LEADING

- **OFF** plays the deterministic CHARACTER profile in the preceding table, with root in the bass.
- **AUTO** enumerates octave placements of all three chord factors, sorts the emitted notes into bass/middle/top, and scores movement of **all three voices** against the preceding chord (including the real bass). It may choose C–F–A for a played F and D–G–B for a played G. It never changes the chord's three pitch classes.
- Bass may lie as much as one octave below the played root. The entire chord stays in a bounded register near the played pitch. Upper voices can move either **up or down**.
- Example C-major input triggers **C3 → F3 → G3 → C3** may produce **C3–E3–G3 → C3–F3–A3 (F/C) → D3–G3–B3 (G/D) → C3–E3–G3**. The second and third chords are genuine **second inversions**; the input F/G is the harmonic root, NOT the bass.
- AUTO stores the first selected three-/four-note voicing for each **absolute trigger pitch + NORMAL/SUS2/SUS4 variant**. Subsequent identical triggers replay that exact chord until musical settings change or the processor resets. This prevents repeated phrases from climbing indefinitely, without inferring phrase starts from arbitrary leap sizes.
- The fixed-size memory is invalidated by KEY, SCALE, CHROMATIC ROOTS, CHARACTER, LEADING, ROOT OCTAVE DOUBLE changes, and by STOP/reset. A song started mid-phrase can initially choose a different voicing because missing earlier chord history cannot be inferred without an explicit song timeline. OFF remains completely history-independent.
- POWER intentionally remains its documented root–fifth–octave power chord, not an ordinary three-factor major/minor inversion (it contains no third).
- The harmonic chord root is stored separately from the emitted lowest note so that **inverted basses cannot mislabel the chord**.
- The actual note count when ROOT OCTAVE DOUBLE is ON is three or four: root+12 is added exactly once and is omitted if it already appears as one of the three voiced factors.

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
