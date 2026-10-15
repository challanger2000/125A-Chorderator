# CHORDERATOR V1 — Source-based functional audit (2026-10-15)

**Branch:** develop/v1.0.0. **Scope:** repository source / GUI description / test suite / available CI; not equivalent to Studio One host certification.

**Baseline:** quantitative musical core CI run 38780396991 PASS; 30,456 transitions, total upper-voice travel AUTO 170,736 vs OFF 281,664; this establishes a metric only, not complete subjective musical quality.

## Source inspection results

| Area | Status | Evidence / defect | Required gate |
| --- | --- | --- | --- |
| Chord generation | TEST PASS (core) | 9 scales, variants, basic output pitch classes and MIDI bounds covered by ChordEngineTests.cpp | Independent reference tables, all 7 degrees in every scale |
| Voice leading | TEST PASS (bounded metric) | Root bass preserved; 30,456-transition movement metric; no proof of all musical situations or repeated host seek behavior | Explicit golden note sequences, high-register and chromatic tests, reset/seek test |
| C0/D0/E0 modifier control | TEST PASS (core); HOST NOT VERIFIED | CORRECTED: fixed Studio One C0/D0/E0 to absolute MIDI 24/26/28; previously 48/50/52 was incorrect; simultaneous same-sample NoteOn only | Native Studio One octave check and silence at both note-on and note-off |
| Legacy modifier base parameter | CORRECTED IN SOURCE; BUILD PENDING | Param 106 was exposed as writable despite processor ignoring updates; now marked read-only/hidden and default 48. v1 stored-state field retained for compatibility. | Build, plugin validator, state recall and host parameter inspection |
| MIDI event ingress | AT RISK | 256 total input-event cap (even irrelevant event types); negative offsets filtered but offset >= numSamples not rejected; notes sorted by offset; MIDI channel and velocity are not preserved | Event stress, channel/velocity requirement, input validation |
| MIDI output/backpressure | **BLOCKER** | MidiChordProcessor mutates active/sounding state before ChordVst3::emit. If output IEventList::addEvent fails partway, no atomic rollback/retry. Subsequent NoteOff/NoteOn may desync from host. | Failure injection: reject output at event 0, 1, or later; recover without stuck/lost notes |
| Automation | **BLOCKER** | ChordVst3::process reads only the final point of each parameter queue and applies it before processing all events in that block; ignores intermediate offsets. | Multi-point within-block automation and exact event/parameter ordering |
| Live chord display | **BLOCKER** | Processor exports number of sounding pitches and only four lowest; controller guesses name from these pitches. 5+ pitch overlaps show no confident name; independent root metadata not transmitted. | Single chord quality, doubles, overlapping chord readout, after note-offs, transport stop |
| GUI controls | **NOT VERIFIED** | UIDESC tags 100–105, zoom 900; static mapping exists. Runtime click/host automation/status persistence not tested here. | Click each widget, automation, Ctrl-reset, open-close, 100/150% |
| State & lifecycle | **NOT VERIFIED** | Version 1 state validation and setActive reset in code; no dedicated state/host-lifecycle tests in current CMake core suite. | save/restore, malformed state, host editor lifecycle, transport, start/stop |
| Core QA | TEST PASS | GitHub Actions runs only the C++ core test executable | Must not be equated with VST3 host/editor release PASS |
| Windows VST3 build | PREVIOUS BUILDS PASS; NEW SOURCE PENDING | Workflow stages plugin and calls Steinberg validator | Recheck after changes and obtain exact validator summary |
| 125A Plugin Tester / native Studio One | NOT VERIFIED FOR CURRENT HEAD | No corresponding local host evidence in repository | Required release gate |

**Release state: BLOCKED.** Fix highest-risk MIDI output/automation and live-display correctness, add adversarial integration tests, then run validator, 125A tester and actual Studio One GUI/transport/recall tests. Never promote a core-only PASS to FULL QA PASS.

## Confirmed on repair commit 2395cb7 (2026-10-15)

- Windows core tests: PASS, run 38783476316, including output-recovery/timeline/chord-identity regressions.
- Steinberg VST3 Validator: 47 PASS / 0 FAIL, Windows run 38783476620.
- Windows development artifact produced by run 38783476620.
- Subsequent source correction pending: note-name display now follows Studio One MIDI 24 = C0, with regression cases MIDI 0 = C-2, MIDI 48 = C2 and 60 = C3.
- **Still NOT YET VERIFIED:** actual Studio One editor clicks/automation/transport and 125A Plugin Tester results for this repair build; neither core PASS nor validator PASS is a FULL-AUDIT or FINAL release pass.

## Verified final CI evidence for this repair phase (2026-10-15)

**Source commit:** `38a87dd4e4dccbcf4e1c3d549e9a33ce12d82146`.

