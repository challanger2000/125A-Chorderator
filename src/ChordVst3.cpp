#include "MidiChordProcessor.h"
#include "MidiDelivery.h"
#include "MidiTimeline.h"
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
    kLiveCount=200, kLivePitch1=201, kLivePitch2=202, kLivePitch3=203, kLivePitch4=204,
    kLiveChordRoot=205, kLiveChordQuality=206 };
constexpr uint32_t kMagic=0x31444843u; // CHD1
constexpr uint32_t kVersion=1;
struct StoredState {
    uint32_t magic=kMagic, version=kVersion;
    int32_t key=0, scale=0, chromatic=0, leading=1, character=0, doubled=0, controlBase=24;
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
    chorderator::MidiDelivery delivery_{};
    static void publish(ProcessData& data,ParamID id,int value,int max) {
        if(!data.outputParameterChanges) return;
        int32 index=0;
        auto* q=data.outputParameterChanges->addParameterData(id,index);
        if(!q) return;
        q->addPoint(0,static_cast<ParamValue>(value)/max,index);
    }
    void publishNotes(ProcessData& data) {
        int count=0;
        std::array<int,4> pitches{};
        const auto& audible=delivery_.hostOn();
        for(int n=0;n<128;++n) if(audible[static_cast<size_t>(n)]) {
            if(count<4)pitches[static_cast<size_t>(count)]=n;
            ++count;
        }
        publish(data,kLiveCount,count,128);
        for(size_t i=0;i<4;++i)
            publish(data,kLivePitch1+static_cast<ParamID>(i),pitches[i],127);
        int root=128;
        chorderator::Quality quality=chorderator::Quality::Major;
        if(!delivery_.recovering() && core_.singleChord(root,quality)) {
            publish(data,kLiveChordRoot,root,128);
            publish(data,kLiveChordQuality,static_cast<int>(quality),7);
        } else {
            publish(data,kLiveChordRoot,128,128);
            publish(data,kLiveChordQuality,7,7);
        }
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
        core_.setControlLayout({24});
    }
    void update(ParamID id,ParamValue v) {
        switch(id) {
        case kKey: settings_.key=quantized(v,12);break;
        case kScale: settings_.scale=quantized(v,9);break;
        case kChromatic: settings_.chromatic=quantized(v,2);break;
        case kVoiceLeading: settings_.leading=quantized(v,2);break;
        case kCharacter: settings_.character=quantized(v,4);break;
        case kRootDouble: settings_.doubled=quantized(v,2);break;
        case kControlBase: return; // legacy state compatibility only
        default:return;
        }
        apply();
    }
    static bool sendEvent(IEventList* dest,const chorderator::MidiMessage& m,int32 offset) {
        if(!dest)return false;
        Event e{};
        e.busIndex=0;
        e.sampleOffset=offset>=0?offset:static_cast<int32>(m.offset);
        e.type=m.on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(m.on) {
            e.noteOn.channel=0;
            e.noteOn.pitch=static_cast<int16>(m.pitch);
            e.noteOn.velocity=static_cast<float>(m.velocity)/127.f;
            e.noteOn.noteId=-1;
        } else {
            e.noteOff.channel=0;
            e.noteOff.pitch=static_cast<int16>(m.pitch);
            e.noteOff.noteId=-1;
        }
        return dest->addEvent(e)==kResultOk;
    }
    bool emit(IEventList* dest,int32 offset=-1) {
        return delivery_.deliver(core_.output(),core_.outputCount(),
            [&](const chorderator::MidiMessage& m) noexcept {
                return sendEvent(dest,m,offset);
            });
    }
    void requestRecovery() noexcept {
        core_.stop();
        delivery_.requireRecovery();
    }
    bool drainRecovery(IEventList* dest) {
        return delivery_.recover([&](const chorderator::MidiMessage& m) noexcept {
            return sendEvent(dest,m,0);
        });
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
        if(!active) { core_.stop(); delivery_.clearForDeactivation(); transportWasPlaying_=false; }
        return AudioEffect::setActive(active);
    }
    tresult PLUGIN_API getState(IBStream* stream) SMTG_OVERRIDE {
        return writeState(stream,settings_)?kResultOk:kResultFalse;
    }
    tresult PLUGIN_API setState(IBStream* stream) SMTG_OVERRIDE {
        StoredState loaded{};
        if(!readState(stream,loaded)) return kResultFalse;
        core_.stop();
        delivery_.requireRecovery();
        settings_=loaded;apply();
        return kResultOk;
    }
    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE {
        // Refuse new notes until NoteOff recovery for every host-accepted
        // note has finished. A permanently rejecting host cannot be forced.
        if(delivery_.recovering() && !drainRecovery(data.outputEvents)) {
            publishNotes(data);
            return kResultOk;
        }
        if(!data.outputEvents) {
            requestRecovery();
            publishNotes(data);
            return kResultOk;
        }
        const bool known=data.processContext!=nullptr;
        const bool playing=known &&
            ((data.processContext->state & ProcessContext::kPlaying)!=0);
        if(known && transportWasPlaying_ && !playing) {
            if(!core_.stop() || !emit(data.outputEvents,0)) {
                requestRecovery();
                publishNotes(data);
                return kResultOk;
            }
        }
        if(known)transportWasPlaying_=playing;

        constexpr size_t kEvents=256,kParams=256;
        std::array<chorderator::InputEvent,kEvents> notes{};
        std::array<chorderator::TimedParameter,kParams> params{};
        size_t count=0,pcount=0;
        if(data.inputParameterChanges) {
            for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i) {
                auto* q=data.inputParameterChanges->getParameterData(i);
                if(!q)continue;
                const ParamID id=q->getParameterId();
                if(id<kKey || id>kRootDouble)continue;
                for(int32 j=0;j<q->getPointCount();++j) {
                    int32 at=0;ParamValue value=0.;
                    if(q->getPoint(j,at,value)!=kResultOk ||
                       at<0 || at>=data.numSamples)continue;
                    if(pcount==kParams) {
                        requestRecovery();publishNotes(data);return kResultOk;
                    }
                    params[pcount++]={static_cast<uint32_t>(at),id,value};
                }
            }
        }
        if(data.inputEvents) {
            const int32 total=data.inputEvents->getEventCount();
            for(int32 i=0;i<total;++i) {
                Event e{};
                if(data.inputEvents->getEvent(i,e)!=kResultOk)continue;
                const bool on=e.type==Event::kNoteOnEvent;
                const bool off=e.type==Event::kNoteOffEvent;
                if(!on && !off)continue;
                const auto pitch=on?e.noteOn.pitch:e.noteOff.pitch;
                const auto id=on?e.noteOn.noteId:e.noteOff.noteId;
                if(pitch<0 || pitch>127 ||
                   e.sampleOffset<0 || e.sampleOffset>=data.numSamples)continue;
                if(count==kEvents) {
                    requestRecovery();publishNotes(data);return kResultOk;
                }
                notes[count++]={static_cast<uint32_t>(e.sampleOffset),
                                static_cast<int16_t>(pitch),on,off,id};
            }
        }
        const bool ok=chorderator::runTimeline(notes,count,params,pcount,
            [&](const chorderator::TimedParameter& p) noexcept {
                update(static_cast<ParamID>(p.id),p.value);
            },
            [&](const auto& group,size_t n) noexcept {
                return core_.process(group,n) && emit(data.outputEvents);
            });
        if(!ok)requestRecovery();
        publishNotes(data);
        return kResultOk;
    }
};

