#include "PluginEditor.h"

namespace
{
const juce::Colour cBg     (0xff0d1117);
const juce::Colour cPanel  (0xff161b22);
const juce::Colour cPanel2 (0xff1c2129);
const juce::Colour cBorder (0xff2d333b);
const juce::Colour cText   (0xffe6edf3);
const juce::Colour cMuted  (0xff8b949e);
const juce::Colour cAccent (0xff58a6ff);
const juce::Colour cGreen  (0xff3fb950);

const mozart::Role kRoles[5] = { mozart::RoleMelody, mozart::RoleChord, mozart::RoleBass, mozart::RolePad, mozart::RoleDrums };
const char* const kRoleNames[5] = { "Melody", "Harmony", "Bass", "Pad", "Drums" };
const juce::Colour kRoleColours[5] = { juce::Colour (0xff58a6ff), juce::Colour (0xff3fb950), juce::Colour (0xffd29922),
                                       juce::Colour (0xffbc8cff), juce::Colour (0xfff85149) };
}

MozartEditor::MozartEditor (MozartProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    styleColours();

    auto initLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, cMuted);
        addAndMakeVisible (l);
    };
    initLabel (genreLbl, "Genre");  initLabel (scaleLbl, "Scale");  initLabel (rootLbl, "Root");
    initLabel (octLbl, "Octave");   initLabel (bpmLbl, "Tempo");    initLabel (barsLbl, "Bars");
    initLabel (seedLbl, "Seed");    initLabel (densLbl, "Density"); initLabel (energyLbl, "Energy");
    initLabel (swingLbl, "Swing");

    // combos
    for (int i = 0; i < mozart::genreCount(); ++i) genreBox.addItem (mozart::genreName (i), i + 1);
    for (int i = 0; i < mozart::scaleCount(); ++i) scaleBox.addItem (mozart::scaleName (i), i + 1);
    for (int i = 0; i < mozart::rootCount();  ++i) rootBox.addItem (mozart::rootName (i), i + 1);
    for (int o = 2; o <= 4; ++o) octaveBox.addItem (juce::String (o), o);
    swingBox.addItem ("Auto (genre)", 1); swingBox.addItem ("On", 2); swingBox.addItem ("Off", 3);
    for (auto* c : { &genreBox, &scaleBox, &rootBox, &octaveBox, &swingBox }) addAndMakeVisible (*c);

    genreBox.onChange  = [this] { proc.applyGenreDefaults (genreBox.getSelectedId() - 1); refreshFromSettings(); };
    scaleBox.onChange  = [this] { proc.settings.scale  = scaleBox.getSelectedId() - 1; };
    rootBox.onChange   = [this] { proc.settings.root   = rootBox.getSelectedId() - 1; };
    octaveBox.onChange = [this] { proc.settings.octave = octaveBox.getSelectedId(); };
    swingBox.onChange  = [this] { proc.settings.swing  = swingBox.getSelectedId() - 1; };

    // text fields
    bpmEdit.setInputRestrictions (3, "0123456789");
    barsEdit.setInputRestrictions (3, "0123456789");
    seedEdit.setInputRestrictions (12, "-0123456789.");
    seedEdit.setTextToShowWhenEmpty ("random", cMuted);
    for (auto* e : { &bpmEdit, &barsEdit, &seedEdit })
    {
        e->onTextChange = [this] { commitTextFields(); };
        e->setJustification (juce::Justification::centredLeft);
        addAndMakeVisible (*e);
    }
    keepSeedBtn.setTooltip ("Copy the seed of the current piece into the Seed field to lock it");
    keepSeedBtn.onClick = [this] { if (auto s = proc.getSong()) seedEdit.setText (juce::String (s->seedText), true); };
    addAndMakeVisible (keepSeedBtn);

    // sliders
    for (auto* s : { &densitySlider, &energySlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
        s->setTextValueSuffix ("%");
        addAndMakeVisible (*s);
    }
    densitySlider.setRange (20, 95, 1);
    energySlider.setRange (0, 100, 1);
    densitySlider.onValueChange = [this] { proc.settings.density = (int) densitySlider.getValue(); };
    energySlider.onValueChange  = [this] { proc.settings.energy  = (int) energySlider.getValue(); };

    // toggles
    humanizeBtn.onClick = [this] { proc.settings.humanize = humanizeBtn.getToggleState(); };
    soundBtn.onClick    = [this] { proc.internalSound = soundBtn.getToggleState(); };
    addAndMakeVisible (humanizeBtn);
    addAndMakeVisible (soundBtn);

    // buttons
    generateBtn.setColour (juce::TextButton::buttonColourId, cGreen);
    generateBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff001800));
    previewBtn.setColour (juce::TextButton::buttonColourId, cAccent);
    previewBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff001a33));
    generateBtn.onClick = [this] { doGenerate(); };
    previewBtn.onClick = [this]
    {
        if (proc.previewPlaying.load()) { proc.previewPlaying = false; return; }
        proc.requestPreviewReset();
        proc.previewPlaying = true;
    };
    saveBtn.onClick = [this]
    {
        auto song = proc.getSong();
        if (song == nullptr) return;
        const auto def = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (fileBase() + ".mid");
        chooser = std::make_unique<juce::FileChooser> ("Save MIDI file", def, "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [song] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  f = f.withFileExtension ("mid");
                                  const auto bytes = mozart::buildMidi (*song);
                                  f.replaceWithData (bytes.data(), bytes.size());
                              });
    };
    for (auto* b : { &generateBtn, &previewBtn, &saveBtn }) addAndMakeVisible (*b);

    dragHandle.makeFile = [this]() -> juce::File
    {
        auto song = proc.getSong();
        if (song == nullptr) return {};
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MozartMidi");
        dir.createDirectory();
        auto f = dir.getChildFile (fileBase() + ".mid");
        const auto bytes = mozart::buildMidi (*song);
        if (! f.replaceWithData (bytes.data(), bytes.size())) return {};
        return f;
    };
    addAndMakeVisible (dragHandle);

    // info + tracks
    infoLbl.setJustificationType (juce::Justification::topLeft);
    sectionsLbl.setJustificationType (juce::Justification::topLeft);
    sectionsLbl.setColour (juce::Label::textColourId, cMuted);
    barLbl.setJustificationType (juce::Justification::centredRight);
    barLbl.setColour (juce::Label::textColourId, cMuted);
    addAndMakeVisible (infoLbl);
    addAndMakeVisible (sectionsLbl);
    addAndMakeVisible (barLbl);

    for (int i = 0; i < kRows; ++i)
    {
        trackToggle[i].setButtonText (kRoleNames[i]);
        trackToggle[i].setColour (juce::ToggleButton::tickColourId, kRoleColours[i]);
        trackToggle[i].onClick = [this, i] { proc.roleEnabled[kRoles[i]] = trackToggle[i].getToggleState(); };
        trackCount[i].setJustificationType (juce::Justification::centredRight);
        trackCount[i].setColour (juce::Label::textColourId, cMuted);
        addAndMakeVisible (trackToggle[i]);
        addAndMakeVisible (trackCount[i]);
    }

    setSize (880, 590);
    refreshFromSettings();
    refreshInfo();
    startTimerHz (20);
}

