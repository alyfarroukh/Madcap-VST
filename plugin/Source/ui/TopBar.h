#pragma once
#include "Displays.h"
#include "Widgets.h"

namespace madcap::ui {

// Tabs, input / BPM status, presets, undo / redo / randomize, PLAY / STOP and the level meter.
class TopBar : public juce::Component, public Tickable {
public:
    static constexpr int W = 960, H = 64;
    TopBar(MadcapProcessor& p, std::function<void(int)> onTab);
    void paint(juce::Graphics&) override;
    void resized() override;
    void tick() override;
    void setCurrentTab(int t);
    void showMessage(const juce::String& m);

private:
    void showPresetMenu();
    void stepPreset(int dir);
    void savePresetDialog();
    juce::String statusLine() const;

    MadcapProcessor& proc_;
    std::function<void(int)> onTab_;
    std::array<std::unique_ptr<HudButton>, 6> tabs_;
    HudButton prev_{"<"}, next_{">"}, presetName_{"Init"}, save_{"Save"};
    HudButton undo_{"Undo"}, redo_{"Redo"}, rnd_{"Rnd", colours::amber};
    PlayButton play_;
    LevelMeter meter_;
    juce::String message_, shownStatus_;
    double messageUntil_ = 0.0;
    int current_ = 0;
};

} // namespace madcap::ui
