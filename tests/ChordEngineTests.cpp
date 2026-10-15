#include "../src/ChordEngine.h"
#include "../src/ChordVoicing.h"
#include "../src/MidiControl.h"
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
    // Control NoteOns on the same sample apply regardless of host event order.
    ControlLayout layout;
    check(layout.valid() && layout.isControl(13) && !layout.isControl(60), "valid controls");
    const std::array<InputEvent,3> firstOrder{{{12,60,true,false,11},
                                                {12,14,true,false,12},
                                                {13,14,false,true,12}}};
    const std::array<InputEvent,3> reverseOrder{{{12,14,true,false,12},
                                                  {12,60,true,false,11},
                                                  {13,14,false,true,12}}};
    check(modifierAt(firstOrder,3,12,layout).variant==Variant::Sus4, "sus4 selected");
    check(modifierAt(reverseOrder,3,12,layout).variant==Variant::Sus4, "order independent");
    check(modifierAt(firstOrder,3,13,layout).variant==Variant::Normal, "no held mode");
    const std::array<InputEvent,2> conflicting{{{4,13,true,false,1},
                                                 {4,14,true,false,2}}};
    const auto rejected=modifierAt(conflicting,2,4,layout);
    check(rejected.conflict && rejected.variant==Variant::Normal, "conflicting modifiers safe");
    layout.basePitch=127;
    check(!layout.valid() && modifierAt(firstOrder,3,12,layout).conflict,
          "invalid control range rejected");
    std::cout << "ChordEngine tests PASS\n";
}
