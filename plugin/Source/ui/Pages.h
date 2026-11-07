// The six tabs. Each page is laid out on a fixed 928 x 464 logical canvas; the editor
// scales the whole UI with a transform, so everything stays vector-sharp at 75-200 %.
#pragma once
#include "Displays.h"
#include "Widgets.h"

#include <vector>

namespace madcap::ui {

class Page : public juce::Component, public Tickable {
public:
    static constexpr int W = 928, H = 464;
    Page(MadcapProcessor& p, int pageIndex, const juce::String& title);
    void tick() override;
    void paint(juce::Graphics&) override;
    void resized() override;

protected:
    template <typename T, typename... Args>
    T& add(Args&&... args)
    {
        auto c = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *c;
        addAndMakeVisible(ref);
        if (auto* t = dynamic_cast<Tickable*>(&ref)) ticks_.push_back(t);
        owned_.push_back(std::move(c));
        return ref;
    }
    RingKnob& knob(int param, juce::Colour c = colours::teal) { return add<RingKnob>(proc_, param, c); }
    SegmentSwitch& seg(int param, juce::StringArray labels = {}, bool title = true) { return add<SegmentSwitch>(proc_, param, std::move(labels), title); }
    Stepper& stepper(int param, Stepper::Formatter f = {}, bool wrap = false) { return add<Stepper>(proc_, param, std::move(f), wrap); }
    LedToggle& led(int param, juce::Colour c = colours::green) { return add<LedToggle>(proc_, param, c); }
    HudButton& button(const juce::String& text, juce::Colour c = colours::teal) { return add<HudButton>(text, c); }
    virtual void layout() = 0;
    virtual void update() {}   // per-frame page logic (labels, button states)

    MadcapProcessor& proc_;
    const int page_;
    juce::String title_;
    LockToggle lock_;

private:
    std::vector<std::unique_ptr<juce::Component>> owned_;
    std::vector<Tickable*> ticks_;
};

std::unique_ptr<Page> createPage(MadcapProcessor& p, int index);

} // namespace madcap::ui
