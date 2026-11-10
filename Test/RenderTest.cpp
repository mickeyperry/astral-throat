// Headless check: plays a phrase through the processor, writes a WAV, reports
// level / NaN stats, and snapshots the editor to a PNG.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void setParam (juce::AudioProcessor& p, const juce::String& id, float normalised)
{
    for (auto* param : p.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            if (withId->paramID == id)
                withId->setValueNotifyingHost (normalised);
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? juce::String (argv[1]) : juce::String (".");
    const double sr = 48000.0;
    const int block = 512;
    int failures = 0;

    for (int pass = 0; pass < 2; ++pass)
    {
        std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
        proc->setPlayConfigDetails (0, 2, sr, block);
        proc->prepareToPlay (sr, block);
        if (pass == 1)
        {
            setParam (*proc, "trip", 1.0f);
            setParam (*proc, "feedback", 1.0f);
            setParam (*proc, "spiral", 1.0f);
            setParam (*proc, "chaos", 1.0f);
            setParam (*proc, "voices", 1.0f);
        }

        const int total = (int) (sr * 8.0);
        juce::AudioBuffer<float> out (2, total), buf (2, block);
        struct Ev { double time; int note; bool on; };
        const Ev events[] = { { 0.05, 45, true }, { 1.5, 52, true }, { 1.6, 45, false },
                              { 3.0, 57, true }, { 3.1, 52, false }, { 5.0, 57, false } };
        bool bad = false;
        double sumSq = 0, sumSqNote = 0, sumSqTail = 0;
        float peak = 0;

        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : events)
            {
                const int at = (int) (e.time * sr);
                if (at >= pos && at < pos + n)
                    midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, 0.9f)
                                        : juce::MidiMessage::noteOff (1, e.note), at - pos);
            }
            proc->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                out.copyFrom (ch, pos, buf, ch, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) bad = true;
                    peak = juce::jmax (peak, std::abs (v));
                    sumSq += v * v;
                    (pos < (int) (sr * 5.0) ? sumSqNote : sumSqTail) += v * v;
                }
            }
        }

        const auto db = [] (double ms) { return 10.0 * std::log10 (ms + 1e-12); };
        const double rmsAll = db (sumSq / (2.0 * total));
        const double rmsNote = db (sumSqNote / (2.0 * sr * 5.0));
        const double rmsTail = db (sumSqTail / (2.0 * sr * 3.0));
        std::cout << "pass " << pass << (pass ? " (trip maxed)" : " (defaults)")
                  << ": peak " << juce::Decibels::gainToDecibels (peak) << " dBFS, rms " << rmsAll
                  << " dB, held-notes rms " << rmsNote << " dB, tail rms " << rmsTail
                  << " dB, non-finite: " << (bad ? "YES" : "no") << std::endl;
        if (bad || rmsNote < -40.0 || peak > 1.0f) ++failures;

        juce::File wav (juce::File::getCurrentWorkingDirectory().getChildFile (outDir)
                            .getChildFile ("render_pass" + juce::String (pass) + ".wav"));
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (wav.createOutputStream()))
        {
            if (auto* writer = fmt.createWriterFor (stream.get(), sr, 2, 16, {}, 0))
            {
                stream.release();
                std::unique_ptr<juce::AudioFormatWriter> w (writer);
                w->writeFromAudioSampleBuffer (out, 0, total);
            }
        }
        std::cout << "  wrote " << wav.getFullPathName() << std::endl;

        if (pass == 1)
        {
            // Snapshot the editor mid-phrase state.
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 50, 0.9f), 0);
            for (int i = 0; i < 40; ++i) { buf.setSize (2, block, false, false, true); proc->processBlock (buf, midi); midi.clear(); }
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditorAndMakeActive());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File png (juce::File::getCurrentWorkingDirectory().getChildFile (outDir).getChildFile ("editor.png"));
            png.deleteFile();
            if (auto os = png.createOutputStream())
            {
                juce::PNGImageFormat pf;
                pf.writeImageToStream (img, *os);
            }
            std::cout << "  wrote " << png.getFullPathName() << " (" << img.getWidth() << "x" << img.getHeight() << ")" << std::endl;
        }
    }

    std::cout << (failures == 0 ? "RESULT: OK" : "RESULT: FAILED") << std::endl;
    return failures;
}
