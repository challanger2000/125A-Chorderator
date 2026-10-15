#pragma once
#include <array>
#include <cstdint>

namespace chorderator {

// Pure, allocation-free music engine. No VST3 SDK dependency.
enum class Scale : uint8_t { Major, NaturalMinor, HarmonicMinor, MelodicMinorAscending, Dorian, Phrygian, Lydian, Mixolydian, Locrian };
enum class Quality : uint8_t { Major, Minor, Diminished, Augmented, Sus2, Sus4 };
enum class Variant : uint8_t { Normal, Sus2, Sus4 };
struct Settings {
    uint8_t tonic = 0; // pitch class 0=C ... 11=B
    Scale scale = Scale::Major;
    bool chromatic = false;
    Variant variant = Variant::Normal;
    bool rootOctaveDouble = false;
};
struct Chord {
    std::array<uint8_t,4> notes{};
    uint8_t count = 0;
    Quality quality = Quality::Major;
    bool accepted = false;
};
constexpr int mod12(int value) noexcept { return ((value % 12) + 12) % 12; }
constexpr std::array<int,7> intervals(Scale scale) noexcept {
    switch (scale) {
        case Scale::Major: return {0,2,4,5,7,9,11};
        case Scale::NaturalMinor: return {0,2,3,5,7,8,10};
        case Scale::HarmonicMinor: return {0,2,3,5,7,8,11};
        case Scale::MelodicMinorAscending: return {0,2,3,5,7,9,11};
        case Scale::Dorian: return {0,2,3,5,7,9,10};
        case Scale::Phrygian: return {0,1,3,5,7,8,10};
        case Scale::Lydian: return {0,2,4,6,7,9,11};
        case Scale::Mixolydian: return {0,2,4,5,7,9,10};
        case Scale::Locrian: return {0,1,3,5,6,8,10};
    }
    return {0,2,4,5,7,9,11};
}
constexpr int degreeFor(int pitch, const Settings& settings) noexcept {
    const auto pitches = intervals(settings.scale);
    for (int i=0;i<7;++i)
        if (mod12(pitch - settings.tonic) == pitches[i]) return i;
    return -1;
}
constexpr Quality classify(int third, int fifth) noexcept {
    if (third == 4 && fifth == 7) return Quality::Major;
    if (third == 3 && fifth == 7) return Quality::Minor;
    if (third == 3 && fifth == 6) return Quality::Diminished;
    if (third == 4 && fifth == 8) return Quality::Augmented;
    return Quality::Major; // unexpected stacked intervals: handled as pitch set, see tests
}
constexpr Chord generate(int midiPitch, Settings settings) noexcept {
    Chord out;
    if (midiPitch < 0 || midiPitch > 127 || settings.tonic > 11) return out;
    const auto scale = intervals(settings.scale);
    const int degree = degreeFor(midiPitch, settings);
    if (degree < 0 && !settings.chromatic) return out;
    int third = 4, fifth = 7;
    if (degree >= 0) {
        const int base = scale[degree];
        third = scale[(degree+2)%7] + ((degree+2)>=7 ? 12:0) - base;
        fifth = scale[(degree+4)%7] + ((degree+4)>=7 ? 12:0) - base;
    }
    out.quality = classify(third,fifth);
    if (settings.variant == Variant::Sus2) { third = 2; out.quality = Quality::Sus2; }
    if (settings.variant == Variant::Sus4) { third = 5; out.quality = Quality::Sus4; }
    // Invalid upper pitches fail atomically; no partial chords.
    if (midiPitch + fifth > 127 || midiPitch + third > 127) return Chord{};
    out.notes[0] = static_cast<uint8_t>(midiPitch);
    out.notes[1] = static_cast<uint8_t>(midiPitch+third);
    out.notes[2] = static_cast<uint8_t>(midiPitch+fifth);
    out.count = 3;
    if (settings.rootOctaveDouble) {
        if (midiPitch+12 > 127) return Chord{};
        out.notes[3] = static_cast<uint8_t>(midiPitch+12);
        out.count = 4;
    }
    out.accepted = true;
    return out;
}

// Stores the exact generated note set from NoteOn for corresponding NoteOff.
// Fixed capacity, no allocation, safe independent of parameter changes.
struct Voice {
    int32_t sourceId = -1;
    int16_t triggerPitch = -1;
    Chord chord{};
    bool active = false;
};
template <size_t N = 64> class VoiceLedger {
    std::array<Voice,N> voices_{};
public:
    bool start(int32_t sourceId, int triggerPitch, Chord chord) noexcept {
        if (!chord.accepted) return false;
        for (auto& v: voices_) if (v.active && v.sourceId==sourceId && v.triggerPitch==triggerPitch) return false;
        for (auto& v: voices_) if (!v.active) {
            v = Voice{sourceId,static_cast<int16_t>(triggerPitch),chord,true};
            return true;
        }
        return false;
    }
    Chord release(int32_t sourceId, int triggerPitch) noexcept {
        for (auto& v: voices_) if (v.active && v.sourceId==sourceId && v.triggerPitch==triggerPitch) {
            v.active = false;
            return v.chord;
        }
        return {};
    }
    template <typename Fn> void flush(Fn&& fn) noexcept {
        for (auto& v: voices_) if (v.active) {
            fn(v.sourceId,v.chord);
            v.active=false;
        }
    }
    size_t activeCount() const noexcept {
        size_t n=0; for(const auto& v:voices_) if(v.active) ++n; return n;
    }
};
} // namespace chorderator
