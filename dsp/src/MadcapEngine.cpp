#include "madcap/MadcapEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace madcap {

namespace {
constexpr int kSlice = 32;               // control rate: parameters / MIDI state update every 32 samples
constexpr float kSmoothSeconds = 0.015f; // parameter smoothing time constant
inline int iround(float v) noexcept { return (int)std::lround(v); }
} // namespace

void SourceBuffer::analyse(int sampleRate)
{
    slicer = GsSlicer{};
    if (!samples.empty())
        gs_slicer_analyse(&slicer, samples.data(), (int)samples.size(), sampleRate);
}

Engine::Engine()
{
    for (int i = 0; i < P_COUNT; ++i) target_[(size_t)i] = current_[(size_t)i] = GS_PARAMS[i].def;
}

Engine::~Engine()
{
    if (e_) { gs_engine_free(e_); std::free(e_); }
    delete pendingSource_.exchange(nullptr);
    delete pendingImage_.exchange(nullptr);
    delete currentSource_;
    delete currentImage_;
    collectGarbage();
}

bool Engine::prepare(double sampleRate)
{
    int sr = (int)std::lround(sampleRate);
    auto* ne = static_cast<GsEngine*>(std::calloc(1, sizeof(GsEngine)));
    if (!ne) return false;
    if (gs_engine_init(ne, sr) != 0) { gs_engine_free(ne); std::free(ne); return false; }
    if (e_) { gs_engine_free(e_); std::free(e_); }
    e_ = ne;
    sampleRate_ = sampleRate;
    snapParams_ = true;
    if (currentSource_)
        gs_engine_swap_source(e_, currentSource_->samples.data(), (int)currentSource_->samples.size(), &currentSource_->slicer);
    if (currentImage_ && currentImage_->luma.size() == (size_t)IMG_ROWS * IMG_COLS)
        gs_img_set_grid(&e_->img, currentImage_->luma.data());
    return true;
}

int Engine::latencySamples() const noexcept { return e_ ? gs_engine_latency(e_) : 0; }

void Engine::setSourceNow(std::unique_ptr<SourceBuffer> src)
{
    delete pendingSource_.exchange(nullptr);
    SourceBuffer* old = currentSource_;
    currentSource_ = src.release();
    if (e_) {
        if (currentSource_)
            gs_engine_swap_source(e_, currentSource_->samples.data(), (int)currentSource_->samples.size(), &currentSource_->slicer);
        else
            gs_engine_swap_source(e_, nullptr, 0, nullptr);
    }
    delete old;
}

void Engine::setImageNow(std::unique_ptr<ImageGrid> img)
{
    delete pendingImage_.exchange(nullptr);
    ImageGrid* old = currentImage_;
    currentImage_ = img.release();
    if (e_ && currentImage_ && currentImage_->luma.size() == (size_t)IMG_ROWS * IMG_COLS)
        gs_img_set_grid(&e_->img, currentImage_->luma.data());
    delete old;
}

void Engine::reset()
{
    if (e_) gs_engine_panic(e_);
    midi_.reset();
}

void Engine::postSource(std::unique_ptr<SourceBuffer> src)
{
    delete pendingSource_.exchange(src.release(), std::memory_order_acq_rel);   // an unconsumed one is ours
}

void Engine::postImage(std::unique_ptr<ImageGrid> img)
{
    if (img && img->luma.size() != (size_t)IMG_ROWS * IMG_COLS) return;
    delete pendingImage_.exchange(img.release(), std::memory_order_acq_rel);
}

void Engine::collectGarbage()
{
    while (SourceBuffer* s = retiredSources_.pop()) delete s;
    while (ImageGrid* g = retiredImages_.pop()) delete g;
}

void Engine::swapPending() noexcept
{
    if (retiredSources_.hasSpace() && pendingSource_.load(std::memory_order_relaxed)) {
        if (SourceBuffer* s = pendingSource_.exchange(nullptr, std::memory_order_acq_rel)) {
            SourceBuffer* old = currentSource_;
            currentSource_ = s;
            gs_engine_swap_source(e_, s->samples.data(), (int)s->samples.size(), &s->slicer);
            if (old) retiredSources_.push(old);
        }
    }
    if (retiredImages_.hasSpace() && pendingImage_.load(std::memory_order_relaxed)) {
        if (ImageGrid* g = pendingImage_.exchange(nullptr, std::memory_order_acq_rel)) {
            ImageGrid* old = currentImage_;
            currentImage_ = g;
            gs_img_set_grid(&e_->img, g->luma.data());
            if (old) retiredImages_.push(old);
        }
    }
}

