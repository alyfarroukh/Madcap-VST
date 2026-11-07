#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "ImageStore.h"
#include "Parameters.h"
#include "SampleStore.h"
#include "madcap/MadcapEngine.h"

#include <array>
#include <atomic>

namespace madcap {

class MadcapProcessor : public juce::AudioProcessor,
                        private juce::Timer {
public:
    MadcapProcessor();
    ~MadcapProcessor() override;

    // ---- AudioProcessor ------------------------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    void processorLayoutsChanged() override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Madcap"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- used by the editor (message thread) -----------------------------------------------
    juce::AudioProcessorValueTreeState& state() noexcept { return apvts_; }
    juce::UndoManager& undoManager() noexcept { return undo_; }
    juce::RangedAudioParameter* param(int p) const noexcept { return params_[(size_t)p]; }
    float value(int p) const noexcept { return raw_[(size_t)p]->load(); }
    void setValue(int p, float v);                      // notifies the host, undoable
    Engine& engine() noexcept { return engine_; }
    SampleStore& sample() noexcept { return sample_; }
    ImageStore& image() noexcept { return image_; }
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }

    void loadSampleAsync(const juce::File& f);
    void loadImageAsync(const juce::File& f);
    void selectPoster(int index);
    void cropToCut();
    void useDemoSample();

    bool startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recActive_.load(); }
    float recordingSeconds() const noexcept;
    float recordingPeak() const noexcept { return recPeak_.load(); }
    float takeInputPeak() noexcept { return inPeak_.exchange(0.0f); }   // editor meter
    int recordChannel() const noexcept { return recChannel_.load(); }   // 0 = all, 1 = in 1, 2 = in 2
    void setRecordChannel(int c) noexcept { recChannel_.store(juce::jlimit(0, 2, c)); }
    // What the host actually connected to Madcap's inputs (updated by the audio thread).
    int inputBusChannels(int bus) const noexcept { return inBusChannels_[(size_t)(bus & 1)].load(); }
    unsigned inputSignalTicks() const noexcept { return signalTicks_.load(); }
    bool hasRecordInput() const noexcept { return getTotalNumInputChannels() > 0; }

    void tapTempo();
    void randomize();
    std::array<bool, PAGE_COUNT>& randomLocks() noexcept { return locks_; }

    // presets
    juce::String currentPresetName() const;
    void loadFactoryPreset(int index);
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::File& f);

    // status line for the editor (errors from background loads etc.)
    juce::String takeStatusMessage();
    double hostBpm() const noexcept { return hostBpm_.load(); }
    bool hostIsPlaying() const noexcept { return hostPlaying_.load(); }
    int sampleRateInt() const noexcept { return (int)std::lround(currentRate_.load()); }

    // editor size persistence
    int editorWidth = 960, editorHeight = 544;

private:
    static BusesProperties makeBuses();
    void timerCallback() override;
    void installSampleIntoEngine(bool now);
    void installImageIntoEngine(bool now);
    void setStatus(const juce::String& s);
    void applyPresetValues(const std::vector<std::pair<int, double>>& values);
    std::unique_ptr<juce::XmlElement> createStateXml(bool includeMedia);
    void restoreStateXml(const juce::XmlElement& xml);
    void handleMidi(const juce::MidiMessage& m);

    juce::UndoManager undo_;
    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, P_COUNT> raw_{};
    std::array<juce::RangedAudioParameter*, P_COUNT> params_{};

    Engine engine_;
    SampleStore sample_;
    ImageStore image_;
    juce::ThreadPool loader_{ juce::ThreadPoolOptions{}.withThreadName("Madcap loader").withNumberOfThreads(1) };
    std::atomic<double> currentRate_{0.0};
    juce::CriticalSection prepareLock_;   // engine source rebuilds vs. prepareToPlay (never on the audio thread)

    // scratch for rendering (allocated in prepareToPlay)
    juce::AudioBuffer<float> scratch_;

    // recording (sidechain / microphone)
    std::vector<float> recBuf_;
    std::atomic<bool> recActive_{false};
    std::atomic<bool> recWriting_{false};
    std::atomic<int> recLen_{0};
    std::atomic<float> recPeak_{0.0f};
    std::atomic<float> inPeak_{0.0f};
    std::atomic<int> recChannel_{0};
    std::array<std::atomic<int>, 2> inBusChannels_{};   // channels the host connected: 0 = main input, 1 = sidechain
    std::atomic<unsigned> signalTicks_{0};
    std::atomic<bool> gridJobPending_{false};              // counts blocks with input above -60 dBFS
    double recRate_ = 48000.0;
    bool recWasActive_ = false;          // message thread

    // host / tempo
    std::atomic<double> hostBpm_{0.0};
    std::atomic<bool> hostPlaying_{false};
    juce::Array<double> taps_;
    double lastTap_ = 0.0;

    int currentProgram_ = 0;
    juce::String presetName_ { "Init" };
    std::array<bool, PAGE_COUNT> locks_{};

    juce::CriticalSection statusLock_;
    juce::String status_;

    JUCE_DECLARE_WEAK_REFERENCEABLE(MadcapProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MadcapProcessor)
};

} // namespace madcap
