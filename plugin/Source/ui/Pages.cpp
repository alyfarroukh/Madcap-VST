#include "Pages.h"
#include "StandaloneIO.h"

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

void Page::keepBright(std::initializer_list<juce::Component*> comps)
{
    for (auto* c : comps) keep_.addIfNotAlreadyThere(c);
}

void Page::setDimmed(bool dim)
{
    if (dim == dimmed_) return;
    dimmed_ = dim;
    for (auto* c : getChildren())
        if (c != &lock_ && !keep_.contains(c)) c->setAlpha(dim ? 0.3f : 1.0f);
    repaint();
}

void Page::resized()
{
    layout();
    lock_.setBounds(W - 74, H - 16, 74, 16);
}

//==============================================================================
// Where REC records from. Standalone: the audio interface (device + input channel).
// In a DAW: the plugin's sidechain bus. Both: which channel(s) to record + a level meter.
class RecordInputPanel : public juce::Component, public Tickable, private juce::ChangeListener {
public:
    explicit RecordInputPanel(MadcapProcessor& p) : proc_(p), led_(p, "Signal"), meter_(p), settings_("Audio settings")
    {
        addAndMakeVisible(led_);
        addAndMakeVisible(meter_);
        addAndMakeVisible(channel_);
        channel_.addItemList({ "Record: all inputs (mix)", "Record: input 1 / left", "Record: input 2 / right" }, 1);
        channel_.setSelectedId(p.recordChannel() + 1, juce::dontSendNotification);
        channel_.setTooltip("Which channel(s) of the record input REC captures");
        channel_.onChange = [this] { proc_.setRecordChannel(channel_.getSelectedId() - 1); };

        dm_ = standalone::deviceManager();
        if (dm_ != nullptr) {
            addAndMakeVisible(device_);
            addAndMakeVisible(input_);
            addAndMakeVisible(settings_);
            device_.setTooltip("Audio input device (microphone, interface, ...)");
            input_.setTooltip("Input channel(s) of the device: plug a mic or guitar into this input");
            settings_.setTooltip("All audio settings: driver type, devices, sample rate, buffer size");
            settings_.fontSize = 11.0f;
            device_.onChange = [this] { chooseDevice(); };
            input_.onChange = [this] { chooseChannels(); };
            settings_.onClick = [] { standalone::showAudioSettings(); };
            dm_->addChangeListener(this);
            refresh();
        }
    }

