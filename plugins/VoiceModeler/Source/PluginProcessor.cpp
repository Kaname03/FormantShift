#include "PluginProcessor.h"
#include "PluginEditor.h"

VoiceModelerAudioProcessor::VoiceModelerAudioProcessor()
: AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    gainParam = apvts.getRawParameterValue("gain");
}

void VoiceModelerAudioProcessor::prepareToPlay (double, int)
{
}

bool VoiceModelerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // stereo in/out only
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VoiceModelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto* gain = gainParam ? gainParam->load() : 1.0f;
    buffer.applyGain (juce::Decibels::decibelsToGain(gain));
}

juce::AudioProcessorEditor* VoiceModelerAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void VoiceModelerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoiceModelerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout VoiceModelerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "gain", "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f));
    return { params.begin(), params.end() };
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceModelerAudioProcessor();
}
