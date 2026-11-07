#include "Displays.h"

namespace madcap::ui {

namespace {
constexpr float kPi = juce::MathConstants<float>::pi;

float hash01(int i)
{
    unsigned h = (unsigned)i * 2654435761u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (float)(h & 0xFFFF) / 65535.0f;
}
} // namespace

ParamPair::ParamPair(MadcapProcessor& p, int a, int b)
    : ia(a), ib(b),
      ax(*p.param(a), [](float) {}, &p.undoManager()),
      bx(*p.param(b), [](float) {}, &p.undoManager())
{
}

//==============================================================================
WaveformView::WaveformView(MadcapProcessor& p) : proc_(p), cut_(p, P_TRIM_IN, P_TRIM_OUT)
{
    setTooltip("Drag the orange IN / OUT flags, or swipe across the waveform to draw a new cut. "
               "Double-click: whole sample. Drop an audio file anywhere on Madcap to load it.");
}

juce::Rectangle<float> WaveformView::waveArea() const
{
    return getLocalBounds().toFloat().reduced(8.0f, 4.0f).withTrimmedTop(16.0f);
}

float WaveformView::xToNorm(float x) const
{
    auto a = waveArea();
    return juce::jlimit(0.0f, 1.0f, (x - a.getX()) / a.getWidth());
}

float WaveformView::normToX(float n) const
{
    auto a = waveArea();
    return a.getX() + n * a.getWidth();
}

void WaveformView::tick()
{
    const int v = proc_.sample().version();
    bool dirty = false;
    if (v != version_) { version_ = v; data_ = proc_.sample().get(); dirty = true; }
    const auto& ui = proc_.engine().ui();
    const float pp = ui.previewPos.load(), in = proc_.value(P_TRIM_IN), out = proc_.value(P_TRIM_OUT);
    const float rs = ui.regionStart.load(), re = ui.regionEnd.load();
    const int sl = ui.numSlices.load();
    using juce::exactlyEqual;
    if (!exactlyEqual(pp, lastPreview_) || !exactlyEqual(in, lastIn_) || !exactlyEqual(out, lastOut_)
        || !exactlyEqual(rs, lastRs_) || !exactlyEqual(re, lastRe_) || sl != lastSlices_) {
        lastPreview_ = pp; lastIn_ = in; lastOut_ = out; lastRs_ = rs; lastRe_ = re; lastSlices_ = sl;
        dirty = true;
    }
    if (dirty) repaint();
}

void WaveformView::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(colours::panel.withAlpha(0.7f));
    g.fillRect(b);
    auto a = waveArea();
    if (!data_ || data_->samples.empty()) {
        drawLabel(g, "Load or drop an audio file", a, juce::Justification::centred, 16.0f, colours::textDim);
        return;
    }
    // header
    juce::String head = data_->name + "   " + juce::String(data_->seconds(), 2) + " S   "
                      + juce::String(juce::roundToInt(data_->rate)) + " HZ" + (data_->isDemo ? "   BUILT-IN DEMO" : "");
    drawLabel(g, head, b.reduced(8.0f, 2.0f).withHeight(16.0f), juce::Justification::centredLeft, 12.0f, colours::text);

    float in = proc_.value(P_TRIM_IN), out = proc_.value(P_TRIM_OUT);
    if (out < in) std::swap(in, out);
    const auto& ui = proc_.engine().ui();
    const float rs = ui.regionStart.load(), re = ui.regionEnd.load();

    g.setColour(colours::teal.withAlpha(0.08f));
    g.fillRect(juce::Rectangle<float>(normToX(rs), a.getY(), normToX(re) - normToX(rs), a.getHeight()));

    const int W = juce::jmax(1, (int)a.getWidth());
    const int bins = (int)data_->overviewMin.size();
    const float mid = a.getCentreY(), amp = a.getHeight() * 0.47f;
    g.setColour(colours::blue.withAlpha(0.25f));
    g.drawHorizontalLine((int)mid, a.getX(), a.getRight());
    for (int x = 0; x < W; ++x) {
        const float n0 = (float)x / (float)W;
        const int bi = juce::jlimit(0, bins - 1, (int)(n0 * (float)bins));
        const float mn = data_->overviewMin[(size_t)bi], mx = data_->overviewMax[(size_t)bi];
        const bool inside = n0 >= in && n0 <= out;
        const bool inRegion = n0 >= rs && n0 <= re;
        g.setColour(inside ? (inRegion ? colours::teal : colours::teal.withAlpha(0.55f)) : colours::blue.withAlpha(0.3f));
        g.drawVerticalLine((int)a.getX() + x, mid - mx * amp, mid - mn * amp + 1.0f);
    }
    // slice lines
    const int n = juce::jlimit(1, GS_MAX_SLICES, ui.numSlices.load());
    g.setColour(colours::blue.withAlpha(0.9f));
    for (int i = 1; i < n; ++i) {
        const float x = normToX(ui.bounds[(size_t)i].load());
        g.drawLine(x, a.getY() + 4.0f, x, a.getBottom(), 1.0f);
    }
    // preview playhead
    const float pp = ui.previewPos.load();
    if (pp >= 0.0f) {
        g.setColour(colours::amber);
        g.drawLine(normToX(pp), a.getY(), normToX(pp), a.getBottom(), 1.5f);
    }
    // IN / OUT flags
    auto flag = [&](float norm, const juce::String& txt, bool right) {
        const float x = normToX(norm);
        g.setColour(colours::orange);
        g.drawLine(x, a.getY(), x, a.getBottom(), 1.5f);
        auto fr = juce::Rectangle<float>(right ? x : x - 30.0f, a.getY(), 30.0f, 14.0f);
        g.fillRect(fr);
        g.setColour(colours::bg);
        g.setFont(font(11.0f, Weight::Bold, 0.1f));
        g.drawText(txt, fr, juce::Justification::centred);
    };
    flag(in, "IN", true);
    flag(out, "OUT", false);
}

