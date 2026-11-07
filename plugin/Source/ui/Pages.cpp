#include "Pages.h"

namespace madcap::ui {

namespace {

juce::String midiNoteName(int m)
{
    return juce::String(GS_NOTE_NAMES[((m % 12) + 12) % 12]) + juce::String(m / 12 - 1);
}

void setGroup(MadcapProcessor& p, const juce::String& undoName, std::initializer_list<std::pair<int, float>> values)
{
    p.undoManager().beginNewTransaction(undoName);
    for (const auto& [id, v] : values) p.setValue(id, v);
}

} // namespace

//==============================================================================
Page::Page(MadcapProcessor& p, int pageIndex, const juce::String& title)
    : proc_(p), page_(pageIndex), title_(title), lock_(p, pageIndex)
{
    addAndMakeVisible(lock_);   // the editor sets the bounds once the derived page exists
}

void Page::tick()
{
    update();
    for (auto* t : ticks_) t->tick();
}

void Page::paint(juce::Graphics&) {}

void Page::resized()
{
    layout();
    lock_.setBounds(W - 74, H - 16, 74, 16);
}

//==============================================================================
class SamplePage : public Page {
public:
    explicit SamplePage(MadcapProcessor& p)
        : Page(p, PAGE_SAMPLE, "Sample"),
          wave_(add<WaveformView>(p)), slices_(add<SliceStrip>(p)),
          load_(button("Load")), rec_(button("Rec", colours::orange)), play_(button("Play", colours::green)),
          loop_(led(P_PREVIEW_LOOP, colours::green)), crop_(button("Crop", colours::orange)), demo_(button("Demo")),
          input_(seg(P_SRC)), root_(stepper(P_SAMPLE_ROOT, {}, true)),
          count_(seg(P_SLICES)), mode_(seg(P_SLICE_MODE)), slice_(stepper(P_SLICE_SEL)), span_(stepper(P_SLICE_SPAN)),
          tempoSrc_(seg(P_TEMPO_SRC)), bpm_(add<BpmField>(p)), follow_(led(P_FOLLOW_HOST, colours::amber)),
          tap_(button("Tap", colours::amber))
    {
        load_.setTooltip("Load WAV, AIFF, FLAC, MP3 or OGG (up to 60 s, mixed to mono). You can also drop a file on Madcap.");
        rec_.setTooltip(p.isStandalone() ? "Record from the audio input (choose it in the audio settings), press again to stop"
                                         : "Record from Madcap's sidechain input, press again to stop");
        play_.setTooltip("Audition the cut dry (works while the transport is stopped)");
        crop_.setTooltip("Make the cut permanent");
        demo_.setTooltip("Load the built-in sung 'ahh' demo voice (C)");
        tap_.setTooltip("Tap the tempo");
        input_.setAccent(colours::green);
        loop_.setLabel("Loop");
        follow_.setLabel("Follow host transport");
        load_.onClick = [this] {
            chooser_ = std::make_unique<juce::FileChooser>("Load a sample", juce::File(), "*.wav;*.wave;*.aif;*.aiff;*.aifc;*.flac;*.mp3;*.ogg");
            chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& fc) {
                                      auto f = fc.getResult();
                                      if (f.existsAsFile()) proc_.loadSampleAsync(f);
                                  });
        };
        rec_.onClick = [this] {
            if (proc_.isRecording()) proc_.stopRecording();
            else proc_.startRecording();
        };
        play_.onClick = [this] { proc_.engine().setPreview(!proc_.engine().isPreviewing()); };
        crop_.onClick = [this] { proc_.cropToCut(); };
        demo_.onClick = [this] { proc_.useDemoSample(); };
        tap_.onClick = [this] { proc_.tapTempo(); };
        const bool sa = p.isStandalone();
        tap_.setVisible(sa);
        tempoSrc_.setVisible(!sa);
        follow_.setVisible(!sa);
    }

    void layout() override
    {
        wave_.setBounds(0, 0, W, 214);
        slices_.setBounds(0, 218, W, 24);
        load_.setBounds(0, 254, 84, 30);
        rec_.setBounds(90, 254, 84, 30);
        play_.setBounds(180, 254, 84, 30);
        loop_.setBounds(270, 254, 84, 30);
        crop_.setBounds(360, 254, 84, 30);
        demo_.setBounds(450, 254, 70, 30);
        input_.setBounds(540, 244, 190, 40);
        root_.setBounds(744, 244, 110, 40);
        count_.setBounds(0, 300, 300, 40);
        mode_.setBounds(312, 300, 160, 40);
        slice_.setBounds(484, 300, 110, 40);
        span_.setBounds(606, 300, 110, 40);
        if (proc_.isStandalone()) {
            tap_.setBounds(0, 364, 110, 40);
            bpm_.setBounds(120, 356, 120, 48);
        } else {
            tempoSrc_.setBounds(0, 356, 180, 40);
            bpm_.setBounds(190, 356, 120, 40);
            follow_.setBounds(322, 370, 210, 26);
        }
    }

    void update() override
    {
        const bool recording = proc_.isRecording();
        rec_.setButtonText(recording ? "Stop" : "Rec");
        rec_.setFilled(recording);
        play_.setFilled(proc_.engine().isPreviewing());
        const bool host = !proc_.isStandalone() && proc_.value(P_TEMPO_SRC) < 0.5f && proc_.hostBpm() > 1.0;
        bpm_.setDisplayOverride(host ? (float)proc_.hostBpm() : -1.0f, host);
        if (recording || recShown_) repaint(0, 410, W - 80, 54);
        recShown_ = recording;
    }

    void paint(juce::Graphics& g) override
    {
        auto info = juce::Rectangle<float>(0, 418, (float)W - 90.0f, 20.0f);
        if (proc_.isRecording()) {
            drawLed(g, { 8.0f, info.getCentreY() }, 5.0f, true, colours::orange);
            drawLabel(g, "REC  " + juce::String(proc_.recordingSeconds(), 1) + " s / 60 s", info.withTrimmedLeft(20.0f),
                      juce::Justification::centredLeft, 15.0f, colours::orange, Weight::Bold);
            const float pk = juce::jlimit(0.0f, 1.0f, proc_.recordingPeak());
            auto bar = juce::Rectangle<float>(200.0f, info.getY() + 6.0f, 240.0f, 8.0f);
            g.setColour(colours::blueDim);
            g.fillRect(bar);
            g.setColour(pk > 0.9f ? colours::red : colours::orange);
            g.fillRect(bar.withWidth(bar.getWidth() * pk));
        } else if (!proc_.hasRecordInput()) {
            drawLabel(g, proc_.isStandalone() ? "No audio input selected - REC needs a microphone (Options > Audio settings)"
                                              : "REC records the sidechain input - enable and route it in your DAW",
                      info, juce::Justification::centredLeft, 11.0f, colours::textDim);
        }
    }

