#include "Widgets.h"

namespace madcap::ui {

//==============================================================================
ParamWidget::ParamWidget(MadcapProcessor& p, int paramIndex)
    : proc_(p),
      param_(*p.param(paramIndex)),
      index_(paramIndex),
      label_(GS_PARAM_SHORT[paramIndex]),
      attach_(*p.param(paramIndex), [this](float v) { value_ = v; updateTooltip(); valueChanged(); }, &p.undoManager())
{
    setWantsKeyboardFocus(true);
    setTitle(GS_PARAMS[paramIndex].name);
    attach_.sendInitialUpdate();
}

void ParamWidget::setValueComplete(float v)
{
    attach_.setValueAsCompleteGesture(gs_param_clamp(index_, v));
}

void ParamWidget::updateTooltip()
{
    setTooltip(juce::String(GS_PARAMS[index_].name) + ": " + valueText());
}

//==============================================================================
RingKnob::RingKnob(MadcapProcessor& p, int paramIndex, juce::Colour accent)
    : ParamWidget(p, paramIndex), accent_(accent)
{
    const auto& d = GS_PARAMS[paramIndex];
    bipolar_ = d.min < 0.0f && d.max > 0.0f;
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void RingKnob::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float labelH = 14.0f;
    auto knobArea = b.withTrimmedBottom(labelH);
    const float d = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) - 2.0f;
    auto ring = juce::Rectangle<float>(d, d).withCentre(knobArea.getCentre());
    const auto c = ring.getCentre();
    const float rOut = d * 0.5f, rIn = rOut * 0.74f;
    const float a0 = juce::MathConstants<float>::pi * -0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
    const int ticks = 29;
    const float v = norm();
    const float centreNorm = bipolar_ ? param_.convertTo0to1(0.0f) : 0.0f;
    const float lo = juce::jmin(centreNorm, v), hi = juce::jmax(centreNorm, v);

    for (int i = 0; i < ticks; ++i) {
        const float t = (float)i / (float)(ticks - 1);
        const float a = a0 + (a1 - a0) * t;
        const float hs = 0.5f / (float)(ticks - 1);
        const bool lit = t >= lo - hs && t <= hi + hs;
        const float sx = std::sin(a), sy = -std::cos(a);
        const float r0 = lit ? rIn - 1.0f : rIn + 1.5f;
        g.setColour(lit ? accent_ : colours::blue.withAlpha(0.32f));
        g.drawLine(c.x + sx * r0, c.y + sy * r0, c.x + sx * rOut, c.y + sy * rOut, lit ? 2.4f : 1.6f);
    }
    // inner ring + pointer dot
    g.setColour(colours::blue.withAlpha(0.45f));
    g.drawEllipse(juce::Rectangle<float>(rIn * 1.3f, rIn * 1.3f).withCentre(c), 1.0f);
    const float a = a0 + (a1 - a0) * v;
    const float pr = rIn * 0.5f;
    g.setColour(accent_.brighter(0.3f));
    g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre({c.x + std::sin(a) * pr, c.y - std::cos(a) * pr}));

    drawLabel(g, label_, b.removeFromBottom(labelH), juce::Justification::centred, 11.0f,
              hasKeyboardFocus(true) ? colours::orange : colours::textDim);

    if (dragging_ || isMouseButtonDown()) {   // value bubble
        auto txt = valueText();
        g.setFont(font(12.0f, Weight::Bold, 0.02f));
        const float w = juce::jmin(getWidth() - 2.0f, juce::jmax(d * 0.9f, (float)txt.length() * 6.5f + 8.0f));
        auto bub = juce::Rectangle<float>(w, 17.0f).withCentre(c);
        g.setColour(colours::bg.withAlpha(0.92f));
        g.fillRoundedRectangle(bub, 3.0f);
        g.setColour(accent_);
        g.drawRoundedRectangle(bub, 3.0f, 1.0f);
        g.setColour(colours::text);
        g.drawFittedText(txt, bub.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    if (hasKeyboardFocus(true)) drawFocus(g, ring.expanded(2.0f));
}

void RingKnob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    dragging_ = true;
    dragStart_ = e.position;
    dragStartNorm_ = norm();
    beginDrag();
    repaint();
}

void RingKnob::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging_) return;
    const float px = (dragStart_.y - e.position.y) + (e.position.x - dragStart_.x) * 0.5f;
    const float sens = e.mods.isShiftDown() ? 900.0f : 180.0f;
    const float n = juce::jlimit(0.0f, 1.0f, dragStartNorm_ + px / sens);
    dragTo(param_.convertFrom0to1(n));
    if (e.mods.isShiftDown()) { dragStart_ = e.position; dragStartNorm_ = n; }
}