void WaveformView::mouseMove(const juce::MouseEvent& e)
{
    const float x = e.position.x;
    const bool nearFlag = std::abs(x - normToX(proc_.value(P_TRIM_IN))) < 8.0f
                       || std::abs(x - normToX(proc_.value(P_TRIM_OUT))) < 8.0f;
    setMouseCursor(nearFlag ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::CrosshairCursor);
}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    const float x = e.position.x;
    const float inX = normToX(proc_.value(P_TRIM_IN)), outX = normToX(proc_.value(P_TRIM_OUT));
    if (std::abs(x - inX) <= 8.0f && std::abs(x - inX) <= std::abs(x - outX)) drag_ = Drag::In;
    else if (std::abs(x - outX) <= 8.0f) drag_ = Drag::Out;
    else { drag_ = Drag::Swipe; anchor_ = xToNorm(x); }
    cut_.begin();
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    const float n = xToNorm(e.position.x);
    const float in = proc_.value(P_TRIM_IN), out = proc_.value(P_TRIM_OUT);
    switch (drag_) {
        case Drag::In: cut_.set(juce::jmin(n, out - 0.002f), out); break;
        case Drag::Out: cut_.set(in, juce::jmax(n, in + 0.002f)); break;
        case Drag::Swipe:
            if (std::abs(e.getDistanceFromDragStartX()) > 3) cut_.set(juce::jmin(anchor_, n), juce::jmax(anchor_, n));
            break;
        case Drag::None: break;
    }
}

void WaveformView::mouseUp(const juce::MouseEvent& e)
{
    if (drag_ == Drag::Swipe && std::abs(e.getDistanceFromDragStartX()) <= 3 && onEmptyClick) onEmptyClick();
    cut_.end();
    drag_ = Drag::None;
}

void WaveformView::mouseDoubleClick(const juce::MouseEvent&)
{
    cut_.begin();
    cut_.set(0.0f, 1.0f);
    cut_.end();
}

//==============================================================================
SliceStrip::SliceStrip(MadcapProcessor& p) : proc_(p), sel_(p, P_SLICE_SEL, P_SLICE_SPAN)
{
    setTooltip("Tap a slice, or drag across slices, to choose where the grains read from");
}

void SliceStrip::tick()
{
    const auto& ui = proc_.engine().ui();
    const int n = juce::jlimit(1, GS_MAX_SLICES, ui.numSlices.load());
    bool dirty = n != n_;
    n_ = n;
    for (int i = 0; i <= n; ++i) {
        const float v = ui.bounds[(size_t)i].load();
        if (!juce::exactlyEqual(v, bounds_[(size_t)i])) { bounds_[(size_t)i] = v; dirty = true; }
    }
    const int key = juce::roundToInt(proc_.value(P_SLICE_SEL)) * 64 + juce::roundToInt(proc_.value(P_SLICE_SPAN));
    if (key != lastKey_) { lastKey_ = key; dirty = true; }
    if (dirty) repaint();
}

int SliceStrip::sliceAt(float x) const
{
    auto a = getLocalBounds().toFloat().reduced(8.0f, 0.0f);
    const float n = juce::jlimit(0.0f, 1.0f, (x - a.getX()) / a.getWidth());
    for (int i = 0; i < n_; ++i)
        if (n < bounds_[(size_t)i + 1]) return i;
    return n_ - 1;
}

void SliceStrip::paint(juce::Graphics& g)
{
    auto a = getLocalBounds().toFloat().reduced(8.0f, 0.0f);
    const int sel = juce::roundToInt(proc_.value(P_SLICE_SEL)), span = juce::roundToInt(proc_.value(P_SLICE_SPAN));
    const int s0 = juce::jmin(sel, n_ - 1), s1 = juce::jmin(n_, s0 + span);
    for (int i = 0; i < n_; ++i) {
        const float x0 = a.getX() + bounds_[(size_t)i] * a.getWidth(), x1 = a.getX() + bounds_[(size_t)i + 1] * a.getWidth();
        auto r = juce::Rectangle<float>(x0, a.getY(), juce::jmax(1.0f, x1 - x0), a.getHeight()).reduced(0.5f, 1.0f);
        const bool on = i >= s0 && i < s1;
        g.setColour(on ? colours::teal.withAlpha(0.85f) : colours::panel);
        g.fillRect(r);
        g.setColour(on ? colours::teal : colours::blue.withAlpha(0.5f));
        g.drawRect(r, 1.0f);
        if (r.getWidth() > 14.0f) {
            g.setColour(on ? colours::bg : colours::textDim);
            g.setFont(font(11.0f, Weight::Bold));
            g.drawText(juce::String(i + 1), r, juce::Justification::centred);
        }
    }
}

