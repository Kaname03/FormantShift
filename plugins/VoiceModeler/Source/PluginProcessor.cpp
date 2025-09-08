#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr auto paramGainId   = "gain";
    constexpr auto paramGainName = "Gain";
}

VoiceModelerAudioProcessor::VoiceModelerAudioProcessor()
: AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    gainParam = apvts.getRawParameterValue (paramGainId); // Atomic<float>* を取得
}

void VoiceModelerAudioProcessor::prepareToPlay (double, int)
{
}

bool VoiceModelerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VoiceModelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // ★ 修正ポイント：float として読み出し、dB→線形に変換
    const float gainDb = gainParam ? gainParam->load() : 0.0f;
    const float lin    = juce::Decibels::decibelsToGain (gainDb);

    buffer.applyGain (lin);
}

juce::AudioProcessorEditor* VoiceModelerAudioProcessor::createEditor()
{
    // 既存のエディタでもOK。簡易なら Generic で。
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

    // dB レンジ：-24 ～ +24dB
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        paramGainId, paramGainName,
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f));

    return { params.begin(), params.end() };
}

// 生成関数
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceModelerAudioProcessor();
}
