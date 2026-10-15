#pragma once
#include "ChordEngine.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace chorderator {
enum class Character : uint8_t { Classic, Dark, Wide, Power };
enum class Leading : uint8_t { Off, Auto };
struct VoicingSettings {
    Character character = Character::Classic;
    Leading leading = Leading::Auto;
};

// The input MIDI root remains the bass in every mode. Character profiles
// preserve the original chord pitch classes; they change register ONLY.
// CLASSIC: compact close triad (root, third, fifth).
// DARK:    open fifth under an octave-raised third (root, fifth, third+12).
// WIDE:    upper triad voices both one octave higher (root, third+12, fifth+12).
// POWER:   root, perfect fifth, octave; intentionally omits third.
// AUTO can choose nearby octave displacements against the preceding chord;
// the processor caches the first voicing per (pitch, variant) so a recurring
// trigger does not migrate over subsequent phrases.
inline Chord voice(Chord in,int rootMidi,VoicingSettings settings,
                   const Chord* previous=nullptr) noexcept {
    if(!in.accepted || in.count<3 || rootMidi<0 || rootMidi>127)return {};
    if(in.count>3)in.count=3;
    if(settings.character==Character::Power) {
        if(rootMidi+12>127)return {};
        in.notes={static_cast<uint8_t>(rootMidi),
                  static_cast<uint8_t>(rootMidi+7),
                  static_cast<uint8_t>(rootMidi+12),0};
        in.count=3;
        in.quality=Quality::Power;
        return in;
    }
    const std::array<int,3> base{{
        static_cast<int>(in.notes[0]),
        static_cast<int>(in.notes[1]),
        static_cast<int>(in.notes[2])}};
    const int third=base[1]-rootMidi;
    const int fifth=base[2]-rootMidi;
    const int p1=settings.character==Character::Dark?fifth:
                 settings.character==Character::Wide?third+12:third;
    const int p2=settings.character==Character::Dark?third+12:
                 settings.character==Character::Wide?fifth+12:fifth;
    const int pref1=rootMidi+p1,pref2=rootMidi+p2;
    // OFF has a complete and deterministic musical meaning for every mode.
    if(settings.leading==Leading::Off) {
        if(pref1>rootMidi && pref2>pref1 && pref2<=127) {
            in.notes[1]=static_cast<uint8_t>(pref1);
            in.notes[2]=static_cast<uint8_t>(pref2);
        }
        return in;
    }
    const bool hasPrevious=previous && previous->accepted && previous->count>=3;
    // Identity is a musical invariant: playing the same harmony again
    // cannot migrate its upper notes merely by updating the voice state.
    // Check the complete pitch-class set and quality, not just the root.
    if(hasPrevious && previous->notes[0]==rootMidi &&
       previous->quality==in.quality) {
        const int a=mod12(base[1]),b=mod12(base[2]);
        const int p=mod12(previous->notes[1]),q=mod12(previous->notes[2]);
        if((a==p && b==q)||(a==q && b==p)) {
            in.notes[1]=previous->notes[1];
            in.notes[2]=previous->notes[2];
            return in;
        }
    }
    std::array<int,3> best=base;
    int64_t bestScore=std::numeric_limits<int64_t>::max();
    for(int shiftThird=-1;shiftThird<=1;++shiftThird)
    for(int shiftFifth=-1;shiftFifth<=1;++shiftFifth) {
        const int n1=base[1]+12*shiftThird;
        const int n2=base[2]+12*shiftFifth;
        if(n1<=rootMidi||n2<=rootMidi||n1>127||n2>127||
           n1>rootMidi+23||n2>rootMidi+23||n1==n2)continue;
        const std::array<int,3> candidate{{
            rootMidi,std::min(n1,n2),std::max(n1,n2)}};
        if(candidate[2]-candidate[1]>12)continue;
        int64_t score=0;
        if(settings.character==Character::Classic) {
            const int span=candidate[2]-rootMidi;
            score=static_cast<int64_t>(span-12)*(span-12)*2;
            score+=static_cast<int64_t>(candidate[1]-rootMidi-4)*
                   (candidate[1]-rootMidi-4);
        } else {
            // The character profile is an explicit reference, not an
            // arbitrary change to the overall output level or chord quality.
            const int d1=candidate[1]-pref1,d2=candidate[2]-pref2;
            score=static_cast<int64_t>(d1)*d1*12+
                  static_cast<int64_t>(d2)*d2*12;
        }
        if(hasPrevious) {
            for(int i=1;i<3;++i) {
                const int movement=candidate[i]-static_cast<int>(previous->notes[i]);
                score+=static_cast<int64_t>(movement)*movement*5;
                if(movement>7 || movement< -7)score+=120;
            }
        } else if(settings.character==Character::Classic) {
            for(int i=1;i<3;++i) {
                const int delta=candidate[i]-base[i];
                score+=static_cast<int64_t>(delta)*delta*6;
            }
        }
        if(score<bestScore || (score==bestScore && candidate<best)) {
            bestScore=score;
            best=candidate;
        }
    }
    if(bestScore==std::numeric_limits<int64_t>::max())return in;
    for(int i=0;i<3;++i)in.notes[i]=static_cast<uint8_t>(best[i]);
    return in;
}
// Add the played root exactly one octave higher, regardless of AUTO voicing.
// If POWER already contains root+12, that note is not duplicated.
inline Chord doubleRootAbove(Chord chord, int rootMidi) noexcept {
    if (!chord.accepted || chord.count!=3 || rootMidi<0 || rootMidi+12>127)
        return {};
    const uint8_t octaveRoot=static_cast<uint8_t>(rootMidi+12);
    for(int i=0;i<3;++i)
        if(chord.notes[i]==octaveRoot) return chord;
    chord.notes[3]=octaveRoot;
    chord.count=4;
    return chord;
}

} // namespace chorderator
