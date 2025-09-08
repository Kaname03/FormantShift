#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace IDs {
    // 基本
    static constexpr auto gainDb        = "gain";          // 出力ゲイン(dB)
    static constexpr auto transposeSemi = "transposeSemi"; // 半音
    static constexpr auto formantRatio  = "formantRatio";  // 0.7–1.4
    static constexpr auto nasalAmt      = "nasalAmt";      // 0–100%

    // 反共鳴
    static constexpr auto nasalNotch    = "nasalNotch";    // 0–100%

    // RBass
    static constexpr auto rbDriveDb     = "rbDriveDb";
    static constexpr auto rbFocusHz     = "rbFocusHz";
    static constexpr auto rbMix         = "rbMix";

    // 3-band EQ
    static constexpr auto eq1Freq = "eq1Freq"; static constexpr auto eq1Gain = "eq1Gain"; static constexpr auto eq1Q = "eq1Q";
    static constexpr auto eq2Freq = "eq2Freq"; static constexpr auto eq2Gain = "eq2Gain"; static constexpr auto eq2Q = "eq2Q";
    static constexpr auto eq3Freq = "eq3Freq"; static constexpr auto eq3Gain = "eq3Gain"; static constexpr auto eq3Q = "eq3Q";
}

VoiceModelerAudioProcessor::VoiceModelerAudioProcessor()
: AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Raw param pointers
    pGainDb        = apvts.getRawParameterValue (IDs::gainDb);
    pTransposeSemi = apvts.getRawParameterValue (IDs::transposeSemi);
    pFormantRatio  = apvts.getRawParameterValue (IDs::formantRatio);
    pNasalAmt      = apvts.getRawParameterValue (IDs::nasalAmt);

    pNasalNotch    = apvts.getRawParameterValue (IDs::nasalNotch);

    pRBassDriveDb  = apvts.getRawParameterValue (IDs::rbDriveDb);
    pRBassFocusHz  = apvts.getRawParameterValue (IDs::rbFocusHz);
    pRBassMix      = apvts.getRawParameterValue (IDs::rbMix);

    pEQ1Freq = apvts.getRawParameterValue (IDs::eq1Freq);
    pEQ1Gain = apvts.getRawParameterValue (IDs::eq1Gain);
    pEQ1Q    = apvts.getRawParameterValue (IDs::eq1Q);
    pEQ2Freq = apvts.getRawParameterValue (IDs::eq2Freq);
    pEQ2Gain = apvts.getRawParameterValue (IDs::eq2Gain);
    pEQ2Q    = apvts.getRawParameterValue (IDs::eq2Q);
    pEQ3Freq = apvts.getRawParameterValue (IDs::eq3Freq);
    pEQ3Gain = apvts.getRawParameterValue (IDs::eq3Gain);
    pEQ3Q    = apvts.getRawParameterValue (IDs::eq3Q);
}

void VoiceModelerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = samplesPerBlock;

    auto specStereo = juce::dsp::ProcessSpec{ sampleRate, (juce::uint32)samplesPerBlock, 2 };
    auto specMono   = juce::dsp::ProcessSpec{ sampleRate, (juce::uint32)samplesPerBlock, 1 };

    // フィルタ群
    formantF1.reset(); formantF1.prepare (specStereo);
    formantF2.reset(); formantF2.prepare (specStereo);
    formantF3.reset(); formantF3.prepare (specStereo);
    nasal1k.reset();   nasal1k.prepare (specStereo);
    nasal3k.reset();   nasal3k.prepare (specStereo);
    nasalNotch1k.reset(); nasalNotch1k.prepare (specStereo);
    nasalNotch3k.reset(); nasalNotch3k.prepare (specStereo);
    eq1.reset(); eq1.prepare (specStereo);
    eq2.reset(); eq2.prepare (specStereo);
    eq3.reset(); eq3.prepare (specStereo);
    hpf20.reset(); hpf20.prepare (specStereo);

    // 出力ゲイン
    outGain.reset(); outGain.prepare (specStereo);
    outGain.setGainLinear (1.0f);

    // リミッター
    limiter.reset(); limiter.prepare (specStereo);
    limiter.setThreshold (-1.0f); // dBFS
    limiter.setRatio (20.0f);     // ほぼリミッター
    limiter.setAttack (2.0f);
    limiter.setRelease (50.0f);

    // RBass
    rbassBand.reset(); rbassBand.prepare (specMono);
    rbassMono.setSize (1, samplesPerBlock);

    // ピッチシフタ
    shifter.prepare (sampleRate, samplesPerBlock, 2, 1024);

    // スムージング
    smoothOutGain.reset (sampleRate, 0.02);
    smoothRBassMix.reset (sampleRate, 0.05);

    updateFilters();
}