void RingKnob::mouseUp(const juce::MouseEvent&)
{
    if (dragging_) endDrag();
    dragging_ = false;
    repaint();
}

void RingKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    setValueComplete(GS_PARAMS[index_].def);
}

void RingKnob::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float dir = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (dir == 0.0f) return;
    float step = e.mods.isShiftDown() ? 0.005f : 0.03f;
    if (GS_PARAMS[index_].discrete) step = 1.0f / juce::jmax(1, numSteps() - 1);
    setValueComplete(param_.convertFrom0to1(juce::jlimit(0.0f, 1.0f, norm() + (dir > 0 ? step : -step))));
}

bool RingKnob::keyPressed(const juce::KeyPress& k)
{
    float step = k.getModifiers().isShiftDown() ? 0.005f : 0.025f;
    if (GS_PARAMS[index_].discrete) step = 1.0f / juce::jmax(1, numSteps() - 1);
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) step = +step;
    else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) step = -step;
    else if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { setValueComplete(GS_PARAMS[index_].def); return true; }
    else return false;
    setValueComplete(param_.convertFrom0to1(juce::jlimit(0.0f, 1.0f, norm() + step)));
    return true;
}

//==============================================================================
SegmentSwitch::SegmentSwitch(MadcapProcessor& p, int paramIndex, juce::StringArray labels, bool showTitle)
    : ParamWidget(p, paramIndex), labels_(std::move(labels)), showTitle_(showTitle)
{
    if (labels_.isEmpty()) {
        const auto& d = GS_PARAMS[paramIndex];
        for (int i = 0; i < numSteps(); ++i) {
            if (d.labels) labels_.add(juce::String(d.labels[i]).toUpperCase());
            else labels_.add(juce::String((int)d.min + i));
        }
    }
}

juce::Rectangle<float> SegmentSwitch::segArea() const
{
    auto b = getLocalBounds().toFloat();
    if (showTitle_) b.removeFromTop(14.0f);
    return b;
}

void SegmentSwitch::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (showTitle_) drawLabel(g, label_, b.removeFromTop(14.0f), juce::Justification::centredLeft, 11.0f,
                              hasKeyboardFocus(true) ? colours::orange : colours::textDim);
    const int n = labels_.size();
    if (n == 0) return;
    const int sel = juce::roundToInt(value_ - GS_PARAMS[index_].min);
    const float w = b.getWidth() / (float)n;
    for (int i = 0; i < n; ++i) {
        auto r = juce::Rectangle<float>(b.getX() + w * (float)i, b.getY(), w, b.getHeight()).reduced(1.0f, 0.0f);
        const bool on = i == sel;
        g.setColour(on ? accent_ : colours::panel);
        g.fillRect(r);
        g.setColour(on ? accent_ : colours::blue.withAlpha(0.45f));
        g.drawRect(r, 1.0f);
        g.setColour(on ? colours::bg : colours::text.withAlpha(0.8f));
        g.setFont(font(juce::jmin(13.0f, r.getHeight() * 0.62f), Weight::Bold, 0.05f));
        g.drawFittedText(labels_[i], r.reduced(2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
    if (hasKeyboardFocus(true)) drawFocus(g, b);
}

void SegmentSwitch::mouseDown(const juce::MouseEvent& e)
{
    auto b = segArea();
    if (!b.contains(e.position) || labels_.isEmpty()) return;
    const int i = juce::jlimit(0, labels_.size() - 1, (int)((e.position.x - b.getX()) / (b.getWidth() / (float)labels_.size())));
    setValueComplete(GS_PARAMS[index_].min + (float)i);
}

bool SegmentSwitch::keyPressed(const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    int dir = 0;
    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey) dir = 1;
    else if (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey) dir = -1;
    else return false;
    setValueComplete(value_ + (float)dir);
    return true;
}

//==============================================================================
Stepper::Stepper(MadcapProcessor& p, int paramIndex, Formatter f, bool wrap)
    : ParamWidget(p, paramIndex), fmt_(std::move(f)), wrap_(wrap)
{
}

void Stepper::step(int dir)
{
    const auto& d = GS_PARAMS[index_];
    float v = value_ + (float)dir;
    if (wrap_) {
        if (v > d.max) v = d.min;
        if (v < d.min) v = d.max;
    }
    setValueComplete(v);
}

void Stepper::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawLabel(g, label_, b.removeFromTop(14.0f), juce::Justification::centredLeft, 11.0f,
              hasKeyboardFocus(true) ? colours::orange : colours::textDim);
    const float aw = juce::jmin(22.0f, b.getHeight());
    auto left = b.removeFromLeft(aw), right = b.removeFromRight(aw);
    g.setColour(colours::panel);
    g.fillRect(b);
    g.setColour(colours::blue.withAlpha(0.45f));
    g.drawRect(b, 1.0f);
    for (auto [r, dir] : { std::pair{left, -1}, std::pair{right, 1} }) {
        g.setColour(colours::teal.withAlpha(0.9f));
        juce::Path tri;
        auto cc = r.getCentre();
        const float s = juce::jmin(5.0f, r.getHeight() * 0.25f);
        if (dir < 0) tri.addTriangle(cc.x + s * 0.7f, cc.y - s, cc.x + s * 0.7f, cc.y + s, cc.x - s * 0.8f, cc.y);
        else tri.addTriangle(cc.x - s * 0.7f, cc.y - s, cc.x - s * 0.7f, cc.y + s, cc.x + s * 0.8f, cc.y);
        g.fillPath(tri);
    }
    g.setColour(colours::text);
    g.setFont(font(juce::jmin(15.0f, b.getHeight() * 0.7f), Weight::Bold, 0.05f));
    const auto txt = fmt_ ? fmt_(value_) : valueText().toUpperCase();
    g.drawFittedText(txt, b.reduced(2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    if (hasKeyboardFocus(true)) drawFocus(g, getLocalBounds().toFloat());
}

void Stepper::mouseDown(const juce::MouseEvent& e)
{
    if (e.position.y < 14.0f) return;
    step(e.position.x < (float)getWidth() * 0.5f ? -1 : 1);
}

void Stepper::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = w.deltaY != 0.0f ? w.deltaY : w.deltaX;
    if (d != 0.0f) step(d > 0 ? 1 : -1);
}

bool Stepper::keyPressed(const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey) { step(1); return true; }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::downKey) { step(-1); return true; }
    return false;
}

