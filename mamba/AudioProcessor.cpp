#include "AudioProcessor.h"

#include <algorithm>
#include <cmath>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

float clampSample(float value)
{
    return std::max(-1.0f, std::min(1.0f, value));
}

} // namespace

bool AudioProcessor::prepareNoisesForSource(const WavData &source,
                                            const QVector<WavData> &noises,
                                            QVector<WavData> &preparedNoises,
                                            QString *errorMessage)
{
    if (source.sampleRate <= 0 || source.channels <= 0 ||
        source.samples.size() < source.channels) {
        setError(errorMessage, QStringLiteral("原始音频无效"));
        return false;
    }

    if (noises.isEmpty()) {
        setError(errorMessage, QStringLiteral("请至少选择一个噪声音频"));
        return false;
    }

    preparedNoises.clear();
    preparedNoises.reserve(noises.size());

    for (int i = 0; i < noises.size(); ++i) {
        const WavData &noise = noises[i];
        if (noise.sampleRate <= 0 || noise.channels <= 0 ||
            noise.samples.size() < noise.channels) {
            setError(errorMessage, QStringLiteral("第 %1 个噪声音频无效").arg(i + 1));
            return false;
        }

        preparedNoises.push_back(convertToMatch(noise, source.sampleRate, source.channels));
    }

    return true;
}

WavData AudioProcessor::convertToMatch(const WavData &input,
                                       int targetSampleRate,
                                       int targetChannels)
{
    const int inputFrames = input.samples.size() / input.channels;
    const int targetFrames = std::max(1, int(std::round(double(inputFrames) *
                                                       double(targetSampleRate) /
                                                       double(input.sampleRate))));

    WavData output;
    output.sampleRate = targetSampleRate;
    output.channels = targetChannels;
    output.samples.resize(targetFrames * targetChannels);

    for (int frame = 0; frame < targetFrames; ++frame) {
        const double sourceFrame = double(frame) * double(input.sampleRate) / double(targetSampleRate);
        const int frame0 = std::max(0, std::min(inputFrames - 1, int(std::floor(sourceFrame))));
        const int frame1 = std::max(0, std::min(inputFrames - 1, frame0 + 1));
        const float ratio = float(sourceFrame - double(frame0));

        for (int ch = 0; ch < targetChannels; ++ch) {
            const float s0 = sampleAtFrame(input, frame0, ch, targetChannels);
            const float s1 = sampleAtFrame(input, frame1, ch, targetChannels);
            output.samples[frame * targetChannels + ch] = s0 + (s1 - s0) * ratio;
        }
    }

    return output;
}

float AudioProcessor::sampleAtFrame(const WavData &input,
                                    int frame,
                                    int targetChannel,
                                    int targetChannels)
{
    if (targetChannels == 1 && input.channels > 1) {
        float sum = 0.0f;
        for (int ch = 0; ch < input.channels; ++ch) {
            sum += input.samples[frame * input.channels + ch];
        }
        return sum / float(input.channels);
    }

    const int sourceChannel = input.channels == 1
        ? 0
        : std::min(targetChannel, input.channels - 1);
    return input.samples[frame * input.channels + sourceChannel];
}

bool AudioProcessor::mixWithNoise(const WavData &source,
                                  const QVector<WavData> &noises,
                                  const Options &options,
                                  WavData &out,
                                  QString *errorMessage)
{
    QVector<WavData> preparedNoises;
    if (!prepareNoisesForSource(source, noises, preparedNoises, errorMessage)) {
        return false;
    }

    const float sourceGain = std::max(0.0f, std::min(2.0f, options.sourceGain));
    const float noiseGain = std::max(0.0f, std::min(2.0f, options.noiseGain));
    QVector<float> result(source.samples.size(), 0.0f);

    for (int i = 0; i < source.samples.size(); ++i) {
        float mixed = source.samples[i] * sourceGain;

        for (const WavData &noise : preparedNoises) {
            mixed += noise.samples[i % noise.samples.size()] * noiseGain;
        }

        result[i] = clampSample(mixed);
    }

    out.sampleRate = source.sampleRate;
    out.channels = source.channels;
    out.samples = std::move(result);
    return true;
}

bool AudioProcessor::denoiseWithNlms(const WavData &source,
                                     const QVector<WavData> &noises,
                                     const Options &options,
                                     WavData &out,
                                     QString *errorMessage)
{
    QVector<WavData> preparedNoises;
    if (!prepareNoisesForSource(source, noises, preparedNoises, errorMessage)) {
        return false;
    }

    const float sourceGain = std::max(0.0f, std::min(2.0f, options.sourceGain));
    const float strength = std::max(0.0f, std::min(2.0f, options.denoiseStrength));
    if (strength <= 0.0f) {
        out.sampleRate = source.sampleRate;
        out.channels = source.channels;
        out.samples.resize(source.samples.size());
        for (int i = 0; i < source.samples.size(); ++i) {
            out.samples[i] = clampSample(source.samples[i] * sourceGain);
        }
        return true;
    }

    int filterLength = std::max(8, options.filterLength);
    filterLength = std::min(filterLength, 2048);

    const float adaptationRate = std::min(1.0f, 0.25f * strength);
    const float cancellation = std::min(1.5f, strength);
    constexpr float kEpsilon = 1.0e-6f;

    QVector<QVector<float>> weights(source.channels, QVector<float>(filterLength, 0.0f));
    QVector<QVector<float>> histories(source.channels, QVector<float>(filterLength, 0.0f));
    QVector<int> heads(source.channels, 0);
    QVector<float> result(source.samples.size(), 0.0f);

    for (int i = 0; i < source.samples.size(); ++i) {
        const int ch = i % source.channels;
        QVector<float> &history = histories[ch];
        QVector<float> &weight = weights[ch];

        int &head = heads[ch];
        history[head] = referenceSampleAt(preparedNoises, i);

        double predictedNoise = 0.0;
        double energy = 0.0;
        for (int k = 0; k < filterLength; ++k) {
            int index = head - k;
            if (index < 0) {
                index += filterLength;
            }

            const float x = history[index];
            predictedNoise += double(weight[k]) * double(x);
            energy += double(x) * double(x);
        }

        const float desired = source.samples[i];
        if (energy > kEpsilon) {
            const float error = float(double(desired) - predictedNoise);
            const float normalizedStep = adaptationRate * error / float(energy + kEpsilon);

            for (int k = 0; k < filterLength; ++k) {
                int index = head - k;
                if (index < 0) {
                    index += filterLength;
                }
                weight[k] += normalizedStep * history[index];
            }
        }

        result[i] = clampSample(desired * sourceGain - cancellation * float(predictedNoise));

        ++head;
        if (head == filterLength) {
            head = 0;
        }
    }

    out.sampleRate = source.sampleRate;
    out.channels = source.channels;
    out.samples = std::move(result);
    return true;
}

float AudioProcessor::referenceSampleAt(const QVector<WavData> &noises, int sampleIndex)
{
    if (noises.isEmpty()) {
        return 0.0f;
    }

    float reference = 0.0f;
    for (const WavData &noise : noises) {
        reference += noise.samples[sampleIndex % noise.samples.size()];
    }

    return reference / float(noises.size());
}