    ~RecordInputPanel() override
    {
        if (dm_ != nullptr) dm_->removeChangeListener(this);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8, 0).withTrimmedTop(20);
        auto row1 = r.removeFromTop(24);
        r.removeFromTop(6);
        auto row2 = r.removeFromTop(22);
        if (dm_ != nullptr) {
            device_.setBounds(row1.removeFromLeft(240));
            row1.removeFromLeft(8);
            input_.setBounds(row1.removeFromLeft(210));
            row1.removeFromLeft(8);
            settings_.setBounds(row1.removeFromLeft(120));
        }
        channel_.setBounds(row2.removeFromLeft(240));
        row2.removeFromLeft(10);
        led_.setBounds(row2.removeFromLeft(80));
        row2.removeFromLeft(6);
        meter_.setBounds(row2.reduced(0, 4));
    }

    void paint(juce::Graphics& g) override
    {
        drawPanel(g, getLocalBounds().toFloat(), "Record input", colours::orange);
        // what the host / audio device actually connected to Madcap's inputs
        drawLabel(g, status_, getLocalBounds().toFloat().reduced(8.0f, 0.0f).withTrimmedTop(3.0f).withHeight(14.0f)
                                 .withTrimmedLeft(dm_ != nullptr ? 130.0f : 130.0f),
                  juce::Justification::centredRight, 10.5f, connected_ ? colours::green : colours::red, Weight::Bold);
        if (dm_ == nullptr)
            drawLabel(g, connected_ ? "Route your mic / guitar track to Madcap's sidechain input, then press REC"
                                    : "Your DAW hasn't connected Madcap's sidechain input yet: set the sidechain source to your mic track",
                      getLocalBounds().toFloat().reduced(8.0f, 0.0f).withTrimmedTop(20.0f).withHeight(24.0f),
                      juce::Justification::centredLeft, 11.0f, colours::text, Weight::Medium);
    }

    void tick() override
    {
        meter_.tick();
        led_.tick();
        const int mainCh = proc_.inputBusChannels(0), scCh = proc_.inputBusChannels(1);
        juce::String st;
        if (dm_ != nullptr) st = mainCh > 0 ? "Audio input: " + juce::String(mainCh) + " ch open" : "No audio input open";
        else {
            st = scCh > 0 ? "Sidechain connected: " + juce::String(scCh) + " ch" : "Sidechain: not connected";
            if (mainCh > 0) st << "  |  main input " << mainCh << " ch";
        }
        const bool conn = (dm_ != nullptr ? mainCh : scCh + mainCh) > 0;
        if (st != status_ || conn != connected_) { status_ = st; connected_ = conn; repaint(); }
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { refresh(); }

    void refresh()
    {
        if (dm_ == nullptr) return;
        auto* type = dm_->getCurrentDeviceTypeObject();
        const auto setup = dm_->getAudioDeviceSetup();
        device_.clear(juce::dontSendNotification);
        input_.clear(juce::dontSendNotification);
        if (type == nullptr) return;
        separate_ = type->hasSeparateInputsAndOutputs();
        const auto names = type->getDeviceNames(separate_);
        for (int i = 0; i < names.size(); ++i) device_.addItem(names[i], i + 1);
        const auto current = separate_ ? setup.inputDeviceName : setup.outputDeviceName;
        device_.setText(current.isNotEmpty() ? current : juce::String("No input device"), juce::dontSendNotification);

        if (auto* dev = dm_->getCurrentAudioDevice()) {
            const auto chans = dev->getInputChannelNames();
            for (int i = 0; i < chans.size(); ++i)
                input_.addItem("In " + juce::String(i + 1) + (chans[i].isNotEmpty() ? ": " + chans[i] : juce::String()), 100 + i);
            for (int i = 0; i + 1 < chans.size(); i += 2)
                input_.addItem("In " + juce::String(i + 1) + " + " + juce::String(i + 2) + " (stereo)", 200 + i);
            const auto bits = dev->getActiveInputChannels();
            const int n = bits.countNumberOfSetBits(), first = bits.findNextSetBit(0);
            if (n == 1) input_.setSelectedId(100 + first, juce::dontSendNotification);
            else if (n == 2 && bits[first + 1] && first % 2 == 0) input_.setSelectedId(200 + first, juce::dontSendNotification);
            else input_.setText(n == 0 ? juce::String("No input channels") : juce::String(n) + " inputs", juce::dontSendNotification);
        } else {
            input_.setText("No device", juce::dontSendNotification);
        }
    }

    void apply(juce::AudioDeviceManager::AudioDeviceSetup setup)
    {
        const auto err = dm_->setAudioDeviceSetup(setup, true);
        standalone::unmuteInput();
        if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Audio input", err);
        refresh();
    }

    void chooseDevice()
    {
        auto setup = dm_->getAudioDeviceSetup();
        const auto name = device_.getText();
        if (separate_) setup.inputDeviceName = name;
        else setup.inputDeviceName = setup.outputDeviceName = name;
        setup.useDefaultInputChannels = true;
        apply(setup);
    }

    void chooseChannels()
    {
        const int id = input_.getSelectedId();
        if (id < 100) return;
        auto setup = dm_->getAudioDeviceSetup();
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        if (id >= 200) setup.inputChannels.setRange(id - 200, 2, true);
        else setup.inputChannels.setBit(id - 100);
        apply(setup);
    }

    MadcapProcessor& proc_;
    juce::AudioDeviceManager* dm_ = nullptr;
    bool separate_ = true;
    SignalLed led_;
    InputMeter meter_;
    juce::String status_;
    bool connected_ = false;
    juce::ComboBox device_, input_, channel_;
    HudButton settings_;
};

