#include "../src/ChordEngine.h"
#include "../src/ChordVoicing.h"
#include "../src/MidiControl.h"
#include "../src/MidiChordProcessor.h"
#include "../src/MidiDelivery.h"
#include "../src/MidiTimeline.h"
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
    // AUTO must preserve the trigger as bass in a C-major to F-major progression.
    Settings majorScale;
    VoicingSettings offMode; offMode.leading=Leading::Off;
    const Chord cMajor=voice(generate(60,majorScale),60,offMode);
    const Chord fOff=voice(generate(65,majorScale),65,offMode,&cMajor);
    VoicingSettings autoMode; autoMode.leading=Leading::Auto;
    const Chord fAuto=voice(generate(65,majorScale),65,autoMode,&cMajor);
    check(fAuto.accepted && fOff.accepted && fAuto.notes[0]==65,
          "AUTO never puts third or fifth below played F root");
    check(fAuto.notes[0]<fAuto.notes[1] && fAuto.notes[1]<fAuto.notes[2],
          "AUTO inversion sorted");
    check(fAuto.notes[2]<=84, "AUTO register stays in the supported range");
    // C major to G major below it: AUTO keeps upper voices nearby,
    // whereas OFF uses the default root-position triad.
    const Chord gOff=voice(generate(55,majorScale),55,offMode,&cMajor);
    const Chord gAuto=voice(generate(55,majorScale),55,autoMode,&cMajor);
    check(gAuto.accepted && gOff.accepted && gAuto.notes!=gOff.notes,
          "AUTO must differ from OFF when smooth upper-voice motion exists");
    check(gAuto.notes[0]==55 && gAuto.notes[1]>55 && gAuto.notes[2]>gAuto.notes[1],
          "AUTO retains trigger bass and sorted upper voices");
    // Repeating the same chord must not walk to a higher octave.
    const Chord gRepeat=voice(generate(55,majorScale),55,autoMode,&gAuto);
    check(gRepeat.notes==gAuto.notes,"AUTO repeated chord remains in stable register");
    // Musical QA: full progressions in all keys, diatonic scales, playable
    // registers and character modes. Check exact pitch classes, bass anchor,
    // deterministic repeat and bounded register throughout long repetitions.
    constexpr std::array<Scale,9> allScales{{
        Scale::Major,Scale::NaturalMinor,Scale::HarmonicMinor,
        Scale::MelodicMinorAscending,Scale::Dorian,Scale::Phrygian,
        Scale::Lydian,Scale::Mixolydian,Scale::Locrian}};
    int verifiedChords=0;
    int autoDifferent=0;
    for (auto scale : allScales) for(int tonic=0;tonic<12;++tonic)
    for(int octave=3;octave<=6;++octave)
    for(auto character : {Character::Classic,Character::Dark,
                          Character::Wide,Character::Power}) {
        Settings tonal; tonal.scale=scale; tonal.tonic=static_cast<uint8_t>(tonic);
        VoicingSettings musical; musical.character=character; musical.leading=Leading::Auto;
        VoicingSettings staticMode=musical; staticMode.leading=Leading::Off;
        Chord prior{};
        const auto degrees=intervals(scale);
        for(int pass=0;pass<4;++pass) for(int step=0;step<8;++step) {
            const int idx=step==7?0:step;
            const int root=octave*12+tonic+degrees[idx];
            if(root>103)continue;
            const Chord generated=generate(root,tonal);
            if(!generated.accepted)continue;
            const Chord autoChord=voice(generated,root,musical,
                                        prior.accepted?&prior:nullptr);
            const Chord offChord=voice(generated,root,staticMode,
                                       prior.accepted?&prior:nullptr);
            check(autoChord.accepted && offChord.accepted,
                  "QA: every legal triad must have valid voicing");
            check(autoChord.count==3 && autoChord.notes[0]==root,
                  "QA: input bass fixed");
            check(autoChord.notes[0]<autoChord.notes[1] &&
                  autoChord.notes[1]<autoChord.notes[2] &&
                  autoChord.notes[2]<=127 &&
                  autoChord.notes[2]-root<=23,
                  "QA: ordered, bounded MIDI register");
            if(character==Character::Power) {
                check(autoChord.notes[0]==root && autoChord.notes[1]==root+7 &&
                      autoChord.notes[2]==root+12,
                      "QA: POWER deliberately uses root/fifth/octave");
            } else {
                for(int n=1;n<3;++n) {
                    bool matches=false;
                    for(int src=0;src<3;++src)
                        if(mod12(static_cast<int>(autoChord.notes[n])-
                                 static_cast<int>(generated.notes[src]))==0)
                            matches=true;
                    check(matches,"QA: upper voice must retain chord pitch class");
                }
            }
            const Chord exactRepeat=voice(generated,root,musical,&autoChord);
            check(exactRepeat.notes==autoChord.notes,
                  "QA: identical held chord must not migrate");
            if(autoChord.notes!=offChord.notes) ++autoDifferent;
            prior=autoChord;
            ++verifiedChords;
        }
    }
    check(verifiedChords>10000,"QA: extensive harmonic coverage");
    check(autoDifferent>0,"QA: AUTO must yield some real inversions");
    // Quantitative AUTO/OFF QA. Compare both upper voices over complete
    // progressions, excluding bass which is deliberately anchored.
    int measuredTransitions=0, improvedTransitions=0, worsenedTransitions=0;
    int64_t autoTravel=0, offTravel=0;
    for(auto scale : allScales) for(int tonic=0;tonic<12;++tonic)
    for(int octave=4;octave<=5;++octave)
    for(auto character : {Character::Classic,Character::Dark,Character::Wide}) {
        Settings h;h.scale=scale;h.tonic=static_cast<uint8_t>(tonic);
        VoicingSettings a;a.leading=Leading::Auto;a.character=character;
        VoicingSettings o;o.leading=Leading::Off;o.character=character;
        Chord pa{},po{};
        constexpr std::array<int,16> steps{{0,4,5,3,1,4,0,2,5,0,3,4,1,5,2,0}};
        for(int round=0;round<3;++round)for(int degree:steps) {
            const int root=octave*12+tonic+intervals(scale)[degree];
            Chord generated=generate(root,h);
            if(!generated.accepted)continue;
            Chord ca=voice(generated,root,a,pa.accepted?&pa:nullptr);
            Chord co=voice(generated,root,o,po.accepted?&po:nullptr);
            if(pa.accepted && po.accepted) {
                int da=0,doo=0;
                for(int i=1;i<3;++i) {
                    da+=std::abs(static_cast<int>(ca.notes[i])-static_cast<int>(pa.notes[i]));
                    doo+=std::abs(static_cast<int>(co.notes[i])-static_cast<int>(po.notes[i]));
                }
                autoTravel+=da;offTravel+=doo;
                ++measuredTransitions;
                if(da<doo)++improvedTransitions;
                if(da>doo)++worsenedTransitions;
            }
            pa=ca;po=co;
        }
    }
    std::cout<<"Voice-leading comparison: transitions="<<measuredTransitions
             <<" AUTO travel="<<autoTravel<<" OFF travel="<<offTravel
             <<" better="<<improvedTransitions<<" worse="<<worsenedTransitions<<"\\n";
    check(measuredTransitions>10000 && improvedTransitions>0,
          "QA: AUTO/OFF comparison must cover and improve real transitions");
    check(autoTravel<offTravel,
          "QA: AUTO must reduce aggregate upper-voice movement vs OFF");
    // Control NoteOns on the same sample apply regardless of host event order.
    ControlLayout layout;
    check(layout.valid() && layout.isControl(26) && !layout.isControl(60), "valid controls");
    const std::array<InputEvent,3> firstOrder{{{12,60,true,false,11},
                                                {12,28,true,false,12},
                                                {13,28,false,true,12}}};
    const std::array<InputEvent,3> reverseOrder{{{12,28,true,false,12},
                                                  {12,60,true,false,11},
                                                  {13,28,false,true,12}}};
    check(modifierAt(firstOrder,3,12,layout).variant==Variant::Sus4, "sus4 selected");
    check(modifierAt(reverseOrder,3,12,layout).variant==Variant::Sus4, "order independent");
    check(modifierAt(firstOrder,3,13,layout).variant==Variant::Normal, "no held mode");
    const std::array<InputEvent,2> conflicting{{{4,26,true,false,1},
                                                 {4,28,true,false,2}}};
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
                                    {0,28,true,false,201},
                                    {48,60,false,true,101},
                                    {48,28,false,true,201}}};
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
    for(int pitch : {24,26,28}) {
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
    std::array<InputEvent,1> onlyControl{{{0,24,true,false,9}}};
    check(proc.process(onlyControl,1) && proc.outputCount()==0, "normal key is silent");
    // Regression: Studio One's C0/D0/E0 labels map to MIDI 24/26/28.
    // The incorrect 48/50/52 mapping used to mute C2/D2/E2.
    Settings cMinor; cMinor.scale=Scale::NaturalMinor;
    MidiChordProcessor<> octaveRepro;
    octaveRepro.setSettings(cMinor);
    octaveRepro.setVoicing(exact);
    const std::array<InputEvent,1> c2Trigger{{{0,48,true,false,320}}};
    check(octaveRepro.process(c2Trigger,1) &&
          octaveRepro.outputCount()==3 &&
          octaveRepro.output()[0].pitch==48 &&
          octaveRepro.activeVoices()==1,
          "C2 must generate a chord instead of acting as C0 control");
    const std::array<InputEvent,1> c2Release{{{0,48,false,true,320}}};
    check(octaveRepro.process(c2Release,1) &&
          octaveRepro.activeVoices()==0,
          "C2 chord releases normally");
    const std::array<InputEvent,2> d0Modifier{{{0,26,true,false,400},
                                                 {0,60,true,false,401}}};
    check(octaveRepro.process(d0Modifier,2) &&
          octaveRepro.outputCount()==3 &&
          octaveRepro.activeVoices()==1 &&
          octaveRepro.output()[0].pitch==60 &&
          octaveRepro.output()[1].pitch==62 &&
          octaveRepro.output()[2].pitch==67,
          "D0 requests SUS2 without generating an overlapping chord");
    int identifiedRoot=-1;
    Quality identifiedQuality=Quality::Major;
    check(octaveRepro.singleChord(identifiedRoot,identifiedQuality) &&
          identifiedRoot==60 && identifiedQuality==Quality::Sus2,
          "D0 leaves one correctly labeled chord active");
    const std::array<InputEvent,1> laterD0{{{0,26,true,false,402}}};
    check(octaveRepro.process(laterD0,1) &&
          octaveRepro.outputCount()==0 &&
          octaveRepro.activeVoices()==1 &&
          octaveRepro.singleChord(identifiedRoot,identifiedQuality),
          "later D0 stays silent and must not cause overlap readout");
    const std::array<InputEvent,1> cNoteOff{{{0,60,false,true,401}}};
    check(octaveRepro.process(cNoteOff,1) &&
          octaveRepro.activeVoices()==0,
          "SUS2 chord releases without hanging notes");
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
    // Rejected MIDI output: compensate only for events the host accepted.
    MidiDelivery outGuard;
    const std::array<MidiMessage,3> outgoing{{{0,60,true,100},
                                               {0,64,true,100},
                                               {0,67,true,100}}};
    int attempts=0;
    check(!outGuard.deliver(outgoing.data(),outgoing.size(),
          [&](const MidiMessage&){return ++attempts==1;}),
          "partial host event rejection must begin recovery");
    check(outGuard.recovering() && outGuard.hostOn()[60] &&
          !outGuard.hostOn()[64],
          "only host-accepted notes count as playing");
    check(!outGuard.recover([&](const MidiMessage&){return false;}),
          "rejected recovery NoteOff must remain pending");
    int recoveryCount=0;
    check(outGuard.recover([&](const MidiMessage& m){
          ++recoveryCount;return !m.on && m.pitch==60;}),
          "recovery retries and releases exact live pitch");
    check(recoveryCount==1 && !outGuard.recovering() &&
          !outGuard.hostOn()[60],"recovery state cleared");
    check(outGuard.deliver(outgoing.data(),outgoing.size(),
          [&](const MidiMessage&){return true;}),"processing can resume");
    outGuard.requireRecovery();
    check(outGuard.recover([&](const MidiMessage& m){return !m.on;}),
          "full panic releases all accepted notes");

    // Verify every automation point is ordered relative to MIDI samples.
    std::array<InputEvent,4> timedNotes{{{24,60,true,false,21},
        {48,60,false,true,21},{72,60,true,false,22},
        {96,60,false,true,22}}};
    std::array<TimedParameter,3> timedParams{{{72,101,1.0},
        {24,101,0.0},{96,101,0.5}}};
    int currentScale=-1,groupsSeen=0,applied=0;
    check(runTimeline(timedNotes,4,timedParams,3,
          [&](const TimedParameter& p){
              currentScale=p.value==0.0?0:1;++applied;
          },
          [&](const auto& group,size_t n){
              if(group[0].noteOn)
                  check(currentScale==(group[0].offset==24?0:1),
                        "parameter point must precede same-sample note");
              ++groupsSeen;return n==1;
          }),"timeline processing successful");
    check(groupsSeen==4 && applied==3,"all events and parameter points processed");

    // The displayed chord must come from its generated identity, not
    // a guess based on the four lowest sounding MIDI notes.
    MidiChordProcessor<> named;
    std::array<InputEvent,1> aChord{{{0,60,true,false,31}}};
    int chordRoot=-1;Quality chordQuality=Quality::Major;
    check(named.process(aChord,1) &&
          named.singleChord(chordRoot,chordQuality) &&
          chordRoot==60 && chordQuality==Quality::Major,
          "one active chord identity is exact");
    std::array<InputEvent,1> bChord{{{0,64,true,false,32}}};
    check(named.process(bChord,1) &&
          !named.singleChord(chordRoot,chordQuality),
          "simultaneous chords have no falsely asserted single identity");
    check(named.stop(),"overlap reset");
    VoicingSettings powerIdentity;powerIdentity.character=Character::Power;
    named.setVoicing(powerIdentity);
    check(named.process(aChord,1) &&
          named.singleChord(chordRoot,chordQuality) &&
          chordQuality==Quality::Power,"POWER identity is explicit");
    check(studioOneOctaveFor(0)==-2 &&
          studioOneOctaveFor(24)==0 &&
          studioOneOctaveFor(48)==2 &&
          studioOneOctaveFor(60)==3,
          "DAW octave notation: MIDI 24=C0, MIDI 48=C2, MIDI 60=C3");
    std::cout << "ChordEngine tests PASS\n";
}
