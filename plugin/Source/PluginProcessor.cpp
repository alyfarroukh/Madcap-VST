#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "madcap/Resample.h"

namespace madcap {

namespace {
constexpr int kScratch = 4096;
} // namespace

juce::AudioProcessor::BusesProperties MadcapProcessor::makeBuses()
{
    // Instrument: stereo out. The input bus is only for REC (sidechain in a DAW, the
    // microphone in the Standalone app), so it is off by default inside a DAW.
    const bool standalone = juce::PluginHostType::getPluginLoadedAs() == juce::AudioProcessor::wrapperType_Standalone;
    return juce::AudioProcessor::BusesProperties()
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), standalone)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

MadcapProcessor::MadcapProcessor()
    : juce::AudioProcessor(makeBuses()),
      apvts_(*this, &undo_, "PARAMS", params::createLayout())
{
    for (int p = 0; p < P_COUNT; ++p) {
        params_[(size_t)p] = apvts_.getParameter(params::id(p));
        raw_[(size_t)p] = apvts_.getRawParameterValue(params::id(p));
        jassert(params_[(size_t)p] != nullptr && raw_[(size_t)p] != nullptr);
    }
    sample_.set(demoVoice(48000.0, 4.0f), 48000.0, "Demo voice (C)", true);
    image_.setPoster(0);
    scratch_.setSize(2, kScratch);
    startTimerHz(15);
}

MadcapProcessor::~MadcapProcessor()
{
    stopTimer();
    recActive_ = false;
    loader_.removeAllJobs(true, 10000);
}

//==============================================================================
void MadcapProcessor::prepareToPlay(double sampleRate, int)
{
    const juce::ScopedLock pl(prepareLock_);
    if (recActive_.load()) recActive_ = false;
    if (sampleRate <= 0) sampleRate = 44100.0;
    if (std::abs(recRate_ - sampleRate) > 0.5) { recBuf_.clear(); recBuf_.shrink_to_fit(); recLen_ = 0; }
    engine_.prepare(sampleRate);
    currentRate_ = sampleRate;
    installSampleIntoEngine(true);
    installImageIntoEngine(true);
    setLatencySamples(engine_.latencySamples());
    scratch_.setSize(2, kScratch, false, false, true);
}

void MadcapProcessor::releaseResources()
{
    engine_.reset();
}

bool MadcapProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    for (const auto& in : layouts.inputBuses)
        if (!in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
            return false;
    return true;
}

void MadcapProcessor::installSampleIntoEngine(bool now)
{
    const juce::ScopedLock pl(prepareLock_);
    const double sr = currentRate_.load();
    auto d = sample_.get();
    if (sr <= 0 || !d) return;
    auto src = std::make_unique<SourceBuffer>();
    src->samples = resampleMono(d->samples.data(), d->samples.size(), d->rate, sr);
    src->analyse((int)std::lround(sr));
    if (now) engine_.setSourceNow(std::move(src));
    else engine_.postSource(std::move(src));
}

void MadcapProcessor::installImageIntoEngine(bool now)
{
    auto d = image_.get();
    if (!d) return;
    auto g = std::make_unique<ImageGrid>();
    g->luma = d->luma;
    if (now) engine_.setImageNow(std::move(g));
    else engine_.postImage(std::move(g));
}

//==============================================================================
void MadcapProcessor::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn()) engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine_.noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine_.allNotesOff();
    else if (m.isPitchWheel()) engine_.pitchBend(m.getPitchWheelValue());
    else if (m.isControllerOfType(1)) engine_.modWheel((float)m.getControllerValue() / 127.0f);
}

void MadcapProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // 1) capture the input for REC before the buffer is overwritten
    if (recActive_.load(std::memory_order_acquire)) {
        recWriting_.store(true, std::memory_order_seq_cst);
        if (recActive_.load(std::memory_order_seq_cst) && getBusCount(true) > 0) {
            auto in = getBusBuffer(buffer, true, 0);
            const int ch = in.getNumChannels();
            const int len = recLen_.load(std::memory_order_relaxed);
            const int k = std::min(n, (int)recBuf_.size() - len);
            if (ch > 0 && k > 0) {
                float pk = recPeak_.load(std::memory_order_relaxed);
                const float g = 1.0f / (float)ch;
                for (int i = 0; i < k; ++i) {
                    float s = 0.0f;
                    for (int c = 0; c < ch; ++c) s += in.getReadPointer(c)[i];
                    s *= g;
                    recBuf_[(size_t)(len + i)] = s;
                    pk = std::max(pk, std::abs(s));
                }
                recPeak_.store(pk, std::memory_order_relaxed);
                recLen_.store(len + k, std::memory_order_release);
            }
            if (len + k >= (int)recBuf_.size()) recActive_.store(false);
        }
        recWriting_.store(false, std::memory_order_seq_cst);
    }

    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, n);

    // 2) host position / tempo
    HostInfo host;
    bool hostKnown = false;
    double hostBpm = 0.0;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            hostKnown = true;
            host.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hostBpm = *b;
            if (auto q = pos->getPpqPosition()) host.ppq = *q;
            if (auto bs = pos->getPpqPositionOfLastBarStart()) host.barStartPpq = *bs;
        }
    }
    if (!host.playing) host.ppq = -1.0;
    hostBpm_.store(hostBpm, std::memory_order_relaxed);
    hostPlaying_.store(host.playing, std::memory_order_relaxed);

    double bpm = raw_[P_BPM]->load();
    if (raw_[P_TEMPO_SRC]->load() < 0.5f && hostBpm > 1.0) bpm = juce::jlimit(30.0, 300.0, hostBpm);

    // 3) parameters -> engine targets (smoothed inside the engine)
    for (int p = 0; p < P_COUNT; ++p) engine_.setTarget(p, raw_[(size_t)p]->load(std::memory_order_relaxed));
    engine_.setTarget(P_BPM, (float)bpm);

    bool play = raw_[P_PLAY]->load() > 0.5f;
    if (raw_[P_FOLLOW_HOST]->load() > 0.5f && hostKnown && !isStandalone()) play = play && host.playing;
    engine_.setPlaying(play);

    // 4) render, splitting at MIDI events (sample accurate)
    float* l = scratch_.getWritePointer(0);
    float* r = scratch_.getWritePointer(1);
    auto out = getBusBuffer(buffer, false, 0);
    const int outCh = out.getNumChannels();
    const double spb = currentRate_.load() * 60.0 / bpm;
    int pos = 0;
    auto renderTo = [&](int end) {
        while (pos < end) {
            const int c = std::min(end - pos, scratch_.getNumSamples());
            HostInfo h = host;
            if (h.ppq >= 0.0 && spb > 0.0) h.ppq += pos / spb;
            engine_.setHost(h);
            engine_.process(l, r, c);
            if (outCh >= 2) {
                out.copyFrom(0, pos, l, c);
                out.copyFrom(1, pos, r, c);
            } else if (outCh == 1) {
                out.copyFrom(0, pos, l, c, 0.5f);
                out.addFrom(0, pos, r, c, 0.5f);
            }
            pos += c;
        }
    };
    for (const auto meta : midi) {
        renderTo(juce::jlimit(0, n, meta.samplePosition));
        handleMidi(meta.getMessage());
    }
    renderTo(n);
    midi.clear();
}

//==============================================================================
juce::AudioProcessorEditor* MadcapProcessor::createEditor()
{
    return new MadcapEditor(*this);
}

void MadcapProcessor::setValue(int p, float v)
{
    auto* prm = params_[(size_t)p];
    if (!prm) return;
    const float norm = prm->convertTo0to1(gs_param_clamp(p, v));
    if (std::abs(prm->getValue() - norm) < 1e-7f) return;
    prm->beginChangeGesture();
    prm->setValueNotifyingHost(norm);
    prm->endChangeGesture();
}

void MadcapProcessor::setStatus(const juce::String& s)
{
    const juce::ScopedLock sl(statusLock_);
    status_ = s;
}

juce::String MadcapProcessor::takeStatusMessage()
{
    const juce::ScopedLock sl(statusLock_);
    auto s = status_;
    status_.clear();
    return s;
}

void MadcapProcessor::timerCallback()
{
    engine_.collectGarbage();
    if (recWasActive_ && !recActive_.load())   // the 60 s buffer filled up: finish automatically
        stopRecording();
}

//==============================================================================
void MadcapProcessor::loadSampleAsync(const juce::File& f)
{
    setStatus("Loading " + f.getFileName() + " ...");
    loader_.addJob([this, f] {
        std::vector<float> mono;
        double rate = 0;
        auto err = SampleStore::decodeFile(f, mono, rate);
        if (err.isNotEmpty()) { setStatus(err); return; }
        sample_.set(std::move(mono), rate, f.getFileNameWithoutExtension(), false);
        installSampleIntoEngine(false);
        setStatus("Loaded " + f.getFileName());
        juce::MessageManager::callAsync([safe = juce::WeakReference<MadcapProcessor>(this)] {
            if (auto* self = safe.get()) {
                self->setValue(P_TRIM_IN, 0.0f);
                self->setValue(P_TRIM_OUT, 1.0f);
                self->setValue(P_SRC, 0.0f);
            }
        });
    });
}

