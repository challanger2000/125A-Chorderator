#include "MidiChordProcessor.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/uidescription/uiattributes.h"
#include <string>
#include <cstring>
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/ftypes.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace Steinberg::Vst {
namespace {
static const FUID kProcessorId(0x125A4301, 0x43484F52, 0x44455241, 0x544F5201);
static const FUID kControllerId(0x125A4302, 0x43484F52, 0x44455241, 0x544F5201);
enum Params : ParamID { kKey=100, kScale=101, kChromatic=102, kVoiceLeading=103,
    kCharacter=104, kRootDouble=105, kControlBase=106,
    kLiveCount=200, kLivePitch1=201, kLivePitch2=202, kLivePitch3=203, kLivePitch4=204 };
constexpr uint32_t kMagic=0x31444843u; // CHD1
constexpr uint32_t kVersion=1;
struct StoredState {
    uint32_t magic=kMagic, version=kVersion;
    int32_t key=0, scale=0, chromatic=0, leading=1, character=0, doubled=0, controlBase=12;
};
template<class T> bool ioWrite(IBStream* s,const T& v) {
    if (!s) return false;
    int32 written=0;
    return s->write(const_cast<T*>(&v),sizeof(T),&written)==kResultOk &&
        written==sizeof(T);
}
template<class T> bool ioRead(IBStream* s,T& v) {
    if(!s) return false;
    int32 read=0;
    return s->read(&v,sizeof(T),&read)==kResultOk && read==sizeof(T);
}
bool stateValid(const StoredState& v) {
    return v.magic==kMagic && v.version==kVersion &&
        v.key>=0 && v.key<12 && v.scale>=0 && v.scale<9 &&
        v.chromatic>=0 && v.chromatic<=1 && v.leading>=0 && v.leading<=1 &&
        v.character>=0 && v.character<4 && v.doubled>=0 && v.doubled<=1 &&
        v.controlBase>=0 && v.controlBase<=125;
}
bool readState(IBStream* stream,StoredState& result) {
    StoredState temp{};
    if(!ioRead(stream,temp.magic) || !ioRead(stream,temp.version) ||
       !ioRead(stream,temp.key) || !ioRead(stream,temp.scale) ||
       !ioRead(stream,temp.chromatic) || !ioRead(stream,temp.leading) ||
       !ioRead(stream,temp.character) || !ioRead(stream,temp.doubled) ||
       !ioRead(stream,temp.controlBase) || !stateValid(temp)) return false;
    result=temp; return true;
}
bool writeState(IBStream* stream,const StoredState& s) {
    return ioWrite(stream,s.magic) && ioWrite(stream,s.version) &&
           ioWrite(stream,s.key) && ioWrite(stream,s.scale) &&
           ioWrite(stream,s.chromatic) && ioWrite(stream,s.leading) &&
           ioWrite(stream,s.character) && ioWrite(stream,s.doubled) &&
           ioWrite(stream,s.controlBase);
}
int quantized(ParamValue normalized,int count) {
    return std::clamp(static_cast<int>(normalized*static_cast<double>(count-1)+0.5),0,count-1);
}
} // namespace

class ChorderatorProcessor final : public AudioEffect {
    chorderator::MidiChordProcessor<64,1024> core_;
    StoredState settings_{};
    bool transportWasPlaying_=false;
    std::array<bool,128> visiblePitches_{};
    static void publish(ProcessData& data,ParamID id,int value,int max) {
        if(!data.outputParameterChanges) return;
        int32 index=0;
        auto* q=data.outputParameterChanges->addParameterData(id,index);
        if(!q) return;
        q->addPoint(0,static_cast<ParamValue>(value)/max,index);
    }
    void publishNotes(ProcessData& data) {
        int count=0; std::array<int,4> pitches{};
        for(int n=0;n<128;++n) if(visiblePitches_[static_cast<size_t>(n)]) {
            if(count<4) pitches[static_cast<size_t>(count)]=n;
            ++count;
        }
        publish(data,kLiveCount,std::min(count,128),128);
        for(size_t i=0;i<4;++i) publish(data,kLivePitch1+static_cast<ParamID>(i),pitches[i],127);
    }
    void apply() {
        chorderator::Settings chord;
        chord.tonic=static_cast<uint8_t>(settings_.key);
        chord.scale=static_cast<chorderator::Scale>(settings_.scale);
        chord.chromatic=settings_.chromatic!=0;
        chord.rootOctaveDouble=settings_.doubled!=0;
        core_.setSettings(chord);
        chorderator::VoicingSettings voicing;
        voicing.leading=static_cast<chorderator::Leading>(settings_.leading);
        voicing.character=static_cast<chorderator::Character>(settings_.character);
        core_.setVoicing(voicing);
        core_.setControlLayout({static_cast<int16_t>(settings_.controlBase)});
    }
    void update(ParamID id,ParamValue v) {
        switch(id) {
        case kKey: settings_.key=quantized(v,12);break;
        case kScale: settings_.scale=quantized(v,9);break;
        case kChromatic: settings_.chromatic=quantized(v,2);break;
        case kVoiceLeading: settings_.leading=quantized(v,2);break;
        case kCharacter: settings_.character=quantized(v,4);break;
        case kRootDouble: settings_.doubled=quantized(v,2);break;
        case kControlBase: settings_.controlBase=quantized(v,126);break;
        default:return;
        }
        apply();
    }
    bool emit(IEventList* dest,int32_t offset) {
        if(!dest) return false;
        for(size_t i=0;i<core_.outputCount();++i) {
            const auto& m=core_.output()[i];
            Event e{};
            e.busIndex=0;
            e.sampleOffset=static_cast<int32>(m.offset);
            if(offset>=0) e.sampleOffset=offset;
            e.ppqPosition=0;
            e.type=m.on?Event::kNoteOnEvent:Event::kNoteOffEvent;
            if(m.on) {
                e.noteOn.channel=0;
                e.noteOn.pitch=static_cast<int16>(m.pitch);
                e.noteOn.velocity=static_cast<float>(m.velocity)/127.f;
                e.noteOn.length=0;
                e.noteOn.tuning=0;
                e.noteOn.noteId=-1;
            } else {
                e.noteOff.channel=0;
                e.noteOff.pitch=static_cast<int16>(m.pitch);
                e.noteOff.velocity=0;
                e.noteOff.tuning=0;
                e.noteOff.noteId=-1;
            }
            if(dest->addEvent(e)!=kResultOk) return false;
            visiblePitches_[m.pitch]=m.on;
        }
        return true;
    }
public:
    ChorderatorProcessor() { setControllerClass(kControllerId);apply(); }
    static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new ChorderatorProcessor()); }
    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE {
        auto res=AudioEffect::initialize(context);
        if(res!=kResultOk) return res;
        addEventInput(STR16("MIDI In"),1);
        addEventOutput(STR16("Chord MIDI Out"),1);
        return kResultOk;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 format) SMTG_OVERRIDE {
        return format==kSample32 || format==kSample64?kResultTrue:kResultFalse;
    }
    // AudioEffect's default setProcessing returns kNotImplemented.
    // A MIDI-only processor still must accept host processing transitions.
    tresult PLUGIN_API setProcessing(TBool state) SMTG_OVERRIDE {
        return kResultOk;
    }
    tresult PLUGIN_API setActive(TBool active) SMTG_OVERRIDE {
        if(!active) { core_.stop(); visiblePitches_.fill(false); transportWasPlaying_=false; }
        return AudioEffect::setActive(active);
    }
    tresult PLUGIN_API getState(IBStream* stream) SMTG_OVERRIDE {
        return writeState(stream,settings_)?kResultOk:kResultFalse;
    }
    tresult PLUGIN_API setState(IBStream* stream) SMTG_OVERRIDE {
        StoredState loaded{};
        if(!readState(stream,loaded)) return kResultFalse;
        core_.stop();
        visiblePitches_.fill(false);
        settings_=loaded;apply();
        return kResultOk;
    }
    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE {
        if(data.inputParameterChanges) {
            for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i) {
                auto* q=data.inputParameterChanges->getParameterData(i);
                if(!q) continue;
                int32 offset=0; ParamValue value=0;
                const int32 n=q->getPointCount();
                if(n>0 && q->getPoint(n-1,offset,value)==kResultOk) update(q->getParameterId(),value);
            }
        }
        if(!data.outputEvents) return kResultOk;
        // Transport discontinuity: release tracked notes before handling this block.
        // Not all hosts send process() after stop, so setActive(false) remains a
        // separate lifecycle reset; native host testing is mandatory.
        const bool transportKnown = data.processContext != nullptr;
        const bool playing = transportKnown &&
            ((data.processContext->state & ProcessContext::kPlaying) != 0);
        if(transportKnown && transportWasPlaying_ && !playing) {
            if(!core_.stop() || !emit(data.outputEvents,0)) return kResultFalse;
        }
        if(transportKnown) transportWasPlaying_=playing;
        constexpr size_t kCapacity=256;
        std::array<chorderator::InputEvent,kCapacity> incoming{};
        size_t count=0;
        if(data.inputEvents) {
            const int32 total=data.inputEvents->getEventCount();
            if(total>static_cast<int32>(kCapacity)) return kResultFalse;
            for(int32 i=0;i<total;++i) {
                Event event{};
                if(data.inputEvents->getEvent(i,event)!=kResultOk) continue;
                const bool on=event.type==Event::kNoteOnEvent;
                const bool off=event.type==Event::kNoteOffEvent;
                if(!on && !off) continue;
                const auto& n=on?event.noteOn.pitch:event.noteOff.pitch;
                const auto id=on?event.noteOn.noteId:event.noteOff.noteId;
                if(n<0 || n>127 || event.sampleOffset<0) continue;
                incoming[count++]={static_cast<uint32_t>(event.sampleOffset),
                                    static_cast<int16_t>(n),on,off,id};
            }
        }
        // Host event ordering is not assumed beyond sample offsets.
        // Stable insertion sort (bounded 256 events; no allocation).
        for(size_t i=1;i<count;++i) {
            const auto item=incoming[i];
            size_t j=i;
            while(j>0 && incoming[j-1].offset>item.offset) {
                incoming[j]=incoming[j-1]; --j;
            }
            incoming[j]=item;
        }
        if(!core_.process(incoming,count)) return kResultFalse;
        if(!emit(data.outputEvents,-1)) return kResultFalse;
        publishNotes(data);
        return kResultOk;
    }
};

