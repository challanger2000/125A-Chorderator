# 125A Chorderator — V1.0.0 Product & MIDI Contract (DRAFT)

Status: DESIGN DRAFT, NOT IMPLEMENTED, NOT TESTED. Engineering source of truth: [125A-Engineering/START-HERE.md](https://github.com/challanger2000/125A-Engineering/blob/main/START-HERE.md).

## Goal
A Windows x64 VST3 MIDI chord processor for Studio One. The user composes with single MIDI notes in the DAW; the plugin expands each accepted trigger into a musically appropriate chord in the selected key/scale. No internal sequencer, Generate, Strum, or rhythm engine.

## Accepted product requirements
- KEY (12 semitones) and SCALE selection; display allowed root-note names clearly.
- Scale-aware diatonic chord generation. C major: C -> C major, E -> E minor, A -> A minor, G -> G major; include diminished triads where required (B -> Bdim).
- DAW trigger pitch octave determines register. Trigger NoteOn begins chord; its NoteOff ends chord.
- Non-scale root triggers generate no notes with CHROMATIC OFF (default); CHROMATIC ON supports explicitly defined out-of-scale triads (harmonization policy to specify/test).
- Per-chord NORMAL/SUS2/SUS4; optional diatonic 7th and add9; never forced globally.
- Separate muted MIDI control notes for per-chord variants, with readable GUI legend and adjustable, non-overlapping control range. Exact selection semantics and same-timestamp event ordering MUST be defined and host-tested before implementation.
- VOICE LEADING OFF/AUTO; no random inversions. CHARACTER CLASSIC/DARK/WIDE/POWER. Character modes need distinct documented pitch-generation algorithms, not superficial labels; POWER omits the third.
- ROOT OCTAVE DOUBLE ON/OFF: duplicates only the chord ROOT above the generated chord, never a full-chord octave duplicate. Must work with inversions and avoid duplicate pitches.
- Live display of chord name and output pitch list. Optional click-to-preview of diatonic chords subject to confirmed host event delivery.
- Produces ordinary MIDI note events for downstream instrument/DAW recording, subject to Studio One routing behavior.

## Correctness contracts and unresolved design validations
1. Music theory: specify degree stacking for each scale; distinguish harmonic/melodic minor scale policy (melodic minor ascending vs classical contextual use); spell enharmonics appropriately; SUS and add9 may contain out-of-scale chord tones only under explicit, documented policy.
2. Root octave vs inversion: input root pitch is an anchor, not a promise that every inverted chord has root in bass. Define permissible pitch span, deterministic voice-leading tie-breaks and behavior when starting transport at arbitrary positions. A history-dependent optimiser cannot promise the same voicing when seeking into a song without preceding chord context.
3. Controls: separate playable roots from muted control events by configurable ranges. Specify how one-time per-chord control notes associate with a trigger, how NORMAL is restored, how zero-length / overlapping controls behave, and how reordering within same host sample offset is handled; test exact note-event sequences.
4. Lifecycle: maintain ownership/reference counts or equivalent tracking for generated overlapping pitches; every emitted NoteOn must pair with a valid NoteOff. Account for release while sustain is active, repeated triggers, velocity, transport jump/stop, bypass, deactivation, state changes and event-output capacity/backpressure.
5. No parameter changes may strand held notes. State/automation defaults must be stable and project-recall reproducible.
6. GUI: 125A branded design comes from 125A-Branding authoritative assets; 100/150% scaling, fully mapped interactive controls, visible control-note legend and explicit GUI/host test.
7. QA: deterministic music theory tests per key/scale/degree, same-sample control-order permutations, negative/non-scale cases, note lifecycle stress and official Steinberg VST3 validator; studio-host testing independent of CI. Follow relevant 125A Engineering guardrails and QA/repository workflow.
8. VST3 design: confirm exact MIDI event input/output contracts and whether a host accepts processor-generated downstream MIDI events and editor preview before relying on them. SDK and host evidence required.

## Execution sequence
- Verify actual SDK and host contracts; derive detailed note mapping and MIDI lifecycle test fixtures.
- Finalize static GUI layout only after consulting 125A-Branding.
- Implement on develop/v1.0.0, not main; batch related edits before expensive Windows CI.
- Build -> deterministic regressions -> VST3 validation -> Studio One practical acceptance -> release decision. No PASS or FINAL claims without evidence.
