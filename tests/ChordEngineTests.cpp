#include "../src/ChordEngine.h"
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <iostream>
using namespace chorderator;
static void check(bool ok, const char* msg) {
    if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(EXIT_FAILURE); }
}
static void pitches(Chord c, std::initializer_list<int> expected) {
    check(c.accepted && c.count==expected.size(), "chord accepted and count", "regression");
    size_t i=0; for (int v:expected) check(c.notes[i++]==v, "exact chord pitch", "regression");
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
    std::cout << "ChordEngine tests PASS\n";
}
