#pragma once
#include "MidiChordProcessor.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace chorderator {
// Tracks only MIDI notes accepted by the host. On partial rejection,
// suspend new NoteOns and retry offs on subsequent processing blocks.
class MidiDelivery {
    std::array<bool,128> active_{};
    bool recovering_=false;
public:
    bool recovering() const noexcept { return recovering_; }
    const std::array<bool,128>& hostOn() const noexcept { return active_; }
    void requireRecovery() noexcept { recovering_=true; }
    void clearForDeactivation() noexcept {
        active_.fill(false);recovering_=false;
    }
    template<class Send>
    bool deliver(const MidiMessage* events,size_t count,Send&& send) noexcept {
        if(recovering_) return false;
        for(size_t i=0;i<count;++i) {
            if(!send(events[i])) {
                recovering_=true;return false;
            }
            active_[events[i].pitch]=events[i].on;
        }
        return true;
    }
    template<class Send>
    bool recover(Send&& send) noexcept {
        if(!recovering_)return true;
        for(int p=0;p<128;++p) if(active_[static_cast<size_t>(p)]) {
            const MidiMessage off{0,static_cast<uint8_t>(p),false,0};
            if(!send(off)) return false;
            active_[static_cast<size_t>(p)]=false;
        }
        recovering_=false;
        return true;
    }
};
} // namespace chorderator