bool VoiceModelerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VoiceModelerAudioProcessor::processRBass (juce::AudioBuffer<float>& buffer)
{
    // Monoにサム
    rbassMono.setSize (1, buffer.getNumSamples(), true, false, true);
    auto* mono = rbassMono.getWritePointer (0);
    auto* L = buffer.getReadPointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : L;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        mono[i] = 0.5f * (L[i] + R[i]);

    // Focus帯域のBPF
    juce::dsp::AudioBlock<float> monoBlock (rbassMono);
    juce::dsp::ProcessContextReplacing<float> monoCtx (monoBlock);
    rbassBand.process (monoCtx);

    // Drive -> tanh で倍音生成
    const float drive = juce::Decibels::decibelsToGain (pRBassDriveDb ? pRBassDriveDb->load() : 0.0f);
    for (int i = 0; i < rbassMono.getNumSamples(); ++i)
    {
        float x = mono[i] * drive;
        x = 0.6f * softClip (x) + 0.4f * softClip (x * 0.5f); // 2/3次混合
        mono[i] = x;
    }

    // ミックス
    const float mix = smoothRBassMix.getNextValue(); // 0..1
    if (mix > 0.0001f)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* dst = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                dst[i] += mono[i] * mix;
        }
    }
}

void VoiceModelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // === HPF ===
    {
        juce::dsp::AudioBlock<float> b (buffer);
        juce::dsp::ProcessContextReplacing<float> c (b);
        hpf20.process (c);
    }

    // === Transpose（実処理） ===
    {
        const float semi = pTransposeSemi ? pTransposeSemi->load() : 0.0f;
        shifter.setSemitone (semi);
        shifter.process (buffer); // in-place
    }

    // スムージング対象
    const float outDb = pGainDb ? pGainDb->load() : 0.0f;
    smoothOutGain.setTargetValue (juce::Decibels::decibelsToGain (outDb));
    const float rbMix01 = juce::jlimit (0.0f, 100.0f, pRBassMix ? pRBassMix->load() : 0.0f) / 100.0f;
    smoothRBassMix.setTargetValue (rbMix01);

    // 係数更新
    updateFilters();

    // === フィルタチェイン（フォルマント→鼻腔→ノッチ→EQ） ===
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    formantF1.process (ctx);
    formantF2.process (ctx);
    formantF3.process (ctx);

    nasal1k.process (ctx);
    nasal3k.process (ctx);

    nasalNotch1k.process (ctx);
    nasalNotch3k.process (ctx);

    eq1.process (ctx);
    eq2.process (ctx);
    eq3.process (ctx);

    // === RBass 合成 ===
    processRBass (buffer);

    // === Output Gain ===
    outGain.setGainLinear (smoothOutGain.getNextValue());
    outGain.process (ctx);

    // === Limiter（安全マージン） ===
    limiter.process (ctx);

    // 仕上げにソフトクリップ（彩度を少し）
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* s = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            s[i] = softClip (s[i]);
    }
}

