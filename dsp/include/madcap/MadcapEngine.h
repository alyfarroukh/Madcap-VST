// C++20 wrapper around the Madcap C engine (dsp/src/engine.c). No JUCE dependency.
//
// Threading model
//  - prepare(), setSourceNow(), setImageNow(): not concurrent with process() (host contract).
//  - postSource() / postImage(): any non-audio thread; lock-free single-slot mailboxes.
//    The audio thread swaps the pointer in at the start of the next process() call and
//    pushes the old buffer onto a lock-free retire queue; collectGarbage() frees it later.
//  - setTarget(), noteOn() ... process(): audio thread only.
//  - ui(): atomics written by the audio thread, read by the editor at 60 fps.
#pragma once

#include "madcap/MidiChord.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

extern "C" {
#include "engine.h"
}

namespace madcap {

struct SourceBuffer {
    std::vector<float> samples;   // mono, at the engine sample rate
    GsSlicer slicer{};            // transient analysis (filled by analyse())
    void analyse(int sampleRate);
};

struct ImageGrid {
    std::vector<float> luma;      // IMG_ROWS x IMG_COLS, row 0 = top
};

struct HostInfo {
    bool playing = false;
    double ppq = -1.0;            // quarter notes since song start, -1 = unknown
    double barStartPpq = -1.0;
};

// Everything the editor animates. Written by the audio thread (relaxed atomics).
struct UiSnapshot {
    std::atomic<float> peakL{0}, peakR{0}, compGr{0}, playhead{0};
    std::atomic<int> numGrains{0};
    std::array<std::atomic<float>, GS_UI_GRAINS> grainPos{}, grainAmp{};
    std::atomic<float> previewPos{-1};
    std::atomic<float> regionStart{0}, regionEnd{1};       // normalised to the source
    std::array<std::atomic<float>, GS_MAX_SLICES + 1> bounds{};
    std::atomic<int> numSlices{1};
    std::atomic<float> imgPhase{0}, imgMonoY{-1};
    std::atomic<int> imgNumNotes{0};
    std::array<std::atomic<float>, IMG_MAX_NOTES> imgAmp{};
    std::atomic<int> arpStep{0}, arpIdx{0}, arpNumNotes{0}, arpTrig{0};
    std::array<std::atomic<float>, ARP_MAX_NOTES> arpNotes{};
    std::array<std::atomic<float>, GS_LIVE_ENV> liveEnv{};
    std::atomic<float> liveHead{0};
    std::atomic<int> degreeMask{1};     // effective chord degrees
    std::atomic<int> midiMask{0};       // degrees selected by held MIDI notes (0 = none)
    std::atomic<int> octave{0};
    std::atomic<int> key{0};            // effective key (ROOT mode may override)
    std::atomic<bool> gate{true};
    std::atomic<bool> running{true};
    std::atomic<float> bpm{120};
    std::atomic<int> numVoices{1};
    std::array<std::atomic<float>, GS_MAX_VOICES> voices{};   // chord semitones
};

class Engine {
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // ---- setup (not concurrent with process) --------------------------------------------
    bool prepare(double sampleRate);
    double sampleRate() const noexcept { return sampleRate_; }
    int latencySamples() const noexcept;
    void setSourceNow(std::unique_ptr<SourceBuffer> src);
    void setImageNow(std::unique_ptr<ImageGrid> img);
    void reset();   // panic: stop all voices and tails

    // ---- any non-audio thread ------------------------------------------------------------
    void postSource(std::unique_ptr<SourceBuffer> src);
    void postImage(std::unique_ptr<ImageGrid> img);
    void collectGarbage();
    void setPreview(bool on) noexcept { previewWanted_.store(on); }
    bool isPreviewing() const noexcept { return previewWanted_.load(); }
    void setScrub(float pos01OrNegative) noexcept { scrub_.store(pos01OrNegative); }
    void setUiPadHeld(bool held) noexcept { uiPadHeld_.store(held); }
    void releaseLatch() noexcept { clearLatch_.store(true); }   // a chord pad click wins over a latched MIDI chord

    // ---- audio thread --------------------------------------------------------------------
    void setTarget(int id, float value) noexcept;
    void setPlaying(bool playing) noexcept { playing_ = playing; }
    void setHost(const HostInfo& h) noexcept { host_ = h; }
    void noteOn(int note, float velocity01) noexcept { midi_.noteOn(note, velocity01); }
    void noteOff(int note) noexcept { midi_.noteOff(note); }
    void allNotesOff() noexcept { midi_.reset(); }
    void pitchBend(int value14) noexcept;          // 0..16383, centre 8192, +/- 2 semitones
    void modWheel(float value01) noexcept { modTarget_ = value01; }
    void process(float* left, float* right, int numSamples) noexcept;

    const UiSnapshot& ui() const noexcept { return ui_; }
    // Meter peak since the last call (editor thread), then reset.
    float takePeak(int channel) noexcept { return (channel ? ui_.peakR : ui_.peakL).exchange(0.0f); }

    // Test / debug access (audio thread or when idle only).
    const GsEngine* raw() const noexcept { return e_; }

private:
    void swapPending() noexcept;
    void runSlice(float* l, float* r, int n) noexcept;
    void updateHarmony() noexcept;
    void publishUi() noexcept;

    GsEngine* e_ = nullptr;
    double sampleRate_ = 0.0;

    // lock-free hand-off
    template <typename T, int N>
    struct RetireRing {   // single producer (audio) / single consumer (collectGarbage)
        std::array<std::atomic<T*>, N> slots{};
        std::atomic<unsigned> write{0}, read{0};
        bool hasSpace() const noexcept { return write.load(std::memory_order_relaxed) - read.load(std::memory_order_acquire) < (unsigned)N; }
        void push(T* p) noexcept
        {
            unsigned w = write.load(std::memory_order_relaxed);
            slots[w % N].store(p, std::memory_order_relaxed);
            write.store(w + 1, std::memory_order_release);
        }
        T* pop() noexcept
        {
            unsigned r = read.load(std::memory_order_relaxed);
            if (r == write.load(std::memory_order_acquire)) return nullptr;
            T* p = slots[r % N].load(std::memory_order_relaxed);
            read.store(r + 1, std::memory_order_release);
            return p;
        }
    };
    std::atomic<SourceBuffer*> pendingSource_{nullptr};
    std::atomic<ImageGrid*> pendingImage_{nullptr};
    SourceBuffer* currentSource_ = nullptr;
    ImageGrid* currentImage_ = nullptr;
    RetireRing<SourceBuffer, 32> retiredSources_;
    RetireRing<ImageGrid, 32> retiredImages_;

    // parameters
    std::array<float, P_COUNT> target_{};
    std::array<float, P_COUNT> current_{};
    bool snapParams_ = true;
    float modTarget_ = 0.0f, modCur_ = 0.0f;
    float bendTarget_ = 0.0f, bendCur_ = 0.0f;

    // harmony / gate
    MidiChordState midi_;
    MidiChordState::Chord latched_;
    bool latchedValid_ = false;
    int latchedPadMask_ = 0;
    int effMask_ = 1, effOctave_ = 0, effKey_ = 0, midiMask_ = 0;
    bool gate_ = true;
    float velGain_ = 1.0f;

    bool playing_ = true;
    HostInfo host_;
    std::atomic<bool> previewWanted_{false};
    bool previewWas_ = false;
    std::atomic<float> scrub_{-1.0f};
    std::atomic<bool> uiPadHeld_{false};
    std::atomic<bool> clearLatch_{false};

    UiSnapshot ui_;
};

} // namespace madcap
