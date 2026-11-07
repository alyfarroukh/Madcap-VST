// Parameter-bound controls. Every one is attached to a host parameter through
// juce::ParameterAttachment (gestures + undo), takes keyboard focus and has a tooltip.
#pragma once
#include "MadcapLookAndFeel.h"
#include "PluginProcessor.h"

#include <functional>

namespace madcap::ui {

class ParamWidget : public juce::Component, public juce::SettableTooltipClient {
public:
    ParamWidget(MadcapProcessor& p, int paramIndex);
    ~ParamWidget() override = default;

    int paramIndex() const noexcept { return index_; }
    float value() const noexcept { return value_; }
    float norm() const noexcept { return param_.convertTo0to1(value_); }
    void setLabel(const juce::String& s) { label_ = s; repaint(); }
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

protected:
    virtual void valueChanged() { repaint(); }
    void setValueComplete(float v);                 // one undoable step
    void beginDrag() { attach_.beginGesture(); }
    void dragTo(float v) { attach_.setValueAsPartOfGesture(gs_param_clamp(index_, v)); }
    void endDrag() { attach_.endGesture(); }
    juce::String valueText() const { return params::format(index_, value_); }
    void updateTooltip();
    int numSteps() const { return gs_param_num_steps(index_); }

    MadcapProcessor& proc_;
    juce::RangedAudioParameter& param_;
    const int index_;
    juce::String label_;
    float value_ = 0.0f;
    juce::ParameterAttachment attach_;
};

//==============================================================================
// Ring gauge knob: an arc of tick segments. Value bubble while touched; double-click
// resets; wheel; Shift = fine drag; arrow keys.
class RingKnob : public ParamWidget {
public:
    RingKnob(MadcapProcessor& p, int paramIndex, juce::Colour accent = colours::teal);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    juce::Colour accent_;
    bool dragging_ = false;
    float dragStartNorm_ = 0.0f;
    juce::Point<float> dragStart_;
    bool bipolar_ = false;
};

//==============================================================================
// Segmented switch for choice / bool / small int parameters.
class SegmentSwitch : public ParamWidget {
public:
    SegmentSwitch(MadcapProcessor& p, int paramIndex, juce::StringArray labels = {}, bool showTitle = true);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }

private:
    juce::Rectangle<float> segArea() const;
    juce::StringArray labels_;
    bool showTitle_;
    juce::Colour accent_ = colours::teal;
};

//==============================================================================
// <  VALUE  >  stepper.
class Stepper : public ParamWidget {
public:
    using Formatter = std::function<juce::String(float)>;
    Stepper(MadcapProcessor& p, int paramIndex, Formatter f = {}, bool wrap = false);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void step(int dir);

private:
    Formatter fmt_;
    bool wrap_;
};

//==============================================================================
// LED toggle for bool parameters.
class LedToggle : public ParamWidget {
public:
    LedToggle(MadcapProcessor& p, int paramIndex, juce::Colour led = colours::green);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    juce::Colour led_;
};

//==============================================================================
// Flat block button (actions: LOAD, REC, CROP, ...).
class HudButton : public juce::Button {
public:
    explicit HudButton(const juce::String& text, juce::Colour accent = colours::teal);
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    void setFilled(bool f) { filled_ = f; repaint(); }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    float fontSize = 13.0f;

private:
    juce::Colour accent_;
    bool filled_ = false;
};

//==============================================================================
// The big PLAY / STOP transport button (bound to the Play parameter).
class PlayButton : public ParamWidget {
public:
    explicit PlayButton(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void setWaitingForHost(bool w) { if (w != waiting_) { waiting_ = w; repaint(); } }

private:
    bool waiting_ = false;
};

//==============================================================================
// Tempo field: drag up / down (Shift = fine), wheel, double-click to type a value.
class BpmField : public ParamWidget, private juce::TextEditor::Listener {
public:
    explicit BpmField(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setDisplayOverride(float bpm, bool host) { if (!juce::exactlyEqual(bpm, shown_) || host != host_) { shown_ = bpm; host_ = host; repaint(); } }

private:
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorEscapeKeyPressed(juce::TextEditor&) override;
    void textEditorFocusLost(juce::TextEditor&) override;
    void closeEditor(bool apply);
    std::unique_ptr<juce::TextEditor> editor_;
    float startValue_ = 0.0f, shown_ = -1.0f;
    bool host_ = false, dragging_ = false;
};

//==============================================================================
// Randomize lock for one page (not a host parameter; saved with the project).
class LockToggle : public juce::Component, public juce::SettableTooltipClient {
public:
    LockToggle(MadcapProcessor& p, int page);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    MadcapProcessor& proc_;
    int page_;
};

} // namespace madcap::ui
