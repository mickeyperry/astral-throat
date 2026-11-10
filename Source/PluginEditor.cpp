#include "PluginEditor.h"

namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

float frac (float v) { return v - std::floor (v); }

juce::Colour hsv (float h, float s, float v, float a = 1.0f)
{
    return juce::Colour::fromHSV (frac (h), juce::jlimit (0.0f, 1.0f, s), juce::jlimit (0.0f, 1.0f, v),
                                  juce::jlimit (0.0f, 1.0f, a));
}

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

const juce::Colour kBg (0xff0b0714), kPanel (0xff150d26), kText (0xffe9defa), kDim (0xff8f7fb0);
const char* const kSections[] = { "VOICE", "CHOIR + OVERTONE", "MOTION", "SPACE" };
const float kSectionHue[] = { 0.86f, 0.55f, 0.36f, 0.07f };
}

//==============================================================================
PsyLookAndFeel::PsyLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBg);
    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::outlineColourId, kDim.withAlpha (0.5f));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, hsv (0.8f, 0.7f, 0.6f));
    setColour (juce::TextButton::buttonColourId, kPanel);
    setColour (juce::TextButton::buttonOnColourId, hsv (0.86f, 0.8f, 0.85f));
    setColour (juce::TextButton::textColourOffId, kDim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::BubbleComponent::backgroundColourId, kPanel);
    setColour (juce::TooltipWindow::textColourId, kText);
}

void PsyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    const float hue = (float) slider.getProperties().getWithDefault ("hue", 0.8);
    auto area = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto c = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (3.0f, r * 0.16f);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0, startAngle, angle, true);
    const juce::PathStrokeType stroke (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    g.setColour (juce::Colour (0xff261a40));
    g.strokePath (track, stroke);
    g.setGradientFill (juce::ColourGradient (hsv (hue, 0.85f, 1.0f), c.x - r, c.y + r,
                                             hsv (hue + 0.14f, 0.75f, 1.0f), c.x + r, c.y - r, false));
    g.strokePath (value, stroke);

    const float inner = r - thick * 1.6f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35245a), c.x, c.y - inner,
                                             juce::Colour (0xff140c24), c.x, c.y + inner, false));
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -inner + 2.0f, 3.2f, inner * 0.55f, 1.6f);
    g.setColour (slider.isMouseOverOrDragging() ? juce::Colours::white : kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
OracleView::OracleView (AstralThroatProcessor& p) : proc (p)
{
    vowelParam = proc.apvts.getParameter ("vowel");
    harmParam = proc.apvts.getParameter ("harmonic");
    vowel = proc.uiVowel.load();
    harmonic = proc.uiHarmonic.load();
    overtone = proc.uiOvertone.load();
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

void OracleView::timerCallback()
{
    const float target = proc.uiLevel.load();
    level += (target - level) * (target > level ? 0.5f : 0.2f);
    vowel += (proc.uiVowel.load() - vowel) * 0.4f;
    overtone += (proc.uiOvertone.load() - overtone) * 0.3f;
    harmonic += (proc.uiHarmonic.load() - harmonic) * 0.3f;
    cx += (proc.uiChaosX.load() - cx) * 0.3f;
    cy += (proc.uiChaosY.load() - cy) * 0.3f;
    trip = proc.uiTrip.load();
    t += (1.0f / 30.0f) * (0.6f + 2.0f * trip + 1.2f * level);
    repaint();
}

void OracleView::setFromMouse (const juce::MouseEvent& e)
{
    const float x = juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, 1.0f - e.position.y / (float) getHeight());
    vowelParam->setValueNotifyingHost (x);
    harmParam->setValueNotifyingHost (y);
}

void OracleView::mouseDown (const juce::MouseEvent& e)
{
    vowelParam->beginChangeGesture();
    harmParam->beginChangeGesture();
    setFromMouse (e);
}

void OracleView::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e); }

void OracleView::mouseUp (const juce::MouseEvent&)
{
    vowelParam->endChangeGesture();
    harmParam->endChangeGesture();
}

