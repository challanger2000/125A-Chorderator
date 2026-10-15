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

// The incoming MIDI note names the HARMONY, not the bass note.
// AUTO enumerates root-position AND true first/second inversions, allowing
// the third or fifth below the harmonic root, then minimizes three-voice
// motion (including the bass). All voices remain within a bounded register.
// OFF preserves the published CHARACTER profiles with played root as bass.
inline Chord voice(Chord in,int rootMidi,VoicingSettings settings,
                   const Chord* previous=nullptr) noexcept {
    if(!in.accepted || in.count<3 || rootMidi<0 || rootMidi>127)return {};
    if(in.count>3)in.count=3;
    in.harmonicRoot=static_cast<uint8_t>(rootMidi);
    if(settings.character==Character::Power) {
        if(rootMidi+12>127)return {};
        in.notes={static_cast<uint8_t>(rootMidi),
                  static_cast<uint8_t>(rootMidi+7),
                  static_cast<uint8_t>(rootMidi+12),0};
        in.count=3;
        in.quality=Quality::Power;
        return in;
    }
    const int third=static_cast<int>(in.notes[1])-rootMidi;
    const int fifth=static_cast<int>(in.notes[2])-rootMidi;
    const std::array<int,3> rootPosition{{rootMidi,rootMidi+third,rootMidi+fifth}};
    const std::array<int,3> characterReference=settings.character==Character::Dark
      ? std::array<int,3>{{rootMidi,rootMidi+fifth,rootMidi+third+12}}
      : settings.character==Character::Wide
      ? std::array<int,3>{{rootMidi,rootMidi+third+12,rootMidi+fifth+12}}
      : rootPosition;
    if(settings.leading==Leading::Off) {
        if(characterReference[2]<=127) {
            for(int i=0;i<3;++i)
                in.notes[i]=static_cast<uint8_t>(characterReference[i]);
        }
        return in;
    }
    const bool hasPrevious=previous && previous->accepted && previous->count>=3;
    // The exact same harmonic root/quality/pitch classes must NEVER drift.
    // harmonicRoot distinguishes symmetric diminished-chord root identities.
    if(hasPrevious && previous->harmonicRoot==rootMidi &&
       previous->quality==in.quality) {
        std::array<int,3> expected{{mod12(rootPosition[0]),mod12(rootPosition[1]),
                                     mod12(rootPosition[2])}};
        std::array<int,3> sounding{{mod12(previous->notes[0]),
                                     mod12(previous->notes[1]),
                                     mod12(previous->notes[2])}};
        std::sort(expected.begin(),expected.end());
        std::sort(sounding.begin(),sounding.end());
        if(expected==sounding) {
            for(int i=0;i<3;++i)in.notes[i]=previous->notes[i];
            return in;
        }
    }
    std::array<int,3> best=rootPosition;
    int64_t bestScore=std::numeric_limits<int64_t>::max();
    // Three chord factors, each independently movable by one/two octaves.
    // Sort the resulting pitches to represent the REAL inversion bass.
    // This fixed 48-candidate search has no allocation or host dependencies.
    for(int rootShift=-1;rootShift<=1;++rootShift)
    for(int thirdShift=-1;thirdShift<=2;++thirdShift)
    for(int fifthShift=-1;fifthShift<=2;++fifthShift) {
        std::array<int,3> candidate{{
          rootMidi+12*rootShift,
          rootMidi+third+12*thirdShift,
          rootMidi+fifth+12*fifthShift}};
        std::sort(candidate.begin(),candidate.end());
        if(candidate[0]<0 || candidate[2]>127 ||
           candidate[0]<rootMidi-12 || candidate[2]>rootMidi+24 ||
           candidate[0]>=candidate[1] || candidate[1]>=candidate[2] ||
           candidate[2]-candidate[0]>23)continue;
        // Character sets an overall voicing preference, but must not lock
        // the harmonic root into the bottom voice.
        int64_t score=0;
        const int profileWeight=settings.character==Character::Classic?2:3;
        for(int i=0;i<3;++i) {
            const int m=candidate[i]-characterReference[i];
            score+=static_cast<int64_t>(m)*m*profileWeight;
        }
        if(hasPrevious) {
            for(int i=0;i<3;++i) {
                const int movement=candidate[i]-static_cast<int>(previous->notes[i]);
                score+=static_cast<int64_t>(movement)*movement*9;
                if(movement>7 || movement< -7)score+=100;
            }
        }
        // Mild penalty for wide spacing when a compact character is chosen.
        const int span=candidate[2]-candidate[0];
        if(settings.character==Character::Classic && span>12)
            score+=static_cast<int64_t>(span-12)*(span-12)*2;
        if(settings.character==Character::Wide && span<15)
            score+=static_cast<int64_t>(15-span)*(15-span)*3;
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
