#include "TopBar.h"
#include "Presets.h"

namespace madcap::ui {

TopBar::TopBar(MadcapProcessor& p, std::function<void(int)> onTab)
    : proc_(p), onTab_(std::move(onTab)), play_(p), meter_(p), inLed_(p, "In")
{
    static const char* const names[6] = {"Sample", "Image", "Grain", "Arp", "FX", "Play"};
    for (int i = 0; i < 6; ++i) {
        tabs_[(size_t)i] = std::make_unique<HudButton>(names[i]);
        tabs_[(size_t)i]->fontSize = 14.0f;
        tabs_[(size_t)i]->setTooltip(juce::String(names[i]) + " page");
        tabs_[(size_t)i]->onClick = [this, i] { onTab_(i); };
        addAndMakeVisible(*tabs_[(size_t)i]);
    }
    for (auto* b : {&prev_, &next_, &presetName_, &save_, &undo_, &redo_, &rnd_}) addAndMakeVisible(*b);
    addAndMakeVisible(play_);
    addAndMakeVisible(recOut_);
    recOut_.fontSize = 12.0f;
    recOut_.setTooltip("REC OUT: record what you hear to a WAV file (stereo, 24-bit) in "
                       + MadcapProcessor::recordingsFolder().getFullPathName()
                       + ". Click to start, click again to stop. Right-click: open the folder.");
    recOut_.onClick = [this] {
        if (proc_.isRecordingOut()) proc_.stopRecordingOut();
        else proc_.startRecordingOut();
    };
    recOut_.onMenu = [this] {
        juce::PopupMenu m;
        const auto last = proc_.lastRecordingOut();
        m.addItem(1, "Open recordings folder");
        m.addItem(2, "Show last recording", last.existsAsFile());
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&recOut_), [last](int r) {
            if (r == 1) {
                auto dir = MadcapProcessor::recordingsFolder();
                dir.createDirectory();
                dir.startAsProcess();
            } else if (r == 2 && last.existsAsFile()) {
                last.revealToUser();
            }
        });
    };
    addAndMakeVisible(meter_);
    addAndMakeVisible(inLed_);
    prev_.setTooltip("Previous factory preset");
    next_.setTooltip("Next factory preset");
    presetName_.setTooltip("Presets: factory and your own");
    save_.setTooltip("Save the current sound (with its sample and picture) as a user preset");
    undo_.setTooltip("Undo (Ctrl/Cmd+Z)");
    redo_.setTooltip("Redo (Ctrl/Cmd+Shift+Z)");
    rnd_.setTooltip("Randomize every page that isn't locked (LOCK at the bottom right of each page)");
    for (auto* b : {&undo_, &redo_, &rnd_}) b->fontSize = 11.0f;
    prev_.onClick = [this] { stepPreset(-1); };
    next_.onClick = [this] { stepPreset(1); };
    presetName_.onClick = [this] { showPresetMenu(); };
    save_.onClick = [this] { savePresetDialog(); };
    undo_.onClick = [this] { proc_.undoManager().undo(); };
    redo_.onClick = [this] { proc_.undoManager().redo(); };
    rnd_.onClick = [this] { proc_.randomize(); };
}

void TopBar::resized()
{
    for (int i = 0; i < 6; ++i) tabs_[(size_t)i]->setBounds(116 + i * 68, 6, 64, 30);
    prev_.setBounds(530, 6, 24, 30);
    presetName_.setBounds(556, 6, 144, 30);
    next_.setBounds(702, 6, 24, 30);
    save_.setBounds(730, 6, 50, 30);
    recOut_.setBounds(784, 6, 60, 30);
    undo_.setBounds(530, 42, 52, 18);
    redo_.setBounds(586, 42, 52, 18);
    rnd_.setBounds(642, 42, 56, 18);
    inLed_.setBounds(702, 42, 40, 18);
    meter_.setBounds(744, 43, 96, 16);
    play_.setBounds(850, 6, 100, 54);
}

void TopBar::setCurrentTab(int t)
{
    current_ = t;
    for (int i = 0; i < 6; ++i) tabs_[(size_t)i]->setFilled(i == t);
}

void TopBar::showMessage(const juce::String& m)
{
    message_ = m;
    messageUntil_ = juce::Time::getMillisecondCounterHiRes() + 4500.0;
    repaint();
}

