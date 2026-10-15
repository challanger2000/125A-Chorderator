#pragma once
#include "ChordEngine.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace chorderator {

// Contract for per-chord MIDI modifiers, prior to VST3 event translation.
// Only simultaneous control NoteOns select a variant; control NoteOffs never
// produce musical MIDI and do not change the selected chord mid-note.
// Later separate NoteOns default to NORMAL, even when a control key is held.
struct InputEvent {
    uint32_t offset = 0; // sample offset within one processing block
    int16_t pitch = -1;
    bool noteOn = false;
    bool noteOff = false;
    int32_t noteId = -1;
};
struct Modifier {
    Variant variant = Variant::Normal;
    bool conflict = false;
};
struct ControlLayout {
    int16_t basePitch = 12; // default C0 with DAW-dependent octave label
    static constexpr int width = 3;
    constexpr bool valid() const noexcept {
        return basePitch >= 0 && basePitch + width <= 128;
    }
    constexpr bool isControl(int pitch) const noexcept {
        return valid() && pitch >= basePitch && pitch < basePitch + width;
    }
};
template <size_t N>
Modifier modifierAt(const std::array<InputEvent,N>& events, size_t count,
                    uint32_t offset, ControlLayout layout) noexcept {
    Modifier result;
    if (!layout.valid()) { result.conflict=true; return result; }
    bool seen=false;
    // Insensitive to the event ordering at the same sample position.
    for (size_t i=0; i<count && i<N; ++i) {
        const auto& event=events[i];
        if (!event.noteOn || event.noteOff || event.offset!=offset ||
            !layout.isControl(event.pitch)) continue;
        const auto candidate=static_cast<Variant>(event.pitch-layout.basePitch);
        if (seen && candidate!=result.variant) result.conflict=true;
        result.variant=candidate;
        seen=true;
    }
    // Conflicting simultaneous controls are rejected (NORMAL instead of
    // choosing based on the arbitrary host event order).
    if (result.conflict) result.variant=Variant::Normal;
    return result;
}
} // namespace chorderator
