#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class VoiceModelerAudioProcessor;

class VoiceModelerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VoiceModelerAudioProcessorEditor (VoiceModelerAudioProcessor&);
    ~VoiceModelerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VoiceModelerAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceModelerAudioProcessorEditor)
};
