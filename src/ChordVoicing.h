#pragma once
#include "ChordEngine.h"
#include <array>
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
        return in;
    }
    // Rotate chord tone placements within a +-12 semitone band of incoming root.
    // Minimize squared movements from the last chord (voice-leading), with stable
    // lexicographic tie-breaking. No change if leading is off.
    const std::array<int,3> base{{in.notes[0],in.notes[1],in.notes[2]}};
    std::array<int,3> best=base;
    int64_t bestScore=std::numeric_limits<int64_t>::max();
    if (settings.leading == Leading::Off && settings.character == Character::Classic) return in;
    for (int shift0=-1;shift0<=1;++shift0)
    for (int shift1=-1;shift1<=1;++shift1)
    for (int shift2=-1;shift2<=1;++shift2) {
        std::array<int,3> c{{base[0]+12*shift0,base[1]+12*shift1,base[2]+12*shift2}};
        if (c[0]<0 || c[2]>127 || c[1]<0 || c[1]>127 || c[0]>127 || c[2]<0) continue;
        if (!(c[0]<c[1] && c[1]<c[2])) continue;
        // Anchor center to the user's trigger octave.
        if (c[0]<rootMidi-12 || c[2]>rootMidi+24) continue;
        const int spread=c[2]-c[0];
        const int reference= settings.character==Character::Wide?19:
                             settings.character==Character::Dark?7:12;
        const int diff=spread-reference;
        int64_t score=static_cast<int64_t>(diff)*diff*4;
        if (settings.character==Character::Dark) score+=(c[2]-rootMidi+12)*2;
        if (settings.leading==Leading::Auto && previous && previous->accepted && previous->count>=3) {
            for(int i=0;i<3;++i) {
                const int movement=c[i]-static_cast<int>(previous->notes[i]);
                score+=static_cast<int64_t>(movement)*movement*3;
            }
        } else {
            for(int i=0;i<3;++i) {
                const int movement=c[i]-base[i];
                score+=static_cast<int64_t>(movement)*movement;
            }
        }
        if (score<bestScore || (score==bestScore && c<best)) {bestScore=score;best=c;}
    }
    if (bestScore==std::numeric_limits<int64_t>::max()) return in;
    for(int i=0;i<3;++i) in.notes[i]=static_cast<uint8_t>(best[i]);
    return in;
}
inline Chord doubleRootAbove(Chord chord, int rootMidi) noexcept {
    if (!chord.accepted || chord.count!=3) return {};
    const int pitchClass=mod12(rootMidi);
    int highest=0;
    for(int i=0;i<3;++i) if(chord.notes[i]>highest) highest=chord.notes[i];
    int octaveRoot=highest+1;
    while(octaveRoot<=127 && mod12(octaveRoot)!=pitchClass) ++octaveRoot;
    if (octaveRoot>127) return {}; // atomic rejection, never send partial MIDI.
    chord.notes[3]=static_cast<uint8_t>(octaveRoot);
    chord.count=4;
    return chord;
}
} // namespace chorderator