private:
    WaveformView& wave_;
    SliceStrip& slices_;
    HudButton &load_, &rec_, &play_;
    LedToggle& loop_;
    HudButton &crop_, &demo_;
    SegmentSwitch& input_;
    Stepper& root_;
    SegmentSwitch &count_, &mode_;
    Stepper &slice_, &span_;
    SegmentSwitch& tempoSrc_;
    BpmField& bpm_;
    LedToggle& follow_;
    HudButton& tap_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool recShown_ = false;
};

//==============================================================================
class ImagePage : public Page {
public:
    explicit ImagePage(MadcapProcessor& p)
        : Page(p, PAGE_IMAGE, "Image"),
          card_(add<ImageCard>(p)), grid_(add<AnalysisGrid>(p)),
          prev_(button("<")), next_(button(">")), load_(button("Load image")),
          hold_(led(P_IMG_HOLD, colours::orange)), sync_(led(P_IMG_SYNC)),
          listen_(seg(P_IMG_READ)), sound_(seg(P_IMG_SOURCE)),
          decay_(knob(P_IMG_DECAY)), sweep_(knob(P_IMG_SWEEP)), level_(knob(P_IMG_LEVEL)), dry_(knob(P_IMG_DRY)),
          mode_(seg(P_IMG_MODE)), gridSeg_(seg(P_IMG_GRID)), wave_(seg(P_IMG_WAVE)),
          key_(stepper(P_IMG_KEY, {}, true)), scale_(stepper(P_IMG_SCALE, {}, true)),
          low_(stepper(P_IMG_LOW, [](float v) { return midiNoteName(juce::roundToInt(v)); })),
          range_(stepper(P_IMG_RANGE, [](float v) { return juce::String(juce::roundToInt(v)) + " OCT"; })),
          voices_(stepper(P_IMG_VOICES))
    {
        prev_.setTooltip("Previous built-in poster");
        next_.setTooltip("Next built-in poster");
        load_.setTooltip("Load a PNG, JPG, BMP or GIF (any size). You can also drop a picture on Madcap.");
        hold_.setLabel("Hold");
        sync_.setLabel("Sync");
        hold_.setTooltip("Freeze the 6 s live buffer that the grains and the arp read from");
        sync_.setTooltip("Each step = a 16th note at the tempo, locked to the host bar");
        prev_.onClick = [this] { stepPoster(-1); };
        next_.onClick = [this] { stepPoster(1); };
        load_.onClick = [this] {
            chooser_ = std::make_unique<juce::FileChooser>("Load a picture", juce::File(), "*.png;*.jpg;*.jpeg;*.bmp;*.gif");
            chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& fc) {
                                      auto f = fc.getResult();
                                      if (f.existsAsFile()) proc_.loadImageAsync(f);
                                  });
        };
    }

    void stepPoster(int dir)
    {
        auto pic = proc_.image().get();
        const int cur = pic && pic->poster >= 0 ? pic->poster : (dir > 0 ? -1 : 0);
        proc_.selectPoster(cur + dir);
    }

    void layout() override
    {
        card_.setBounds(0, 0, 512, 256);
        prev_.setBounds(0, 266, 30, 30);
        next_.setBounds(188, 266, 30, 30);
        load_.setBounds(226, 266, 112, 30);
        hold_.setBounds(346, 266, 80, 30);
        sync_.setBounds(432, 266, 80, 30);
        listen_.setBounds(0, 308, 160, 40);
        sound_.setBounds(170, 308, 342, 40);
        decay_.setBounds(16, 360, 96, 96);
        sweep_.setBounds(144, 360, 96, 96);
        level_.setBounds(272, 360, 96, 96);
        dry_.setBounds(400, 360, 96, 96);
        grid_.setBounds(528, 0, 400, 150);
        mode_.setBounds(528, 158, 400, 40);
        gridSeg_.setBounds(528, 206, 400, 40);
        wave_.setBounds(528, 254, 400, 40);
        key_.setBounds(528, 304, 90, 40);
        scale_.setBounds(626, 304, 144, 40);
        low_.setBounds(778, 304, 72, 40);
        range_.setBounds(856, 304, 72, 40);
        voices_.setBounds(528, 352, 110, 40);
    }

    void update() override
    {
        const int v = proc_.image().version();
        int notes[IMG_MAX_NOTES];
        const int n = gs_img_build_notes(juce::roundToInt(proc_.value(P_IMG_KEY)), juce::roundToInt(proc_.value(P_IMG_SCALE)),
                                         juce::roundToInt(proc_.value(P_IMG_LOW)), juce::roundToInt(proc_.value(P_IMG_RANGE)),
                                         notes, IMG_MAX_NOTES);
        juce::String info = n > 0 ? midiNoteName(notes[0]) + " - " + midiNoteName(notes[n - 1]) + "   " + juce::String(n) + " NOTES" : "NO NOTES";
        if (v != version_ || info != info_) { version_ = v; info_ = info; repaint(); }
    }

    void paint(juce::Graphics& g) override
    {
        auto pic = proc_.image().get();
        drawLabel(g, pic ? pic->name : juce::String("-"), { 32.0f, 266.0f, 154.0f, 30.0f }, juce::Justification::centred, 13.0f,
                  colours::text, Weight::Bold);
        drawLabel(g, info_, { 646.0f, 366.0f, 282.0f, 20.0f }, juce::Justification::centredLeft, 13.0f, colours::teal, Weight::Bold);
        drawLabel(g, "Harmony from the PLAY tab: interval stacks companions, chord degree shifts the melody",
                  { 528.0f, 400.0f, 320.0f, 30.0f }, juce::Justification::topLeft, 10.5f, colours::textDim, Weight::Medium);
    }

