# CHORDERATOR V1 — Source-based functional audit (2026-10-15)

**Branch:** develop/v1.0.0. **Scope:** repository source / GUI description / test suite / available CI; not equivalent to Studio One host certification.

**Baseline:** quantitative musical core CI run 38780396991 PASS; 30,456 transitions, total upper-voice travel AUTO 170,736 vs OFF 281,664; this establishes a metric only, not complete subjective musical quality.

## Source inspection results

| Area | Status | Evidence / defect | Required gate |
| --- | --- | --- | --- |
| Chord generation | TEST PASS (core) | 9 scales, variants, basic output pitch classes and MIDI bounds covered by ChordEngineTests.cpp | Independent reference tables, all 7 degrees in every scale |
| Voice leading | TEST PASS (bounded metric) | Root bass preserved; 30,456-transition movement metric; no proof of all musical situations or repeated host seek behavior | Explicit golden note sequences, high-register and chromatic tests, reset/seek test |
| C0/D0/E0 modifier control | TEST PASS (core); HOST NOT VERIFIED | Layout in MidiControl.h fixed at absolute MIDI 48/50/52; simultaneous same-sample NoteOn only | Native Studio One octave check and silence at both note-on and note-off |
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