MozartEditor::~MozartEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MozartEditor::styleColours()
{
    laf.setColour (juce::ResizableWindow::backgroundColourId, cBg);
    laf.setColour (juce::ComboBox::backgroundColourId, cPanel2);
    laf.setColour (juce::ComboBox::textColourId, cText);
    laf.setColour (juce::ComboBox::outlineColourId, cBorder);
    laf.setColour (juce::ComboBox::arrowColourId, cMuted);
    laf.setColour (juce::PopupMenu::backgroundColourId, cPanel2);
    laf.setColour (juce::PopupMenu::textColourId, cText);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, cAccent.darker (0.5f));
    laf.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    laf.setColour (juce::TextEditor::backgroundColourId, cPanel2);
    laf.setColour (juce::TextEditor::textColourId, cText);
    laf.setColour (juce::TextEditor::outlineColourId, cBorder);
    laf.setColour (juce::TextEditor::focusedOutlineColourId, cAccent);
    laf.setColour (juce::CaretComponent::caretColourId, cText);
    laf.setColour (juce::Slider::thumbColourId, cAccent);
    laf.setColour (juce::Slider::trackColourId, cAccent.darker (0.4f));
    laf.setColour (juce::Slider::backgroundColourId, cBorder);
    laf.setColour (juce::Slider::textBoxTextColourId, cText);
    laf.setColour (juce::Slider::textBoxBackgroundColourId, cPanel2);
    laf.setColour (juce::Slider::textBoxOutlineColourId, cBorder);
    laf.setColour (juce::TextButton::buttonColourId, cPanel2);
    laf.setColour (juce::TextButton::textColourOffId, cText);
    laf.setColour (juce::TextButton::textColourOnId, cText);
    laf.setColour (juce::ToggleButton::textColourId, cText);
    laf.setColour (juce::ToggleButton::tickColourId, cGreen);
    laf.setColour (juce::ToggleButton::tickDisabledColourId, cMuted);
    laf.setColour (juce::Label::textColourId, cText);
}

