# 125A Chorderator — Static GUI design contract (V1 DRAFT)

Date: 2026-10-15. Product design before VSTGUI implementation, following 125A-Engineering/STANDARDS/GUI-GUIDELINES.md. Not an implemented or verified editor.

## Window / scale
- Logical editor: 1120 x 600 px at 100%; 1680 x 900 at 150%; 100% must fit usable 1920 x 1080 Studio One workspace where possible. Both actual host viewport dimensions to be tested.
- Layout uses a header and 2-column content; avoid scrolling, nested hidden pages, giant zoom switch, overlapping hit regions and sliders requiring mouse wheel.
- Zoom selector: explicit 100% / 150% buttons; current choice persists in controller state; Ctrl+click resets controls to defined default.

## Static layout zones (logical coordinates)
- Header [24,20,1072,68]: true 125A Logo master at left; 'CHORDERATOR' name; MIDI CHORD ENGINE subtitle; Zoom at right.
- Harmonic setup [24,104,520,180]: KEY select (12) and SCALE select (4); visible 12 chromatic pitches with 7 in-scale indicators; root labels use MIDI pitch class names independent of DAW octave convention.
- Character setup [560,104,536,180]: VOICE LEADING OFF / AUTO; CHARACTER CLASSIC / DARK / WIDE / POWER. Clear selected state, one active option in each group.
- Performance [24,302,520,140]: ROOT OCTAVE DOUBLE toggle; CHROMATIC toggle; display of invalid-note silence behavior. The octave is selected by DAW incoming note, not a confusing extra transpose control.
- MIDI modifiers [560,302,536,140]: CONTROL BASE (MIDI 0..125) plus always-visible note number and three assignments NORMAL / SUS2 / SUS4. Note IDs must be displayed without unreliable C-1/C0 naming.
- Live chord display [24,460,1072,116]: current input/root and generated chord name/actual output pitches; empty/no-note indicator; preview only after separate host verification.

## Control bindings
| ID | Name | Steps / default | UI |
| --- | --- | --- | --- |
| 100 | KEY | 12 / C | discrete selector with direct access |
| 101 | SCALE | 4 / Major | direct selector |
| 102 | CHROMATIC | OFF | toggle |
| 103 | VOICE LEADING | AUTO | OFF/AUTO |
| 104 | CHARACTER | CLASSIC | CLASSIC/DARK/WIDE/POWER |
| 105 | ROOT OCTAVE DOUBLE | OFF | toggle |
| 106 | CONTROL BASE | MIDI 12 | numeric direct selector + legend |

These IDs are frozen once release compatibility is needed. Validate parameter -> controller -> widget -> processor -> MIDI output -> state recall for each.

## Visual authority
- Logo source: 125A-Branding/125A_Logo_Master_FINAL.svg. Preserve exact source geometry/proportions and original red A stroke. No text recreation.
- 125A-Knob-Designer is authoritative for approved rendered knobs. Chorderator V1 prioritizes segmented buttons/selectors; no filmstrip required if no knob is used.
- Clean dark metallic panel is a *proposed* styling direction, not yet locked or owner-approved. Consistent hierarchy, accessible contrast, muted inactive labels and distinct active buttons.

## Acceptance (NOT YET VERIFIED)
1. Static layout bounds, hit targets, no overlap and all seven UI controls present.
2. Zoom 100/150% and saved/restored selection in Studio One.
3. Correct selectable key and scale with in-scale note display.
4. MIDI modifier legend reflects actual Control Base value; no control note reaches output.
5. Host automation updates editor and MIDI behavior, project state restores controls.
6. Live display follows true output, never guesses from trigger.
7. Editor 20x close/reopen + lifetime and routing regression; Steinberg validator and native Studio One separately.