void SliceStrip::mouseDown(const juce::MouseEvent& e)
{
    anchor_ = sliceAt(e.position.x);
    sel_.begin();
    sel_.set((float)anchor_, 1.0f);
}

void SliceStrip::mouseDrag(const juce::MouseEvent& e)
{
    const int i = sliceAt(e.position.x);
    const int lo = juce::jmin(anchor_, i), hi = juce::jmax(anchor_, i);
    sel_.set((float)lo, (float)(hi - lo + 1));
}

void SliceStrip::mouseUp(const juce::MouseEvent&) { sel_.end(); }

//==============================================================================
ImageCard::ImageCard(MadcapProcessor& p) : proc_(p)
{
    setTooltip("Click and hold to grab and scrub the scan line. Drop a picture anywhere on Madcap to load it.");
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void ImageCard::tick()
{
    const int v = proc_.image().version();
    if (v != version_) { version_ = v; pic_ = proc_.image().get(); repaint(); return; }
    const float ph = proc_.engine().ui().imgPhase.load();
    if (proc_.value(P_SRC) > 0.5f || scrubbing_ || std::abs(ph - phase_) > 0.0005f) {
        phase_ = ph;
        repaint();
    }
}

void ImageCard::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel(g, b, {}, colours::blue);
    auto a = b.reduced(2.0f);
    if (pic_ && pic_->image.isValid()) {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(pic_->image, a, juce::RectanglePlacement::stretchToFit);
    }
    const bool live = proc_.value(P_SRC) > 0.5f;
    if (!live) {
        g.setColour(colours::bg.withAlpha(0.55f));
        g.fillRect(a);
        drawLabel(g, "Input is SAMPLE - switch INPUT to IMAGE to hear the sweeper", a, juce::Justification::centred, 13.0f, colours::text);
    }
    // scan line with glow
    const float x = a.getX() + phase_ * a.getWidth();
    juce::ColourGradient glow(colours::teal.withAlpha(0.0f), x - 10.0f, 0, colours::teal.withAlpha(0.0f), x + 10.0f, 0, false);
    glow.addColour(0.5, colours::teal.withAlpha(0.45f));
    g.setGradientFill(glow);
    g.fillRect(juce::Rectangle<float>(x - 10.0f, a.getY(), 20.0f, a.getHeight()));
    g.setColour(scrubbing_ ? colours::orange : colours::teal.brighter(0.4f));
    g.drawLine(x, a.getY(), x, a.getBottom(), 1.6f);

    // sounding notes on the scan line
    const auto& ui = proc_.engine().ui();
    if (live) {
        if (juce::roundToInt(proc_.value(P_IMG_MODE)) == 1) {
            const float my = ui.imgMonoY.load();
            if (my >= 0.0f) {
                g.setColour(colours::orange);
                g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre({x, a.getBottom() - my * a.getHeight()}));
            }
        } else {
            const int n = juce::jlimit(0, IMG_MAX_NOTES, ui.imgNumNotes.load());
            for (int i = 0; i < n; ++i) {
                const float amp = ui.imgAmp[(size_t)i].load();
                if (amp < 0.02f) continue;
                const float y = a.getBottom() - ((float)i + 0.5f) / (float)n * a.getHeight();
                g.setColour(colours::orange.withAlpha(juce::jlimit(0.25f, 1.0f, amp * 1.6f)));
                g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({x, y}));
            }
        }
    }
    if (proc_.value(P_IMG_HOLD) > 0.5f)
        drawLabel(g, "HOLD - live buffer frozen", a.removeFromTop(18.0f).reduced(6.0f, 0.0f), juce::Justification::centredRight,
                  12.0f, colours::orange, Weight::Bold);
}

void ImageCard::mouseDown(const juce::MouseEvent& e)
{
    scrubbing_ = true;
    mouseDrag(e);
}

void ImageCard::mouseDrag(const juce::MouseEvent& e)
{
    const float n = juce::jlimit(0.0f, 0.9999f, e.position.x / (float)juce::jmax(1, getWidth()));
    proc_.engine().setScrub(n);
    phase_ = n;
    repaint();
}

void ImageCard::mouseUp(const juce::MouseEvent&)
{
    scrubbing_ = false;
    proc_.engine().setScrub(-1.0f);
}

//==============================================================================
AnalysisGrid::AnalysisGrid(MadcapProcessor& p) : proc_(p)
{
    setTooltip("Brightness of each column split into one band per scale note (low notes at the bottom)");
}