//==============================================================================
LedToggle::LedToggle(MadcapProcessor& p, int paramIndex, juce::Colour led)
    : ParamWidget(p, paramIndex), led_(led)
{
}

void LedToggle::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const bool on = value_ > 0.5f;
    g.setColour(on ? led_.withAlpha(0.12f) : colours::panel);
    g.fillRect(b.reduced(0.5f));
    g.setColour(on ? led_.withAlpha(0.8f) : colours::blue.withAlpha(0.45f));
    g.drawRect(b.reduced(0.5f), 1.0f);
    const float r = juce::jmin(4.5f, b.getHeight() * 0.18f);
    drawLed(g, { b.getX() + 12.0f, b.getCentreY() }, r, on, led_);
    drawLabel(g, label_, b.withTrimmedLeft(22.0f).withTrimmedRight(4.0f), juce::Justification::centredLeft, 12.0f,
              on ? colours::text : colours::textDim, Weight::Bold);
    if (hasKeyboardFocus(true)) drawFocus(g, b);
}

void LedToggle::mouseDown(const juce::MouseEvent&)
{
    setValueComplete(value_ > 0.5f ? 0.0f : 1.0f);
}

bool LedToggle::keyPressed(const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::spaceKey || k.getKeyCode() == juce::KeyPress::returnKey) {
        setValueComplete(value_ > 0.5f ? 0.0f : 1.0f);
        return true;
    }
    return false;
}

//==============================================================================
HudButton::HudButton(const juce::String& text, juce::Colour accent)
    : juce::Button(text), accent_(accent)
{
    setWantsKeyboardFocus(true);
}

void HudButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = filled_ || getToggleState();
    g.setColour(on ? accent_ : colours::panel.brighter(over ? 0.25f : 0.0f));
    g.fillRect(b);
    g.setColour(down ? colours::orange : (on ? accent_ : accent_.withAlpha(0.6f)));
    g.drawRect(b, 1.0f);
    g.setColour(!isEnabled() ? colours::textDim.withAlpha(0.5f) : (on ? colours::bg : colours::text));
    g.setFont(font(fontSize, Weight::Bold, 0.08f));
    g.drawFittedText(getButtonText().toUpperCase(), b.reduced(3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    if (hasKeyboardFocus(true)) drawFocus(g, b);
}

//==============================================================================
PlayButton::PlayButton(MadcapProcessor& p) : ParamWidget(p, P_PLAY)
{
    setTooltip("PLAY / STOP: stops grains, arp and all FX tails. Press again to restart.");
}

void PlayButton::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(1.0f);
    const bool on = value_ > 0.5f;
    const juce::Colour c = on ? (waiting_ ? colours::amber : colours::green) : colours::orange;
    g.setColour(on && !waiting_ ? c : colours::panel);
    g.fillRect(b);
    g.setColour(c);
    g.drawRect(b, on ? 2.0f : 1.5f);
    auto icon = b.removeFromLeft(b.getHeight()).reduced(b.getHeight() * 0.3f);
    g.setColour(on && !waiting_ ? colours::bg : c);
    if (on) {
        juce::Path tri;
        tri.addTriangle(icon.getX(), icon.getY(), icon.getX(), icon.getBottom(), icon.getRight(), icon.getCentreY());
        g.fillPath(tri);
    } else {
        g.fillRect(icon.reduced(1.0f));
    }
    g.setFont(font(juce::jmin(20.0f, b.getHeight() * 0.42f), Weight::Bold, 0.1f));
    g.drawText(on ? (waiting_ ? "HOST" : "PLAY") : "STOP", b.withTrimmedRight(6.0f), juce::Justification::centred);
    if (hasKeyboardFocus(true)) drawFocus(g, getLocalBounds().toFloat());
}

