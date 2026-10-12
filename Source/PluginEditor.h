#pragma once
#include "PluginProcessor.h"
#include <functional>

// Small component that starts an OS-level file drag (so the .mid can be dropped straight into a DAW track).
class DragHandle : public juce::Component
{
public:
    std::function<juce::File()> makeFile;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff1c2129));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (0xff58a6ff));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setFont (14.0f);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xE2\xA0\xBF  Drag MIDI into your DAW")), getLocalBounds(),
                    juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { dragging = false; }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        if (! makeFile) return;
        const juce::File f = makeFile();
        if (f.existsAsFile())
        {
            juce::StringArray files;
            files.add (f.getFullPathName());
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

private:
    bool dragging = false;
};

class MozartEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MozartEditor (MozartProcessor&);
    ~MozartEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void styleColours();
    void refreshFromSettings();
    void refreshInfo();
    void commitTextFields();
    void doGenerate();
    juce::String fileBase() const;

    static juce::Rectangle<int> leftPanel()  { return { 16, 70, 380, 440 }; }
    static juce::Rectangle<int> rightPanel() { return { 412, 70, 452, 440 }; }

    MozartProcessor& proc;
    juce::LookAndFeel_V4 laf;

    juce::ComboBox genreBox, scaleBox, rootBox, octaveBox, swingBox;
    juce::Label genreLbl, scaleLbl, rootLbl, octLbl, bpmLbl, barsLbl, seedLbl, densLbl, energyLbl, swingLbl;
    juce::TextEditor bpmEdit, barsEdit, seedEdit;
    juce::Slider densitySlider, energySlider;
    juce::ToggleButton humanizeBtn { "Humanize" }, soundBtn { "Play built-in sound while host plays" };
    juce::TextButton keepSeedBtn { "Keep" };
    juce::TextButton generateBtn { "Generate" }, previewBtn { "Preview" }, saveBtn { "Save .mid" };
    DragHandle dragHandle;
    juce::Label infoLbl, sectionsLbl, barLbl;

    static constexpr int kRows = 5;
    juce::ToggleButton trackToggle[kRows];
    juce::Label trackCount[kRows];

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MozartEditor)
};