//==============================================================================
class SamplePage : public Page {
public:
    explicit SamplePage(MadcapProcessor& p)
        : Page(p, PAGE_SAMPLE, "Sample"),
          power_(add<SourceButton>(p, 0)), input_(seg(P_SRC, {"SAMPLE", "IMAGE"}, false)),
          wave_(add<WaveformView>(p)), slices_(add<SliceStrip>(p)),
          load_(button("Load")), rec_(button("Rec", colours::orange)), play_(button("Play", colours::green)),
          loop_(led(P_PREVIEW_LOOP, colours::green)), crop_(button("Crop", colours::orange)), demo_(button("Demo")),
          root_(stepper(P_SAMPLE_ROOT, {}, true)),
          count_(seg(P_SLICES)), mode_(seg(P_SLICE_MODE)), slice_(stepper(P_SLICE_SEL)), span_(stepper(P_SLICE_SPAN)),
          tempoSrc_(seg(P_TEMPO_SRC)), bpm_(add<BpmField>(p)), follow_(led(P_FOLLOW_HOST, colours::amber)),
          tap_(button("Tap", colours::amber)), recPanel_(add<RecordInputPanel>(p))
    {
        load_.setTooltip("Load WAV, AIFF, FLAC, MP3 or OGG (up to 60 s, mixed to mono). You can also drop a file on Madcap.");
        rec_.setTooltip("Record from the RECORD INPUT below, press again to stop: the take becomes the sample");
        play_.setTooltip("Audition the cut dry (works while the transport is stopped)");
        crop_.setTooltip("Make the cut permanent");
        demo_.setTooltip("Load the built-in sung 'ahh' demo voice (C)");
        tap_.setTooltip("Tap the tempo");
        input_.setAccent(colours::green);
        input_.setTooltip("Input: SAMPLE or IMAGE (only one plays at a time)");
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
        keepBright({ &power_, &input_, &recPanel_, &rec_, &load_, &demo_, &tap_, &bpm_, &tempoSrc_, &follow_ });
    }

    void layout() override
    {
        power_.setBounds(0, 0, 150, 32);
        input_.setBounds(160, 2, 200, 28);
        wave_.setBounds(0, 40, W, 176);
        slices_.setBounds(0, 220, W, 24);
        load_.setBounds(0, 254, 84, 30);
        rec_.setBounds(90, 254, 84, 30);
        play_.setBounds(180, 254, 84, 30);
        loop_.setBounds(270, 254, 84, 30);
        crop_.setBounds(360, 254, 84, 30);
        demo_.setBounds(450, 254, 70, 30);
        root_.setBounds(540, 244, 110, 40);
        count_.setBounds(0, 298, 300, 40);
        mode_.setBounds(312, 298, 160, 40);
        slice_.setBounds(484, 298, 110, 40);
        span_.setBounds(606, 298, 110, 40);
        if (proc_.isStandalone()) {
            tap_.setBounds(0, 358, 110, 44);
            bpm_.setBounds(120, 350, 120, 52);
        } else {
            tempoSrc_.setBounds(0, 350, 160, 40);
            bpm_.setBounds(168, 350, 110, 40);
            follow_.setBounds(0, 398, 210, 24);
        }
        recPanel_.setBounds(296, 348, 632, 76);
    }

    void update() override
    {
        setDimmed(juce::roundToInt(proc_.value(P_SRC)) != 0);
        const bool recording = proc_.isRecording();
        rec_.setButtonText(recording ? "Stop" : "Rec");
        rec_.setFilled(recording);
        play_.setFilled(proc_.engine().isPreviewing());
        const bool host = !proc_.isStandalone() && proc_.value(P_TEMPO_SRC) < 0.5f && proc_.hostBpm() > 1.0;
        bpm_.setDisplayOverride(host ? (float)proc_.hostBpm() : -1.0f, host);
        if (recording || recShown_) repaint(0, 426, W - 80, 30);
        recShown_ = recording;
    }