void VoiceModelerAudioProcessor::updateFilters()
{
    const float ratio = pFormantRatio ? pFormantRatio->load() : 1.0f; // 0.7–1.4
    const float nasal  = pNasalAmt ? pNasalAmt->load() : 0.0f;        // 0–100
    const float nasalGainDb = juce::jmap (nasal, 0.0f, 100.0f, 0.0f, 8.0f);

    // フォルマント基準
    const float F1 = 500.0f  * ratio;
    const float F2 = 1500.0f * ratio;
    const float F3 = 2500.0f * ratio;

    const float Qf = 1.2f;
    const float G1 = juce::Decibels::decibelsToGain ( 2.0f * (ratio - 1.0f));
    const float G2 = juce::Decibels::decibelsToGain ( 1.5f * (ratio - 1.0f));
    const float G3 = juce::Decibels::decibelsToGain ( 1.0f * (ratio - 1.0f));

    *formantF1.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, F1, Qf, juce::jlimit (0.5f, 1.5f, G1));
    *formantF2.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, F2, Qf, juce::jlimit (0.5f, 1.5f, G2));
    *formantF3.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, F3, Qf, juce::jlimit (0.5f, 1.5f, G3));

    // 鼻腔レゾナンス
    *nasal1k.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 2.0f,
                        juce::Decibels::decibelsToGain (nasalGainDb));
    *nasal3k.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 3000.0f, 2.5f,
                        juce::Decibels::decibelsToGain (nasalGainDb * 0.7f));

    // 反共鳴ノッチ（=負ゲインのピーク）
    const float notchPct = pNasalNotch ? pNasalNotch->load() : 0.0f; // 0–100
    const float notchDepthDb = -juce::jmap (notchPct, 0.0f, 100.0f, 0.0f, 12.0f);
    *nasalNotch1k.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 2.0f,
                            juce::Decibels::decibelsToGain (notchDepthDb));
    *nasalNotch3k.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 3000.0f, 2.5f,
                            juce::Decibels::decibelsToGain (notchDepthDb * 0.8f));

    // 3-band EQ
    const auto mkPeak = [this](float f, float q, float gDb)
    {
        return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, f, q,
                juce::Decibels::decibelsToGain (gDb));
    };
    *eq1.state = *mkPeak (juce::jlimit (20.0f, 18000.0f, pEQ1Freq->load()),
                          juce::jlimit (0.3f, 5.0f,    pEQ1Q->load()),
                          juce::jlimit (-18.0f, 18.0f, pEQ1Gain->load()));
    *eq2.state = *mkPeak (juce::jlimit (20.0f, 18000.0f, pEQ2Freq->load()),
                          juce::jlimit (0.3f, 5.0f,    pEQ2Q->load()),
                          juce::jlimit (-18.0f, 18.0f, pEQ2Gain->load()));
    *eq3.state = *mkPeak (juce::jlimit (20.0f, 18000.0f, pEQ3Freq->load()),
                          juce::jlimit (0.3f, 5.0f,    pEQ3Q->load()),
                          juce::jlimit (-18.0f, 18.0f, pEQ3Gain->load()));

    // RBass BPF
    const float focus = juce::jlimit (40.0f, 240.0f, pRBassFocusHz ? pRBassFocusHz->load() : 100.0f);
    *rbassBand.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, focus, 1.0f);

    // HPF（20～30Hz 目安）
    *hpf20.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 25.0f);
}

juce::AudioProcessorEditor* VoiceModelerAudioProcessor::createEditor()
{
    // まずは Generic で全パラメータ操作可能に
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
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;

    // 出力ゲイン
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::gainDb, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

    // 物理寄りパラメータ
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::transposeSemi, "Transpose (semitones)",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::formantRatio, "Formant Ratio",
        juce::NormalisableRange<float> (0.7f, 1.4f, 0.001f), 1.0f));
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::nasalAmt, "Nasal Amount (%)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    // 反共鳴（ノッチ）
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::nasalNotch, "Nasal Notch (%)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    // RBass ライク
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::rbDriveDb, "RBass Drive (dB)",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f), 6.0f));
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::rbFocusHz, "RBass Focus (Hz)",
        juce::NormalisableRange<float> (60.0f, 160.0f, 0.1f), 100.0f));
    ps.push_back (std::make_unique<juce::AudioParameterFloat>(
        IDs::rbMix, "RBass Mix (%)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));

    // 3-band EQ
    auto addEq = [&ps](const char* fId, const char* gId, const char* qId, const char* fName, const char* gName, const char* qName, float f0)
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat>(
            fId, fName, juce::NormalisableRange<float>(20.0f, 18000.0f, 0.01f, 0.3f), f0));
        ps.push_back (std::make_unique<juce::AudioParameterFloat>(
            gId, gName, juce::NormalisableRange<float>(-18.0f, 18.0f, 0.01f), 0.0f));
        ps.push_back (std::make_unique<juce::AudioParameterFloat>(
            qId, qName, juce::NormalisableRange<float>(0.3f, 5.0f, 0.001f), 1.0f));
    };
    addEq (IDs::eq1Freq, IDs::eq1Gain, IDs::eq1Q, "EQ1 Freq", "EQ1 Gain", "EQ1 Q", 120.0f);
    addEq (IDs::eq2Freq, IDs::eq2Gain, IDs::eq2Q, "EQ2 Freq", "EQ2 Gain", "EQ2 Q", 1000.0f);
    addEq (IDs::eq3Freq, IDs::eq3Gain, IDs::eq3Q, "EQ3 Freq", "EQ3 Gain", "EQ3 Q", 3500.0f);

    return { ps.begin(), ps.end() };
}

// 生成関数
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceModelerAudioProcessor();
}