class ChorderatorController final : public EditControllerEx1,
    public VSTGUI::VST3EditorDelegate, public VSTGUI::IControlListener {
    VSTGUI::CTextLabel* allowedNotes_=nullptr;
    VSTGUI::CTextLabel* liveNotes_=nullptr;
    void refreshLiveNotes() {
        if(!liveNotes_) return;
        static constexpr const char* names[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        const int count=quantized(getParamNormalized(kLiveCount),129);
        std::string label=count==0?"No active chord notes":"Active MIDI notes: ";
        for(int i=0;i<std::min(count,4);++i) {
            if(i) label+="  ";
            const auto n=quantized(getParamNormalized(kLivePitch1+static_cast<ParamID>(i)),128);
            label+=names[n%12]; label+=std::to_string(n/12); label+=" ("+std::to_string(n)+")";
        }
        if(count>4) label+="  +"+std::to_string(count-4)+" more";
        liveNotes_->setText(label.c_str());liveNotes_->invalid();
    }
    VSTGUI::CTextButton* zoomButton_=nullptr;
    VSTGUI::VST3Editor* activeEditor_=nullptr;
    double zoom_=1.0;
    void updateZoomText() {
        if(!zoomButton_) return;
        zoomButton_->setTitle(zoom_ >= 1.25 ? "ZOOM 150 %" : "ZOOM 100 %");
        zoomButton_->invalid();
    }
    void valueChanged(VSTGUI::CControl* control) SMTG_OVERRIDE {
        if(control!=zoomButton_ || !activeEditor_) return;
        zoom_=zoom_ >= 1.25 ? 1.0 : 1.5;
        activeEditor_->setZoomFactor(zoom_);
        updateZoomText();
    }
    void refreshScaleNotes() {
        if(!allowedNotes_) return;
        static constexpr const char* names[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        const int tonic=quantized(getParamNormalized(kKey),12);
        const auto scale=static_cast<chorderator::Scale>(quantized(getParamNormalized(kScale),9));
        const auto degrees=chorderator::intervals(scale);
        std::string value="Allowed notes: ";
        for(size_t i=0;i<degrees.size();++i) {
            if(i) value+="  ";
            value+=names[(tonic+degrees[i])%12];
        }
        allowedNotes_->setText(value.c_str());
        allowedNotes_->invalid();
    }
public:
    VSTGUI::CView* verifyView(VSTGUI::CView* view,
        const VSTGUI::UIAttributes& attributes,
        const VSTGUI::IUIDescription*, VSTGUI::VST3Editor* editor) SMTG_OVERRIDE {
        activeEditor_=editor;
        const auto* id=attributes.getAttributeValue("chorderator-id");
        if(id && *id=="allowedNotes") allowedNotes_=dynamic_cast<VSTGUI::CTextLabel*>(view);
        if(id && *id=="liveNotes") liveNotes_=dynamic_cast<VSTGUI::CTextLabel*>(view);
        if(auto* control=dynamic_cast<VSTGUI::CControl*>(view)) {
            if(control->getTag()==900) {
                zoomButton_=dynamic_cast<VSTGUI::CTextButton*>(control);
                control->setListener(this);
                updateZoomText();
            }
        }
        refreshScaleNotes();refreshLiveNotes();
        return view;
    }
    void willClose(VSTGUI::VST3Editor*) SMTG_OVERRIDE {
        allowedNotes_=nullptr;liveNotes_=nullptr;zoomButton_=nullptr;activeEditor_=nullptr;
    }
    tresult PLUGIN_API setParamNormalized(ParamID id,ParamValue value) SMTG_OVERRIDE {
        const auto result=EditControllerEx1::setParamNormalized(id,value);
        if(id==kKey || id==kScale) refreshScaleNotes();
        if(id>=kLiveCount && id<=kLivePitch4) refreshLiveNotes();
        return result;
    }

public:
    static FUnknown* create(void*) { return static_cast<IEditController*>(new ChorderatorController()); }
    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE {
        auto r=EditControllerEx1::initialize(context);
        if(r!=kResultOk) return r;
        auto* key=new StringListParameter(STR16("Key"),kKey);
        const char16_t* keys[]={STR16("C"),STR16("C#"),STR16("D"),STR16("Eb"),
          STR16("E"),STR16("F"),STR16("F#"),STR16("G"),STR16("Ab"),
          STR16("A"),STR16("Bb"),STR16("B")};
        for(auto* n:keys) key->appendString(n);
        parameters.addParameter(key);
        auto* scale=new StringListParameter(STR16("Scale"),kScale);
        for(auto* n:{STR16("Major"),STR16("Natural Minor"),
                     STR16("Harmonic Minor"),STR16("Melodic Minor"),
                     STR16("Dorian"),STR16("Phrygian"),STR16("Lydian"),
                     STR16("Mixolydian"),STR16("Locrian")})
            scale->appendString(n);
        parameters.addParameter(scale);
        auto* chromatic=new StringListParameter(STR16("Chromatic"),kChromatic);
        chromatic->appendString(STR16("OFF"));chromatic->appendString(STR16("ON"));
        parameters.addParameter(chromatic);
        auto* leading=new StringListParameter(STR16("Voice Leading"),kVoiceLeading);
        leading->appendString(STR16("OFF"));leading->appendString(STR16("AUTO"));
        leading->getInfo().defaultNormalizedValue=1.0;leading->setNormalized(1.0);
        parameters.addParameter(leading);
        auto* character=new StringListParameter(STR16("Character"),kCharacter);
        for(auto* n:{STR16("CLASSIC"),STR16("DARK"),STR16("WIDE"),STR16("POWER")})
            character->appendString(n);
        parameters.addParameter(character);
        auto* doubled=new StringListParameter(STR16("Root Double"),kRootDouble);
        doubled->appendString(STR16("OFF"));doubled->appendString(STR16("ON"));
        parameters.addParameter(doubled);
        auto* controlBase=new StringListParameter(STR16("Modifier MIDI Base"),kControlBase);
        for(int n=0;n<=125;++n) {
            const auto label=std::to_string(n);
            Steinberg::Vst::String128 text{};
            Steinberg::UString(text,128).fromAscii(label.c_str());
            controlBase->appendString(text);
        }
        controlBase->getInfo().defaultNormalizedValue=12.0/125.0;
        controlBase->setNormalized(12.0/125.0);
        parameters.addParameter(controlBase);
        for(ParamID id=kLiveCount;id<=kLivePitch4;++id) {
            auto* p=new RangeParameter(STR16("Live MIDI"),id,nullptr,0.,
                                       id==kLiveCount?128.:127.,0.,
                                       id==kLiveCount?128:127);
            p->getInfo().flags=ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden;
            parameters.addParameter(p);
        }
        return kResultOk;
    }
    IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE {
        if(name && std::strcmp(name,ViewType::kEditor)==0) {
            auto* editor=new VSTGUI::VST3Editor(this,"ChorderatorView","chorderator.uidesc");
            editor->setDelegate(this);
            editor->setAllowedZoomFactors({1.0,1.5});
            editor->setZoomFactor(zoom_);
            return editor;
        }
        return nullptr;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) SMTG_OVERRIDE {
        StoredState state{};
        if(!readState(stream,state)) return kResultFalse;
        setParamNormalized(kKey,state.key/11.0);
        setParamNormalized(kScale,state.scale/8.0);
        setParamNormalized(kChromatic,state.chromatic);
        setParamNormalized(kVoiceLeading,state.leading);
        setParamNormalized(kCharacter,state.character/3.0);
        setParamNormalized(kRootDouble,state.doubled);
        setParamNormalized(kControlBase,state.controlBase/125.0);
        return kResultOk;
    }
};

} // namespace Steinberg::Vst

BEGIN_FACTORY_DEF("125A Audio Software", "https://github.com/challanger2000", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::kProcessorId),
           PClassInfo::kManyInstances, kVstAudioEffectClass, "125A CHORDERATOR",
           Vst::kDistributable, Vst::PlugType::kInstrumentSynth, "1.0.0",
           kVstVersionString, Steinberg::Vst::ChorderatorProcessor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::kControllerId),
           PClassInfo::kManyInstances, kVstComponentControllerClass, "125A CHORDERATOR Controller",
           0, "", "1.0.0", kVstVersionString,
           Steinberg::Vst::ChorderatorController::create)
END_FACTORY
