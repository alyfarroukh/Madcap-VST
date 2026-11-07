#pragma once
#include "Displays.h"
#include "Widgets.h"

namespace madcap::ui {

// REC OUT: left click starts / stops recording Madcap's output to WAV, right click = folder menu.
class RecOutButton : public HudButton {
public:
    RecOutButton() : HudButton("Rec out", colours::red) {}
    std::function<void()> onMenu;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onMenu) onMenu(); return; }
        HudButton::mouseDown(e);
    }
};

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
    RecOutButton recOut_;
    LevelMeter meter_;
    SignalLed inLed_;
    juce::String message_, shownStatus_;
    double messageUntil_ = 0.0;
    int current_ = 0;
};

} // namespace madcap::ui
