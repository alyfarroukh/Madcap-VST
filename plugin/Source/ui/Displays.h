// Animated displays. tick() is called at the display refresh rate (VBlankAttachment) by the
// editor; each display reads the engine's UI snapshot (atomics) and repaints only itself.
#pragma once
#include "MadcapLookAndFeel.h"
#include "PluginProcessor.h"

namespace madcap::ui {

struct Tickable {
    virtual ~Tickable() = default;
    virtual void tick() = 0;
};

// Two-parameter gesture helper (XY pads, cut flags).
struct ParamPair {
    ParamPair(MadcapProcessor& p, int a, int b);
    void begin() { ax.beginGesture(); bx.beginGesture(); }
    void set(float va, float vb) { ax.setValueAsPartOfGesture(gs_param_clamp(ia, va)); bx.setValueAsPartOfGesture(gs_param_clamp(ib, vb)); }
    void end() { ax.endGesture(); bx.endGesture(); }
    int ia, ib;
    juce::ParameterAttachment ax, bx;
};

//==============================================================================
class WaveformView : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit WaveformView(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    std::function<void()> onEmptyClick;

private:
    float xToNorm(float x) const;
    float normToX(float n) const;
    juce::Rectangle<float> waveArea() const;
    MadcapProcessor& proc_;
    ParamPair cut_;
    std::shared_ptr<const SampleData> data_;
    int version_ = -1;
    enum class Drag { None, In, Out, Swipe } drag_ = Drag::None;
    float anchor_ = 0.0f;
    float lastPreview_ = -2.0f, lastIn_ = -1, lastOut_ = -1, lastRs_ = -1, lastRe_ = -1;
    int lastSlices_ = -1;
};

//==============================================================================
class SliceStrip : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit SliceStrip(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    int sliceAt(float x) const;
    MadcapProcessor& proc_;
    ParamPair sel_;
    int anchor_ = 0;
    std::array<float, GS_MAX_SLICES + 1> bounds_{};
    int n_ = 1;
    int lastKey_ = -1;
};

//==============================================================================
class ImageCard : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit ImageCard(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> imageRect() const;   // the picture, fitted with its own aspect ratio
    MadcapProcessor& proc_;
    std::shared_ptr<const PictureData> pic_;
    int version_ = -1;
    float phase_ = 0.0f;
    bool scrubbing_ = false;
};

//==============================================================================
// 3D wireframe terrain generated from the picture, rolling through a fixed sweeper.
class TerrainView : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit TerrainView(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;

private:
    float heightAt(float col, float row) const;
    MadcapProcessor& proc_;
    std::shared_ptr<const PictureData> pic_;
    int version_ = -1;
    float phase_ = 0.0f;
    bool dark_ = false;
};

//==============================================================================
// Record input level bar.
class InputMeter : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit InputMeter(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;

private:
    MadcapProcessor& proc_;
    float level_ = 0.0f;
};

//==============================================================================
class AnalysisGrid : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit AnalysisGrid(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;
    int noteCount() const noexcept { return nNotes_; }

private:
    void rebuild();
    MadcapProcessor& proc_;
    juce::Image cache_;
    int nNotes_ = 0, listen_ = -1, cacheKey_ = -1;
    float phase_ = 0.0f;
};

//==============================================================================
class ArpLane : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit ArpLane(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    int cellAt(float x) const;
    MadcapProcessor& proc_;
    juce::ParameterAttachment attach_;
    unsigned bits_ = 0xFFFF;
    bool paintOn_ = true;
    int lastCell_ = -1, step_ = -1;
    bool dragging_ = false;
};

//==============================================================================
class OrbitDisplay : public juce::Component, public Tickable {
public:
    explicit OrbitDisplay(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;

private:
    MadcapProcessor& proc_;
    int trig_ = 0, idx_ = 0, n_ = 0, step_ = 0;
    float flash_ = 0.0f, spin_ = 0.0f;
    std::array<float, ARP_MAX_NOTES> notes_{};
};

//==============================================================================
// XY pad: X = grain POSITION, Y = SPRAY, with the waveform / live buffer and the live grains.
class GrainField : public juce::Component, public juce::SettableTooltipClient, public Tickable {
public:
    explicit GrainField(MadcapProcessor& p, bool interactive = true);
    void paint(juce::Graphics&) override;
    void tick() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    MadcapProcessor& proc_;
    ParamPair xy_;
    bool interactive_;
    std::shared_ptr<const SampleData> data_;
    int version_ = -1;
    int n_ = 0;
    std::array<float, GS_UI_GRAINS> pos_{}, amp_{};
    std::array<float, GS_LIVE_ENV> live_{};
};

//==============================================================================
class ChordPads : public juce::Component, public Tickable {
public:
    explicit ChordPads(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    int padAt(juce::Point<float>) const;
    MadcapProcessor& proc_;
    int mask_ = 1, midi_ = 0, key_ = 0, scale_ = 1, pressed_ = -1;
};

//==============================================================================
class LevelMeter : public juce::Component, public Tickable {
public:
    explicit LevelMeter(MadcapProcessor& p);
    void paint(juce::Graphics&) override;
    void tick() override;

private:
    MadcapProcessor& proc_;
    float level_[2] = {0, 0}, hold_[2] = {0, 0};
    int holdT_[2] = {0, 0};
};

} // namespace madcap::ui