void PlayButton::mouseDown(const juce::MouseEvent&) { setValueComplete(value_ > 0.5f ? 0.0f : 1.0f); }

bool PlayButton::keyPressed(const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::returnKey || k.getKeyCode() == juce::KeyPress::spaceKey) {
        setValueComplete(value_ > 0.5f ? 0.0f : 1.0f);
        return true;
    }
    return false;
}

//==============================================================================
BpmField::BpmField(MadcapProcessor& p) : ParamWidget(p, P_BPM)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setTooltip("Tempo (30-300 BPM): drag, Shift = fine, double-click to type");
}

void BpmField::resized()
{
    if (editor_) editor_->setBounds(getLocalBounds().withTrimmedTop(14));
}

void BpmField::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawLabel(g, host_ ? "BPM (HOST)" : "BPM", b.removeFromTop(14.0f), juce::Justification::centredLeft, 11.0f,
              hasKeyboardFocus(true) ? colours::orange : colours::textDim);
    g.setColour(colours::panel);
    g.fillRect(b);
    g.setColour(host_ ? colours::blue.withAlpha(0.5f) : colours::teal.withAlpha(0.8f));
    g.drawRect(b, 1.0f);
    g.setColour(host_ ? colours::textDim : colours::text);
    g.setFont(font(juce::jmin(20.0f, b.getHeight() * 0.75f), Weight::Bold, 0.05f));
    g.drawText(juce::String(shown_ > 0 ? shown_ : value_, 1), b, juce::Justification::centred);
}

void BpmField::mouseDown(const juce::MouseEvent&)
{
    startValue_ = value_;
    dragging_ = true;
    beginDrag();
}

void BpmField::mouseDrag(const juce::MouseEvent& e)
{
    const float d = (float)-e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.05f : 0.5f);
    dragTo(std::round((startValue_ + d) * 10.0f) / 10.0f);
}

void BpmField::mouseUp(const juce::MouseEvent&)
{
    if (dragging_) endDrag();
    dragging_ = false;
}

void BpmField::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = w.deltaY != 0.0f ? w.deltaY : w.deltaX;
    if (d != 0.0f) setValueComplete(value_ + (d > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.1f : 1.0f));
}

void BpmField::mouseDoubleClick(const juce::MouseEvent&)
{
    editor_ = std::make_unique<juce::TextEditor>();
    editor_->setFont(font(18.0f, Weight::Bold));
    editor_->setJustification(juce::Justification::centred);
    editor_->setInputRestrictions(6, "0123456789.");
    editor_->setText(juce::String(value_, 1), false);
    editor_->addListener(this);
    addAndMakeVisible(*editor_);
    resized();
    editor_->selectAll();
    editor_->grabKeyboardFocus();
}

void BpmField::closeEditor(bool apply)
{
    if (!editor_) return;
    if (apply) setValueComplete(editor_->getText().getFloatValue());
    auto* ed = editor_.release();
    ed->removeListener(this);
    removeChildComponent(ed);
    juce::MessageManager::callAsync([ed] { delete ed; });   // never delete it from inside its own callback
    repaint();
}

void BpmField::textEditorReturnKeyPressed(juce::TextEditor&) { closeEditor(true); }
void BpmField::textEditorEscapeKeyPressed(juce::TextEditor&) { closeEditor(false); }
void BpmField::textEditorFocusLost(juce::TextEditor&) { closeEditor(true); }

//==============================================================================
LockToggle::LockToggle(MadcapProcessor& p, int page) : proc_(p), page_(page)
{
    setTooltip("Lock this page so RND (randomize) leaves it alone");
}

void LockToggle::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const bool on = proc_.randomLocks()[(size_t)page_];
    drawLed(g, { b.getX() + 7.0f, b.getCentreY() }, 3.5f, on, colours::orange);
    drawLabel(g, on ? "Locked" : "Lock", b.withTrimmedLeft(15.0f), juce::Justification::centredLeft, 11.0f,
              on ? colours::orange : colours::textDim, Weight::Bold);
}

void LockToggle::mouseDown(const juce::MouseEvent&)
{
    auto& l = proc_.randomLocks()[(size_t)page_];
    l = !l;
    repaint();
}

} // namespace madcap::ui
