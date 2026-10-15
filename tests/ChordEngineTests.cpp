#include "../src/ChordEngine.h"
#include <cassert>
#include <iostream>
using namespace chorderator;
static void pitches(Chord c, std::initializer_list<int> expected) {
    assert(c.accepted && c.count==expected.size());
    size_t i=0; for (int v:expected) assert(c.notes[i++]==v);
}
int main() {
    Settings s;
    pitches(generate(60,s),{60,64,67}); // C
    pitches(generate(64,s),{64,67,71}); // Em
    pitches(generate(69,s),{69,72,76}); // Am
    pitches(generate(67,s),{67,71,74}); // G
    pitches(generate(71,s),{71,74,77}); // Bdim
    assert(!generate(61,s).accepted);
    s.chromatic=true; pitches(generate(61,s),{61,65,68});
    s.chromatic=false;
    s.variant=Variant::Sus2; pitches(generate(60,s),{60,62,67});
    s.variant=Variant::Sus4; pitches(generate(60,s),{60,65,67});
    s.variant=Variant::Normal; s.rootOctaveDouble=true;
    pitches(generate(60,s),{60,64,67,72});
    assert(!generate(120,s).accepted); // avoid out-of-range NoteOn
    s.rootOctaveDouble=false; s.scale=Scale::NaturalMinor;
    pitches(generate(60,s),{60,63,67});
    pitches(generate(62,s),{62,65,68});
    s.scale=Scale::HarmonicMinor;
    pitches(generate(67,s),{67,71,74});
    s.scale=Scale::MelodicMinorAscending;
    pitches(generate(62,s),{62,65,69});
    VoiceLedger<2> ledger;
    const auto c=generate(60,s);
    assert(ledger.start(1,60,c));
    assert(!ledger.start(1,60,c));
    assert(ledger.start(2,60,c));
    assert(!ledger.start(3,60,c));
    assert(ledger.activeCount()==2);
    auto released=ledger.release(1,60); assert(released.accepted && released.count==3);
    assert(ledger.activeCount()==1);
    int flushed=0; ledger.flush([&](int32_t,Chord chord){assert(chord.accepted);++flushed;});
    assert(flushed==1 && ledger.activeCount()==0);
    assert(!ledger.release(2,60).accepted);
    std::cout << "ChordEngine tests PASS\n";
}