void AnalysisGrid::rebuild()
{
    auto pic = proc_.image().get();
    int notes[IMG_MAX_NOTES];
    nNotes_ = gs_img_build_notes(juce::roundToInt(proc_.value(P_IMG_KEY)), juce::roundToInt(proc_.value(P_IMG_SCALE)),
                                 juce::roundToInt(proc_.value(P_IMG_LOW)), juce::roundToInt(proc_.value(P_IMG_RANGE)),
                                 notes, IMG_MAX_NOTES);
    listen_ = juce::roundToInt(proc_.value(P_IMG_READ));
    const int N = juce::jmax(1, nNotes_), C = 64;
    cache_ = juce::Image(juce::Image::ARGB, C, N, true, juce::SoftwareImageType());
    if (!pic || pic->luma.size() != (size_t)IMG_ROWS * IMG_COLS) return;
    juce::Image::BitmapData bd(cache_, juce::Image::BitmapData::writeOnly);
    for (int band = 0; band < N; ++band) {
        int r1 = IMG_ROWS - band * IMG_ROWS / N, r0 = IMG_ROWS - (band + 1) * IMG_ROWS / N;
        if (r1 <= r0) r1 = r0 + 1;
        for (int c = 0; c < C; ++c) {
            float sum = 0.0f;
            int cnt = 0;
            for (int r = r0; r < r1; ++r)
                for (int k = c * IMG_COLS / C; k < (c + 1) * IMG_COLS / C; ++k) { sum += pic->luma[(size_t)r * IMG_COLS + (size_t)k]; ++cnt; }
            float v = cnt ? sum / (float)cnt : 0.0f;
            if (listen_ == 1) v = 1.0f - v;
            v = juce::jlimit(0.0f, 1.0f, (v - 0.06f) / 0.94f);
            bd.setPixelColour(c, N - 1 - band, colours::bg.interpolatedWith(colours::teal, v));
        }
    }
}

void AnalysisGrid::tick()
{
    const int key = proc_.image().version() * 1000003 + juce::roundToInt(proc_.value(P_IMG_KEY)) * 7919
                  + juce::roundToInt(proc_.value(P_IMG_SCALE)) * 104729 + juce::roundToInt(proc_.value(P_IMG_LOW)) * 31
                  + juce::roundToInt(proc_.value(P_IMG_RANGE)) * 1301 + juce::roundToInt(proc_.value(P_IMG_READ)) * 17;
    if (key != cacheKey_) { cacheKey_ = key; rebuild(); repaint(); return; }
    const float ph = proc_.engine().ui().imgPhase.load();
    if (std::abs(ph - phase_) > 0.001f || proc_.value(P_SRC) > 0.5f) { phase_ = ph; repaint(); }
}

void AnalysisGrid::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel(g, b, "Analysis", colours::blue);
    auto a = b.reduced(6.0f).withTrimmedTop(16.0f).withTrimmedRight(10.0f);
    if (cache_.isValid()) {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(cache_, a, juce::RectanglePlacement::stretchToFit);
    }
    const int N = juce::jmax(1, nNotes_);
    if (N <= 40) {
        g.setColour(colours::bg.withAlpha(0.6f));
        for (int i = 1; i < N; ++i) g.drawHorizontalLine((int)(a.getY() + a.getHeight() * (float)i / (float)N), a.getX(), a.getRight());
    }
    const float x = a.getX() + phase_ * a.getWidth();
    g.setColour(colours::orange);
    g.drawLine(x, a.getY(), x, a.getBottom(), 1.2f);
    // sounding notes on the right edge
    const auto& ui = proc_.engine().ui();
    const int n = juce::jmin(N, ui.imgNumNotes.load());
    for (int i = 0; i < n; ++i) {
        const float amp = ui.imgAmp[(size_t)i].load();
        if (amp < 0.02f) continue;
        g.setColour(colours::orange.withAlpha(juce::jlimit(0.2f, 1.0f, amp * 1.6f)));
        const float y0 = a.getBottom() - a.getHeight() * (float)(i + 1) / (float)N;
        g.fillRect(juce::Rectangle<float>(a.getRight() + 3.0f, y0, 6.0f, juce::jmax(1.5f, a.getHeight() / (float)N - 1.0f)));
    }
    drawLabel(g, juce::String(nNotes_) + " notes", b.reduced(8.0f, 3.0f).withHeight(14.0f), juce::Justification::centredRight,
              11.0f, colours::text);
}

//==============================================================================
ArpLane::ArpLane(MadcapProcessor& p)
    : proc_(p),
      attach_(*p.param(P_ARP_PATTERN), [this](float v) { bits_ = (unsigned)juce::roundToInt(v); repaint(); }, &p.undoManager())
{
    attach_.sendInitialUpdate();
    setTooltip("Click or drag to mute / unmute steps of the 16-step lane");
    setWantsKeyboardFocus(false);
}

int ArpLane::cellAt(float x) const
{
    return juce::jlimit(0, ARP_STEPS - 1, (int)(x / ((float)getWidth() / (float)ARP_STEPS)));
}

void ArpLane::tick()
{
    const bool active = proc_.value(P_ARP_ON) > 0.5f && proc_.engine().ui().running.load();
    const int s = active ? proc_.engine().ui().arpStep.load() : -1;
    if (s != step_) { step_ = s; repaint(); }
}

