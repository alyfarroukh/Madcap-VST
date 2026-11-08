// Madcap look: ~70 % sci-fi HUD (thin outlines, corner brackets, ring gauges) and
// ~30 % Teenage Engineering (clear uppercase labels, bold flat blocks, orange accents).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace madcap::ui {

namespace colours {
inline const juce::Colour bg       { 0xFF030708 };
inline const juce::Colour panel    { 0xFF07100F };
inline const juce::Colour teal     { 0xFF38B2AA };
inline const juce::Colour tealDim  { 0xFF1C5955 };
inline const juce::Colour blue     { 0xFF547CA0 };
inline const juce::Colour blueDim  { 0xFF26394A };
inline const juce::Colour orange   { 0xFFFF8A1E };
inline const juce::Colour amber    { 0xFFFFB347 };
inline const juce::Colour green    { 0xFF4EE08A };
inline const juce::Colour text     { 0xFFCFE9E6 };
inline const juce::Colour textDim  { 0xFF6E8C93 };
inline const juce::Colour red      { 0xFFFF4B3E };
// FX modules
inline const juce::Colour fxDrive  { 0xFFE2581E };
inline const juce::Colour fxComp   { 0xFFAAB9B7 };
inline const juce::Colour fxDelay  { 0xFF3FAE62 };
inline const juce::Colour fxReverb { 0xFF38B2AA };
} // namespace colours

enum class Weight { Medium, SemiBold, Bold };

juce::Font font(float height, Weight w = Weight::SemiBold, float tracking = 0.06f);

// Thin panel outline with small corner brackets and an optional title.
void drawPanel(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {},
               juce::Colour c = colours::blue);
// Uppercase, letter-spaced label.
void drawLabel(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
               juce::Justification j = juce::Justification::centred, float height = 11.0f,
               juce::Colour c = colours::textDim, Weight w = Weight::SemiBold);
// Glowing LED dot.
void drawLed(juce::Graphics& g, juce::Point<float> c, float radius, bool on, juce::Colour col = colours::green);
// Keyboard focus outline.
void drawFocus(juce::Graphics& g, juce::Rectangle<float> r);

class MadcapLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MadcapLookAndFeel();
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
};

} // namespace madcap::ui
