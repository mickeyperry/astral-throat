#pragma once
#include "PluginProcessor.h"

//==============================================================================
class PsyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PsyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
// The animated oracle. Drag inside it: X = vowel, Y = overtone harmonic.
class OracleView : public juce::Component, private juce::Timer
{
public:
    explicit OracleView (AstralThroatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setFromMouse (const juce::MouseEvent&);

    AstralThroatProcessor& proc;
    juce::RangedAudioParameter* vowelParam = nullptr;
    juce::RangedAudioParameter* harmParam = nullptr;
    float t = 0, level = 0, vowel = 0.5f, overtone = 0, harmonic = 8, cx = 0, cy = 0, trip = 0;
};

//==============================================================================
class AstralThroatEditor : public juce::AudioProcessorEditor
{
public:
    explicit AstralThroatEditor (AstralThroatProcessor&);
    ~AstralThroatEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    Knob& addKnob (const char* id, const char* name, float hue);

    AstralThroatProcessor& proc;
    PsyLookAndFeel lnf;
    OracleView oracle;
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* tripKnob = nullptr;
    juce::TextButton droneButton { "DRONE" };
    juce::ComboBox syncBox;
    std::unique_ptr<APVTS::ButtonAttachment> droneAttachment;
    std::unique_ptr<APVTS::ComboBoxAttachment> syncAttachment;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AstralThroatEditor)
};
