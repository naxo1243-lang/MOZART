#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <vector>
#include "Engine.h"
#include "PreviewSynth.h"

class MozartProcessor : public juce::AudioProcessor
{
public:
    struct Settings
    {
        int genre = 0, scale = 0, root = 0, octave = 3, bpm = 120, bars = 32;
        int swing = 0;                 // 0 auto, 1 on, 2 off
        int density = 60, energy = 50;
        bool humanize = true;
        juce::String seed;             // what the user typed ("" = random each time)
    };

    MozartProcessor();
    ~MozartProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mozart Generative MIDI"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-thread API ----
    Settings settings;
    std::shared_ptr<const mozart::Song> getSong() const { return song; }
    void regenerate (const juce::String& seedOverride = {});
    void applyGenreDefaults (int genre);
    void requestPreviewReset() { previewReset = true; }

    // ---- shared with audio thread ----
    std::atomic<bool> previewPlaying { false };
    std::atomic<bool> internalSound { false };
    std::atomic<bool> roleEnabled[mozart::NumRoles];
    std::atomic<int>  bpmAtomic { 120 };
    std::atomic<int>  currentBar { 0 };
    std::atomic<bool> transportRunning { false };

private:
    juce::SpinLock songLock;
    std::shared_ptr<const mozart::Song> song;
    juce::String resolvedSeed;

    std::atomic<bool> flushRequested { false };
    std::atomic<bool> previewReset { false };

    // audio-thread state
    struct Pending { int offs; bool on; int ch, note, vel; };
    std::vector<Pending> pending;
    bool held[16][128] {};
    PreviewSynth synth;
    double previewTick = 0.0, expectedTick = 0.0;
    int lastMode = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MozartProcessor)
};
