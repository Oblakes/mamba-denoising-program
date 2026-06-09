#pragma once

#include "WavFile.h"

#include <QString>

class AudioProcessor
{
public:
    struct Options
    {
        float sourceGain = 1.0f;
        float noiseGain = 1.0f;
        float denoiseStrength = 1.0f;
        int filterLength = 257;
    };

    static bool mixWithNoise(const WavData &source,
                             const QVector<WavData> &noises,
                             const Options &options,
                             WavData &out,
                             QString *errorMessage = nullptr);

    static bool denoiseWithNlms(const WavData &source,
                                const QVector<WavData> &noises,
                                const Options &options,
                                WavData &out,
                                QString *errorMessage = nullptr);

private:
    static bool prepareNoisesForSource(const WavData &source,
                                       const QVector<WavData> &noises,
                                       QVector<WavData> &preparedNoises,
                                       QString *errorMessage);
    static WavData convertToMatch(const WavData &input,
                                  int targetSampleRate,
                                  int targetChannels);
    static float sampleAtFrame(const WavData &input,
                               int frame,
                               int targetChannel,
                               int targetChannels);
    static float referenceSampleAt(const QVector<WavData> &noises, int sampleIndex);
};