private:
    ImageCard& card_;
    AnalysisGrid& grid_;
    HudButton &prev_, &next_, &load_;
    LedToggle &hold_, &sync_;
    SegmentSwitch &listen_, &sound_;
    RingKnob &decay_, &sweep_, &level_, &dry_;
    SegmentSwitch &mode_, &gridSeg_, &wave_;
    Stepper &key_, &scale_, &low_, &range_, &voices_;
    std::unique_ptr<juce::FileChooser> chooser_;
    int version_ = -1;
    juce::String info_;
};

//==============================================================================
class GrainPage : public Page {
public:
    explicit GrainPage(MadcapProcessor& p)
        : Page(p, PAGE_GRAIN, "Grain"),
          on_(led(P_GRAIN_ON)), mode_(seg(P_MODE)), hold_(led(P_GRAIN_HOLD, colours::orange)), interp_(seg(P_GRAIN_INTERP)),
          field_(add<GrainField>(p, true))
    {
        on_.setLabel("Grains");
        on_.setTooltip("Grain engine on / off (off: the input is heard only through the ARP, plus image DRY)");
        hold_.setLabel("Hold");
        hold_.setTooltip("Freeze the read position");
        const int ids[12] = {P_SIZE, P_DENSITY, P_POSITION, P_SPRAY, P_SCAN, P_REVERSE,
                             P_SPREAD, P_OCTAVES, P_DETUNE, P_GLIDE, P_TONE, P_GRAIN_LVL};
        for (int i = 0; i < 12; ++i) knobs_[(size_t)i] = &knob(ids[i]);
    }

