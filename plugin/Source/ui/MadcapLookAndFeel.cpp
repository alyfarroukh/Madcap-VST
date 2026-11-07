#include "MadcapLookAndFeel.h"
#include "BinaryData.h"

namespace madcap::ui {

namespace {

struct Typefaces {
    juce::Typeface::Ptr medium, semibold, bold;
    Typefaces()
    {
        medium = juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniMedium_ttf, (size_t)BinaryData::RajdhaniMedium_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniSemiBold_ttf, (size_t)BinaryData::RajdhaniSemiBold_ttfSize);
        bold = juce::Typeface::createSystemTypefaceFor(BinaryData::RajdhaniBold_ttf, (size_t)BinaryData::RajdhaniBold_ttfSize);
    }
};

Typefaces& typefaces()
{
    static Typefaces t;   // created on first use (message thread), never from DllMain
    return t;
}

} // namespace

juce::Font font(float height, Weight w, float tracking)
{
    auto& t = typefaces();
    juce::Typeface::Ptr tf = w == Weight::Bold ? t.bold : (w == Weight::Medium ? t.medium : t.semibold);
    if (tf == nullptr) return juce::Font(juce::FontOptions(height)).withExtraKerningFactor(tracking);
    return juce::Font(juce::FontOptions(tf).withHeight(height).withKerningFactor(tracking));
}

void drawPanel(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour c)
{
    g.setColour(colours::panel.withAlpha(0.55f));
    g.fillRect(r);
    g.setColour(c.withAlpha(0.35f));
    g.drawRect(r, 1.0f);
    // corner brackets
    const float L = 7.0f, t = 1.5f;
    const float x = r.getX(), y = r.getY(), rt = r.getRight(), bt = r.getBottom();
    g.setColour(c.withAlpha(0.95f));
    g.fillRect(x, y, L, t);           g.fillRect(x, y, t, L);
    g.fillRect(rt - L, y, L, t);      g.fillRect(rt - t, y, t, L);
    g.fillRect(x, bt - t, L, t);      g.fillRect(x, bt - L, t, L);
    g.fillRect(rt - L, bt - t, L, t); g.fillRect(rt - t, bt - L, t, L);
    if (title.isNotEmpty()) {
        auto tr = juce::Rectangle<float>(r.getX() + 10.0f, r.getY() + 4.0f, r.getWidth() - 20.0f, 14.0f);
        drawLabel(g, title, tr, juce::Justification::centredLeft, 12.0f, c.brighter(0.4f), Weight::Bold);
    }
}

void drawLabel(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
               float height, juce::Colour c, Weight w)
{
    g.setColour(c);
    g.setFont(font(height, w, 0.08f));
    g.drawFittedText(text.toUpperCase(), r.toNearestInt(), j, 1, 0.8f);
}

void drawLed(juce::Graphics& g, juce::Point<float> c, float radius, bool on, juce::Colour col)
{
    if (on) {
        juce::ColourGradient glow(col.withAlpha(0.55f), c, col.withAlpha(0.0f), c.translated(radius * 3.0f, 0), true);
        g.setGradientFill(glow);
        g.fillEllipse(juce::Rectangle<float>(radius * 6.0f, radius * 6.0f).withCentre(c));
        g.setColour(col);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c));
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillEllipse(juce::Rectangle<float>(radius * 0.8f, radius * 0.8f).withCentre(c.translated(-radius * 0.25f, -radius * 0.25f)));
    } else {
        g.setColour(colours::blueDim);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c));
        g.setColour(colours::blue.withAlpha(0.5f));
        g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c), 1.0f);
    }
}

void drawFocus(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(colours::orange.withAlpha(0.85f));
    auto rr = r.reduced(0.5f);
    g.drawLine(rr.getX(), rr.getY(), rr.getX() + 6, rr.getY(), 1.5f);
    g.drawLine(rr.getX(), rr.getY(), rr.getX(), rr.getY() + 6, 1.5f);
    g.drawLine(rr.getRight() - 6, rr.getBottom(), rr.getRight(), rr.getBottom(), 1.5f);
    g.drawLine(rr.getRight(), rr.getBottom() - 6, rr.getRight(), rr.getBottom(), 1.5f);
}

//==============================================================================
MadcapLookAndFeel::MadcapLookAndFeel()
{
    using namespace colours;
    setColour(juce::ResizableWindow::backgroundColourId, bg);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF0A1517));
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, teal);
    setColour(juce::PopupMenu::highlightedTextColourId, bg);
    setColour(juce::PopupMenu::headerTextColourId, orange);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xF00A1517));
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, teal);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A1517));
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, blue);
    setColour(juce::TextEditor::focusedOutlineColourId, teal);
    setColour(juce::TextEditor::highlightColourId, teal.withAlpha(0.4f));
    setColour(juce::CaretComponent::caretColourId, orange);
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xFF081012));
    setColour(juce::AlertWindow::textColourId, text);
    setColour(juce::AlertWindow::outlineColourId, teal);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF0A1517));
    setColour(juce::TextButton::buttonOnColourId, teal);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, bg);
    setColour(juce::Label::textColourId, text);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0A1517));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, blue);
    setColour(juce::ScrollBar::thumbColourId, teal.withAlpha(0.6f));
    setColour(juce::ListBox::backgroundColourId, bg);
}

juce::Typeface::Ptr MadcapLookAndFeel::getTypefaceForFont(const juce::Font& f)
{
    auto& t = typefaces();
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty()) {
        auto tf = f.isBold() ? t.bold : t.semibold;
        if (tf != nullptr) return tf;
    }
    return juce::LookAndFeel_V4::getTypefaceForFont(f);
}

void MadcapLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto r = juce::Rectangle<float>(0, 0, (float)width, (float)height);
    g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
    g.fillRect(r);
    g.setColour(colours::teal.withAlpha(0.8f));
    g.drawRect(r, 1.0f);
    g.setColour(colours::text);
    g.setFont(font(13.0f, Weight::Medium, 0.03f));
    g.drawFittedText(text, r.reduced(8, 4).toNearestInt(), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> MadcapLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                                         juce::Rectangle<int> parentArea)
{
    const int w = juce::jlimit(80, 360, tipText.length() * 7 + 20);
    const int lines = 1 + tipText.length() * 7 / 340;
    const int h = 10 + 16 * lines;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void MadcapLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(colours::teal.withAlpha(0.6f));
    g.drawRect(0, 0, width, height, 1);
}

juce::Font MadcapLookAndFeel::getPopupMenuFont() { return font(15.0f, Weight::SemiBold, 0.04f); }

void MadcapLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(b.getToggleState() ? colours::teal : juce::Colour(0xFF0A1517).brighter(over ? 0.15f : 0.0f));
    g.fillRect(r);
    g.setColour(down ? colours::orange : colours::teal.withAlpha(0.7f));
    g.drawRect(r, 1.0f);
}

juce::Font MadcapLookAndFeel::getTextButtonFont(juce::TextButton&, int h) { return font(juce::jmin(16.0f, (float)h * 0.6f), Weight::Bold); }
juce::Font MadcapLookAndFeel::getAlertWindowTitleFont() { return font(20.0f, Weight::Bold); }
juce::Font MadcapLookAndFeel::getAlertWindowMessageFont() { return font(15.0f, Weight::Medium, 0.02f); }
juce::Font MadcapLookAndFeel::getAlertWindowFont() { return font(15.0f, Weight::Medium, 0.02f); }

} // namespace madcap::ui
