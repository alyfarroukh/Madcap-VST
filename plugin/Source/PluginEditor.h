#pragma once
#include "PluginProcessor.h"
#include "ui/MadcapLookAndFeel.h"
#include "ui/Pages.h"
#include "ui/TopBar.h"

namespace madcap {

// The whole UI is laid out on a fixed 960 x 544 logical canvas and scaled with an
// AffineTransform, so it stays vector-sharp from 75 % to 200 %.
class MadcapEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget {
public:
    static constexpr int kW = 960, kH = 544;
    explicit MadcapEditor(MadcapProcessor&);
    ~MadcapEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { dragOver_ = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dragOver_ = false; repaint(); }
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void showPage(int index);

private:
    void tick();

    ui::MadcapLookAndFeel lnf_;          // declared first: outlives every child
    MadcapProcessor& proc_;
    juce::Component content_;
    std::unique_ptr<ui::TopBar> top_;
    std::array<std::unique_ptr<ui::Page>, 6> pages_;
    int page_ = 0;
    bool dragOver_ = false;
    juce::TooltipWindow tooltips_{ this, 700 };
    juce::VBlankAttachment vblank_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MadcapEditor)
};

} // namespace madcap