void MozartEditor::paint (juce::Graphics& g)
{
    g.fillAll (cBg);

    g.setColour (cAccent);
    g.setFont (g.getCurrentFont().withHeight (28.0f).boldened());
    g.drawText ("Mozart", 20, 14, 110, 34, juce::Justification::centredLeft);
    g.setColour (cMuted);
    g.setFont (13.0f);
    g.drawText ("Generative MIDI: song structure, hit progressions, hooks and humanized grooves, by genre.",
                132, 22, 720, 22, juce::Justification::centredLeft);

    for (auto r : { leftPanel(), rightPanel() })
    {
        g.setColour (cPanel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (cBorder);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    g.setColour (cMuted);
    g.setFont (g.getCurrentFont().withHeight (12.0f).boldened());
    g.drawText ("SETTINGS", leftPanel().getX() + 16, leftPanel().getY() + 12, 200, 18, juce::Justification::centredLeft);
    g.drawText ("GENERATED PIECE", rightPanel().getX() + 16, rightPanel().getY() + 12, 200, 18, juce::Justification::centredLeft);
    g.drawText ("TRACKS  (tick = send to host / built-in sound)", rightPanel().getX() + 16, rightPanel().getY() + 176, 400, 18,
                juce::Justification::centredLeft);
}

void MozartEditor::resized()
{
    // ---- left panel ----
    const auto lp = leftPanel().reduced (16, 0);
    int y = leftPanel().getY() + 40;
    auto nextRow = [&]() { juce::Rectangle<int> r (lp.getX(), y, lp.getWidth(), 28); y += 36; return r; };
    const int lw = 70;

    { auto r = nextRow(); genreLbl.setBounds (r.removeFromLeft (lw)); genreBox.setBounds (r); }
    { auto r = nextRow(); scaleLbl.setBounds (r.removeFromLeft (lw)); scaleBox.setBounds (r); }
    { auto r = nextRow(); rootLbl.setBounds (r.removeFromLeft (lw)); rootBox.setBounds (r.removeFromLeft (90));
      r.removeFromLeft (14); octLbl.setBounds (r.removeFromLeft (56)); octaveBox.setBounds (r); }
    { auto r = nextRow(); bpmLbl.setBounds (r.removeFromLeft (lw)); bpmEdit.setBounds (r.removeFromLeft (90));
      r.removeFromLeft (14); barsLbl.setBounds (r.removeFromLeft (56)); barsEdit.setBounds (r); }
    { auto r = nextRow(); seedLbl.setBounds (r.removeFromLeft (lw)); keepSeedBtn.setBounds (r.removeFromRight (60));
      r.removeFromRight (8); seedEdit.setBounds (r); }
    { auto r = nextRow(); densLbl.setBounds (r.removeFromLeft (lw)); densitySlider.setBounds (r); }
    { auto r = nextRow(); energyLbl.setBounds (r.removeFromLeft (lw)); energySlider.setBounds (r); }
    { auto r = nextRow(); swingLbl.setBounds (r.removeFromLeft (lw)); swingBox.setBounds (r.removeFromLeft (120));
      r.removeFromLeft (14); humanizeBtn.setBounds (r); }
    { auto r = nextRow(); soundBtn.setBounds (r); }

    // ---- right panel ----
    const auto rp = rightPanel().reduced (16, 0);
    infoLbl.setBounds (rp.getX(), rightPanel().getY() + 38, rp.getWidth(), 54);
    sectionsLbl.setBounds (rp.getX(), rightPanel().getY() + 96, rp.getWidth(), 70);
    int ty = rightPanel().getY() + 204;
    for (int i = 0; i < kRows; ++i)
    {
        trackToggle[i].setBounds (rp.getX(), ty, 200, 28);
        trackCount[i].setBounds (rp.getX() + 200, ty, rp.getWidth() - 200, 28);
        ty += 40;
    }

    // ---- bottom bar ----
    juce::Rectangle<int> bar (16, 526, getWidth() - 32, 44);
    generateBtn.setBounds (bar.removeFromLeft (150)); bar.removeFromLeft (10);
    previewBtn.setBounds (bar.removeFromLeft (150));  bar.removeFromLeft (10);
    saveBtn.setBounds (bar.removeFromLeft (110));     bar.removeFromLeft (10);
    dragHandle.setBounds (bar.removeFromLeft (230));  bar.removeFromLeft (10);
    barLbl.setBounds (bar);
}

void MozartEditor::refreshFromSettings()
{
    const auto& s = proc.settings;
    genreBox.setSelectedId (s.genre + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (s.scale + 1, juce::dontSendNotification);
    rootBox.setSelectedId (s.root + 1, juce::dontSendNotification);
    octaveBox.setSelectedId (juce::jlimit (2, 4, s.octave), juce::dontSendNotification);
    swingBox.setSelectedId (s.swing + 1, juce::dontSendNotification);
    bpmEdit.setText (juce::String (s.bpm), false);
    barsEdit.setText (juce::String (s.bars), false);
    seedEdit.setText (s.seed, false);
    densitySlider.setValue (s.density, juce::dontSendNotification);
    energySlider.setValue (s.energy, juce::dontSendNotification);
    humanizeBtn.setToggleState (s.humanize, juce::dontSendNotification);
    soundBtn.setToggleState (proc.internalSound.load(), juce::dontSendNotification);
    for (int i = 0; i < kRows; ++i)
        trackToggle[i].setToggleState (proc.roleEnabled[kRoles[i]].load(), juce::dontSendNotification);
}

void MozartEditor::commitTextFields()
{
    auto& s = proc.settings;
    const auto bpmText = bpmEdit.getText();
    const auto barsText = barsEdit.getText();
    if (bpmText.isNotEmpty())  s.bpm  = juce::jlimit (40, 240, bpmText.getIntValue());
    if (barsText.isNotEmpty()) s.bars = juce::jlimit (4, 128, barsText.getIntValue());
    s.seed = seedEdit.getText().trim();
    proc.bpmAtomic = s.bpm;
}

void MozartEditor::doGenerate()
{
    commitTextFields();
    proc.regenerate();
    refreshInfo();
}

void MozartEditor::refreshInfo()
{
    auto song = proc.getSong();
    if (song == nullptr) return;
    const auto& o = song->opts;

    infoLbl.setText (juce::String (mozart::genreName (o.genre)) + "  |  " + mozart::scaleName (o.scale) + " in "
                         + mozart::rootName (o.root) + "  |  " + juce::String (o.bpm) + " BPM  |  " + juce::String (o.bars)
                         + " bars  |  " + (song->swing ? "swing" : "straight") + (o.humanize ? "  |  humanized" : "")
                         + "  |  seed " + juce::String (song->seedText),
                     juce::dontSendNotification);

    juce::StringArray secs;
    for (auto& s : song->sections) secs.add (juce::String (s.type) + " " + juce::String (s.bars) + "b");
    sectionsLbl.setText (secs.joinIntoString ("  >  "), juce::dontSendNotification);

    for (int i = 0; i < kRows; ++i)
    {
        const mozart::Track* t = song->find (kRoles[i]);
        trackToggle[i].setVisible (t != nullptr);
        trackCount[i].setVisible (t != nullptr);
        if (t != nullptr)
            trackCount[i].setText (juce::String (t->noteCount()) + " notes  |  MIDI ch " + juce::String (t->channel + 1),
                                   juce::dontSendNotification);
    }
}

juce::String MozartEditor::fileBase() const
{
    auto clean = [] (const char* s)
    {
        return juce::String (s).replaceCharacters (" /&#", "___s")
                               .retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-");
    };
    auto song = proc.getSong();
    if (song == nullptr) return "Mozart";
    const auto& o = song->opts;
    return clean (mozart::genreName (o.genre)) + "_" + clean (mozart::scaleName (o.scale)) + "_" + clean (mozart::rootName (o.root))
           + "_" + juce::String (o.bpm) + "bpm_seed" + clean (song->seedText.c_str());
}

void MozartEditor::timerCallback()
{
    const juce::String txt = proc.previewPlaying.load() ? "Stop preview" : "Preview";
    if (previewBtn.getButtonText() != txt) previewBtn.setButtonText (txt);

    juce::String bar = "Stopped";
    if (proc.transportRunning.load())
    {
        auto song = proc.getSong();
        bar = "Bar " + juce::String (proc.currentBar.load() + 1) + " / " + juce::String (song ? song->opts.bars : 0);
    }
    if (barLbl.getText() != bar) barLbl.setText (bar, juce::dontSendNotification);
}
