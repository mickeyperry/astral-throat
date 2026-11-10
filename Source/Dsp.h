#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

namespace astral
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

//==============================================================================
// Zero-delay-feedback state variable filter (bandpass normalised to unity peak).
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;

    void set (float freq, float q, float sr)
    {
        freq = juce::jlimit (20.0f, sr * 0.45f, freq);
        g = std::tan (kPi * freq / sr);
        k = 1.0f / juce::jmax (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float bandpass (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return k * v1;
    }

    void reset() { ic1 = ic2 = 0; }
};

//==============================================================================
// Vowel formant data (tenor), ordered along the mouth path U - O - A - E - I.
struct VowelTable
{
    static constexpr int numVowels = 5, numFormants = 5;

    static constexpr float freq[numVowels][numFormants] = {
        { 350, 600, 2700, 2900, 3300 },   // U
        { 400, 800, 2600, 2800, 3000 },   // O
        { 650, 1080, 2650, 2900, 3250 },  // A
        { 400, 1700, 2600, 3200, 3580 },  // E
        { 290, 1870, 2800, 3250, 3540 } };// I
    static constexpr float ampDb[numVowels][numFormants] = {
        { 0, -20, -17, -14, -26 },
        { 0, -10, -12, -12, -26 },
        { 0, -6, -7, -8, -22 },
        { 0, -14, -12, -14, -20 },
        { 0, -15, -18, -20, -30 } };
    static constexpr float bw[numVowels][numFormants] = {
        { 40, 60, 100, 120, 120 },
        { 40, 80, 100, 120, 120 },
        { 80, 90, 120, 130, 140 },
        { 70, 80, 100, 120, 120 },
        { 40, 90, 100, 120, 120 } };

    static void get (float vowel01, float* f, float* a, float* b)
    {
        const float pos = juce::jlimit (0.0f, 1.0f, vowel01) * (numVowels - 1);
        const int i0 = juce::jmin ((int) pos, numVowels - 2);
        const float t = pos - (float) i0;
        for (int i = 0; i < numFormants; ++i)
        {
            f[i] = freq[i0][i] + t * (freq[i0 + 1][i] - freq[i0][i]);
            b[i] = bw[i0][i] + t * (bw[i0 + 1][i] - bw[i0][i]);
            a[i] = juce::Decibels::decibelsToGain (ampDb[i0][i] + t * (ampDb[i0 + 1][i] - ampDb[i0][i]));
        }
    }
};

//==============================================================================
// Monophonic singing voice: glottal source -> parallel formants + overtone
// resonator, with a unison "choir" of slightly different throats.
class ThroatSynth
{
public:
    static constexpr int maxVoices = 5;

    struct Params
    {
        float vowel = 0.5f, throat = 1.0f, tension = 0.5f, breath = 0.1f;
        float glide = 0.1f, vibrato = 0.3f, detune = 0.3f;
        float overtone = 0.0f, harmonic = 8.0f, attack = 0.05f, release = 0.4f;
        int voices = 3;
    };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        adsr.setSampleRate (sampleRate);
        for (auto& v : voice)
        {
            v = {};
            v.phase = nextRand() * 0.5f + 0.5f;
        }
        vowelS = p.vowel; harmS = p.harmonic; throatS = p.throat;
        ctrl = 0; numHeld = 0;
        adsr.reset();
    }

    void setParams (const Params& np)
    {
        p = np;
        p.voices = juce::jlimit (1, maxVoices, p.voices);
        adsr.setParameters ({ p.attack, 0.0f, 1.0f, p.release });
    }

    void noteOn (int note, float vel, bool replaceHeld)
    {
        const bool fresh = ! adsr.isActive();
        if (replaceHeld) numHeld = 0;
        removeHeld (note);
        if (numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
        targetPitch = (float) note;
        if (fresh) { curPitch = targetPitch; vibFade = 0; }
        velocity = 0.4f + 0.6f * vel;
        adsr.noteOn();
    }

    void noteOff (int note)
    {
        removeHeld (note);
        if (numHeld > 0) targetPitch = (float) held[(size_t) (numHeld - 1)];
        else adsr.noteOff();
    }

    void allNotesOff() { numHeld = 0; adsr.noteOff(); }
    void setPitchBend (float semis) { bend = semis; }
    float getEnv() const { return envOut; }
    float getPitch() const { return curPitch; }

    // Adds into L / R.
    void render (float* L, float* R, int n)
    {
        for (int s = 0; s < n; ++s)
        {
            if (ctrl == 0) updateControl();
            ctrl = (ctrl + 1) & 15;

            const float env = adsr.getNextSample();
            envOut = env;
            if (env <= 0.0f && ! adsr.isActive()) continue;

            float l = 0, r = 0;
            for (int u = 0; u < p.voices; ++u)
            {
                auto& v = voice[(size_t) u];
                v.phase += v.inc;
                if (v.phase >= 1.0f) v.phase -= 1.0f;

                const float saw = 2.0f * v.phase - 1.0f - polyBlep (v.phase, v.inc);
                const float noise = nextRand() * p.breath * 1.5f * (0.3f + 0.7f * v.phase);
                v.lp += tiltCoef * (saw + noise - v.lp);

                float y = 0;
                for (int i = 0; i < VowelTable::numFormants; ++i)
                    y += amp[i] * v.form[(size_t) i].bandpass (v.lp);
                y += otGain * v.ot.bandpass (saw);

                l += y * v.gL;
                r += y * v.gR;
            }

            const float g = env * velocity * norm;
            L[s] += l * g;
            R[s] += r * g;
        }
    }

private:
    struct Voice
    {
        float phase = 0, inc = 0, lp = 0, drift = 0, driftTarget = 0, gL = 0.7f, gR = 0.7f;
        std::array<Svf, VowelTable::numFormants> form;
        Svf ot;
    };

    static float polyBlep (float t, float dt)
    {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float nextRand() // -1..1
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;
    }

    void updateControl()
    {
        const float dt = 16.0f / sr;

        const float gc = p.glide < 0.002f ? 1.0f : 1.0f - std::exp (-dt * 3.0f / p.glide);
        curPitch += (targetPitch - curPitch) * gc;

        vowelS += (p.vowel - vowelS) * 0.2f;
        harmS += (p.harmonic - harmS) * 0.12f;
        throatS += (p.throat - throatS) * 0.2f;

        vibFade = juce::jmin (1.0f, vibFade + dt / 0.45f);
        vibPhase += 5.4f * dt;
        if (vibPhase >= 1.0f) vibPhase -= 1.0f;
        const float vib = std::sin (kTwoPi * vibPhase) * p.vibrato * 0.4f * vibFade;
        const float base = curPitch + bend + vib;

        float F[5], A[5], B[5];
        VowelTable::get (vowelS, F, A, B);
        for (int i = 0; i < 5; ++i) amp[i] = A[i];

        const float fc = 200.0f + p.tension * p.tension * 9000.0f;
        tiltCoef = 1.0f - std::exp (-kTwoPi * fc / sr);
        otGain = p.overtone * 3.0f;
        norm = 3.0f / std::sqrt ((float) p.voices);

        for (int u = 0; u < p.voices; ++u)
        {
            auto& v = voice[(size_t) u];
            const float d = p.voices == 1 ? 0.0f : 2.0f * (float) u / (float) (p.voices - 1) - 1.0f;

            if ((nextRand() + 1.0f) < 0.06f) v.driftTarget = nextRand() * 0.07f;
            v.drift += (v.driftTarget - v.drift) * 0.02f;

            const float semis = base + d * p.detune * 0.35f + v.drift;
            const float f0 = 440.0f * std::pow (2.0f, (semis - 69.0f) / 12.0f);
            v.inc = juce::jmin (0.45f, f0 / sr);

            const float scale = throatS * (1.0f + d * 0.05f);
            for (int i = 0; i < 5; ++i)
            {
                float f = F[i] * scale;
                if (i == 0) f = juce::jmax (f, f0);
                v.form[(size_t) i].set (f, f / B[i], sr);
            }
            v.ot.set (juce::jmin (harmS * f0, sr * 0.4f), 28.0f, sr);

            const float ang = (d * 0.8f + 1.0f) * 0.25f * kPi;
            v.gL = std::cos (ang);
            v.gR = std::sin (ang);
        }
    }

    Params p;
    float sr = 48000.0f;
    std::array<Voice, maxVoices> voice;
    std::array<int, 32> held {};
    int numHeld = 0, ctrl = 0;
    float targetPitch = 57, curPitch = 57, bend = 0, velocity = 1, envOut = 0;
    float vowelS = 0.5f, harmS = 8, throatS = 1, vibPhase = 0, vibFade = 0;
    float amp[5] = { 1, 0, 0, 0, 0 }, tiltCoef = 0.2f, otGain = 0, norm = 1;
    uint32_t rng = 0x9E3779B9u;
    juce::ADSR adsr;
};

//==============================================================================
// Hilbert-pair frequency shifter (Niemitalo allpass coefficients).
class FreqShifter
{
public:
    FreqShifter()
    {
        const float c1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        const float c2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        for (int i = 0; i < 4; ++i) { p1[i].a = c1[i] * c1[i]; p2[i].a = c2[i] * c2[i]; }
    }

    void reset()
    {
        for (auto& s : p1) s.clear();
        for (auto& s : p2) s.clear();
        d1 = 0; phase = 0;
    }

    float process (float x, float phaseInc)
    {
        float i = x, q = x;
        for (auto& s : p1) i = s.process (i);
        for (auto& s : p2) q = s.process (q);
        const float iDel = d1;
        d1 = i;

        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase < 0.0f) phase += 1.0f;
        return iDel * std::cos (kTwoPi * phase) - q * std::sin (kTwoPi * phase);
    }

private:
    struct Section
    {
        float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float process (float x)
        {
            const float y = a * (x + y2) - x2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
        void clear() { x1 = x2 = y1 = y2 = 0; }
    };
    Section p1[4], p2[4];
    float d1 = 0, phase = 0;
};

//==============================================================================
// Drive -> phaser -> spiralling ping-pong delay -> reverb -> output.
class PsyFx
{
public:
    struct Params
    {
        float drive = 0, swirl = 0, delaySec = 0.38f, feedback = 0.4f, spiralHz = 0;
        float delayMix = 0.3f, wobble = 0, revSize = 0.7f, revMix = 0.3f, gain = 1.0f;
    };

    void prepare (double sampleRate, int blockSize)
    {
        sr = (float) sampleRate;
        const int len = (int) (sampleRate * 2.6) + 8;
        bufL.assign ((size_t) len, 0.0f);
        bufR.assign ((size_t) len, 0.0f);
        wp = 0;
        curDelay = 0.38f * sr;
        lpL = lpR = 0;
        shL.reset(); shR.reset();
        phaser.prepare ({ sampleRate, (juce::uint32) blockSize, 2 });
        phaser.reset();
        reverb.setSampleRate (sampleRate);
        reverb.reset();
        lpCoef = 1.0f - std::exp (-kTwoPi * 5200.0f / sr);
    }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int n = buffer.getNumSamples();
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        // Drive
        if (p.drive > 0.001f)
        {
            const float g = 1.0f + p.drive * 9.0f, comp = 1.0f / std::sqrt (g);
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::tanh (L[i] * g) * comp;
                R[i] = std::tanh (R[i] * g) * comp;
            }
        }

        // Phaser
        if (p.swirl > 0.001f)
        {
            phaser.setRate (0.08f + p.swirl * 0.9f);
            phaser.setDepth (0.85f);
            phaser.setCentreFrequency (900.0f);
            phaser.setFeedback (0.7f * p.swirl);
            phaser.setMix (0.6f * p.swirl);
            juce::dsp::AudioBlock<float> block (buffer);
            phaser.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // Spiral ping-pong delay
        const int len = (int) bufL.size();
        const float target = juce::jlimit (16.0f, (float) len - 8.0f, p.delaySec * sr * (1.0f + p.wobble));
        const float inc = p.spiralHz / sr;
        const float fb = juce::jlimit (0.0f, 0.95f, p.feedback);
        for (int i = 0; i < n; ++i)
        {
            curDelay += (target - curDelay) * 0.0004f;
            float rp = (float) wp - curDelay;
            if (rp < 0) rp += (float) len;
            const int i0 = (int) rp;
            const int i1 = i0 + 1 >= len ? 0 : i0 + 1;
            const float fr = rp - (float) i0;
            const float dl = bufL[(size_t) i0] + fr * (bufL[(size_t) i1] - bufL[(size_t) i0]);
            const float dr = bufR[(size_t) i0] + fr * (bufR[(size_t) i1] - bufR[(size_t) i0]);

            lpL += lpCoef * (shL.process (dr, inc) - lpL);
            lpR += lpCoef * (shR.process (dl, inc) - lpR);

            bufL[(size_t) wp] = 0.5f * (L[i] + R[i]) + fb * std::tanh (lpL);
            bufR[(size_t) wp] = fb * std::tanh (lpR);
            if (++wp >= len) wp = 0;

            L[i] += p.delayMix * dl;
            R[i] += p.delayMix * dr;
        }

        // Reverb (juce::Reverb scales dry x2 internally)
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, p.revSize);
        rp.damping = 0.35f;
        rp.width = 1.0f;
        rp.wetLevel = 0.45f * p.revMix;
        rp.dryLevel = 0.5f * (1.0f - 0.35f * p.revMix);
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);

        // Output gain + soft ceiling
        for (int i = 0; i < n; ++i)
        {
            L[i] = std::tanh (L[i] * p.gain);
            R[i] = std::tanh (R[i] * p.gain);
        }
    }

