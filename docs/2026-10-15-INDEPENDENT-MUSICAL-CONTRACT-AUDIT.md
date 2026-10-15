# 125A CHORDERATOR — Independent musical contract audit
Date: 2026-10-15
Scope: develop/v1.0.0; **no musical-engine changes during this audit**.
Result: **MUSICAL CONTRACT FAIL**. No FINAL / commercial release authorization.

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
