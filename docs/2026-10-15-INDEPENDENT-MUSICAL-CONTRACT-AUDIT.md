# 125A CHORDERATOR — Independent musical contract audit
Date: 2026-10-15
Scope: develop/v1.0.0; **no musical-engine changes during this audit**.
Result at initial audit date: **HISTORICAL MUSICAL CONTRACT FAIL**. Resolved in later CI-confirmed candidate `fc5f4fe`; see resolution section below. **Native Studio One / 125A Tester remain NOT VERIFIED, so no FINAL / release authorization.**

## Evidence and reproducibility

- Independent test source: `tests/MusicalContractAudit.cpp` — uses **hard-coded degree and chord-quality reference tables**, not the engine's own `classify()` or derived source expectation.
- First run: https://github.com/challanger2000/125A-Chorderator/actions/runs/38807816655
- Extended short-phrase repeat regression: https://github.com/challanger2000/125A-Chorderator/actions/runs/38808252443
- Both tests compiled with MSVC; the **audit itself intentionally fails** on unmet product contracts. Earlier green core/validator runs do not negate these findings.
- Requirements: `docs/CHORDERATOR-V1-SPEC.md` (explicitly marked DRAFT), actual user Studio One screenshots, `src/ChordEngine.h`, `src/ChordVoicing.h`, `src/MidiChordProcessor.h`, `src/MidiControl.h`, `src/ChordVst3.cpp`.

## Results from the current executable audit

| Area | Result | Interpretation |
| --- | --- | --- |
| Diatonic three-note chords / all nine scales x 12 keys x seven degrees | **756/756 valid independent oracle comparisons** | Base triad classes/root/3rd/5th behave as expected in the tested register |
| CHARACTER DARK vs CLASSIC with VOICE LEADING OFF | **0/756 different** | **FAIL**: user-facing character setting has no MIDI pitch effect in test population |
| CHARACTER WIDE vs CLASSIC with VOICE LEADING OFF | **0/756 different** | **FAIL**: likewise |
| DARK / WIDE vs CLASSIC with AUTO on one ascending scale phrase per key | **0/756 different each** | No characteristic differentiation in this population; other phrase classes may differ |
| SUS2/SUS4 out-of-scale notes with CHROMATIC OFF | **396/1512** | Policy unresolved: CHROMATIC currently gates trigger roots, not all generated voices |
| POWER perfect fifth outside selected scale | **156/756** | A perfect fifth is intentional for power chords, but exception to mode must be disclosed |
| Input NoteOn velocity | **always 100 in core** | **Musical limitation:** InputEvent has no velocity field and VST3 ingestion drops original velocity |
| Consecutive overlapping equal-pitch triggers | **0 NoteOns emitted for second identical chord** | Intentional reference-count/legato behavior currently suppresses repeated attack; articulation policy must be agreed |
| Four-note natural-minor phrase C2-Eb2-G2-Bb2, repeated 4x, CLASSIC/DARK/WIDE x OCTAVE DOUBLE ON/OFF | **60 different chords** | **FAIL**: AUTO drifts despite repeat. Specific C2: first C2-Eb2-G2, repeat C2-Eb3-G3 |

## Musical correctness and conflicting requirements

