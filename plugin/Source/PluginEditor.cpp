#include "PluginEditor.h"

namespace madcap {

namespace {
int lastPage = PAGE_SAMPLE;   // remembered while the plugin stays loaded
}

MadcapEditor::MadcapEditor(MadcapProcessor& p)
    : juce::AudioProcessorEditor(p), proc_(p), vblank_(this, [this] { tick(); })
{
    setLookAndFeel(&lnf_);
    juce::LookAndFeel::setDefaultLookAndFeel(&lnf_);   // popups, alert windows, file choosers
    content_.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(content_);
    content_.setBounds(0, 0, kW, kH);

    top_ = std::make_unique<ui::TopBar>(p, [this](int i) { showPage(i); });
    content_.addAndMakeVisible(*top_);
    top_->setBounds(0, 0, kW, ui::TopBar::H);

    for (int i = 0; i < 6; ++i) {
        pages_[(size_t)i] = ui::createPage(p, i);
        content_.addChildComponent(*pages_[(size_t)i]);
        pages_[(size_t)i]->setBounds(16, ui::TopBar::H + 10, ui::Page::W, ui::Page::H);
    }
    showPage(lastPage);

    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(kW * 3 / 4, kH * 3 / 4, kW * 2, kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double)kW / (double)kH);
    setSize(juce::jlimit(kW * 3 / 4, kW * 2, proc_.editorWidth), juce::jlimit(kH * 3 / 4, kH * 2, proc_.editorHeight));
}

MadcapEditor::~MadcapEditor()
{
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == &lnf_) juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void MadcapEditor::showPage(int index)
{
    page_ = juce::jlimit(0, 5, index);
    lastPage = page_;
    for (int i = 0; i < 6; ++i) pages_[(size_t)i]->setVisible(i == page_);
    top_->setCurrentTab(page_);
}

void MadcapEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::colours::bg);
}

void MadcapEditor::paintOverChildren(juce::Graphics& g)
{
    if (!dragOver_) return;
    auto r = getLocalBounds().toFloat().reduced(6.0f);
    g.setColour(ui::colours::bg.withAlpha(0.6f));
    g.fillRect(r);
    ui::drawPanel(g, r, {}, ui::colours::orange);
    ui::drawLabel(g, "Drop audio (WAV AIFF FLAC MP3 OGG) or a picture (PNG JPG BMP GIF)", r,
                  juce::Justification::centred, 20.0f * (float)getWidth() / (float)kW, ui::colours::orange, ui::Weight::Bold);
}

void MadcapEditor::resized()
{
    const float s = (float)getWidth() / (float)kW;
    content_.setTransform(juce::AffineTransform::scale(s));
    proc_.editorWidth = getWidth();
    proc_.editorHeight = getHeight();
}

void MadcapEditor::tick()
{
    auto msg = proc_.takeStatusMessage();
    if (msg.isNotEmpty()) top_->showMessage(msg);
    top_->tick();
    pages_[(size_t)page_]->tick();
}

bool MadcapEditor::keyPressed(const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    // Space stops a recording (or cancels one that is still waiting for signal); otherwise
    // Space is left to the host (transport)
    if (k.getKeyCode() == juce::KeyPress::spaceKey && !mods.isAnyModifierKeyDown()
        && (proc_.isRecording() || proc_.isRecordArmed())) {
        proc_.stopRecording();
        return true;
    }
    if (mods.isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z')) {
        if (mods.isShiftDown()) proc_.undoManager().redo();
        else proc_.undoManager().undo();
        return true;
    }
    if (mods.isCommandDown() && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) {
        proc_.undoManager().redo();
        return true;
    }
    if (mods.isCommandDown() && k.getKeyCode() >= '1' && k.getKeyCode() <= '6') {
        showPage(k.getKeyCode() - '1');
        return true;
    }
    return false;
}

bool MadcapEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files) {
        const auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (SampleStore::supportedExtensions().contains(ext) || ImageStore::supportedExtensions().contains(ext)) return true;
    }
    return false;
}

void MadcapEditor::filesDropped(const juce::StringArray& files, int, int)
{
    dragOver_ = false;
    repaint();
    for (const auto& path : files) {
        const juce::File f(path);
        const auto ext = f.getFileExtension().toLowerCase();
        if (SampleStore::supportedExtensions().contains(ext)) { proc_.loadSampleAsync(f); showPage(PAGE_SAMPLE); return; }
        if (ImageStore::supportedExtensions().contains(ext)) { proc_.loadImageAsync(f); showPage(PAGE_IMAGE); return; }
    }
}

} // namespace madcap
