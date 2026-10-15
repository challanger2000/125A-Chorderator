#pragma once
#include "ChordEngine.h"
#include "ChordVoicing.h"
#include "MidiControl.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace chorderator {
// Fixed-capacity sample-accurate event translator; no host API dependency.
// Input events are expected sorted by offset (order within an offset arbitrary).
// No partial state mutation on output saturation. Output requires enough room
// for every note emission at an offset. Not yet a VST3 processor.
struct MidiMessage {
    uint32_t offset=0;
    uint8_t pitch=0;
    bool on=false;
    uint8_t velocity=0;
};
template<size_t MaxVoices=64, size_t MaxOutput=1024>
class MidiChordProcessor {
    struct Held {
        bool active=false;
        int32_t id=-1;
        int16_t trigger=-1;
        Chord chord{};
    };
    std::array<Held,MaxVoices> held_{};
    std::array<uint16_t,128> sounding_{}; // distinct trigger owners per MIDI pitch
    std::array<MidiMessage,MaxOutput> output_{};
    size_t outputCount_=0;
    Settings settings_{};
    VoicingSettings voicing_{};
    ControlLayout controls_{};
    Chord previous_{};
public:
    void setSettings(Settings value) noexcept { settings_=value; }
    void setVoicing(VoicingSettings value) noexcept { voicing_=value; }
    bool setControlLayout(ControlLayout value) noexcept {
        if (!value.valid()) return false;
        controls_=value; return true;
    }
    const MidiMessage* output() const noexcept { return output_.data(); }
    size_t outputCount() const noexcept { return outputCount_; }
    size_t activeVoices() const noexcept {
        size_t n=0; for(const auto& h:held_) if(h.active) ++n; return n;
    }
    // Every block clears only the output, not the currently held notes.
    // Caller must drain all emitted notes; false means no state changes made
    // for the rejected event group (caller must retry or reset safely).
    template<size_t N>
    bool process(const std::array<InputEvent,N>& events, size_t count) noexcept {
        outputCount_=0;
        if(count>N) return false;
        for(size_t i=1;i<count;++i)
            if(events[i].offset<events[i-1].offset) return false;
        for(size_t i=0;i<count;) {
            const uint32_t offset=events[i].offset;
            size_t end=i+1;
            while(end<count && events[end].offset==offset) ++end;
            auto mod=modifierAt(events,count,offset,controls_);
            // Reserve upper bound for this group's on/off actions *before* modifying state.
            size_t maxNew=0;
            for(size_t j=i;j<end;++j) {
                if(controls_.isControl(events[j].pitch)) continue;
                if(events[j].noteOn || events[j].noteOff) maxNew+=4;
            }
            if(maxNew>MaxOutput-outputCount_) return false;
            // Releases first: same-sample restrikes work with overlap counts.
            for(size_t j=i;j<end;++j) {
                const auto& e=events[j];
                if(!e.noteOff || controls_.isControl(e.pitch)) continue;
                for(auto& h:held_) if(h.active && h.id==e.noteId && h.trigger==e.pitch) {
                    for(size_t k=0;k<h.chord.count;++k) {
                        const auto pitch=h.chord.notes[k];
                        if(sounding_[pitch] && --sounding_[pitch]==0)
                            output_[outputCount_++]={offset,pitch,false,0};
                    }
                    h.active=false; break;
                }
            }
            for(size_t j=i;j<end;++j) {
                const auto& e=events[j];
                if(!e.noteOn || e.noteOff || controls_.isControl(e.pitch)) continue;
                bool duplicate=false;
                Held* freeSlot=nullptr;
                for(auto& h:held_) {
                    if(e.noteId>=0 && h.active && h.id==e.noteId && h.trigger==e.pitch) duplicate=true;
                    if(!h.active && !freeSlot) freeSlot=&h;
                }
                if(duplicate || !freeSlot) continue;
                Settings chosen=settings_;
                chosen.variant=mod.variant;
                // Root double is added AFTER voicing to avoid doubling the wrong pitch.
                const bool octave=chosen.rootOctaveDouble;
                chosen.rootOctaveDouble=false;
                auto chord=generate(e.pitch,chosen);
                if(!chord.accepted) continue;
                chord=voice(chord,e.pitch,voicing_,
                            previous_.accepted ? &previous_ : nullptr);
                if(octave) chord=doubleRootAbove(chord,e.pitch);
                if(!chord.accepted) continue;
                *freeSlot={true,e.noteId,e.pitch,chord};
                previous_=chord;
                for(size_t k=0;k<chord.count;++k) {
                    const auto pitch=chord.notes[k];
                    if(sounding_[pitch]++==0)
                        output_[outputCount_++]={offset,pitch,true,100};
                }
            }
            i=end;
        }
        return true;
    }
    // Explicit panic: host must forward returned NoteOffs before clearing.
    bool stop(uint32_t offset=0) noexcept {
        outputCount_=0;
        size_t required=0;
        for(uint16_t n:sounding_) if(n) ++required;
        if(required>MaxOutput) return false;
        for(size_t i=0;i<sounding_.size();++i)
            if(sounding_[i]) output_[outputCount_++]={offset,static_cast<uint8_t>(i),false,0};
        sounding_.fill(0);
        for(auto& h:held_) h=Held{};
        previous_=Chord{};
        return true;
    }
};
} // namespace chorderator