void ArpLane::paint(juce::Graphics& g)
{
    const int len = juce::roundToInt(proc_.value(P_ARP_LEN));
    const float w = (float)getWidth() / (float)ARP_STEPS;
    for (int i = 0; i < ARP_STEPS; ++i) {
        auto r = juce::Rectangle<float>(w * (float)i, 0.0f, w, (float)getHeight()).reduced(i % 4 == 0 ? 3.0f : 1.5f, 2.0f)
                     .withTrimmedLeft(i % 4 == 0 ? 0.0f : 0.0f);
        const bool on = (bits_ >> i) & 1u;
        const bool inLen = i < len;
        juce::Colour fill = on ? colours::teal : colours::panel;
        if (!inLen) fill = fill.withAlpha(0.25f);
        g.setColour(fill);
        g.fillRect(r);
        g.setColour(i == step_ ? colours::orange : colours::blue.withAlpha(inLen ? 0.7f : 0.3f));
        g.drawRect(r, i == step_ ? 2.0f : 1.0f);
        g.setColour(on ? colours::bg : colours::textDim);
        g.setFont(font(11.0f, Weight::Bold));
        g.drawText(juce::String(i + 1), r.removeFromBottom(14.0f), juce::Justification::centred);
    }
}

void ArpLane::mouseDown(const juce::MouseEvent& e)
{
    const int c = cellAt(e.position.x);
    paintOn_ = !((bits_ >> c) & 1u);
    attach_.beginGesture();
    dragging_ = true;
    lastCell_ = -1;
    mouseDrag(e);
}

void ArpLane::mouseDrag(const juce::MouseEvent& e)
{
    const int c = cellAt(e.position.x);
    if (c == lastCell_) return;
    lastCell_ = c;
    bits_ = paintOn_ ? (bits_ | (1u << c)) : (bits_ & ~(1u << c));
    attach_.setValueAsPartOfGesture((float)bits_);
    repaint();
}

void ArpLane::mouseUp(const juce::MouseEvent&)
{
    if (dragging_) attach_.endGesture();
    dragging_ = false;
}

//==============================================================================
OrbitDisplay::OrbitDisplay(MadcapProcessor& p) : proc_(p) {}

void OrbitDisplay::tick()
{
    const auto& ui = proc_.engine().ui();
    n_ = juce::jlimit(0, ARP_MAX_NOTES, ui.arpNumNotes.load());
    for (int i = 0; i < n_; ++i) notes_[(size_t)i] = ui.arpNotes[(size_t)i].load();
    idx_ = ui.arpIdx.load();
    step_ = ui.arpStep.load();
    const int t = ui.arpTrig.load();
    if (t != trig_) { trig_ = t; flash_ = 1.0f; }
    flash_ *= 0.9f;
    spin_ += 0.006f;
    repaint();
}

void OrbitDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel(g, b, "Orbit", colours::blue);
    auto c = b.getCentre().translated(0.0f, 6.0f);
    const float R = juce::jmin(b.getWidth(), b.getHeight() - 16.0f) * 0.5f - 18.0f;
    const bool on = proc_.value(P_ARP_ON) > 0.5f;

    for (int k = 1; k <= 3; ++k) {
        g.setColour(colours::blue.withAlpha(0.18f + 0.06f * (float)k));
        g.drawEllipse(juce::Rectangle<float>(R * 2.0f * (float)k / 3.0f, R * 2.0f * (float)k / 3.0f).withCentre(c), 1.0f);
    }
    const int key = proc_.engine().ui().key.load();
    for (int pc = 0; pc < 12; ++pc) {
        const float a = (float)pc / 12.0f * 2.0f * kPi;
        const float sx = std::sin(a), sy = -std::cos(a);
        g.setColour(colours::blue.withAlpha(0.6f));
        g.drawLine(c.x + sx * R, c.y + sy * R, c.x + sx * (R + 5.0f), c.y + sy * (R + 5.0f), 1.0f);
        const int name = (key + pc) % 12;
        g.setColour(pc == 0 ? colours::teal : colours::textDim);
        g.setFont(font(10.0f, Weight::Bold));
        g.drawText(GS_NOTE_NAMES[name], juce::Rectangle<float>(22.0f, 12.0f).withCentre({c.x + sx * (R + 13.0f), c.y + sy * (R + 13.0f)}),
                   juce::Justification::centred);
    }
    // satellite sweeping with the step clock
    const float sat = on ? (float)step_ / (float)ARP_STEPS * 2.0f * kPi : spin_;
    juce::Path trail;
    trail.addCentredArc(c.x, c.y, R + 2.0f, R + 2.0f, 0.0f, sat - 0.9f, sat, true);
    g.setColour(colours::teal.withAlpha(0.6f));
    g.strokePath(trail, juce::PathStrokeType(2.0f));
    g.setColour(colours::teal);
    g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre({c.x + std::sin(sat) * (R + 2.0f), c.y - std::cos(sat) * (R + 2.0f)}));

    // chord notes: angle = pitch class (relative to the key), radius = octave
    juce::Path orbit;
    std::array<juce::Point<float>, ARP_MAX_NOTES> pts{};
    for (int i = 0; i < n_; ++i) {
        const float s = notes_[(size_t)i];
        const int pc = ((int)std::lround(s) % 12 + 12) % 12;
        const float oct = std::floor(s / 12.0f);
        const float rr = R * juce::jlimit(0.18f, 1.0f, 0.38f + 0.2f * oct);
        const float a = (float)pc / 12.0f * 2.0f * kPi;
        pts[(size_t)i] = { c.x + std::sin(a) * rr, c.y - std::cos(a) * rr };
        if (i == 0) orbit.startNewSubPath(pts[0]);
        else orbit.lineTo(pts[(size_t)i]);
    }
    if (n_ > 1) {
        orbit.closeSubPath();
        g.setColour(colours::teal.withAlpha(0.25f));
        g.strokePath(orbit, juce::PathStrokeType(1.0f));
    }
    for (int i = 0; i < n_; ++i) {
        const bool lit = on && i == idx_;
        auto p = pts[(size_t)i];
        if (lit) {
            g.setColour(colours::orange.withAlpha(0.35f * flash_ + 0.1f));
            g.fillEllipse(juce::Rectangle<float>(26.0f, 26.0f).withCentre(p));
        }
        g.setColour(lit ? colours::orange : colours::teal.withAlpha(0.85f));
        g.fillEllipse(juce::Rectangle<float>(lit ? 11.0f : 7.0f, lit ? 11.0f : 7.0f).withCentre(p));
    }
    auto centre = juce::Rectangle<float>(120.0f, 34.0f).withCentre(c);
    drawLabel(g, on ? params::format(P_ARP_MODE, proc_.value(P_ARP_MODE)) : "ARP OFF", centre.removeFromTop(17.0f),
              juce::Justification::centred, 15.0f, on ? colours::text : colours::textDim, Weight::Bold);
    drawLabel(g, params::format(P_ARP_RATE, proc_.value(P_ARP_RATE)), centre, juce::Justification::centred, 12.0f, colours::teal);
}