void OracleView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = b.getWidth() * 0.5f;
    const float hue = t * 0.035f;

    juce::Path clip;
    clip.addRoundedRectangle (b, 18.0f);
    g.saveState();
    g.reduceClipRegion (clip);

    // Deep-space backdrop
    g.setGradientFill (juce::ColourGradient (hsv (hue + 0.62f, 0.9f, 0.30f), c.x, c.y,
                                             hsv (hue + 0.78f, 0.95f, 0.04f), b.getX(), b.getY(), true));
    g.fillRect (b);

    // Expanding rings
    for (int k = 0; k < 7; ++k)
    {
        const float ph = frac (t * 0.11f + (float) k / 7.0f);
        const float rr = ph * R * 1.45f;
        g.setColour (hsv (hue + 0.3f + ph * 0.4f, 0.8f, 1.0f, (1.0f - ph) * (0.18f + 0.35f * level)));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.5f + 5.0f * level * (1.0f - ph));
    }

    // Kaleidoscope petals
    for (int ring = 4; ring >= 0; --ring)
    {
        const int n = 8 + ring * 2;
        const float rad = R * (0.50f + 0.13f * (float) ring) * (1.0f + 0.10f * level * std::sin (t * 2.3f + (float) ring));
        const float rot = t * (0.14f + 0.05f * (float) ring) * ((ring & 1) ? -1.0f : 1.0f);
        const float pw = R * (0.16f + 0.02f * (float) ring), ph = R * (0.34f + 0.10f * overtone);
        juce::Path petal;
        petal.startNewSubPath (0, -rad - ph * 0.5f);
        petal.quadraticTo (pw * 0.5f, -rad, 0, -rad + ph * 0.5f);
        petal.quadraticTo (-pw * 0.5f, -rad, 0, -rad - ph * 0.5f);
        petal.closeSubPath();
        for (int i = 0; i < n; ++i)
        {
            const float ang = rot + kTwoPi * (float) i / (float) n;
            g.setColour (hsv (hue + 0.09f * (float) ring + 0.35f * (float) i / (float) n, 0.85f, 1.0f,
                              0.14f + 0.22f * level + 0.10f * trip));
            g.fillPath (petal, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
        }
    }

    // The oracle's head
    const float a = R * 0.36f, h = R * 0.46f;
    const float hx = c.x + cx * 5.0f, hy = c.y + std::sin (t * 0.9f) * 5.0f + R * 0.03f;
    for (int i = 5; i >= 1; --i)
    {
        const float gr = (float) i * (3.0f + 5.0f * level);
        g.setColour (hsv (hue + 0.5f, 0.6f, 1.0f, 0.05f + 0.03f * level));
        g.fillEllipse (hx - a - gr, hy - h - gr, (a + gr) * 2.0f, (h + gr) * 2.0f);
    }
    g.setGradientFill (juce::ColourGradient (hsv (hue + 0.50f, 0.50f, 0.98f), hx - a * 0.4f, hy - h,
                                             hsv (hue + 0.80f, 0.75f, 0.45f), hx + a * 0.4f, hy + h, false));
    g.fillEllipse (hx - a, hy - h, a * 2.0f, h * 2.0f);
    g.setColour (hsv (hue + 0.55f, 0.3f, 1.0f, 0.6f));
    g.drawEllipse (hx - a, hy - h, a * 2.0f, h * 2.0f, 1.6f);

    // Eyes: squint blissfully while singing, pupils wander with the chaos source
    const float ew = a * 0.36f, eh = a * 0.20f * (1.0f - 0.6f * level) + 1.5f;
    for (int side = -1; side <= 1; side += 2)
    {
        const float ex = hx + (float) side * a * 0.45f, ey = hy - h * 0.10f;
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillEllipse (ex - ew * 0.5f, ey - eh * 0.5f, ew, eh);
        const float pr = juce::jmin (eh * 0.48f, ew * 0.22f);
        g.setColour (juce::Colour (0xff120820));
        g.fillEllipse (ex + cx * ew * 0.22f - pr, ey + cy * eh * 0.15f - pr, pr * 2.0f, pr * 2.0f);
        g.setColour (juce::Colour (0xff2a1445));
        g.drawEllipse (ex - ew * 0.5f, ey - eh * 0.5f, ew, eh, 1.4f);
    }

    // Third eye opens with the overtone amount; iris hue follows the harmonic
    {
        const float tx = hx, ty = hy - h * 0.52f, tw = a * 0.34f, th = a * 0.36f * overtone;
        if (th > 2.5f)
        {
            juce::Path eye;
            eye.startNewSubPath (tx, ty - th * 0.5f);
            eye.quadraticTo (tx + tw * 0.5f, ty, tx, ty + th * 0.5f);
            eye.quadraticTo (tx - tw * 0.5f, ty, tx, ty - th * 0.5f);
            eye.closeSubPath();
            g.setColour (juce::Colours::white);
            g.fillPath (eye);
            const float ir = juce::jmin (th * 0.42f, tw * 0.2f);
            g.setColour (hsv (harmonic / 16.0f + hue * 2.0f, 0.9f, 1.0f));
            g.fillEllipse (tx - ir, ty - ir, ir * 2.0f, ir * 2.0f);
            g.setColour (juce::Colour (0xff120820));
            g.fillEllipse (tx - ir * 0.45f, ty - ir * 0.45f, ir * 0.9f, ir * 0.9f);
            g.setColour (hsv (hue + 0.12f, 0.8f, 1.0f, 0.9f));
            g.strokePath (eye, juce::PathStrokeType (1.6f));
        }
        else
        {
            g.setColour (juce::Colour (0xff2a1445));
            g.drawLine (tx, ty - a * 0.10f, tx, ty + a * 0.10f, 1.6f);
        }
    }

    // Mouth shape follows the vowel (U O A E I), opening follows the envelope
    {
        static const float mw[] = { 0.20f, 0.30f, 0.44f, 0.54f, 0.58f };
        static const float mh[] = { 0.22f, 0.34f, 0.50f, 0.26f, 0.12f };
        const float pos = juce::jlimit (0.0f, 1.0f, vowel) * 4.0f;
        const int i0 = juce::jmin ((int) pos, 3);
        const float fr = pos - (float) i0;
        const float w = a * 1.9f * (mw[i0] + fr * (mw[i0 + 1] - mw[i0]));
        const float mhh = juce::jmax (3.0f, h * 1.3f * (mh[i0] + fr * (mh[i0 + 1] - mh[i0])) * (0.10f + 0.90f * level));
        const float mx = hx, my = hy + h * 0.45f;
        g.setGradientFill (juce::ColourGradient (hsv (hue + 0.05f, 0.9f, 0.9f * level + 0.1f), mx, my + mhh * 0.3f,
                                                 juce::Colour (0xff0d0318), mx, my - mhh * 0.5f, false));
        g.fillEllipse (mx - w * 0.5f, my - mhh * 0.5f, w, mhh);
        g.setColour (hsv (hue + 0.92f, 0.7f, 0.5f));
        g.drawEllipse (mx - w * 0.5f, my - mhh * 0.5f, w, mhh, 2.2f);
    }

    // Readout
    static const char* const names[] = { "U", "O", "A", "E", "I" };
    g.setColour (kText.withAlpha (0.75f));
    g.setFont (font (13.0f, true));
    g.drawText (juce::String (names[juce::jlimit (0, 4, juce::roundToInt (vowel * 4.0f))]) + "   H"
                    + juce::String (juce::roundToInt (harmonic)),
                b.reduced (14.0f, 10.0f), juce::Justification::bottomLeft);
    g.setColour (kText.withAlpha (0.4f));
    g.setFont (font (11.0f));
    g.drawText ("drag:  X vowel  /  Y harmonic", b.reduced (14.0f, 10.0f), juce::Justification::bottomRight);

    g.restoreState();
    g.setColour (hsv (hue + 0.7f, 0.6f, 0.9f, 0.5f));
    g.drawRoundedRectangle (b.reduced (0.75f), 18.0f, 1.5f);
}