- Core regression run: https://github.com/challanger2000/125A-Chorderator/actions/runs/38784146250 — **PASS**.
- Windows VST3 build and Steinberg validator: https://github.com/challanger2000/125A-Chorderator/actions/runs/38784146224 — **PASS, validator 47 passed / 0 failed**.
- Windows development VST3 artifact is present in run 38784146224.
- In addition to the three repair areas, displayed note octaves now honor the observed Studio One convention: MIDI 0 = C-2, 24 = C0, 48 = C2, 60 = C3; the core suite asserts this mapping.
- **NOT VERIFIED / release blocking:** actual DAW editor mouse/zoom/automation/restore and transport observations; 125A Plugin Tester v0.2.10 run on this exact development binary; real-host partial event rejection and dense automation fixtures. No FULL QA / FINAL designation is authorized by this report.

## 2026-10-15 Studio One octave / modifier hotfix (PENDING CI)

The previous OCTAVE mapping was incorrect. Studio One convention is MIDI note 60 = C3, so C0 = 24, D0 = 26, E0 = 28. The development processor incorrectly reserved pitches 48/50/52 (C2/D2/E2), which accounts for the user's report that Eb2 (D#2) was the first audible C-natural-minor scale note in that range: C2 was swallowed, C#2 out of scale, D2 swallowed, Eb2 allowed. The user's D0 was not consumed and created a second audible chord, causing the overlap indicator.

Changes pending CI: MidiControl fixed basePitch 24; VST3 processor applies 24; preserved legacy controlBase field has neutral initial value 24; Studio One octave GUI display updated from pitch/12-4 to pitch/12-2. Regression tests assert all control keys are silent, C2 is melodic, D0+chord requests SUS2 with exactly one held chord, and later D0 cannot cause overlap. Host integration not yet verified. Existing state v1 schema and parameter IDs retained.

## Studio One octave/modifier hotfix — verified CI

Code commit: `226b95378494e14d089cb466dabbf123e71b2e0a`.

- Core suite: **PASS**, including C2 no-longer-reserved and D0+chord no-overlap regressions; https://github.com/challanger2000/125A-Chorderator/actions/runs/38787579732
- Windows VST3 build and official Steinberg Validator: **PASS, 47 passed / 0 failed**; development ZIP available in https://github.com/challanger2000/125A-Chorderator/actions/runs/38787579299
- Host-local demonstration of these exact notes in Studio One is still pending. This is not a FULL QA / release authorization.

## 2026-10-15 — Studio One repeated ascending-scale screenshot / VOICE LEADING AUTO (pending CI)

**Observed:** In C Natural Minor, DARK, VOICE LEADING AUTO, ROOT OCTAVE DOUBLE ON, a repeated C2-D2-Eb2-F2-G2-Ab2-Bb2-C3 MIDI phrase retains the C2 bass on repeat while moving the upper chord notes higher. The first screenshot is insufficient for an acoustic judgement; the second captured MIDI output visibly demonstrates different upper registers after the repeated low C2.

**Two source causes:**
1. `MidiChordProcessor` retained its last AUTO voicing after an octave descent C3 -> C2. Root locking alone did not guarantee complete repeatability.
2. `doubleRootAbove` appended a pitch-class match *above the highest upper voice*, potentially selecting root+24 instead of root+12. Despite the UI label ROOT OCTAVE DOUBLE, the octave was not fixed relative to the played root.

**Targeted changes (not yet accepted):**
- Treat downward leaps of at least 12 semitones in the played trigger as a new phrase for AUTO's voicing reference while maintaining the exact trigger root.
- Ensure ROOT OCTAVE DOUBLE outputs pitch `rootMidi+12` rather than a register that depends on the highest upper voice. When root+12 is already part of a POWER chord, reuse that existing pitch rather than duplicating it.
- Deterministic core regression: 6 repeats of the 8-note rising Natural Minor progression with 3 CHARACTER modes x ROOT OCTAVE DOUBLE OFF/ON. Every full four-/three-note chord must be identical at the same passage position across repeats. Independent regression verifies a root+12 octave that lies below the upper voices.

**Status:** implementation/test committed only after review; CI PASS, validator and native Studio One review not yet evidenced.

## Repeated ascending-phrase voice-leading fix — verified CI (2026-10-15)

**Source commit:** `faa1ac9a392d3952d6f1c2573a37a0c1425d5e4d`.

- Core tests: **PASS**; https://github.com/challanger2000/125A-Chorderator/actions/runs/38806057995
- Windows VST3 build: **PASS**; Steinberg Validator **47 tests passed / 0 tests failed**; https://github.com/challanger2000/125A-Chorderator/actions/runs/38806057823
- Windows x64 DEVELOPMENT VST3 ZIP confirmed in build run artifacts.
- Core regression enforces six complete repetitions of C2–D2–Eb2–F2–G2–Ab2–Bb2–C3 with identical **full** chord notes at matching steps, across AUTO with CLASSIC / DARK / WIDE and ROOT OCTAVE DOUBLE OFF/ON.
- ROOT OCTAVE DOUBLE output adds exactly root+12; if root+12 already sounds in POWER voicing, no duplicate note is emitted.
- **Not yet verified:** source build running in real Studio One MIDI routing with Pneuma Pro; 125A Plugin Tester / all native-host release gates. The development ZIP is **not** FINAL.