    void paint(juce::Graphics& g) override
    {
        auto info = juce::Rectangle<float>(0, 430, (float)W - 90.0f, 20.0f);
        if (proc_.isRecording()) {
            drawLed(g, { 8.0f, info.getCentreY() }, 5.0f, true, colours::orange);
            drawLabel(g, "REC  " + juce::String(proc_.recordingSeconds(), 1) + " s / 60 s", info.withTrimmedLeft(20.0f),
                      juce::Justification::centredLeft, 15.0f, colours::orange, Weight::Bold);
        } else if (!proc_.hasRecordInput()) {
            drawLabel(g, proc_.isStandalone() ? "No audio input: choose a device and input channel in RECORD INPUT"
                                              : "No sidechain connected: REC needs audio routed to Madcap's sidechain input",
                      info, juce::Justification::centredLeft, 11.0f, colours::textDim);
        }
        if (dimmed())
            drawLabel(g, "IMAGE is the live input - press SAMPLE ON to play the sample", { 380.0f, 2.0f, 548.0f, 28.0f },
                      juce::Justification::centredRight, 12.0f, colours::amber, Weight::Bold);
    }

private:
    SourceButton& power_;
    SegmentSwitch& input_;
    WaveformView& wave_;
    SliceStrip& slices_;
    HudButton &load_, &rec_, &play_;
    LedToggle& loop_;
    HudButton &crop_, &demo_;
    Stepper& root_;
    SegmentSwitch &count_, &mode_;
    Stepper &slice_, &span_;
    SegmentSwitch& tempoSrc_;
    BpmField& bpm_;
    LedToggle& follow_;
    HudButton& tap_;
    RecordInputPanel& recPanel_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool recShown_ = false;
};

//==============================================================================
class ImagePage : public Page {
public:
    explicit ImagePage(MadcapProcessor& p)
        : Page(p, PAGE_IMAGE, "Image"),
          power_(add<SourceButton>(p, 1)), input_(seg(P_SRC, {"SAMPLE", "IMAGE"}, false)),
          prev_(button("<")), next_(button(">")), load_(button("Load image")),
          hold_(led(P_IMG_HOLD, colours::orange)), sync_(led(P_IMG_SYNC)),
          card_(add<ImageCard>(p)), terrain_(add<TerrainView>(p)), grid_(add<AnalysisGrid>(p)),
          mode_(seg(P_IMG_MODE)), sound_(seg(P_IMG_SOURCE)), wave_(seg(P_IMG_WAVE)), listen_(seg(P_IMG_READ)),
          key_(stepper(P_IMG_KEY, {}, true)), scale_(stepper(P_IMG_SCALE, {}, true)),
          low_(stepper(P_IMG_LOW, [](float v) { return midiNoteName(juce::roundToInt(v)); })),
          range_(stepper(P_IMG_RANGE, [](float v) { return juce::String(juce::roundToInt(v)) + " OCT"; })),
          voices_(stepper(P_IMG_VOICES)), res_(seg(P_IMG_RES)),
          steps_(knob(P_IMG_GRID, colours::amber)), decay_(knob(P_IMG_DECAY)), sweep_(knob(P_IMG_SWEEP)),
          level_(knob(P_IMG_LEVEL)), dry_(knob(P_IMG_DRY))
    {
        prev_.setTooltip("Previous built-in poster");
        next_.setTooltip("Next built-in poster");
        load_.setTooltip("Load a PNG, JPG, BMP or GIF of any size: the whole picture is used, never cropped. "
                         "You can also drop a picture on Madcap.");
        hold_.setLabel("Hold");
        sync_.setLabel("Sync");
        hold_.setTooltip("Freeze the 6 s live buffer that the grains and the arp read from");
        sync_.setTooltip("Each step = a 16th note at the tempo, locked to the host bar");
        steps_.setTooltip("Grid steps per sweep: 0 = smooth (held, sliding tones), 1-128 = separate notes per step");
        res_.setTooltip("Analysis grid resolution: the most columns the sweeper reads (rows = half). "
                        "The picture is never stretched: smaller pictures keep their own size.");
        input_.setAccent(colours::green);
        input_.setTooltip("Input: SAMPLE or IMAGE (only one plays at a time)");
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
        keepBright({ &power_, &input_, &prev_, &next_, &load_ });
    }