    void layout() override
    {
        on_.setBounds(0, 8, 110, 30);
        mode_.setBounds(122, 0, 520, 40);
        hold_.setBounds(654, 8, 90, 30);
        interp_.setBounds(756, 0, 172, 40);
        field_.setBounds(0, 50, W, 150);
        for (int i = 0; i < 12; ++i) {
            const int row = i / 6, col = i % 6;
            knobs_[(size_t)i]->setBounds(22 + col * 152, 214 + row * 120, 104, 110);
        }
    }

private:
    LedToggle& on_;
    SegmentSwitch& mode_;
    LedToggle& hold_;
    SegmentSwitch& interp_;
    GrainField& field_;
    std::array<RingKnob*, 12> knobs_{};
};

//==============================================================================
class ArpPage : public Page {
public:
    explicit ArpPage(MadcapProcessor& p)
        : Page(p, PAGE_ARP, "Arp"),
          on_(led(P_ARP_ON)), pattern_(seg(P_ARP_MODE)), rate_(seg(P_ARP_RATE)),
          lane_(add<ArpLane>(p)), len_(stepper(P_ARP_LEN)), oct_(stepper(P_ARP_OCT)),
          all_(button("All")), random_(button("Random")), clear_(button("Clear")),
          orbit_(add<OrbitDisplay>(p))
    {
        on_.setLabel("Arp");
        on_.setTooltip("Arpeggiator: plays the live input as pitched notes of the held chord, before the FX");
        all_.onClick = [this] { setGroup(proc_, "Arp lane", {{P_ARP_PATTERN, 65535.0f}}); };
        clear_.onClick = [this] { setGroup(proc_, "Arp lane", {{P_ARP_PATTERN, 0.0f}}); };
        random_.onClick = [this] {
            juce::Random r;
            unsigned bits = 1u;   // keep the downbeat
            for (int i = 1; i < 16; ++i) if (r.nextFloat() < 0.6f) bits |= 1u << i;
            setGroup(proc_, "Arp lane", {{P_ARP_PATTERN, (float)bits}});
        };
        const int ids[8] = {P_ARP_GATE, P_ARP_SWING, P_ARP_ATTACK, P_ARP_RELEASE, P_ARP_START, P_ARP_WALK, P_ARP_GRAINS, P_ARP_LEVEL};
        for (int i = 0; i < 8; ++i) knobs_[(size_t)i] = &knob(ids[i]);
    }