void MadcapProcessor::loadImageAsync(const juce::File& f)
{
    setStatus("Loading " + f.getFileName() + " ...");
    loader_.addJob([this, f] {
        std::vector<uint32_t> px;
        auto err = ImageStore::decodeFile(f, px);
        if (err.isNotEmpty()) { setStatus("Image: " + err); return; }
        image_.setPicture(px, f.getFileNameWithoutExtension());
        installImageIntoEngine(false);
        setStatus("Loaded " + f.getFileName());
        juce::MessageManager::callAsync([safe = juce::WeakReference<MadcapProcessor>(this)] {
            if (auto* self = safe.get()) {
                self->setValue(P_IMG_READ, 0.0f);   // photos: listen to light areas
                self->setValue(P_SRC, 1.0f);
            }
        });
    });
}

void MadcapProcessor::selectPoster(int index)
{
    image_.setPoster(index);
    installImageIntoEngine(false);
    setValue(P_IMG_READ, 1.0f);                     // posters: listen to the dark ink
}

void MadcapProcessor::useDemoSample()
{
    sample_.set(demoVoice(48000.0, 4.0f), 48000.0, "Demo voice (C)", true);
    installSampleIntoEngine(false);
    setValue(P_SAMPLE_ROOT, 0.0f);
}

void MadcapProcessor::cropToCut()
{
    auto d = sample_.get();
    if (!d || d->samples.empty()) return;
    float a = value(P_TRIM_IN), b = value(P_TRIM_OUT);
    if (b < a) std::swap(a, b);
    const size_t n = d->samples.size();
    const size_t s = (size_t)((double)a * (double)n), e = std::min(n, (size_t)((double)b * (double)n));
    if (e <= s || e - s < 1024) { setStatus("Cut too short to crop"); return; }
    std::vector<float> cut(d->samples.begin() + (std::ptrdiff_t)s, d->samples.begin() + (std::ptrdiff_t)e);
    const double rate = d->rate;
    const juce::String name = d->name.endsWith(" (crop)") ? d->name : d->name + " (crop)";
    undo_.beginNewTransaction("Crop");
    setValue(P_TRIM_IN, 0.0f);
    setValue(P_TRIM_OUT, 1.0f);
    loader_.addJob([this, samples = std::move(cut), rate, name]() mutable {
        sample_.set(std::move(samples), rate, name, false);
        installSampleIntoEngine(false);
        setStatus("Cropped");
    });
}

//==============================================================================
bool MadcapProcessor::startRecording()
{
    const double sr = currentRate_.load();
    if (sr <= 0) { setStatus("Audio is not running"); return false; }
    if (!hasRecordInput()) {
        setStatus(isStandalone() ? "No microphone: pick an input in the audio settings"
                                 : "No input: route audio to Madcap's sidechain input");
        return false;
    }
    for (int i = 0; i < 200 && recWriting_.load(); ++i) juce::Thread::sleep(1);
    const size_t size = (size_t)(SampleStore::kMaxSeconds * sr);
    if (recBuf_.size() != size) recBuf_.assign(size, 0.0f);
    recRate_ = sr;
    recLen_ = 0;
    recPeak_ = 0.0f;
    recActive_.store(true, std::memory_order_release);
    recWasActive_ = true;
    setStatus("Recording ...");
    return true;
}

void MadcapProcessor::stopRecording()
{
    recWasActive_ = false;
    recActive_.store(false, std::memory_order_seq_cst);
    for (int i = 0; i < 200 && recWriting_.load(); ++i) juce::Thread::sleep(1);
    const int len = std::min(recLen_.exchange(0), (int)recBuf_.size());
    if (len < (int)(recRate_ * 0.05)) { setStatus("Recording too short"); return; }
    std::vector<float> take(recBuf_.begin(), recBuf_.begin() + len);
    normalizePeak(take, 0.89f);
    const double rate = recRate_;
    const auto name = "Recording " + juce::Time::getCurrentTime().formatted("%H:%M:%S");
    undo_.beginNewTransaction("Record");
    setValue(P_TRIM_IN, 0.0f);
    setValue(P_TRIM_OUT, 1.0f);
    setValue(P_SRC, 0.0f);
    loader_.addJob([this, samples = std::move(take), rate, name]() mutable {
        sample_.set(std::move(samples), rate, name, false);
        installSampleIntoEngine(false);
        setStatus(name + " is the new sample");
    });
}