1. **P0 — Repeated-phrase reproducibility.** The latest descending-octave reset in `MidiChordProcessor.h` only triggers when preceding root exceeds new trigger by >=12 semitones. E.g. Bb2->C2 descends 10, so the reset does not trigger. It is **not a general phrase-boundary detector**. Lowering the threshold arbitrarily will mistake intentional melodic leaps for phrase boundaries. A musical contract is needed for explicit phrase reset, transport position recognition, deliberately history-independent voicings, or another deterministic phrase-level rule. Test complete pitches, not just bass.
2. **P0/P1 — CHARACTER identities.** Duplicated results for DARK/WIDE versus CLASSIC with LEADING OFF demonstrate their parameter names lack meaningful pitch-based behavior in the tested set. The promised algorithmic difference cannot simply be assumed from distinct score targets. Specify audible target behaviors and independent examples before tuning.
3. **P1 — Dynamic input velocity.** Current input `InputEvent` has no velocity; `MidiChordProcessor` emits a hard-coded velocity 100 for generated note-ons. Define and test faithful velocity mapping and practical lower/upper limits.
4. **P1 — Chord-name accuracy for suspended altered fifths.** SUS2/SUS4 replace only the third; a diminished or augmented fifth remains unchanged. A plain 'sus2' or 'sus4' name conventionally implies a perfect fifth. Example in C Locrian, C trigger + SUS2 produces C-D-Gb, not C-D-G, but is labelled Csus2. Similarly with augmented fifth. Decide whether to permit altered suspended chords with accurate suffixes (sus2b5/sus4#5), replace fifth, or reject those combinations. Some SUS tones are outside the key.
5. **P1 — CHROMATIC/OFF semantics.** Current OFF blocks non-scale *input roots* only. It does NOT ensure that SUS or POWER output pitch classes are in the selected key. This needs an explicit documented policy and UI distinction (allowed *trigger roots* versus guaranteed output pitches). Do not simply clamp notes to scale and silently change chord quality.
6. **P2 — Shared-pitch re-articulation.** Voice reference counting prevents overlapping chords from generating a fresh attack for shared pitches. This may be desired legato or undesirable for rhythmic patterns; the present contract does not decide. Add rhythmic overlap fixtures after policy decision.
7. **P2 — Simultaneous control note semantics.** C0/D0/E0 modifiers only apply if their NoteOn shares the **same sample offset** as the played root. Holding D0 across later triggers does NOT sustain SUS2 mode. That matches code comments, but is not useful for conventional live keyboard modifier performance without a documented design choice.
8. **P2 — Out-of-scale chromatic roots.** When CHROMATIC ON, any non-scale root defaults to a major triad irrespective of scale. The initial product spec explicitly leaves harmonization policy open.
9. **P2 — Incomplete design-draft features.** `docs/CHORDERATOR-V1-SPEC.md` proposes optional seventh/add9 and adjustable control note base. Neither is user-selectable in the current GUI/engine. These were a draft, so owner must confirm whether intentionally deferred; do not call them hidden bugs without deciding V1 scope.
10. **P2 — Fixed channel and articulation data.** Output VST3 notes are forced onto channel 0 and source note IDs are not retained. Whether these behaviors are acceptable for the intended Piano Roll workflow is not yet specified/tested.
11. **P2 — ROOT OCTAVE DOUBLE spec drift.** Draft says a root above *the highest generated chord voice*, while the latest repair intentionally uses **played root + 12**. These contracts differ when upper voices are already high. The current implementation is root+12; align the formal product spec to owner intent.

## Interpretation of PASS

- Green original core tests: validate their configured invariants and many lifecycle cases, **not** CHARACTER differentiation, velocity preservation, or general phrase periodicity.
- Green Steinberg validator (47/47 on previous exact plugin code): API compliance only, not musical approval.
- Failing independent musical contract audit: **confirmed product-level defects**. Leave the current developed binary unapproved. Do not mask by weakening new assertions.

## Repair strategy (not executed)

1. Freeze current failing audit as baseline. Define an explicit, reproducible *phrase-boundary policy* and expected MIDI note arrays for recurring motifs.
2. Define how CLASSIC/DARK/WIDE must differ musically with OFF as well as AUTO. Specify and review 10 representative voicing fixtures with owner before implementation.
3. Restore MIDI velocity propagation from VST3 input through core to individual output notes.
4. Decide SUS outside-scale and altered-fifth naming; add golden pitch+name tables across all keys/modes; explicitly state POWER exceptions.
5. Decide held-control semantics and shared-pitch retrigger/legato policy; exercise in sample-accurate event sequences.
6. Run independent contract audit plus existing core suite, Windows VST3 build, official validator and 125A Tester, then perform actual routed Studio One piano-roll comparison/loop/recall.

Status: **FAIL — audited, not repaired or approved**.

## Follow-up resolution — 2026-10-15 musical repair candidate (CONFIRMED)

**Code commit:** `fc5f4fe867c6bf0f3b24ad01e64e5cc1444af39e` on `develop/v1.0.0`.
The original FAIL evidence above is preserved, not erased.

| Original audit finding | Resolution | Confirming test |
| --- | --- | --- |
| AUTO upper voices drift when short motifs repeat | Note-addressed bounded memory indexed by absolute MIDI root and chord modifier; no arbitrary 12-semitone phrase detector. Changes to musical settings and processor STOP reset voice memory. The first encountered voicing is preserved through uninterrupted repeats. | Independent musical audit 38812386016 PASS; original long-loop regression in core 38812386017 PASS |
| DARK and WIDE indistinguishable from CLASSIC | OFF profiles defined as CLASSIC [root, third, fifth], DARK [root, fifth, third+12], WIDE [root, third+12, fifth+12]. AUTO balances previous voice movement against each reference profile. POWER [root, +7, +12]. | 756/756 DIFFERENT DARK vs CLASSIC and 756/756 DIFFERENT WIDE vs CLASSIC in OFF and AUTO fixture set |
| MIDI velocity fixed 100 | InputEvent carries velocity, VST3 reads normalized host velocity and maps to MIDI 1–127, every chord NoteOn inherits input. Cached voicings do not cache the dynamic. | Independent & core tests check 1,19,37,109,127; PASS |
| Suspensions with altered fifth named merely sus2/sus4 | Added explicit Quality identities and GUI suffixes for sus2(b5), sus2(#5), sus4(b5), sus4(#5) retaining actual original fifth. | Independent golden tests (C Locrian, harmonic minor) PASS |
| CHROMATIC OFF contains SUS and POWER exceptions | Clarified user-facing label CHROMATIC ROOTS and allowed root notes. OFF limits trigger roots, not output pitch classes. POWER keeps a perfect fifth even in Locrian. | 396/1512 SUS out-of-scale and 156/756 POWER fifth exceptions are measured and documented |
| Shared repeated pitch does not re-articulate | **INTENTIONAL LEGATO POLICY**, not changed. Overlapping note owners share a playing pitch until final NoteOff; source re-articulation is intentionally not guaranteed. | Original lifecycle tests; product contract describes limitation |
| Control held key requires same-sample onset | **DELIBERATE V1 CONTRACT**: controls only apply when the control NoteOn and musical NoteOn have identical sample offset. | Original same-offset regression tests |
| Optional seventh/add9 and adjustable control base missing | Deferred from early unapproved DRAFT; not represented as current product functions. | Implementation contract |

### Actual verified CI for this code
- [Core tests: PASS](https://github.com/challanger2000/125A-Chorderator/actions/runs/38812386017), including the 30,456-transition AUTO/OFF comparison (AUTO movement 240,816 vs OFF 281,664, 6,264 better individual transitions, zero worse in this corpus).
- [Independent musical contract: PASS](https://github.com/challanger2000/125A-Chorderator/actions/runs/38812386016) including 756 independent reference diatonic triads, distinct CHARACTER profiles, short 4-note repeated motifs, accurate suspension identity, expressive velocities and invalidation of cached voicings.
- [Windows VST3 build & Steinberg Validator: PASS 47/47](https://github.com/challanger2000/125A-Chorderator/actions/runs/38812386087). Exact development ZIP artifact: `125A-CHORDERATOR-v1.0.0-Windows-x64-DEVELOPMENT`, artifact ID `12008902980`.

### Acceptance limits (NOT VERIFIED)
- Native Studio One capture/playback of actual generated MIDI and the current GUI's character controls, octaves, live chord names, input velocity, routes to Pneuma Pro, 100/150% zoom, state recall, stop/seek/retrigger, and varying block sizes.
- Local 125A Plugin Tester 0.2.10 on this exact VST3.
- Musical taste, harmonic suitability and expressive feel remain a human listening judgment; CI demonstrates conformity to the defined contract, not universal sonic quality.

**Status: Musical regression / build / official VST3 validator PASS. Release gate: OPEN / NO FINAL.**