    void layout() override
    {
        on_.setBounds(0, 8, 100, 30);
        pattern_.setBounds(112, 0, 300, 40);
        rate_.setBounds(422, 0, 262, 40);
        lane_.setBounds(0, 52, 684, 70);
        len_.setBounds(0, 132, 120, 40);
        oct_.setBounds(130, 132, 110, 40);
        all_.setBounds(256, 146, 70, 26);
        random_.setBounds(332, 146, 86, 26);
        clear_.setBounds(424, 146, 70, 26);
        for (int i = 0; i < 8; ++i) {
            const int row = i / 4, col = i % 4;
            knobs_[(size_t)i]->setBounds(20 + col * 168, 190 + row * 128, 108, 112);
        }
        orbit_.setBounds(700, 0, 228, 300);
    }

    void paint(juce::Graphics& g) override
    {
        drawLabel(g, proc_.value(P_SRC) > 0.5f ? "Input IMAGE: notes start just behind the live buffer's write head"
                                               : "Input SAMPLE: notes play the cut / selected slices",
                  { 700.0f, 310.0f, 228.0f, 40.0f }, juce::Justification::topLeft, 10.5f, colours::textDim, Weight::Medium);
    }

    void update() override
    {
        const int src = juce::roundToInt(proc_.value(P_SRC));
        if (src != src_) { src_ = src; repaint(); }
    }

private:
    LedToggle& on_;
    SegmentSwitch &pattern_, &rate_;
    ArpLane& lane_;
    Stepper &len_, &oct_;
    HudButton &all_, &random_, &clear_;
    OrbitDisplay& orbit_;
    std::array<RingKnob*, 8> knobs_{};
    int src_ = -1;
};

//==============================================================================
class FxModule : public juce::Component, public Tickable {
public:
    FxModule(MadcapProcessor& p, int which, std::function<void(int, int)> move)
        : proc_(p), which_(which), move_(std::move(move)), left_("<"), right_(">")
    {
        static const int onIds[4] = {P_DRV_ON, P_CMP_ON, P_DLY_ON, P_REV_ON};
        on_ = std::make_unique<LedToggle>(p, onIds[which]);
        addAndMakeVisible(*on_);
        auto k = [&](int id) { knobs_.push_back(std::make_unique<RingKnob>(p, id)); addAndMakeVisible(*knobs_.back()); };
        switch (which) {
            case 0:
                seg1_ = std::make_unique<SegmentSwitch>(p, P_DRV_TYPE);
                seg2_ = std::make_unique<SegmentSwitch>(p, P_DRV_OS);
                k(P_DRV_AMT); k(P_DRV_TONE); k(P_DRV_MIX);
                break;
            case 1:
                k(P_CMP_THR); k(P_CMP_RATIO); k(P_CMP_ATK); k(P_CMP_REL); k(P_CMP_MAKEUP); k(P_CMP_MIX);
                break;
            case 2:
                sync_ = std::make_unique<Stepper>(p, P_DLY_DIV);
                k(P_DLY_TIME); k(P_DLY_FB); k(P_DLY_DAMP); k(P_DLY_PING); k(P_DLY_MIX);
                break;
            default:
                k(P_REV_SIZE); k(P_REV_WIDTH); k(P_REV_MOD); k(P_REV_DAMP); k(P_REV_PRE); k(P_REV_MIX);
                break;
        }
        for (auto* c : { (juce::Component*)seg1_.get(), (juce::Component*)seg2_.get(), (juce::Component*)sync_.get() })
            if (c) addAndMakeVisible(*c);
        addAndMakeVisible(left_);
        addAndMakeVisible(right_);
        left_.setTooltip("Move this effect earlier in the chain");
        right_.setTooltip("Move this effect later in the chain");
        left_.onClick = [this] { move_(which_, -1); };
        right_.onClick = [this] { move_(which_, 1); };
    }

    static const char* title(int which)
    {
        static const char* const t[4] = {"Drive", "Comp", "Delay", "Reverb"};
        return t[which & 3];
    }

