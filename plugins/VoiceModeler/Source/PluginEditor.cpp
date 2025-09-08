#include "PluginEditor.h"
#include "PluginProcessor.h"

VoiceModelerAudioProcessorEditor::VoiceModelerAudioProcessorEditor (VoiceModelerAudioProcessor& p)
: AudioProcessorEditor (&p), processorRef (p)
{
    setSize (400, 240);
}

void VoiceModelerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("VoiceModeler (Generic UI)\nUse host parameter view to adjust gain.",
                      getLocalBounds(), juce::Justification::centred, 3);
}

void VoiceModelerAudioProcessorEditor::resized()
{
}
