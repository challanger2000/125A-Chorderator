#pragma once
#include "MidiControl.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
namespace chorderator {
struct TimedParameter {uint32_t offset=0,id=0;double value=0.;};
template<size_t NE,size_t NP,class Apply,class Process>
bool runTimeline(std::array<InputEvent,NE>& notes,size_t count,
                 std::array<TimedParameter,NP>& params,size_t pcount,
                 Apply&& apply,Process&& process) noexcept {
    if(count>NE || pcount>NP)return false;
    for(size_t i=1;i<count;++i) {
        const auto x=notes[i];size_t j=i;
        while(j>0 && notes[j-1].offset>x.offset){notes[j]=notes[j-1];--j;}
        notes[j]=x;
    }
    for(size_t i=1;i<pcount;++i) {
        const auto x=params[i];size_t j=i;
        while(j>0 && params[j-1].offset>x.offset){params[j]=params[j-1];--j;}
        params[j]=x;
    }
    std::array<InputEvent,NE> group{};
    size_t ni=0,pi=0;
    while(ni<count || pi<pcount) {
        const uint32_t at=std::min(ni<count?notes[ni].offset:UINT32_MAX,
                                   pi<pcount?params[pi].offset:UINT32_MAX);
        while(pi<pcount && params[pi].offset==at)apply(params[pi++]);
        size_t n=0;
        while(ni<count && notes[ni].offset==at)group[n++]=notes[ni++];
        if(n && !process(group,n))return false;
    }
    return true;
}
} // namespace chorderator
