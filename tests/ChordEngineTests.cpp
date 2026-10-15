#include "../src/ChordEngine.h"
#include "../src/ChordVoicing.h"
#include "../src/MidiControl.h"
#include "../src/MidiChordProcessor.h"
#include <cstdlib>
#include <initializer_list>
#include <iostream>
using namespace chorderator;
static void check(bool ok, const char* msg) {
    if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(EXIT_FAILURE); }
}
static void pitches(Chord c, std::initializer_list<int> expected) {
    check(c.accepted && c.count==expected.size(), "chord accepted and count");
    size_t i=0; for (int v:expected) check(c.notes[i++]==v, "exact chord pitch");
}
int main() {
    Settings s;
    pitches(generate(60,s),{60,64,67}); // C
    pitches(generate(64,s),{64,67,71}); // Em
    pitches(generate(69,s),{69,72,76}); // Am
    pitches(generate(67,s),{67,71,74}); // G
    pitches(generate(71,s),{71,74,77}); // Bdim
    check(!generate(61,s).accepted, "regression");
    s.chromatic=true; pitches(generate(61,s),{61,65,68});
    s.chromatic=false;
    s.variant=Variant::Sus2; pitches(generate(60,s),{60,62,67});
    s.variant=Variant::Sus4; pitches(generate(60,s),{60,65,67});
    s.variant=Variant::Normal; s.rootOctaveDouble=true;
    pitches(generate(60,s),{60,64,67,72});
    check(!generate(120,s).accepted, "regression"); // avoid out-of-range NoteOn
    s.rootOctaveDouble=false; s.scale=Scale::NaturalMinor;
    pitches(generate(60,s),{60,63,67});
    pitches(generate(62,s),{62,65,68});
    s.scale=Scale::HarmonicMinor;
    pitches(generate(67,s),{67,71,74});
    s.scale=Scale::MelodicMinorAscending;
    pitches(generate(62,s),{62,65,69});
    // All added church modes build diatonic triads on the expected degrees.
    s.scale=Scale::Dorian; pitches(generate(60,s),{60,63,67});
    s.scale=Scale::Phrygian; pitches(generate(60,s),{60,63,67});
    s.scale=Scale::Lydian; pitches(generate(60,s),{60,64,67});
    s.scale=Scale::Mixolydian; pitches(generate(60,s),{60,64,67});
    s.scale=Scale::Locrian; pitches(generate(60,s),{60,63,66});
    s.scale=Scale::Major;
    VoiceLedger<2> ledger;
    const auto c=generate(60,s);
    check(ledger.start(1,60,c), "regression");
    check(!ledger.start(1,60,c), "regression");
    check(ledger.start(2,60,c), "regression");
    check(!ledger.start(3,60,c), "regression");
    check(ledger.activeCount()==2, "regression");
    auto released=ledger.release(1,60); check(released.accepted && released.count==3, "regression");
    check(ledger.activeCount()==1, "regression");
    int flushed=0; ledger.flush([&](int32_t,Chord chord){check(chord.accepted, "regression");++flushed;});
    check(flushed==1 && ledger.activeCount()==0, "regression");
    check(!ledger.release(2,60).accepted, "regression");
    // Voicing stays in MIDI range and stays reproducible for the same context.
    Settings vs;
    const Chord base=generate(60,vs);
    VoicingSettings vo;
    const Chord first=voice(base,60,vo);
    const Chord repeat=voice(base,60,vo);
    check(first.accepted && first.count==3, "voicing accepted");
    check(first.notes==repeat.notes, "deterministic voicing");
    const Chord next=voice(generate(64,vs),64,vo,&first);
    check(next.accepted, "voice leading accepted");
    check(next.notes[0]<next.notes[1] && next.notes[1]<next.notes[2], "sorted pitches");
    vo.character=Character::Power;
    pitches(voice(base,60,vo),{60,67,72});
    vo.character=Character::Classic; vo.leading=Leading::Off;
    pitches(voice(base,60,vo),{60,64,67});
    const Chord doubled=doubleRootAbove(voice(base,60,vo),60);
    pitches(doubled,{60,64,67,72});
    vo.leading=Leading::Auto; vo.character=Character::Wide;
    const Chord wide=voice(base,60,vo);
    check(wide.accepted && wide.notes[2]>wide.notes[0], "wide candidate");
    // Repro reported in Studio One: C-D-G-Ab-C in C Natural Minor.
    // Replaying the same C must not shift the root to another octave.
    Settings naturalMinor;
    naturalMinor.scale=Scale::NaturalMinor;
    VoicingSettings autoClassic;
    Chord previousVoicing{};
    const std::array<int,5> progression{{60,62,67,68,60}};
    std::array<Chord,5> played{};
    for(size_t i=0;i<progression.size();++i) {
        const int root=progression[i];
        played[i]=voice(generate(root,naturalMinor),root,autoClassic,
                        previousVoicing.accepted?&previousVoicing:nullptr);
        check(played[i].accepted && (played[i].notes[0]==root || played[i].notes[1]==root || played[i].notes[2]==root),
              "Natural Minor root remains in played MIDI octave");
        previousVoicing=played[i];
    }
    check((played[0].notes[0]==60 || played[0].notes[1]==60 || played[0].notes[2]==60) &&
          (played[4].notes[0]==60 || played[4].notes[1]==60 || played[4].notes[2]==60),
          "C-D-G-Ab-C return C root does not jump octave");
    // The reported screenshot used DARK + Voice Leading OFF.
    // The played root must stay at exactly its input register.
    VoicingSettings darkOff;
    darkOff.character=Character::Dark;
    darkOff.leading=Leading::Off;
    for(int root : std::array<int,5>{{60,62,67,68,60}}) {
        const auto dark=voice(generate(root,naturalMinor),root,darkOff);
        check(dark.accepted && dark.notes[0]==root,
              "DARK/OFF: root stays in original octave");
    }
    // Every character must preserve incoming root octave for AUTO and OFF.
    for(auto ch : {Character::Classic,Character::Dark,Character::Wide,Character::Power}) {
        for(auto lead : {Leading::Off,Leading::Auto}) {
            VoicingSettings vset; vset.character=ch; vset.leading=lead;
            Chord prior{};
            for(int root : std::array<int,5>{{60,62,67,68,60}}) {
                auto current=voice(generate(root,naturalMinor),root,vset,
                                   prior.accepted?&prior:nullptr);
                check(current.accepted && (current.notes[0]==root || current.notes[1]==root || current.notes[2]==root),
                      "voicing all character/leading combinations preserve root");
                for(int n=1;n<current.count;++n)
                    check(current.notes[n]>current.notes[n-1],
                          "voicing output strictly ascending");
                prior=current;
            }
        }
    }
    // AUTO must produce a genuinely different, smoother voicing on a
    // progression that benefits from inversion: C major -> F major.
    Settings majorScale;
    VoicingSettings offMode; offMode.leading=Leading::Off;
    const Chord cMajor=voice(generate(60,majorScale),60,offMode);
    const Chord fOff=voice(generate(65,majorScale),65,offMode,&cMajor);
    VoicingSettings autoMode; autoMode.leading=Leading::Auto;
    const Chord fAuto=voice(generate(65,majorScale),65,autoMode,&cMajor);
    check(fAuto.accepted && fOff.accepted && fAuto.notes!=fOff.notes,
          "AUTO must differ from OFF when smooth inversion exists");
    check(fAuto.notes[0]<fAuto.notes[1] && fAuto.notes[1]<fAuto.notes[2],
          "AUTO inversion sorted");
    check(fAuto.notes[0]==65 || fAuto.notes[1]==65 || fAuto.notes[2]==65,
          "AUTO inversion preserves played F root register");
    // Control NoteOns on the same sample apply regardless of host event order.
    ControlLayout layout;
    check(layout.valid() && layout.isControl(50) && !layout.isControl(60), "valid controls");
    const std::array<InputEvent,3> firstOrder{{{12,60,true,false,11},
                                                {12,52,true,false,12},
                                                {13,52,false,true,12}}};
    const std::array<InputEvent,3> reverseOrder{{{12,52,true,false,12},
                                                  {12,60,true,false,11},
                                                  {13,52,false,true,12}}};
    check(modifierAt(firstOrder,3,12,layout).variant==Variant::Sus4, "sus4 selected");
    check(modifierAt(reverseOrder,3,12,layout).variant==Variant::Sus4, "order independent");
    check(modifierAt(firstOrder,3,13,layout).variant==Variant::Normal, "no held mode");
    const std::array<InputEvent,2> conflicting{{{4,50,true,false,1},
                                                 {4,52,true,false,2}}};
    const auto rejected=modifierAt(conflicting,2,4,layout);
    check(rejected.conflict && rejected.variant==Variant::Normal, "conflicting modifiers safe");
    layout.basePitch=127;
    check(!layout.valid() && modifierAt(firstOrder,3,12,layout).conflict,
          "invalid control range rejected");
    // End-to-end event core: one trigger -> chord, NoteOff releases it.
    MidiChordProcessor<> proc;
    VoicingSettings exact;
    exact.leading=Leading::Off;
    proc.setVoicing(exact);
    std::array<InputEvent,4> batch{{{0,60,true,false,101},
                                    {0,52,true,false,201},
                                    {48,60,false,true,101},
                                    {48,52,false,true,201}}};
    check(proc.process(batch,4), "processor accepted event block");
    check(proc.outputCount()==6, "three NoteOn and three NoteOff");
    check(proc.output()[0].on && proc.output()[0].pitch==60, "root NoteOn");
    check(proc.output()[1].on && proc.output()[1].pitch==65, "SUS4 third");
    check(proc.output()[2].on && proc.output()[2].pitch==67, "fifth NoteOn");
    check(!proc.output()[3].on && proc.output()[3].offset==48, "released on correct offset");
    check(proc.activeVoices()==0, "no hanging voice");
    // Two identical chords share sounding pitch ownership; only last off stops pitch.
    std::array<InputEvent,2> overlap{{{0,60,true,false,101},{10,60,true,false,102}}};
    check(proc.process(overlap,2) && proc.outputCount()==3, "overlap avoids double NoteOn");
    std::array<InputEvent,1> releaseFirst{{{0,60,false,true,101}}};
    check(proc.process(releaseFirst,1) && proc.outputCount()==0, "first release keeps second alive");
    std::array<InputEvent,1> releaseLast{{{0,60,false,true,102}}};
    check(proc.process(releaseLast,1) && proc.outputCount()==3, "last release emits offs");
    check(proc.stop() && proc.outputCount()==0, "panic after silence");
    // Muted control and invalid notes generate no MIDI.
    // Every reserved control key must be completely silent on NoteOn and
    // NoteOff, including while a musical chord is already sounding.
    for(int pitch : {48,50,52}) {
        MidiChordProcessor<> silent;
        silent.setVoicing(exact);
        std::array<InputEvent,2> pair{{{0,static_cast<int16_t>(pitch),true,false,9},
                                       {24,static_cast<int16_t>(pitch),false,true,9}}};
        check(silent.process(pair,2) && silent.outputCount()==0,
              "C/D/E modifier never emits MIDI NoteOn or NoteOff");
        check(silent.activeVoices()==0, "control note never owns a voice");
        std::array<InputEvent,1> musical{{{0,60,true,false,10}}};
        check(silent.process(musical,1) && silent.outputCount()==3,
              "musical chord still sounds after control");
        check(silent.process(pair,2) && silent.outputCount()==0,
              "control notes never interrupt playing chord");
        std::array<InputEvent,1> release{{{0,60,false,true,10}}};
        check(silent.process(release,1) && silent.outputCount()==3,
              "musical chord releases normally");
    }
    std::array<InputEvent,1> onlyControl{{{0,48,true,false,9}}};
    check(proc.process(onlyControl,1) && proc.outputCount()==0, "normal key is silent");
    // Hosts may send noteId=-1 for every note. Two overlapping C notes
    // must remain independently owned; first off cannot silence the second.
    MidiChordProcessor<> untagged;
    untagged.setVoicing(exact);
    const std::array<InputEvent,2> repeatedUntaged{{{0,60,true,false,-1},
                                                     {8,60,true,false,-1}}};
    check(untagged.process(repeatedUntaged,2), "untagged overlapping accepted");
    check(untagged.activeVoices()==2, "untagged voice ownership");
    check(untagged.outputCount()==3, "only first note triggers common pitches");
    const std::array<InputEvent,1> untaggedOff{{{0,60,false,true,-1}}};
    check(untagged.process(untaggedOff,1), "untagged first release accepted");
    check(untagged.activeVoices()==1 && untagged.outputCount()==0,
          "first untagged off keeps second sounding");
    check(untagged.process(untaggedOff,1), "untagged final release accepted");
    check(untagged.activeVoices()==0 && untagged.outputCount()==3,
          "last untagged release emits all note offs");
    std::cout << "ChordEngine tests PASS\n";
}