class ChorderatorController final : public EditControllerEx1,
    public VSTGUI::VST3EditorDelegate, public VSTGUI::IControlListener {
    VSTGUI::CTextLabel* allowedNotes_=nullptr;
    VSTGUI::CTextLabel* liveNotes_=nullptr;
    void refreshLiveNotes() {
        if(!liveNotes_)return;
        static constexpr const char* notes[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        static constexpr const char* qualities[]={"","m","dim","aug","sus2","sus4","5"};
        const int count=quantized(getParamNormalized(kLiveCount),129);
        if(count==0) {
            liveNotes_->setText("No active chord");
            liveNotes_->invalid();
            return;
        }
        const int root=quantized(getParamNormalized(kLiveChordRoot),129);
        const int quality=quantized(getParamNormalized(kLiveChordQuality),8);
        std::string line=root<128 && quality<7 ?
            std::string("CHORD: ")+notes[root%12]+qualities[quality] :
            "CHORD: -- (overlap / recovery)";
        line+="    |    Notes: ";
        for(int i=0;i<std::min(count,4);++i) {
            if(i)line+="  ";
            const int pitch=quantized(getParamNormalized(kLivePitch1+static_cast<ParamID>(i)),128);
            line+=notes[pitch%12];
            line+=std::to_string(chorderator::studioOneOctaveFor(pitch));
        }
        if(count>4)line+="  +"+std::to_string(count-4)+" more";
        liveNotes_->setText(line.c_str());
        liveNotes_->invalid();
    }
    VSTGUI::CTextButton* chromaticButton_=nullptr;
    VSTGUI::CTextButton* doubleButton_=nullptr;
    void refreshToggles() {
        if(chromaticButton_) {
            chromaticButton_->setTitle(getParamNormalized(kChromatic)>=0.5 ? "ON" : "OFF");
            chromaticButton_->invalid();
        }
        if(doubleButton_) {
            doubleButton_->setTitle(getParamNormalized(kRootDouble)>=0.5 ? "ON" : "OFF");
            doubleButton_->invalid();
        }
    }
    VSTGUI::CTextButton* zoomButton_=nullptr;
    VSTGUI::VST3Editor* activeEditor_=nullptr;
    double zoom_=1.0;
    void updateZoomText() {
        if(!zoomButton_) return;
        zoomButton_->setTitle(zoom_ >= 1.25 ? "150 %" : "100 %");
        zoomButton_->invalid();
    }
    void valueChanged(VSTGUI::CControl* control) SMTG_OVERRIDE {
        // Match the working 125A MIDIator kick-button handling: act only
        // on the press edge, not on the release/reset notification.
        if(control!=zoomButton_ || control->getValueNormalized()<=0.5f) return;
        zoom_=zoom_ >= 1.25 ? 1.0 : 1.5;
        updateZoomText();
        if(activeEditor_) activeEditor_->setZoomFactor(zoom_);
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
            if(control->getTag()==102) chromaticButton_=dynamic_cast<VSTGUI::CTextButton*>(control);
            if(control->getTag()==105) doubleButton_=dynamic_cast<VSTGUI::CTextButton*>(control);
            if(control->getTag()==900) {
                zoomButton_=dynamic_cast<VSTGUI::CTextButton*>(control);
                control->setListener(this);
                updateZoomText();
            }
        }
        refreshScaleNotes();refreshLiveNotes();refreshToggles();
        return view;
    }
    void willClose(VSTGUI::VST3Editor*) SMTG_OVERRIDE {
        allowedNotes_=nullptr;liveNotes_=nullptr;zoomButton_=nullptr;chromaticButton_=nullptr;doubleButton_=nullptr;activeEditor_=nullptr;
    }
    tresult PLUGIN_API setParamNormalized(ParamID id,ParamValue value) SMTG_OVERRIDE {
        const auto result=EditControllerEx1::setParamNormalized(id,value);
        if(id==kKey || id==kScale) refreshScaleNotes();
        if(id==kChromatic || id==kRootDouble) refreshToggles();
        if(id>=kLiveCount && id<=kLiveChordQuality) refreshLiveNotes();
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
        // Legacy state field is retained for backward-compatible preset recall.
        // The modifier layout itself is fixed at 48/50/52 and must not
        // pretend to offer a user-adjustable (nonfunctional) parameter.
        controlBase->getInfo().flags=ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden;
        controlBase->getInfo().defaultNormalizedValue=24.0/125.0;
        controlBase->setNormalized(24.0/125.0);
        parameters.addParameter(controlBase);
        for(ParamID id=kLiveCount;id<=kLiveChordQuality;++id) {
            const int max=id==kLiveCount || id==kLiveChordRoot?128:
                          id==kLiveChordQuality?7:127;
            auto* p=new RangeParameter(STR16("Live MIDI"),id,nullptr,0.,
                                       static_cast<double>(max),0.,max);
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