//==============================================================================
GrainField::GrainField(MadcapProcessor& p, bool interactive)
    : proc_(p), xy_(p, P_POSITION, P_SPRAY), interactive_(interactive)
{
    if (interactive_) {
        setTooltip("Grain field: drag X = position, Y = spray");
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }
}

void GrainField::tick()
{
    const auto& ui = proc_.engine().ui();
    const int v = proc_.sample().version();
    if (v != version_) { version_ = v; data_ = proc_.sample().get(); }
    n_ = juce::jlimit(0, GS_UI_GRAINS, ui.numGrains.load());
    for (int i = 0; i < n_; ++i) { pos_[(size_t)i] = ui.grainPos[(size_t)i].load(); amp_[(size_t)i] = ui.grainAmp[(size_t)i].load(); }
    if (proc_.value(P_SRC) > 0.5f)
        for (int i = 0; i < GS_LIVE_ENV; ++i) live_[(size_t)i] = ui.liveEnv[(size_t)i].load();
    repaint();
}

void GrainField::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawPanel(g, b, "Grain field", colours::blue);
    auto a = b.reduced(6.0f).withTrimmedTop(14.0f);
    const bool image = proc_.value(P_SRC) > 0.5f;
    const auto& ui = proc_.engine().ui();
    const float rs = ui.regionStart.load(), re = juce::jmax(ui.regionStart.load() + 1e-6f, ui.regionEnd.load());
    const int W = juce::jmax(1, (int)a.getWidth());
    const float mid = a.getCentreY(), amp = a.getHeight() * 0.45f;
    g.setColour(colours::blue.withAlpha(0.45f));
    if (image) {
        for (int x = 0; x < W; ++x) {
            const float v = live_[(size_t)juce::jlimit(0, GS_LIVE_ENV - 1, x * GS_LIVE_ENV / W)];
            g.drawVerticalLine((int)a.getX() + x, mid - v * amp, mid + v * amp + 1.0f);
        }
        const float hx = a.getX() + ui.liveHead.load() * a.getWidth();
        g.setColour(colours::green.withAlpha(0.8f));
        g.drawLine(hx, a.getY(), hx, a.getBottom(), 1.0f);
    } else if (data_ && !data_->overviewMin.empty()) {
        const int bins = (int)data_->overviewMin.size();
        for (int x = 0; x < W; ++x) {
            const float n0 = rs + (re - rs) * (float)x / (float)W;
            const int bi = juce::jlimit(0, bins - 1, (int)(n0 * (float)bins));
            g.drawVerticalLine((int)a.getX() + x, mid - data_->overviewMax[(size_t)bi] * amp,
                               mid - data_->overviewMin[(size_t)bi] * amp + 1.0f);
        }
    }
    // grains
    for (int i = 0; i < n_; ++i) {
        float x01 = image ? pos_[(size_t)i] : (pos_[(size_t)i] - rs) / (re - rs);
        if (x01 < -0.01f || x01 > 1.01f) continue;
        const float y01 = 0.08f + 0.84f * hash01(i * 7 + 3);
        const float am = amp_[(size_t)i];
        const float r = 2.0f + am * 4.0f;
        g.setColour(colours::teal.withAlpha(0.25f + 0.7f * am));
        g.fillEllipse(juce::Rectangle<float>(r, r).withCentre({a.getX() + x01 * a.getWidth(), a.getY() + y01 * a.getHeight()}));
    }
    // crosshair = POSITION / SPRAY
    const float px = a.getX() + proc_.value(P_POSITION) * a.getWidth();
    const float py = a.getBottom() - proc_.value(P_SPRAY) * a.getHeight();
    g.setColour(colours::orange.withAlpha(0.5f));
    g.drawLine(px, a.getY(), px, a.getBottom(), 1.0f);
    g.drawLine(a.getX(), py, a.getRight(), py, 1.0f);
    g.setColour(colours::orange);
    g.drawEllipse(juce::Rectangle<float>(14.0f, 14.0f).withCentre({px, py}), 1.5f);
    if (interactive_) {
        drawLabel(g, "Position", a.removeFromBottom(12.0f), juce::Justification::centredRight, 10.0f, colours::textDim);
        drawLabel(g, "Spray", b.reduced(8.0f, 3.0f).withHeight(14.0f), juce::Justification::centredRight, 10.0f, colours::textDim);
    }
}