float MadcapProcessor::recordingSeconds() const noexcept
{
    return recRate_ > 0 ? (float)recLen_.load() / (float)recRate_ : 0.0f;
}

void MadcapProcessor::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = now - lastTap_;
    lastTap_ = now;
    if (dt > 2000.0 || dt < 150.0) { taps_.clear(); return; }
    taps_.add(dt);
    while (taps_.size() > 4) taps_.remove(0);
    double sum = 0;
    for (auto t : taps_) sum += t;
    const double bpm = juce::jlimit(30.0, 300.0, 60000.0 / (sum / taps_.size()));
    setValue(P_TEMPO_SRC, 1.0f);
    setValue(P_BPM, (float)bpm);
}

void MadcapProcessor::randomize()
{
    undo_.beginNewTransaction("Randomize");
    juce::Random rng;
    for (int p = 0; p < P_COUNT; ++p) {
        const int page = GS_PARAMS[p].page;
        if (page == PAGE_GLOBAL || locks_[(size_t)page]) continue;
        switch (p) {
            case P_SRC: case P_TRIM_IN: case P_TRIM_OUT: case P_MASTER: case P_SAMPLE_ROOT: case P_KEY:
            case P_IMG_KEY: case P_MIDI_MODE: case P_LATCH: case P_VEL_AMT: case P_MOD_DEST: case P_PREVIEW_LOOP:
            case P_GRAIN_ON: case P_ARP_ON: case P_GRAIN_HOLD: case P_IMG_HOLD: case P_OCTAVE:
            case P_DEG1: case P_DEG2: case P_DEG3: case P_DEG4: case P_DEG5: case P_DEG6: case P_DEG7:
                continue;
            default: break;
        }
        float v = gs_param_from_norm(p, rng.nextFloat());
        switch (p) {   // keep it musical
            case P_REV_SIZE: v = std::min(v, 0.93f); break;
            case P_DLY_FB: v = std::min(v, 0.8f); break;
            case P_CMP_MAKEUP: v = std::min(v, 8.0f); break;
            case P_GRAIN_LVL: case P_ARP_LEVEL: case P_IMG_LEVEL: v = 0.6f + 0.6f * rng.nextFloat(); break;
            case P_ARP_PATTERN: v = (float)(rng.nextInt(65536) | 1); break;
            default: break;
        }
        setValue(p, v);
    }
    presetName_ = "Random";
}

//==============================================================================
int MadcapProcessor::getNumPrograms() { return (int)factoryPresets().size(); }

void MadcapProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < (int)factoryPresets().size()) loadFactoryPreset(index);
}

const juce::String MadcapProcessor::getProgramName(int index)
{
    if (index >= 0 && index < (int)factoryPresets().size()) return factoryPresets()[(size_t)index].name;
    return {};
}

juce::String MadcapProcessor::currentPresetName() const { return presetName_; }

void MadcapProcessor::applyPresetValues(const std::vector<std::pair<int, double>>& values)
{
    for (int p = 0; p < P_COUNT; ++p)
        if (!presetKeepsParameter(p)) setValue(p, GS_PARAMS[p].def);
    for (const auto& [p, v] : values)
        if (p >= 0 && p < P_COUNT && !presetKeepsParameter(p)) setValue(p, (float)v);
}

void MadcapProcessor::loadFactoryPreset(int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= (int)all.size()) return;
    const auto& fp = all[(size_t)index];
    undo_.beginNewTransaction(juce::String("Preset ") + fp.name);
    applyPresetValues(fp.values);
    if (fp.poster >= 0) {
        image_.setPoster(fp.poster);
        installImageIntoEngine(false);
    }
    currentProgram_ = index;
    presetName_ = fp.name;
}

bool MadcapProcessor::saveUserPreset(const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName(name.trim());
    if (clean.isEmpty()) return false;
    auto dir = userPresetDirectory();
    if (!dir.createDirectory()) return false;
    presetName_ = clean;
    auto xml = createStateXml(true);
    const auto file = dir.getChildFile(clean + presetExtension());
    const bool ok = xml->writeTo(file);
    setStatus(ok ? "Saved preset " + clean : "Could not save " + file.getFullPathName());
    return ok;
}

