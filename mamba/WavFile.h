#pragma once

#include <QString>
#include <QVector>

struct WavData
{
    int sampleRate = 0;
    int channels = 0;
    QVector<float> samples; // Interleaved, normalized to [-1.0, 1.0].
};

class WavFile
{
public:
    static bool load(const QString &path, WavData &out, QString *errorMessage = nullptr);
    static bool save16BitPcm(const QString &path, const WavData &data, QString *errorMessage = nullptr);
};
