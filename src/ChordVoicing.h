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

// Deterministic, no heap allocation. The current incoming pitch is the register anchor.
// Previous chord is optional: after seek/stop we explicitly start from a new anchor.
inline Chord voice(Chord in, int rootMidi, VoicingSettings settings, const Chord* previous = nullptr) noexcept {
    if (!in.accepted || in.count < 3 || rootMidi < 0 || rootMidi > 127) return {};
    // First process a triad. Root octave doubling remains separate at the host layer.
    if (in.count > 3) in.count = 3;
    if (settings.character == Character::Power) {
        if (rootMidi + 12 > 127) return {};
        in.notes = {static_cast<uint8_t>(rootMidi), static_cast<uint8_t>(rootMidi+7),
                    static_cast<uint8_t>(rootMidi+12), 0};
        in.count = 3;
        in.quality = Quality::Power;
        return in;
    }
    // The trigger note is always the bass. OFF is deterministic and does
    // not depend on the previous chord. AUTO only moves the two upper voices.
    const std::array<int,3> base{{in.notes[0],in.notes[1],in.notes[2]}};
    if (settings.leading==Leading::Off && settings.character==Character::Classic)
        return in;
    std::array<int,3> best=base;
    int64_t bestScore=std::numeric_limits<int64_t>::max();
    const bool hasPrevious=settings.leading==Leading::Auto && previous &&
                           previous->accepted && previous->count>=3;
    // The previous MIDI chord is already stored in ascending pitch order.
    // Keep a stable register near the trigger, rather than following the
    // previous chord through unlimited octave changes.
    for(int shiftThird=-1;shiftThird<=1;++shiftThird)
    for(int shiftFifth=-1;shiftFifth<=1;++shiftFifth) {
        const int n1=base[1]+12*shiftThird;
        const int n2=base[2]+12*shiftFifth;
        if(n1<=rootMidi || n2<=rootMidi || n1>127 || n2>127 ||
           n1>rootMidi+23 || n2>rootMidi+23 || n1==n2) continue;
        std::array<int,3> candidate{{rootMidi,std::min(n1,n2),std::max(n1,n2)}};
        if (candidate[2]-candidate[1]>12) continue;
        // Register and spacing penalty prevents gradual octave drift.
        const int span=candidate[2]-rootMidi;
        const int target=settings.character==Character::Wide?19:
                         settings.character==Character::Dark?10:12;
        int64_t score=static_cast<int64_t>(span-target)*(span-target)*2;
        // Stay near the played octave even after many repetitions.
        score+=static_cast<int64_t>(candidate[1]-rootMidi-4)*
               (candidate[1]-rootMidi-4);
        if(hasPrevious) {
            for(int i=1;i<3;++i) {
                const int movement=candidate[i]-static_cast<int>(previous->notes[i]);
                score+=static_cast<int64_t>(movement)*movement*5;
                // Discourage large individual voice jumps.
                if(movement>7 || movement< -7) score+=120;
            }
        } else {
            for(int i=1;i<3;++i) {
                const int movement=candidate[i]-base[i];
                score+=static_cast<int64_t>(movement)*movement*6;
            }
        }
        if(score<bestScore || (score==bestScore && candidate<best)) {
            bestScore=score;
            best=candidate;
        }
    }
    if (bestScore==std::numeric_limits<int64_t>::max()) return in;
    for(int i=0;i<3;++i) in.notes[i]=static_cast<uint8_t>(best[i]);
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
