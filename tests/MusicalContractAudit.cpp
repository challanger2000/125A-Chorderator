#include "../src/ChordEngine.h"
#include "../src/ChordVoicing.h"
#include "../src/MidiChordProcessor.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace chorderator;

// Independent musical oracle: fixed modal pitch sets and stacked-triad
// quality tables. Do NOT compute expected qualities by calling generate().
static constexpr std::array<std::array<int,7>,9> kDegrees{{
    {{0,2,4,5,7,9,11}},{{0,2,3,5,7,8,10}},{{0,2,3,5,7,8,11}},
    {{0,2,3,5,7,9,11}},{{0,2,3,5,7,9,10}},{{0,1,3,5,7,8,10}},
    {{0,2,4,6,7,9,11}},{{0,2,4,5,7,9,10}},{{0,1,3,5,6,8,10}}
}};
static constexpr std::array<const char*,9> kQualities{{
    "MmmMMmd","mdMmmMM","mdamMMd","mmaMMdd","mmMMmdM",
    "mMMmdMm","MMmdMmm","MmdMmmM","dMmmMMm"
}};
static Quality decode(char v) {
    switch(v) {
        case 'M':return Quality::Major;
        case 'm':return Quality::Minor;
        case 'd':return Quality::Diminished;
        case 'a':return Quality::Augmented;
    }
    return Quality::Major;
}
static int mismatch(const char* message,int& failures) {
    ++failures;
    if(failures<=12)std::cout<<"FAIL: "<<message<<"\n";
    return failures;
}
int main() {
    int errors=0,triads=0,nonScaleSuspensions=0;
    int darkOffDistinct=0,wideOffDistinct=0,darkAutoDistinct=0,wideAutoDistinct=0;
    int nonScalePower=0;
    constexpr std::array<Scale,9> scales{{
        Scale::Major,Scale::NaturalMinor,Scale::HarmonicMinor,
        Scale::MelodicMinorAscending,Scale::Dorian,Scale::Phrygian,
        Scale::Lydian,Scale::Mixolydian,Scale::Locrian
    }};
    for(size_t si=0;si<scales.size();++si) {
        if(intervals(scales[si])!=kDegrees[si])
            mismatch("modal scale differs from independent pitch table",errors);
        for(int key=0;key<12;++key) {
            Settings s;s.scale=scales[si];s.tonic=static_cast<uint8_t>(key);
            for(int degree=0;degree<7;++degree) {
                const int root=48+key+kDegrees[si][static_cast<size_t>(degree)];
                const auto chord=generate(root,s);
                const int diatonicThird=kDegrees[si][static_cast<size_t>((degree+2)%7)] +
                                        (degree+2>=7?12:0)-kDegrees[si][static_cast<size_t>(degree)];
                const int diatonicFifth=kDegrees[si][static_cast<size_t>((degree+4)%7)] +
                                        (degree+4>=7?12:0)-kDegrees[si][static_cast<size_t>(degree)];
                if(!chord.accepted || chord.count!=3 || chord.notes[0]!=root ||
                   chord.notes[1]!=root+diatonicThird ||
                   chord.notes[2]!=root+diatonicFifth ||
                   chord.quality!=decode(kQualities[si][degree]))
                    mismatch("diatonic chord differs from independent interval oracle",errors);
                ++triads;
                for(auto variant:{Variant::Sus2,Variant::Sus4}) {
                    s.variant=variant;
                    const auto sus=generate(root,s);
                    const int inserted=variant==Variant::Sus2?2:5;
                    if(!sus.accepted || sus.notes[1]!=root+inserted ||
                       sus.notes[2]!=root+diatonicFifth)
                        mismatch("SUS replacement pitch mismatch",errors);
                    bool insertedInScale=false;
                    for(int n:kDegrees[si])if((n+key)%12==(root+inserted)%12)
                        insertedInScale=true;
                    if(!insertedInScale)++nonScaleSuspensions;
                }
                s.variant=Variant::Normal;
                const auto classic=voice(chord,root,{Character::Classic,Leading::Off});
                const auto dark=voice(chord,root,{Character::Dark,Leading::Off});
                const auto wide=voice(chord,root,{Character::Wide,Leading::Off});
                if(dark.notes!=classic.notes)++darkOffDistinct;
                if(wide.notes!=classic.notes)++wideOffDistinct;
                const auto power=voice(chord,root,{Character::Power,Leading::Off});
                bool perfectFifthInScale=false;
                for(int n:kDegrees[si])if((n+key)%12==(root+7)%12)
                    perfectFifthInScale=true;
                if(!perfectFifthInScale)++nonScalePower;
                if(power.notes[0]!=root || power.notes[1]!=root+7 ||
                   power.notes[2]!=root+12)
                    mismatch("POWER chord must be root fifth octave",errors);
            }
            Chord priorClassic{},priorDark{},priorWide{};
            for(int degree=0;degree<7;++degree) {
                const int root=48+key+kDegrees[si][static_cast<size_t>(degree)];
                const auto c=generate(root,s);
                const auto classic=voice(c,root,{Character::Classic,Leading::Auto},
                                         priorClassic.accepted?&priorClassic:nullptr);
                const auto dark=voice(c,root,{Character::Dark,Leading::Auto},
                                      priorDark.accepted?&priorDark:nullptr);
                const auto wide=voice(c,root,{Character::Wide,Leading::Auto},
                                      priorWide.accepted?&priorWide:nullptr);
                if(classic.notes!=dark.notes)++darkAutoDistinct;
                if(classic.notes!=wide.notes)++wideAutoDistinct;
                priorClassic=classic;priorDark=dark;priorWide=wide;
            }
            // Every pitch outside the scale should be silent with CHROMATIC OFF.
            for(int delta=0;delta<12;++delta) {
                bool inScale=false;
                for(int n:kDegrees[si])if(n==delta)inScale=true;
                if(!inScale && generate(48+key+delta,s).accepted)
                    mismatch("chromatic-disabled root produced a chord",errors);
            }
        }
    }
    std::cout<<"REFERENCE TRIADS: "<<triads<<"\n";
    std::cout<<"CHARACTER OFF different from CLASSIC: DARK="<<darkOffDistinct
             <<" WIDE="<<wideOffDistinct<<" of "<<triads<<"\n";
    std::cout<<"CHARACTER AUTO different from CLASSIC: DARK="<<darkAutoDistinct
             <<" WIDE="<<wideAutoDistinct<<" of "<<9*12*7<<"\n";
    std::cout<<"SUS NOTE OUTSIDE SELECTED SCALE, CHROMATIC OFF: "
             <<nonScaleSuspensions<<" of "<<triads*2<<"\n";
    std::cout<<"POWER FIFTH OUTSIDE SELECTED SCALE: "<<nonScalePower
             <<" of "<<triads<<"\n";
    // The product contract requires CHARACTER modes to have musically
    // different pitch-generation behavior, not merely renamed settings.
    if(darkOffDistinct==0)
        mismatch("DARK+OFF never changes chord notes relative to CLASSIC",errors);
    if(wideOffDistinct==0)
        mismatch("WIDE+OFF never changes chord notes relative to CLASSIC",errors);
    MidiChordProcessor<> trigger;
    const std::array<InputEvent,1> a{{{0,60,true,false,1,37}}};
    if(!trigger.process(a,1) || trigger.outputCount()!=3)
        mismatch("single note did not produce three MIDI NoteOns",errors);
    else {
        for(size_t i=0;i<trigger.outputCount();++i)
            if(trigger.output()[i].velocity!=37)
                mismatch("chord must preserve input velocity of 37",errors);
        std::cout<<"INPUT-TO-OUTPUT VELOCITY: input=37 output="
                 <<static_cast<int>(trigger.output()[0].velocity)<<"\\n";
    }
    const std::array<InputEvent,1> b{{{0,60,true,false,2}}};
    if(!trigger.process(b,1))mismatch("overlap rejected",errors);
    else std::cout<<"OVERLAPPED IDENTICAL TRIGGER: second NoteOn events="
                  <<trigger.outputCount()<<" (zero means no re-articulation)\n";
    // Same root and chord positions; each new attack keeps its new velocity.
    MidiChordProcessor<> expressive;
    std::array<uint8_t,3> original{};
    for(int dynamic:{19,109,127}) {
        const std::array<InputEvent,1> on{{{
             0,60,true,false,77,static_cast<uint8_t>(dynamic)}}};
        if(!expressive.process(on,1) || expressive.outputCount()!=3)
            mismatch("velocity fixture should emit three NoteOns",errors);
        else for(size_t i=0;i<3;++i) {
            const auto& event=expressive.output()[i];
            if(event.velocity!=dynamic)
                mismatch("generated velocity differs from played velocity",errors);
            if(dynamic==19)original[i]=event.pitch;
            else if(event.pitch!=original[i])
                mismatch("new velocity must not change cached chord notes",errors);
        }
        const std::array<InputEvent,1> off{{{0,60,false,true,77}}};
        if(!expressive.process(off,1) || expressive.activeVoices()!=0)
            mismatch("dynamic note failed to release",errors);
    }
    // Explicit chord inversions with independent reference pitches.
    const auto majorChord=generate(60,Settings{});
    const std::array<Chord,4> profiles{{
        voice(majorChord,60,{Character::Classic,Leading::Off}),
        voice(majorChord,60,{Character::Dark,Leading::Off}),
        voice(majorChord,60,{Character::Wide,Leading::Off}),
        voice(majorChord,60,{Character::Power,Leading::Off})}};
    constexpr std::array<std::array<int,3>,4> expectedProfiles{{
        {{60,64,67}},{{60,67,76}},{{60,76,79}},{{60,67,72}}}};
    for(size_t i=0;i<profiles.size();++i)
        for(size_t j=0;j<3;++j)
            if(profiles[i].notes[j]!=expectedProfiles[i][j])
                mismatch("CHARACTER independent C-major golden voicing failed",errors);
    Settings locrian;locrian.scale=Scale::Locrian;locrian.variant=Variant::Sus2;
    const auto altered=generate(60,locrian);
    if(!altered.accepted || altered.notes[0]!=60 || altered.notes[1]!=62 ||
       altered.notes[2]!=66 || altered.quality!=Quality::Sus2Flat5)
        mismatch("Locrian C-D-Gb must be identified as Csus2(b5)",errors);
    locrian.variant=Variant::Sus4;
    if(generate(60,locrian).quality!=Quality::Sus4Flat5)
        mismatch("Locrian suspended-fourth altered fifth is incorrectly named",errors);
    locrian.scale=Scale::HarmonicMinor;locrian.variant=Variant::Sus2;
    if(generate(63,locrian).quality!=Quality::Sus2Sharp5)
        mismatch("augmented-fifth sus2 lacks altered-fifth identity",errors);
    // Changing tonic/scale must invalidate voicing memory.
    MidiChordProcessor<> changeKey;
    const std::array<InputEvent,1> cOn{{{0,60,true,false,70}}};
    const std::array<InputEvent,1> cOff{{{0,60,false,true,70}}};
    if(!changeKey.process(cOn,1) || changeKey.outputCount()!=3 ||
       changeKey.output()[1].pitch!=64)
        mismatch("initial major harmony",errors);
    changeKey.process(cOff,1);
    Settings cMinor;cMinor.scale=Scale::NaturalMinor;
    changeKey.setSettings(cMinor);
    if(!changeKey.process(cOn,1) || changeKey.outputCount()!=3 ||
       changeKey.output()[1].pitch!=63)
        mismatch("voicing cache did not invalidate after scale change",errors);
    changeKey.process(cOff,1);
    // Golden true inversions (input keys C-F-G, root note is NOT always bass).
    // These are independent harmonic references, NOT generated expectations.
    MidiChordProcessor<> inverted;
    constexpr std::array<int,4> sequence{{60,65,67,60}};
    constexpr std::array<std::array<int,3>,4> inversionReference{{
        {{60,64,67}}, // C major root position
        {{60,65,69}}, // F/C  = second inversion
        {{62,67,71}}, // G/D  = second inversion
        {{60,64,67}}  // identical C on return, not a new raised register
    }};
    bool movedUp=false,movedDown=false;
    int firstInversionCount=0,secondInversionCount=0;
    for(size_t i=0;i<sequence.size();++i) {
        const int root=sequence[i],id=200+static_cast<int>(i);
        const std::array<InputEvent,1> on{{{0,static_cast<int16_t>(root),true,false,id}}};
        if(!inverted.process(on,1) || inverted.outputCount()!=3)
            mismatch("golden inversion: three notes expected",errors);
        else {
            for(int n=0;n<3;++n) {
                const int actual=inverted.output()[n].pitch;
                if(actual!=inversionReference[i][n])
                    mismatch("golden inverted chord pitch differs from reference",errors);
                if(i>0) {
                    const int movement=actual-inversionReference[i-1][n];
                    if(movement>0)movedUp=true;
                    if(movement<0)movedDown=true;
                }
            }
            int harmonicRoot=-1;Quality quality=Quality::Major;
            if(!inverted.singleChord(harmonicRoot,quality) || harmonicRoot!=root)
                mismatch("chord name/root must survive inversion",errors);
        }
        const std::array<InputEvent,1> off{{{0,static_cast<int16_t>(root),false,true,id}}};
        if(!inverted.process(off,1) || inverted.activeVoices()!=0)
            mismatch("inverted chord NoteOff ownership incorrect",errors);
    }
    if(!movedUp || !movedDown)
        mismatch("voicing must permit upward AND downward note movements",errors);
    // Check first/second inversion basses across many scale, character and
    // key contexts, not just one hand-picked F/C and G/D example.
    for(auto scale:scales)for(int tonic=0;tonic<12;++tonic) {
        Settings musical;musical.scale=scale;musical.tonic=static_cast<uint8_t>(tonic);
        for(auto character:{Character::Classic,Character::Dark,Character::Wide}) {
            MidiChordProcessor<> engine;
            engine.setSettings(musical);
            engine.setVoicing({character,Leading::Auto});
            const auto degrees=intervals(scale);
            for(int repeat=0;repeat<3;++repeat)
            for(int idx:{0,3,4,5,1,4,0,6}) {
                const int root=48+tonic+degrees[idx];
                const int id=300+repeat*8+idx;
                const auto source=generate(root,musical);
                const std::array<InputEvent,1> on{{{0,static_cast<int16_t>(root),true,false,id}}};
                if(!engine.process(on,1))
                    mismatch("inversion corpus note rejected",errors);
                else if(engine.outputCount()==3 && source.accepted) {
                    const int bass=engine.output()[0].pitch;
                    if(mod12(bass)==mod12(source.notes[1]))
                        ++firstInversionCount;
                    if(mod12(bass)==mod12(source.notes[2]))
                        ++secondInversionCount;
                }
                const std::array<InputEvent,1> off{{{0,static_cast<int16_t>(root),false,true,id}}};
                if(!engine.process(off,1))
                    mismatch("inversion corpus NoteOff failed",errors);
            }
        }
    }
    std::cout<<"GENUINE INVERSIONS: first="<<firstInversionCount
             <<" second="<<secondInversionCount<<"\\n";
    if(firstInversionCount==0 || secondInversionCount==0)
        mismatch("AUTO must produce both first and second inversions",errors);
    // Phrase reproducibility must not depend on having a full octave
    // descent at the end. C2-Eb2-G2-Bb2 -> C2 falls only ten semitones.
    int loopDiscrepancies=0;
    for(auto mode:{Character::Classic,Character::Dark,Character::Wide})
    for(bool doubleOn:{false,true}) {
        MidiChordProcessor<> pattern;
        Settings settings;settings.scale=Scale::NaturalMinor;
        settings.rootOctaveDouble=doubleOn;
        pattern.setSettings(settings);
        VoicingSettings voiceSettings;
        voiceSettings.character=mode;
        voiceSettings.leading=Leading::Auto;
        pattern.setVoicing(voiceSettings);
        constexpr std::array<int,4> phrase{{48,51,55,58}};
        std::array<std::array<uint8_t,4>,4> firstPitches{};
        std::array<size_t,4> firstCounts{};
        for(int cycle=0;cycle<4;++cycle) {
            for(size_t i=0;i<phrase.size();++i) {
                const int root=phrase[i],id=100+cycle*4+static_cast<int>(i);
                const std::array<InputEvent,1> on{{{
                    0,static_cast<int16_t>(root),true,false,id}}};
                if(!pattern.process(on,1))mismatch("phrase NoteOn rejected",errors);
                std::array<uint8_t,4> actual{};
                const size_t count=pattern.outputCount();
                if(count>4)mismatch("phrase emitted too many pitches",errors);
                for(size_t n=0;n<count && n<actual.size();++n)
                    actual[n]=pattern.output()[n].pitch;
                if(cycle==0) {
                    firstCounts[i]=count;
                    firstPitches[i]=actual;
                } else if(firstCounts[i]!=count || firstPitches[i]!=actual) {
                    if(++loopDiscrepancies<=6)
                        std::cout<<"LOOP REGISTER DRIFT: mode="<<static_cast<int>(mode)
                                 <<" doubled="<<doubleOn
                                 <<" cycle="<<cycle<<" index="<<i
                                 <<" root="<<root
                                 <<" expected upper="<<static_cast<int>(firstPitches[i][1])
                                 <<","<<static_cast<int>(firstPitches[i][2])
                                 <<" got="<<static_cast<int>(actual[1])
                                 <<","<<static_cast<int>(actual[2])<<"\\n";
                }
                const std::array<InputEvent,1> off{{{
                    0,static_cast<int16_t>(root),false,true,id}}};
                if(!pattern.process(off,1))
                    mismatch("phrase NoteOff rejected",errors);
            }
        }
    }
    std::cout<<"REPEATED SHORT-OCTAVE PHRASE DRIFT: "
             <<loopDiscrepancies<<" mismatches across 6 configurations\\n";
    if(loopDiscrepancies>0)
        mismatch("AUTO chord phrases drift when descending less than 12 semitones",errors);
    std::cout<<"MUSICAL CONTRACT RESULT: "<<(errors?"FAIL":"PASS")
             <<" / "<<errors<<" failures\n";
    return errors?1:0;
}