void Engine::setTarget(int id, float value) noexcept
{
    if (id < 0 || id >= P_COUNT) return;
    target_[(size_t)id] = gs_param_clamp(id, value);
}

void Engine::pitchBend(int value14) noexcept
{
    value14 = std::clamp(value14, 0, 16383);
    bendTarget_ = (float)(value14 - 8192) / 8192.0f * 2.0f;
}

void Engine::updateHarmony() noexcept
{
    int padMask = 0;
    for (int d = 0; d < 7; ++d)
        if (current_[(size_t)(P_DEG1 + d)] > 0.5f) padMask |= 1 << d;
    if (!padMask) padMask = 1;
    const int key = iround(current_[P_KEY]);
    const int scale = iround(current_[P_SCALE]);
    const int mode = iround(current_[P_MIDI_MODE]);
    const bool latch = current_[P_LATCH] > 0.5f;
    const bool padHeld = uiPadHeld_.load(std::memory_order_relaxed);
    if (clearLatch_.exchange(false, std::memory_order_relaxed)) latchedValid_ = false;

    if (midi_.anyHeld()) {
        latched_ = midi_.compute(key, scale, mode);
        latchedValid_ = true;
        latchedPadMask_ = padMask;
        gate_ = true;
        midiMask_ = latched_.degreeMask;
    } else {
        midiMask_ = 0;
        if (latchedValid_ && padMask != latchedPadMask_) latchedValid_ = false;   // clicking a pad takes over
        if (!latch && padHeld) latchedValid_ = false;
        gate_ = latch || padHeld;
    }
    if (latchedValid_) {
        effMask_ = latched_.degreeMask;
        effOctave_ = latched_.octave;
        effKey_ = latched_.rootKey >= 0 ? latched_.rootKey : key;
        const float amt = current_[P_VEL_AMT];
        velGain_ = 1.0f - amt * (1.0f - latched_.velocity);
    } else {
        effMask_ = padMask;
        effOctave_ = iround(current_[P_OCTAVE]);
        effKey_ = key;
        velGain_ = 1.0f;
    }
}

void Engine::runSlice(float* l, float* r, int n) noexcept
{
    const float k = 1.0f - std::exp(-(float)n / (kSmoothSeconds * (float)sampleRate_));
    for (int i = 0; i < P_COUNT; ++i) {
        float& c = current_[(size_t)i];
        const float t = target_[(size_t)i];
        if (snapParams_ || !GS_PARAMS[i].smooth) c = t;
        else {
            c += (t - c) * k;
            if (std::abs(t - c) < 1e-6f) c = t;
        }
    }
    if (snapParams_) { modCur_ = modTarget_; bendCur_ = bendTarget_; }
    modCur_ += (modTarget_ - modCur_) * k;
    bendCur_ += (bendTarget_ - bendCur_) * k;
    snapParams_ = false;

    updateHarmony();

    float* p = e_->p;
    std::memcpy(p, current_.data(), sizeof(float) * P_COUNT);
    p[P_OCTAVE] = (float)effOctave_;
    p[P_KEY] = (float)effKey_;
    p[P_GRAIN_LVL] *= velGain_;
    p[P_ARP_LEVEL] *= velGain_;
    const int dest = gs_mod_dest_param(iround(current_[P_MOD_DEST]));
    if (dest >= 0 && modCur_ > 1e-4f)
        p[dest] = gs_param_from_norm(dest, gs_param_to_norm(dest, p[dest]) + modCur_);

    e_->degree_mask = effMask_;
    e_->gate = gate_ ? 1 : 0;
    e_->bend_semis = bendCur_;
    gs_engine_process(e_, l, r, n);
}