    void stepPoster(int dir)
    {
        auto pic = proc_.image().get();
        const int cur = pic && pic->poster >= 0 ? pic->poster : (dir > 0 ? -1 : 0);
        proc_.selectPoster(cur + dir);
    }

    void layout() override
    {
        power_.setBounds(0, 0, 150, 32);
        input_.setBounds(160, 2, 200, 28);
        prev_.setBounds(380, 2, 28, 28);
        next_.setBounds(566, 2, 28, 28);
        load_.setBounds(602, 2, 118, 28);
        hold_.setBounds(728, 2, 96, 28);
        sync_.setBounds(832, 2, 96, 28);
        card_.setBounds(0, 40, 452, 196);
        terrain_.setBounds(0, 242, 452, 222);
        grid_.setBounds(464, 40, 464, 64);
        res_.setBounds(464, 108, 464, 38);
        mode_.setBounds(464, 150, 236, 40);
        sound_.setBounds(708, 150, 220, 40);
        wave_.setBounds(464, 196, 236, 40);
        listen_.setBounds(708, 196, 220, 40);
        key_.setBounds(464, 242, 84, 40);
        scale_.setBounds(556, 242, 128, 40);
        low_.setBounds(692, 242, 74, 40);
        range_.setBounds(774, 242, 74, 40);
        voices_.setBounds(856, 242, 72, 40);
        RingKnob* ks[5] = { &steps_, &decay_, &sweep_, &level_, &dry_ };
        for (int i = 0; i < 5; ++i) ks[i]->setBounds(464 + i * 93, 312, 88, 100);
    }

    void update() override
    {
        setDimmed(juce::roundToInt(proc_.value(P_SRC)) != 1);
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
        drawLabel(g, pic ? pic->name : juce::String("-"), { 410.0f, 2.0f, 154.0f, 28.0f }, juce::Justification::centred, 13.0f,
                  colours::text, Weight::Bold);
        drawLabel(g, info_, { 464.0f, 288.0f, 464.0f, 20.0f }, juce::Justification::centredLeft, 13.0f, colours::teal, Weight::Bold);
        drawLabel(g, dimmed() ? "SAMPLE is the live input - press IMAGE ON to hear the sweeper"
                              : "Harmony from the PLAY tab: interval stacks companions, chord degree shifts the melody",
                  { 464.0f, 416.0f, 380.0f, 30.0f }, juce::Justification::topLeft, 10.5f,
                  dimmed() ? colours::amber : colours::textDim, Weight::Medium);
    }

private:
    SourceButton& power_;
    SegmentSwitch& input_;
    HudButton &prev_, &next_, &load_;
    LedToggle &hold_, &sync_;
    ImageCard& card_;
    TerrainView& terrain_;
    AnalysisGrid& grid_;
    SegmentSwitch &mode_, &sound_, &wave_, &listen_;
    Stepper &key_, &scale_, &low_, &range_, &voices_;
    SegmentSwitch& res_;
    RingKnob &steps_, &decay_, &sweep_, &level_, &dry_;
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
          field_(add<GrainField>(p, true)),
          dry_(knob(P_DRY_LVL, colours::green)), wet_(knob(P_WET_LVL)),
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
        dry_.setTooltip("DRY: the plain input mixed in, with no grains, arp or FX: the sample's cut looping at "
                        "its own pitch (SAMPLE input) or the plain sweeper (IMAGE input)");
        wet_.setTooltip("WET: the level of everything Madcap generates: grains, arp and the FX chain");
    }

    void layout() override
    {
        field_.setBounds(0, 0, 600, 214);
        dry_.setBounds(612, 0, 100, 104);
        wet_.setBounds(612, 110, 100, 104);
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
    RingKnob &dry_, &wet_;
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
