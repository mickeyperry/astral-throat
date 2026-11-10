#include "PluginProcessor.h"
#include "PluginEditor.h"

using Range = juce::NormalisableRange<float>;

static Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout AstralThroatProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&l] (const char* id, const char* name, Range r, float def)
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def));
    };

    f ("vowel", "Vowel", Range (0, 1), 0.5f);
    f ("throat", "Throat Size", Range (0.6f, 1.6f), 1.0f);
    f ("tension", "Tension", Range (0, 1), 0.5f);
    f ("breath", "Breath", Range (0, 1), 0.12f);
    f ("glide", "Glide", skewed (0.0f, 1.5f, 0.2f), 0.12f);
    f ("vibrato", "Vibrato", Range (0, 1), 0.35f);

    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "voices", 1 }, "Voices", 1, 5, 3));
    f ("detune", "Detune", Range (0, 1), 0.3f);
    f ("overtone", "Overtone", Range (0, 1), 0.25f);
    f ("harmonic", "Harmonic", Range (4, 16), 8.0f);
    f ("attack", "Attack", skewed (0.002f, 2.0f, 0.15f), 0.06f);
    f ("release", "Release", skewed (0.02f, 4.0f, 0.6f), 0.5f);

    f ("mouthRate", "Mouth Rate", skewed (0.02f, 8.0f, 0.6f), 0.25f);
    f ("mouthDepth", "Mouth Depth", Range (0, 1), 0.2f);
    f ("chaos", "Chaos", Range (0, 1), 0.15f);
    f ("drive", "Drive", Range (0, 1), 0.1f);
    f ("swirl", "Swirl", Range (0, 1), 0.25f);
    f ("gain", "Gain", Range (-24, 12), 0.0f);

    f ("delayTime", "Delay Time", skewed (40.0f, 2000.0f, 400.0f), 380.0f);
    f ("feedback", "Feedback", Range (0, 0.95f), 0.45f);
    f ("spiral", "Spiral", Range (-1, 1), 0.1f);
    f ("delayMix", "Delay Mix", Range (0, 1), 0.3f);
    f ("revSize", "Reverb Size", Range (0, 1), 0.7f);
    f ("revMix", "Reverb Mix", Range (0, 1), 0.3f);

    f ("trip", "Trip", Range (0, 1), 0.0f);
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "drone", 1 }, "Drone", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sync", 1 }, "Delay Sync",
               juce::StringArray { "Free", "1/2", "1/4", "1/8.", "1/8", "1/8T", "1/16" }, 0));
    return l;
}

AstralThroatProcessor::AstralThroatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool AstralThroatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AstralThroatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void AstralThroatProcessor::handleMidi (const juce::MidiMessage& m, bool drone)
{
    if (m.isNoteOn())                       synth.noteOn (m.getNoteNumber(), m.getFloatVelocity(), drone);
    else if (m.isNoteOff())               { if (! drone) synth.noteOff (m.getNoteNumber()); }
    else if (m.isPitchWheel())              synth.setPitchBend (((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f);
    else if (m.isControllerOfType (1))      modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
}

void AstralThroatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (buffer.getNumChannels() < 2 || n == 0) return;

    const auto clamp01 = [] (float v) { return juce::jlimit (0.0f, 1.0f, v); };
    const float blockSec = (float) n / (float) sr;
    const float trip = get ("trip");
    const float chaos = clamp01 (get ("chaos") + 0.6f * trip);
    const float mouthDepth = clamp01 (get ("mouthDepth") + 0.5f * trip);

    // Modulators
    mouthPhase += get ("mouthRate") * (1.0f + trip) * blockSec;
    mouthPhase -= std::floor (mouthPhase);
    slowPhase += 0.07f * blockSec;
    slowPhase -= std::floor (slowPhase);
    lorenz.advance ((double) blockSec * (0.35 + 1.4 * (double) chaos));
    const float mouthLfo = std::sin (astral::kTwoPi * mouthPhase);
    const float cx = lorenz.nx(), cy = lorenz.ny(), cz = lorenz.nz();

    // Voice
    astral::ThroatSynth::Params sp;
    sp.vowel = clamp01 (get ("vowel") + 0.6f * modWheel + 0.5f * mouthDepth * mouthLfo + 0.35f * chaos * cx);
    sp.throat = get ("throat");
    sp.tension = get ("tension");
    sp.breath = get ("breath");
    sp.glide = get ("glide");
    sp.vibrato = get ("vibrato");
    sp.voices = juce::roundToInt (get ("voices"));
    sp.detune = get ("detune");
    sp.overtone = clamp01 (get ("overtone") + 0.6f * trip);
    sp.harmonic = juce::jlimit (3.0f, 20.0f, get ("harmonic") + 4.0f * chaos * cy
                                                 + 3.0f * trip * std::sin (astral::kTwoPi * slowPhase));
    sp.attack = get ("attack");
    sp.release = get ("release");
    synth.setParams (sp);

    const bool drone = get ("drone") > 0.5f;
    if (lastDrone && ! drone) synth.allNotesOff();
    lastDrone = drone;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { synth.render (L + pos, R + pos, at - pos); pos = at; }
        handleMidi (meta.getMessage(), drone);
    }
    if (pos < n) synth.render (L + pos, R + pos, n - pos);

    // Effects
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto position = ph->getPosition())
            if (auto b = position->getBpm())
                bpm = juce::jmax (20.0, *b);

    static constexpr float beats[] = { 0.0f, 2.0f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
    const int sync = juce::jlimit (0, 6, juce::roundToInt (get ("sync")));
    const float spiral = get ("spiral");
    const float spiralDir = spiral < 0.0f ? -1.0f : 1.0f;

    astral::PsyFx::Params fp;
    fp.drive = get ("drive");
    fp.swirl = clamp01 (get ("swirl") + 0.7f * trip);
    fp.delaySec = sync == 0 ? get ("delayTime") * 0.001f
                            : juce::jmin (2.4f, beats[sync] * 60.0f / (float) bpm);
    fp.feedback = juce::jmin (0.93f, get ("feedback") + 0.3f * trip);
    fp.spiralHz = spiralDir * (spiral * spiral * 40.0f + 6.0f * trip);
    fp.delayMix = clamp01 (get ("delayMix") + 0.35f * trip);
    fp.wobble = 0.012f * chaos * cz;
    fp.revSize = clamp01 (get ("revSize") + 0.25f * trip);
    fp.revMix = clamp01 (get ("revMix") + 0.4f * trip);
    fp.gain = juce::Decibels::decibelsToGain (get ("gain"));
    fx.process (buffer, fp);

    uiLevel = synth.getEnv();
    uiVowel = sp.vowel;
    uiHarmonic = sp.harmonic;
    uiOvertone = sp.overtone;
    uiChaosX = cx;
    uiChaosY = cy;
    uiTrip = trip;
}

juce::AudioProcessorEditor* AstralThroatProcessor::createEditor()
{
    return new AstralThroatEditor (*this);
}

void AstralThroatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AstralThroatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AstralThroatProcessor();
}