void GrainField::mouseDown(const juce::MouseEvent& e)
{
    if (!interactive_) return;
    xy_.begin();
    mouseDrag(e);
}

void GrainField::mouseDrag(const juce::MouseEvent& e)
{
    if (!interactive_) return;
    auto a = getLocalBounds().toFloat().reduced(6.0f).withTrimmedTop(14.0f);
    xy_.set(juce::jlimit(0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth()),
            juce::jlimit(0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight()));
}

void GrainField::mouseUp(const juce::MouseEvent&)
{
    if (interactive_) xy_.end();
}

//==============================================================================
ChordPads::ChordPads(MadcapProcessor& p) : proc_(p)
{
    setWantsKeyboardFocus(false);
}

void ChordPads::tick()
{
    const auto& ui = proc_.engine().ui();
    const int m = ui.degreeMask.load(), mi = ui.midiMask.load(), k = ui.key.load(), s = juce::roundToInt(proc_.value(P_SCALE));
    if (m != mask_ || mi != midi_ || k != key_ || s != scale_) {
        mask_ = m; midi_ = mi; key_ = k; scale_ = s;
        repaint();
    }
}

int ChordPads::padAt(juce::Point<float> p) const
{
    if (!getLocalBounds().toFloat().contains(p)) return -1;
    return juce::jlimit(0, 6, (int)(p.x / ((float)getWidth() / 7.0f)));
}