private:
    float sr = 48000.0f, curDelay = 18000.0f, lpL = 0, lpR = 0, lpCoef = 0.5f;
    std::vector<float> bufL, bufR;
    int wp = 0;
    FreqShifter shL, shR;
    juce::dsp::Phaser<float> phaser;
    juce::Reverb reverb;
};

//==============================================================================
// Lorenz attractor used as a slow chaotic modulation source.
struct Lorenz
{
    double x = 0.1, y = 0.0, z = 20.0;

    void advance (double seconds)
    {
        while (seconds > 0.0)
        {
            const double h = juce::jmin (seconds, 0.004);
            const double dx = 10.0 * (y - x), dy = x * (28.0 - z) - y, dz = x * y - (8.0 / 3.0) * z;
            x += h * dx; y += h * dy; z += h * dz;
            seconds -= h;
        }
        if (! std::isfinite (x) || std::abs (x) > 100.0) { x = 0.1; y = 0.0; z = 20.0; }
    }

    float nx() const { return juce::jlimit (-1.0f, 1.0f, (float) (x / 20.0)); }
    float ny() const { return juce::jlimit (-1.0f, 1.0f, (float) (y / 27.0)); }
    float nz() const { return juce::jlimit (-1.0f, 1.0f, (float) ((z - 25.0) / 22.0)); }
};
} // namespace astral