//==============================================================================
AstralThroatEditor::AstralThroatEditor (AstralThroatProcessor& p)
    : AudioProcessorEditor (&p), proc (p), oracle (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (oracle);

    struct Def { const char* id; const char* name; };
    static const Def defs[] = {
        { "vowel", "Vowel" }, { "throat", "Throat" }, { "tension", "Tension" },
        { "breath", "Breath" }, { "glide", "Glide" }, { "vibrato", "Vibrato" },
        { "voices", "Voices" }, { "detune", "Detune" }, { "overtone", "Overtone" },
        { "harmonic", "Harmonic" }, { "attack", "Attack" }, { "release", "Release" },
        { "mouthRate", "Mouth Rate" }, { "mouthDepth", "Mouth Depth" }, { "chaos", "Chaos" },
        { "drive", "Drive" }, { "swirl", "Swirl" }, { "gain", "Gain" },
        { "delayTime", "Delay" }, { "feedback", "Feedback" }, { "spiral", "Spiral" },
        { "delayMix", "Delay Mix" }, { "revSize", "Verb Size" }, { "revMix", "Verb Mix" } };
    for (int i = 0; i < 24; ++i)
        addKnob (defs[i].id, defs[i].name, kSectionHue[i / 6]);
    tripKnob = &addKnob ("trip", "TRIP", 0.95f);
    tripKnob->label.setFont (font (14.0f, true));
    tripKnob->label.setColour (juce::Label::textColourId, kText);

    droneButton.setClickingTogglesState (true);
    addAndMakeVisible (droneButton);
    droneAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "drone", droneButton);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("sync")))
        syncBox.addItemList (choice->choices, 1);
    addAndMakeVisible (syncBox);
    syncAttachment = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "sync", syncBox);

    keyboard.setAvailableRange (24, 96);
    keyboard.setKeyWidth (928.0f / 43.0f);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, hsv (0.86f, 0.8f, 1.0f, 0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, hsv (0.55f, 0.6f, 1.0f, 0.4f));
    addAndMakeVisible (keyboard);

    setSize (960, 652);
}