void Engine::process(float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0) return;
    if (!e_) {
        std::memset(left, 0, sizeof(float) * (size_t)numSamples);
        std::memset(right, 0, sizeof(float) * (size_t)numSamples);
        return;
    }
    swapPending();

    e_->playing = playing_ ? 1 : 0;
    gs_engine_set_host(e_, host_.playing ? 1 : 0, host_.ppq, host_.barStartPpq);

    const bool want = previewWanted_.load(std::memory_order_relaxed);
    if (want && !previewWas_) { e_->preview_on = 1; e_->preview_pos = -1; }
    if (!want) e_->preview_on = 0;
    e_->preview_loop = target_[P_PREVIEW_LOOP] > 0.5f ? 1 : 0;
    e_->img.scrub = scrub_.load(std::memory_order_relaxed);

    for (int off = 0; off < numSamples;) {
        const int c = std::min(kSlice, numSamples - off);
        runSlice(left + off, right + off, c);
        off += c;
    }

    if (want && !e_->preview_on) previewWanted_.store(false);   // one-shot audition reached OUT
    previewWas_ = e_->preview_on != 0;
    publishUi();
}

void Engine::publishUi() noexcept
{
    constexpr auto rx = std::memory_order_relaxed;
    const GsEngine& e = *e_;
    ui_.peakL.store(std::max(ui_.peakL.load(rx), e.peak_l), rx);   // editor resets after reading
    ui_.peakR.store(std::max(ui_.peakR.load(rx), e.peak_r), rx);
    ui_.compGr.store(e.comp_gr, rx);
    ui_.playhead.store(e.ui_playhead, rx);
    ui_.numGrains.store(e.ui_ngrains, rx);
    for (int i = 0; i < e.ui_ngrains; ++i) {
        ui_.grainPos[(size_t)i].store(e.ui_grain_pos[i], rx);
        ui_.grainAmp[(size_t)i].store(e.ui_grain_amp[i], rx);
    }
    ui_.previewPos.store(e.ui_preview_pos, rx);
    const float inv = e.src_len > 0 ? 1.0f / (float)e.src_len : 0.0f;
    ui_.regionStart.store((float)e.region_s * inv, rx);
    ui_.regionEnd.store((float)e.region_e * inv, rx);
    ui_.numSlices.store(e.nslices, rx);
    for (int i = 0; i <= e.nslices && i <= GS_MAX_SLICES; ++i) ui_.bounds[(size_t)i].store((float)e.bounds[i] * inv, rx);
    ui_.imgPhase.store(e.img.ui_phase, rx);
    ui_.imgMonoY.store(e.img.ui_mono_y, rx);
    ui_.imgNumNotes.store(e.img.ui_nnotes, rx);
    for (int i = 0; i < e.img.ui_nnotes && i < IMG_MAX_NOTES; ++i) ui_.imgAmp[(size_t)i].store(e.img.ui_amp[i], rx);
    ui_.arpStep.store(e.arp.ui_step, rx);
    ui_.arpIdx.store(e.arp.ui_idx, rx);
    ui_.arpNumNotes.store(e.arp.ui_nnotes, rx);
    ui_.arpTrig.store(e.arp.ui_trig, rx);
    for (int i = 0; i < e.arp.ui_nnotes && i < ARP_MAX_NOTES; ++i) ui_.arpNotes[(size_t)i].store(e.arp.ui_notes[i], rx);
    for (int i = 0; i < GS_LIVE_ENV; ++i) ui_.liveEnv[(size_t)i].store(e.live_env[i], rx);
    ui_.liveHead.store(e.live_len > 0 ? (float)e.live_w / (float)e.live_len : 0.0f, rx);
    ui_.degreeMask.store(effMask_, rx);
    ui_.midiMask.store(midiMask_, rx);
    ui_.octave.store(effOctave_, rx);
    ui_.key.store(effKey_, rx);
    ui_.gate.store(gate_, rx);
    ui_.running.store(e.run_state != 0 || e.playing != 0, rx);
    ui_.bpm.store(e.p[P_BPM], rx);
    ui_.numVoices.store(e.nvoices, rx);
    for (int i = 0; i < e.nvoices && i < GS_MAX_VOICES; ++i) ui_.voices[(size_t)i].store(e.voice_tgt[i], rx);
}

} // namespace madcap