    void paint(juce::Graphics& g) override
    {
        const bool on = proc_.value(on_->paramIndex()) > 0.5f;
        drawPanel(g, getLocalBounds().toFloat(), title(which_), on ? colours::teal : colours::blue);
        if (which_ == 1) {   // gain reduction meter
            auto r = juce::Rectangle<float>(12.0f, 344.0f, (float)getWidth() - 24.0f, 8.0f);
            g.setColour(colours::blueDim);
            g.fillRect(r);
            const float gr = juce::jlimit(0.0f, 24.0f, grShown_);
            g.setColour(colours::orange);
            g.fillRect(r.withWidth(r.getWidth() * gr / 24.0f));
            drawLabel(g, "GR " + juce::String(gr, 1) + " dB", r.translated(0.0f, -14.0f).withHeight(12.0f),
                      juce::Justification::centredLeft, 10.0f, colours::textDim);
        }
    }

    void resized() override
    {
        on_->setBounds(getWidth() - 92, 6, 84, 24);
        int y = 40;
        if (seg1_) { seg1_->setBounds(10, y, getWidth() - 20, 40); y += 46; }
        if (seg2_) { seg2_->setBounds(10, y, getWidth() - 20, 40); y += 46; }
        if (sync_) { sync_->setBounds(10, y, getWidth() - 20, 40); y += 46; }
        const int kw = (getWidth() - 20) / 2;
        const int kh = (which_ == 0 || which_ == 2) ? 96 : 98;
        for (size_t i = 0; i < knobs_.size(); ++i)
            knobs_[i]->setBounds(10 + (int)(i % 2) * kw, y + (int)(i / 2) * kh, kw, kh - 4);
        left_.setBounds(getWidth() / 2 - 54, getHeight() - 34, 50, 26);
        right_.setBounds(getWidth() / 2 + 4, getHeight() - 34, 50, 26);
    }

    void tick() override
    {
        const bool on = proc_.value(on_->paramIndex()) > 0.5f;
        float gr = which_ == 1 ? proc_.engine().ui().compGr.load() : 0.0f;
        grShown_ = juce::jmax(gr, grShown_ * 0.9f);
        if (on != wasOn_ || which_ == 1) { wasOn_ = on; repaint(); }
    }

private:
    MadcapProcessor& proc_;
    int which_;
    std::function<void(int, int)> move_;
    std::unique_ptr<LedToggle> on_;
    std::unique_ptr<SegmentSwitch> seg1_, seg2_;
    std::unique_ptr<Stepper> sync_;
    std::vector<std::unique_ptr<RingKnob>> knobs_;
    HudButton left_, right_;
    bool wasOn_ = false;
    float grShown_ = 0.0f;
};

class FxPage : public Page {
public:
    explicit FxPage(MadcapProcessor& p)
        : Page(p, PAGE_FX, "FX"),
          order_(stepper(P_FX_ORDER))
    {
        for (int i = 0; i < 4; ++i)
            modules_[(size_t)i] = &add<FxModule>(p, i, [this](int which, int dir) { move(which, dir); });
        order_.setLabel("Chain order");
    }

    void move(int which, int dir)
    {
        int o = juce::jlimit(0, 23, juce::roundToInt(proc_.value(P_FX_ORDER)));
        int cur[4];
        std::copy(GS_FX_ORDERS[o], GS_FX_ORDERS[o] + 4, cur);
        int pos = 0;
        while (pos < 4 && cur[pos] != which) ++pos;
        const int np = pos + dir;
        if (pos >= 4 || np < 0 || np > 3) return;
        std::swap(cur[pos], cur[np]);
        const int idx = gs_fx_order_index(cur);
        if (idx >= 0) setGroup(proc_, "FX order", {{P_FX_ORDER, (float)idx}});
    }

    void layout() override
    {
        const int o = juce::jlimit(0, 23, juce::roundToInt(proc_.value(P_FX_ORDER)));
        for (int s = 0; s < 4; ++s)
            modules_[(size_t)GS_FX_ORDERS[o][s]]->setBounds(s * 234, 0, 226, 404);
        order_.setBounds(W - 300, 414, 210, 40);
        shownOrder_ = o;
    }

    void update() override
    {
        const int o = juce::jlimit(0, 23, juce::roundToInt(proc_.value(P_FX_ORDER)));
        if (o != shownOrder_) { layout(); repaint(); }
    }