AstralThroatEditor::~AstralThroatEditor()
{
    setLookAndFeel (nullptr);
}

AstralThroatEditor::Knob& AstralThroatEditor::addKnob (const char* id, const char* name, float hue)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.getProperties().set ("hue", hue);
    k->slider.setPopupDisplayEnabled (true, true, this);
    k->slider.setNumDecimalPlacesToDisplay (2);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (font (11.5f));
    k->label.setColour (juce::Label::textColourId, kDim);
    k->label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void AstralThroatEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1034), 0, 0, kBg, 0, 120.0f, false));
    g.fillRect (0, 0, getWidth(), 120);

    g.setGradientFill (juce::ColourGradient (hsv (0.86f, 0.7f, 1.0f), 16.0f, 0, hsv (0.52f, 0.7f, 1.0f), 300.0f, 0, false));
    g.setFont (font (25.0f, true));
    g.drawText ("ASTRAL THROAT", 18, 8, 400, 36, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (font (12.0f));
    g.drawText ("psychedelic throat-singing oracle", 432, 8, 512, 36, juce::Justification::centredRight);

    for (int row = 0; row < 4; ++row)
    {
        const juce::Rectangle<float> r (432.0f, 56.0f + 101.0f * (float) row, 512.0f, 96.0f);
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (hsv (kSectionHue[row], 0.7f, 1.0f, 0.9f));
        g.setFont (font (10.5f, true));
        g.drawText (kSections[row], r.reduced (10.0f, 5.0f), juce::Justification::topLeft);
    }

    const juce::Rectangle<float> strip (16.0f, 464.0f, 928.0f, 92.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (strip, 10.0f);
    g.setColour (kDim);
    g.setFont (font (11.0f));
    g.drawText ("DELAY SYNC", 250, 474, 120, 14, juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawFittedText ("TRIP pushes overtones, chaos, swirl, feedback, spiral and reverb all at once.  "
                      "DRONE latches the last note so you can play the knobs.  Mod wheel opens the mouth.",
                      400, 472, 530, 76, juce::Justification::centredLeft, 4);
}

void AstralThroatEditor::resized()
{
    oracle.setBounds (16, 56, 400, 400);

    for (int i = 0; i < 24; ++i)
    {
        const int row = i / 6, col = i % 6;
        const int x = 432 + 6 + col * 84, y = 56 + 101 * row;
        knobs[(size_t) i]->slider.setBounds (x + 12, y + 18, 60, 60);
        knobs[(size_t) i]->label.setBounds (x, y + 77, 84, 15);
    }

    tripKnob->slider.setBounds (28, 468, 84, 70);
    tripKnob->label.setBounds (28, 536, 84, 16);
    droneButton.setBounds (128, 492, 104, 36);
    syncBox.setBounds (250, 494, 120, 32);
    keyboard.setBounds (16, 564, 928, 80);
}