juce::String TopBar::statusLine() const
{
    if (message_.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() < messageUntil_) return message_;
    const auto& ui = proc_.engine().ui();
    const bool image = proc_.value(P_SRC) > 0.5f;
    const bool host = !proc_.isStandalone() && proc_.value(P_TEMPO_SRC) < 0.5f && proc_.hostBpm() > 1.0;
    char deg[32];
    int d = 0;
    const int mask = ui.degreeMask.load();
    while (d < 6 && !((mask >> d) & 1)) ++d;
    gs_degree_name(ui.key.load(), juce::roundToInt(proc_.value(P_SCALE)), d, deg, (int)sizeof deg);
    juce::String s;
    s << "INPUT " << (image ? "IMAGE" : "SAMPLE") << "     BPM " << juce::String(ui.bpm.load(), 1) << (host ? " HOST" : "")
      << "     " << juce::String(deg).toUpperCase() << "  " << params::format(P_CHORD, proc_.value(P_CHORD)).toUpperCase()
      << "     OCT " << juce::String(ui.octave.load()) << (ui.gate.load() ? "" : "     (PLAY A NOTE)");
    return s;
}

void TopBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::panel.withAlpha(0.8f));
    g.fillRect(b);
    g.setColour(colours::blue.withAlpha(0.35f));
    g.drawHorizontalLine(H - 1, 0.0f, (float)W);
    drawLabel(g, "Madcap", { 12.0f, 4.0f, 100.0f, 32.0f }, juce::Justification::centredLeft, 28.0f, colours::teal, Weight::Bold);
    const bool image = proc_.value(P_SRC) > 0.5f;
    drawLed(g, { 18.0f, 51.0f }, 4.0f, true, image ? colours::amber : colours::green);
    const bool isMessage = message_.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() < messageUntil_;
    drawLabel(g, shownStatus_, { 28.0f, 42.0f, 494.0f, 18.0f }, juce::Justification::centredLeft, 12.5f,
              isMessage ? colours::orange : colours::text, Weight::SemiBold);
}

void TopBar::tick()
{
    meter_.tick();
    inLed_.tick();
    const bool waiting = proc_.value(P_FOLLOW_HOST) > 0.5f && !proc_.isStandalone() && !proc_.hostIsPlaying();
    play_.setWaitingForHost(waiting && proc_.value(P_PLAY) > 0.5f);
    const auto s = statusLine();
    if (s != shownStatus_) { shownStatus_ = s; repaint(0, 40, 525, 22); }
    const auto name = proc_.currentPresetName();
    if (presetName_.getButtonText() != name) presetName_.setButtonText(name);
    const bool rec = proc_.isRecordingOut();
    recOut_.setFilled(rec);
    juce::String recText = "Rec out";
    if (rec) {
        const int secs = (int)proc_.recordingOutSeconds();
        recText = juce::String::formatted("%d:%02d", secs / 60, secs % 60);
    }
    if (recOut_.getButtonText() != recText) recOut_.setButtonText(recText);
    undo_.setEnabled(proc_.undoManager().canUndo());
    redo_.setEnabled(proc_.undoManager().canRedo());
}

void TopBar::stepPreset(int dir)
{
    const int n = (int)factoryPresets().size();
    const int cur = proc_.getCurrentProgram();
    proc_.loadFactoryPreset(((cur + dir) % n + n) % n);
}

void TopBar::showPresetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("FACTORY");
    const auto& fp = factoryPresets();
    for (int i = 0; i < (int)fp.size(); ++i) m.addItem(1 + i, fp[(size_t)i].name, true, false);
    const auto users = userPresetFiles();
    if (!users.isEmpty()) {
        m.addSectionHeader("USER");
        for (int i = 0; i < users.size(); ++i) m.addItem(1000 + i, users[i].getFileNameWithoutExtension());
    }
    m.addSeparator();
    m.addItem(900, "Save preset...");
    m.addItem(901, "Open preset folder");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetName_),
                    [this, users](int r) {
                        if (r <= 0) return;
                        if (r >= 1000) proc_.loadUserPreset(users[r - 1000]);
                        else if (r == 900) savePresetDialog();
                        else if (r == 901) {
                            auto dir = userPresetDirectory();
                            dir.createDirectory();
                            dir.startAsProcess();
                        } else proc_.loadFactoryPreset(r - 1);
                    });
}

void TopBar::savePresetDialog()
{
    auto* w = new juce::AlertWindow("Save preset", "Name your preset. It is saved with its sample and picture to\n"
                                                   + userPresetDirectory().getFullPathName(),
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", proc_.currentPresetName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TopBar> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int r) {
                           if (r == 1 && safe != nullptr) safe->proc_.saveUserPreset(w->getTextEditorContents("name"));
                       }),
                       true);
}

} // namespace madcap::ui
