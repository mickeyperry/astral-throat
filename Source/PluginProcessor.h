#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dsp.h"

class AstralThroatProcessor : public juce::AudioProcessor
{
public:
    AstralThroatProcessor();
    ~AstralThroatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Astral Throat"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // Published for the editor's animation.
    std::atomic<float> uiLevel { 0 }, uiVowel { 0.5f }, uiHarmonic { 8 }, uiOvertone { 0 },
                       uiChaosX { 0 }, uiChaosY { 0 }, uiTrip { 0 };

private:
    void handleMidi (const juce::MidiMessage&, bool drone);
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    astral::ThroatSynth synth;
    astral::PsyFx fx;
    astral::Lorenz lorenz;
    double sr = 48000.0;
    float mouthPhase = 0, slowPhase = 0, modWheel = 0;
    bool lastDrone = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AstralThroatProcessor)
};
