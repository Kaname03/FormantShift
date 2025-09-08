#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class VoiceModelerAudioProcessor : public juce::AudioProcessor
{
public:
    VoiceModelerAudioProcessor();
    ~VoiceModelerAudioProcessor() override = default;

    //=== AudioProcessor overrides ===
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VoiceModeler"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Params
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //=== Params (raw pointers) ===
    std::atomic<float>* pGainDb        = nullptr; // 出力ゲイン(dB)
    std::atomic<float>* pFormantRatio  = nullptr; // 0.7–1.4
    std::atomic<float>* pNasalAmt      = nullptr; // 0–100 %

    std::atomic<float>* pRBassDriveDb  = nullptr; // 0–24 dB
    std::atomic<float>* pRBassFocusHz  = nullptr; // 60–160 Hz
    std::atomic<float>* pRBassMix      = nullptr; // 0–100 %

    std::atomic<float>* pEQ1Freq = nullptr; std::atomic<float>* pEQ1Gain = nullptr; std::atomic<float>* pEQ1Q = nullptr;
    std::atomic<float>* pEQ2Freq = nullptr; std::atomic<float>* pEQ2Gain = nullptr; std::atomic<float>* pEQ2Q = nullptr;
    std::atomic<float>* pEQ3Freq = nullptr; std::atomic<float>* pEQ3Gain = nullptr; std::atomic<float>* pEQ3Q = nullptr;

    std::atomic<float>* pNasalNotch = nullptr;   // 0–100 %

    //=== DSP blocks ===
    using Peak = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                juce::dsp::IIR::Coefficients<float>>;
    Peak formantF1, formantF2, formantF3;         // フォルマント模擬（ピーク）
    Peak nasal1k, nasal3k;                         // 鼻腔レゾナンス（ピーク）
    Peak nasalNotch1k, nasalNotch3k;               // 鼻腔反共鳴（ノッチ相当：負ゲインのピーク）
    Peak eq1, eq2, eq3;                            // 3バンドEQ
    Peak hpf20;                                    // サブソニックHPF

    juce::dsp::Gain<float> outGain;                // 出力ゲイン
    juce::dsp::Compressor<float> limiter;          // リミッター

    // RBass 生成用：モノ抽出＋BPF
    juce::AudioBuffer<float> rbassMono;
    Peak rbassBand;

    // Smoothers
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothOutGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothRBassMix;

    double sr = 48000.0;
    int maxBlock = 0;

    // ユーティリティ
    inline float softClip (float x) noexcept { return juce::dsp::FastMathApproximations::tanh (x); }

    // 内部処理
    void updateFilters();
    void processRBass (juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceModelerAudioProcessor)
};