void ChordPads::paint(juce::Graphics& g)
{
    static const char* const roman[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    const float w = (float)getWidth() / 7.0f;
    for (int i = 0; i < 7; ++i) {
        auto r = juce::Rectangle<float>(w * (float)i, 0.0f, w, (float)getHeight()).reduced(3.0f, 0.0f);
        const bool lit = (mask_ >> i) & 1;
        const bool midi = (midi_ >> i) & 1;
        g.setColour(lit ? colours::teal : colours::panel);
        g.fillRect(r);
        g.setColour(midi ? colours::orange : (lit ? colours::teal : colours::blue.withAlpha(0.55f)));
        g.drawRect(r, midi ? 2.0f : 1.0f);
        if (i == pressed_) { g.setColour(colours::text.withAlpha(0.15f)); g.fillRect(r); }
        char buf[32];
        gs_degree_name(key_, scale_, i, buf, (int)sizeof buf);
        auto tr = r.reduced(6.0f, 4.0f);
        g.setColour(lit ? colours::bg : colours::text);
        g.setFont(font(juce::jmin(26.0f, r.getHeight() * 0.42f), Weight::Bold, 0.04f));
        g.drawText(roman[i], tr.removeFromTop(tr.getHeight() * 0.55f), juce::Justification::centredLeft);
        g.setFont(font(13.0f, Weight::Bold, 0.08f));
        g.setColour(lit ? colours::bg.withAlpha(0.85f) : colours::teal);
        g.drawText(juce::String(buf).toUpperCase(), tr, juce::Justification::centredLeft);
        if (midi) drawLed(g, { r.getRight() - 9.0f, r.getY() + 9.0f }, 3.5f, true, colours::orange);
    }
}

void ChordPads::mouseDown(const juce::MouseEvent& e)
{
    const int i = padAt(e.position);
    if (i < 0) return;
    pressed_ = i;
    proc_.undoManager().beginNewTransaction("Chord");
    if (e.mods.isShiftDown()) {
        int m = 0;
        for (int d = 0; d < 7; ++d) if (proc_.value(P_DEG1 + d) > 0.5f) m |= 1 << d;
        m ^= 1 << i;
        if (m == 0) m = 1 << i;
        for (int d = 0; d < 7; ++d) proc_.setValue(P_DEG1 + d, (m >> d) & 1 ? 1.0f : 0.0f);
    } else {
        for (int d = 0; d < 7; ++d) proc_.setValue(P_DEG1 + d, d == i ? 1.0f : 0.0f);
    }
    proc_.engine().releaseLatch();
    proc_.engine().setUiPadHeld(true);
    repaint();
}

void ChordPads::mouseUp(const juce::MouseEvent&)
{
    proc_.engine().setUiPadHeld(false);
    pressed_ = -1;
    repaint();
}

//==============================================================================
RearTouchPad::RearTouchPad(MadcapProcessor& p) : proc_(p), xy_(p, P_CHORD, P_VOICING)
{
    setTooltip("Rear touch: X picks the chord type, Y sets the voicing (close at the bottom, open at the top)");
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void RearTouchPad::tick()
{
    const float c = proc_.value(P_CHORD), v = proc_.value(P_VOICING);
    if (!juce::exactlyEqual(c, chord_) || !juce::exactlyEqual(v, voicing_) || touching_) { chord_ = c; voicing_ = v; repaint(); }
}

void RearTouchPad::paint(juce::Graphics& g)
{
    static const char* const abbr[] = {"UNI", "OCT", "3RD", "4TH", "5TH", "TRI", "SU2", "SU4", "7TH", "9TH"};
    auto b = getLocalBounds().toFloat();
    drawPanel(g, b, "Rear touch", colours::blue);
    auto a = b.reduced(6.0f).withTrimmedTop(14.0f);
    const float zw = a.getWidth() / 10.0f;
    const int sel = juce::roundToInt(chord_);
    for (int i = 0; i < 10; ++i) {
        auto z = juce::Rectangle<float>(a.getX() + zw * (float)i, a.getY(), zw, a.getHeight());
        if (i == sel) { g.setColour(colours::teal.withAlpha(0.22f)); g.fillRect(z); }
        g.setColour(colours::blue.withAlpha(0.25f));
        if (i > 0) g.drawVerticalLine((int)z.getX(), z.getY(), z.getBottom());
        g.setColour(i == sel ? colours::teal : colours::textDim);
        g.setFont(font(9.5f, Weight::Bold));
        g.drawText(abbr[i], z.removeFromBottom(12.0f), juce::Justification::centred);
    }
    const float y = a.getBottom() - voicing_ * a.getHeight();
    g.setColour(colours::orange.withAlpha(0.6f));
    g.drawHorizontalLine((int)y, a.getX(), a.getRight());
    const float x = a.getX() + ((float)sel + 0.5f) * zw;
    g.setColour(colours::orange);
    g.drawEllipse(juce::Rectangle<float>(touching_ ? 22.0f : 12.0f, touching_ ? 22.0f : 12.0f).withCentre({touching_ ? touch_.x : x, y}), 1.5f);
    drawLabel(g, "Open", a.withHeight(12.0f), juce::Justification::centredRight, 9.5f, colours::textDim);
}

void RearTouchPad::setFrom(juce::Point<float> p)
{
    auto a = getLocalBounds().toFloat().reduced(6.0f).withTrimmedTop(14.0f);
    touch_ = p;
    const float x01 = juce::jlimit(0.0f, 0.9999f, (p.x - a.getX()) / a.getWidth());
    const float y01 = juce::jlimit(0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight());
    xy_.set(std::floor(x01 * 10.0f), y01);
}

void RearTouchPad::mouseDown(const juce::MouseEvent& e) { touching_ = true; xy_.begin(); setFrom(e.position); }
void RearTouchPad::mouseDrag(const juce::MouseEvent& e) { setFrom(e.position); }
void RearTouchPad::mouseUp(const juce::MouseEvent&) { touching_ = false; xy_.end(); repaint(); }

//==============================================================================
LevelMeter::LevelMeter(MadcapProcessor& p) : proc_(p) {}

void LevelMeter::tick()
{
    for (int ch = 0; ch < 2; ++ch) {
        const float pk = proc_.engine().takePeak(ch);
        level_[ch] = juce::jmax(pk, level_[ch] * 0.86f);
        if (pk >= hold_[ch]) { hold_[ch] = pk; holdT_[ch] = 45; }
        else if (--holdT_[ch] <= 0) hold_[ch] *= 0.95f;
    }
    repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const int segs = 18;
    const float h = (b.getHeight() - 2.0f) / 2.0f;
    for (int ch = 0; ch < 2; ++ch) {
        auto row = juce::Rectangle<float>(b.getX(), b.getY() + (float)ch * (h + 2.0f), b.getWidth(), h);
        const float db = juce::Decibels::gainToDecibels(level_[ch], -60.0f);
        const float holdDb = juce::Decibels::gainToDecibels(hold_[ch], -60.0f);
        const float sw = row.getWidth() / (float)segs;
        for (int s = 0; s < segs; ++s) {
            const float segDb = -48.0f + 48.0f * (float)(s + 1) / (float)segs;
            const bool lit = db >= segDb - 48.0f / (float)segs;
            const bool holdSeg = holdDb >= segDb - 48.0f / (float)segs && holdDb < segDb;
            juce::Colour c = segDb > -3.0f ? colours::orange : (segDb > -12.0f ? colours::amber : colours::teal);
            g.setColour(lit || holdSeg ? c : colours::blueDim.withAlpha(0.6f));
            g.fillRect(juce::Rectangle<float>(row.getX() + sw * (float)s, row.getY(), sw - 1.0f, row.getHeight()));
        }
    }
}

} // namespace madcap::ui