bool MadcapProcessor::loadUserPreset(const juce::File& f)
{
    auto xml = juce::parseXML(f);
    if (!xml || !xml->hasTagName("MADCAP")) { setStatus("Not a Madcap preset"); return false; }
    // a preset never changes transport / MIDI setup
    std::array<float, P_COUNT> kept{};
    for (int p = 0; p < P_COUNT; ++p) kept[(size_t)p] = value(p);
    undo_.beginNewTransaction("Preset " + f.getFileNameWithoutExtension());
    restoreStateXml(*xml);
    for (int p = 0; p < P_COUNT; ++p)
        if (presetKeepsParameter(p) && p != P_TRIM_IN && p != P_TRIM_OUT) setValue(p, kept[(size_t)p]);
    presetName_ = f.getFileNameWithoutExtension();
    return true;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> MadcapProcessor::createStateXml(bool includeMedia)
{
    auto root = std::make_unique<juce::XmlElement>("MADCAP");
    root->setAttribute("version", 1);
    root->setAttribute("preset", presetName_);
    root->setAttribute("program", currentProgram_);
    root->setAttribute("editorW", editorWidth);
    root->setAttribute("editorH", editorHeight);
    juce::String lockBits;
    for (bool b : locks_) lockBits << (b ? "1" : "0");
    root->setAttribute("locks", lockBits);

    if (auto x = apvts_.copyState().createXml()) root->addChildElement(x.release());

    if (includeMedia) {
        if (auto d = sample_.get()) {
            auto* s = root->createNewChildElement("SAMPLE");
            s->setAttribute("name", d->name);
            s->setAttribute("rate", d->rate);
            if (d->isDemo) s->setAttribute("demo", 1);
            else s->addTextElement(sample_.flac().toBase64Encoding());
        }
        if (auto img = image_.get()) {
            auto* e = root->createNewChildElement("IMAGE");
            e->setAttribute("name", img->name);
            if (img->poster >= 0) e->setAttribute("poster", img->poster);
            else e->addTextElement(image_.png().toBase64Encoding());
        }
    }
    return root;
}

void MadcapProcessor::restoreStateXml(const juce::XmlElement& xml)
{
    if (auto* p = xml.getChildByName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*p));
    presetName_ = xml.getStringAttribute("preset", presetName_);
    currentProgram_ = xml.getIntAttribute("program", currentProgram_);
    editorWidth = juce::jlimit(720, 1920, xml.getIntAttribute("editorW", editorWidth));
    editorHeight = juce::jlimit(408, 1088, xml.getIntAttribute("editorH", editorHeight));
    const auto lockBits = xml.getStringAttribute("locks");
    for (int i = 0; i < PAGE_COUNT && i < lockBits.length(); ++i) locks_[(size_t)i] = lockBits[i] == '1';

    if (auto* s = xml.getChildByName("SAMPLE")) {
        auto current = sample_.get();
        if (s->getBoolAttribute("demo")) {
            if (!current || !current->isDemo) {
                sample_.set(demoVoice(48000.0, 4.0f), 48000.0, "Demo voice (C)", true);
                installSampleIntoEngine(false);
            }
        } else {
            juce::MemoryBlock flac;
            if (flac.fromBase64Encoding(s->getAllSubText().trim()) && flac.getSize() > 0
                && !(current && !current->isDemo && sample_.flac() == flac)) {
                std::vector<float> mono;
                double rate = 0;
                if (SampleStore::decodeFlac(flac, mono, rate)) {
                    sample_.set(std::move(mono), rate, s->getStringAttribute("name", "Sample"), false);
                    installSampleIntoEngine(false);
                }
            }
        }
    }
    if (auto* e = xml.getChildByName("IMAGE")) {
        auto current = image_.get();
        if (e->hasAttribute("poster")) {
            const int poster = e->getIntAttribute("poster");
            if (!current || current->poster != poster) {
                image_.setPoster(poster);
                installImageIntoEngine(false);
            }
        } else {
            juce::MemoryBlock png;
            if (png.fromBase64Encoding(e->getAllSubText().trim()) && png.getSize() > 0
                && !(current && current->poster < 0 && image_.png() == png)) {
                if (image_.setFromPng(png, e->getStringAttribute("name", "Picture")))
                    installImageIntoEngine(false);
            }
        }
    }
}

void MadcapProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = createStateXml(true);
    copyXmlToBinary(*xml, destData);
}

void MadcapProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml) return;
    if (xml->hasTagName("MADCAP")) restoreStateXml(*xml);
    else if (xml->hasTagName(apvts_.state.getType())) apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    undo_.clearUndoHistory();
}

} // namespace madcap

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new madcap::MadcapProcessor();
}