    void paint(juce::Graphics& g) override
    {
        const int o = juce::jlimit(0, 23, juce::roundToInt(proc_.value(P_FX_ORDER)));
        juce::String chain;
        for (int s = 0; s < 4; ++s) chain << (s ? "  >  " : "") << GS_FX_NAMES[GS_FX_ORDERS[o][s]];
        drawLabel(g, "Chain", { 0.0f, 414.0f, 70.0f, 40.0f }, juce::Justification::centredLeft, 12.0f, colours::textDim);
        drawLabel(g, chain, { 70.0f, 414.0f, 520.0f, 40.0f }, juce::Justification::centredLeft, 24.0f, colours::teal, Weight::Bold);
    }

private:
    std::array<FxModule*, 4> modules_{};
    Stepper& order_;
    int shownOrder_ = -1;
};

//==============================================================================
class PlayPage : public Page {
public:
    explicit PlayPage(MadcapProcessor& p)
        : Page(p, PAGE_PLAY, "Play"),
          field_(add<GrainField>(p, true)), rear_(add<RearTouchPad>(p)),
          key_(stepper(P_KEY, {}, true)), oct_(stepper(P_OCTAVE)), scale_(stepper(P_SCALE, {}, true)),
          voicing_(knob(P_VOICING)), master_(knob(P_MASTER, colours::orange)),
          interval_(seg(P_CHORD)), pads_(add<ChordPads>(p)),
          latch_(led(P_LATCH, colours::amber)), midiMode_(seg(P_MIDI_MODE)), vel_(knob(P_VEL_AMT)),
          mod_(stepper(P_MOD_DEST, {}, true))
    {
        interval_.setLabel("Interval");
        latch_.setLabel("Latch");
        latch_.setTooltip("On: the last chord keeps playing with no MIDI held. Off: silent until a note is played.");
        midiMode_.setTooltip("DEGREE: a MIDI note picks the chord degree of KEY / SCALE. ROOT: the note becomes the chord root.");
        midiMode_.setLabel("MIDI mode");
        vel_.setLabel("Velocity");
        mod_.setLabel("Mod wheel");
        master_.setTooltip("Master level (-40..+18 dB), followed by a look-ahead limiter so it never clips");
    }

    void layout() override
    {
        field_.setBounds(0, 0, 440, 214);
        rear_.setBounds(452, 0, 260, 214);
        key_.setBounds(724, 0, 96, 40);
        oct_.setBounds(832, 0, 96, 40);
        scale_.setBounds(724, 50, 204, 40);
        voicing_.setBounds(724, 100, 96, 108);
        master_.setBounds(832, 100, 96, 108);
        interval_.setBounds(0, 224, W, 44);
        pads_.setBounds(0, 280, W, 110);
        latch_.setBounds(0, 414, 100, 30);
        midiMode_.setBounds(112, 404, 200, 40);
        vel_.setBounds(326, 398, 76, 66);
        mod_.setBounds(414, 404, 200, 40);
    }

    void paint(juce::Graphics& g) override
    {
        drawLabel(g, "Click a pad to select, Shift-click to add (cluster). MIDI notes light the pads.",
                  { 630.0f, 404.0f, 210.0f, 40.0f }, juce::Justification::centredLeft, 10.5f, colours::textDim, Weight::Medium);
    }

private:
    GrainField& field_;
    RearTouchPad& rear_;
    Stepper &key_, &oct_, &scale_;
    RingKnob &voicing_, &master_;
    SegmentSwitch& interval_;
    ChordPads& pads_;
    LedToggle& latch_;
    SegmentSwitch& midiMode_;
    RingKnob& vel_;
    Stepper& mod_;
};

//==============================================================================
std::unique_ptr<Page> createPage(MadcapProcessor& p, int index)
{
    switch (index) {
        case PAGE_SAMPLE: return std::make_unique<SamplePage>(p);
        case PAGE_IMAGE: return std::make_unique<ImagePage>(p);
        case PAGE_GRAIN: return std::make_unique<GrainPage>(p);
        case PAGE_ARP: return std::make_unique<ArpPage>(p);
        case PAGE_FX: return std::make_unique<FxPage>(p);
        default: return std::make_unique<PlayPage>(p);
    }
}

} // namespace madcap::ui
