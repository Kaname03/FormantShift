#pragma once
#include <juce_dsp/juce_dsp.h>

// 軽量・低遅延な簡易ピッチシフタ（2タップ＋クロスフェード）
// 高品位化したい場合は RubberBand / PSOLA 等に差し替え可能
class SimplePitchShifter
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels,
                  int windowSizeSamples = 1024)
    {
        juce::ignoreUnused (maxBlock);

        sr = sampleRate;
        channels = numChannels;
        windowSize = juce::jmax (256, windowSizeSamples);
        bufLen = juce::nextPowerOfTwo (windowSize * 8);

        buffer.setSize (channels, bufLen);
        buffer.clear();
        writePos = 0;

        phaseA = 0.0f;
        phaseB = 0.5f;

        setSemitone (0.0f);
    }

    void reset()
    {
        buffer.clear();
        writePos = 0;
        phaseA = 0.0f;
        phaseB = 0.5f;
    }

    // 半音指定（例：+7, -5 など）
    void setSemitone (float semi)
    {
        ratio = std::pow (2.0f, semi / 12.0f);
        readSpeed = 1.0f / ratio; // 読み速度（原音=1.0）
    }

    // in-placeで処理（ステレオ想定）
    void process (juce::AudioBuffer<float>& block)
    {
        const int numSamples = block.getNumSamples();
        const int chs = juce::jmin (channels, block.getNumChannels());

        for (int i = 0; i < numSamples; ++i)
        {
            // 書き込み
            for (int ch = 0; ch < chs; ++ch)
                buffer.setSample (ch, writePos, block.getSample (ch, i));

            // 2本の読み取りタップ（0.5位相ずれ）
            float posA = getReadPosFromPhase (phaseA);
            float posB = getReadPosFromPhase (phaseB);

            float wA = hannWindow (phaseA);
            float wB = hannWindow (phaseB);

            for (int ch = 0; ch < chs; ++ch)
            {
                float a = readInterp (ch, posA);
                float b = readInterp (ch, posB);
                block.setSample (ch, i, a * wA + b * wB);
            }

            // 位相を進める
            phaseA += readSpeed / (float)windowSize;
            phaseB += readSpeed / (float)windowSize;
            if (phaseA >= 1.0f) phaseA -= 1.0f;
            if (phaseB >= 1.0f) phaseB -= 1.0f;

            // 書きポインタ前進（リング）
            writePos = (writePos + 1) & (bufLen - 1);
        }
    }

private:
    juce::AudioBuffer<float> buffer;
    int bufLen = 0, writePos = 0, windowSize = 1024, channels = 2;
    double sr = 48000.0;

    float ratio = 1.0f;
    float readSpeed = 1.0f;
    float phaseA = 0.0f, phaseB = 0.5f;

    inline float getReadPosFromPhase (float p) const noexcept
    {
        float offset = p * (float)windowSize;
        float rp = (float)writePos - offset;
        while (rp < 0.0f) rp += (float)bufLen;
        while (rp >= (float)bufLen) rp -= (float)bufLen;
        return rp;
    }

    inline float hannWindow (float p) const noexcept
    {
        return 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * p));
    }

    inline float readInterp (int ch, float pos) const noexcept
    {
        int i0 = (int)pos;
        int i1 = (i0 + 1) & (bufLen - 1);
        float frac = pos - (float)i0;

        float s0 = buffer.getSample (ch, i0);
        float s1 = buffer.getSample (ch, i1);
        return s0 + (s1 - s0) * frac;
    }
};
