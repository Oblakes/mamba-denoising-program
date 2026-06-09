#include "WavFile.h"

#include <QDataStream>
#include <QFile>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace {

QString readFourCc(QDataStream &stream)
{
    char id[4] = {};
    if (stream.readRawData(id, 4) != 4) {
        return {};
    }
    return QString::fromLatin1(id, 4);
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

float readSigned24(const uchar *p)
{
    int value = int(p[0]) | (int(p[1]) << 8) | (int(p[2]) << 16);
    if (value & 0x800000) {
        value |= ~0xFFFFFF;
    }
    return float(value) / 8388608.0f;
}

float clampSample(float value)
{
    return std::max(-1.0f, std::min(1.0f, value));
}

} // namespace

bool WavFile::load(const QString &path, WavData &out, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("无法打开文件：%1").arg(path));
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    if (readFourCc(stream) != QStringLiteral("RIFF")) {
        setError(errorMessage, QStringLiteral("不是 RIFF WAV 文件"));
        return false;
    }

    quint32 riffSize = 0;
    stream >> riffSize;
    Q_UNUSED(riffSize);

    if (readFourCc(stream) != QStringLiteral("WAVE")) {
        setError(errorMessage, QStringLiteral("不是 WAVE 文件"));
        return false;
    }

    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 bitsPerSample = 0;
    QByteArray audioBytes;
    bool hasFmt = false;

    while (!stream.atEnd()) {
        const QString chunkId = readFourCc(stream);
        if (chunkId.size() != 4) {
            break;
        }

        quint32 chunkSize = 0;
        stream >> chunkSize;

        if (chunkId == QStringLiteral("fmt ")) {
            hasFmt = true;
            quint32 byteRate = 0;
            quint16 blockAlign = 0;
            stream >> audioFormat >> channels >> sampleRate >> byteRate >> blockAlign >> bitsPerSample;
            Q_UNUSED(byteRate);
            Q_UNUSED(blockAlign);

            const qint64 remaining = qint64(chunkSize) - 16;
            if (remaining > 0) {
                file.seek(file.pos() + remaining);
            }
        } else if (chunkId == QStringLiteral("data")) {
            audioBytes = file.read(chunkSize);
        } else {
            file.seek(file.pos() + chunkSize);
        }

        if (chunkSize % 2 == 1) {
            file.seek(file.pos() + 1);
        }
    }

    if (!hasFmt || audioBytes.isEmpty()) {
        setError(errorMessage, QStringLiteral("WAV 文件缺少 fmt 或 data 数据块"));
        return false;
    }

    if (channels == 0 || sampleRate == 0) {
        setError(errorMessage, QStringLiteral("WAV 声道数或采样率无效"));
        return false;
    }

    if (!((audioFormat == 1 && (bitsPerSample == 8 || bitsPerSample == 16 ||
                                bitsPerSample == 24 || bitsPerSample == 32)) ||
          (audioFormat == 3 && bitsPerSample == 32))) {
        setError(errorMessage, QStringLiteral("仅支持 PCM 8/16/24/32 位或 32 位浮点 WAV"));
        return false;
    }

    const int bytesPerSample = bitsPerSample / 8;
    const int totalSamples = audioBytes.size() / bytesPerSample;
    QVector<float> samples;
    samples.reserve(totalSamples);

    const uchar *raw = reinterpret_cast<const uchar *>(audioBytes.constData());
    for (int i = 0; i < totalSamples; ++i) {
        const uchar *p = raw + i * bytesPerSample;
        float value = 0.0f;

        if (audioFormat == 3) {
            float floatValue = 0.0f;
            std::memcpy(&floatValue, p, sizeof(float));
            value = clampSample(floatValue);
        } else if (bitsPerSample == 8) {
            value = (float(int(p[0]) - 128)) / 128.0f;
        } else if (bitsPerSample == 16) {
            const qint16 v = qint16(int(p[0]) | (int(p[1]) << 8));
            value = float(v) / 32768.0f;
        } else if (bitsPerSample == 24) {
            value = readSigned24(p);
        } else if (bitsPerSample == 32) {
            const qint32 v = qint32(quint32(p[0]) | (quint32(p[1]) << 8) |
                                   (quint32(p[2]) << 16) | (quint32(p[3]) << 24));
            value = float(v) / 2147483648.0f;
        }

        samples.push_back(clampSample(value));
    }

    out.sampleRate = int(sampleRate);
    out.channels = int(channels);
    out.samples = std::move(samples);
    return true;
}

bool WavFile::save16BitPcm(const QString &path, const WavData &data, QString *errorMessage)
{
    if (data.sampleRate <= 0 || data.channels <= 0 || data.samples.isEmpty()) {
        setError(errorMessage, QStringLiteral("没有可保存的音频数据"));
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage, QStringLiteral("无法写入文件：%1").arg(path));
        return false;
    }

    const quint16 audioFormat = 1;
    const quint16 channels = quint16(data.channels);
    const quint32 sampleRate = quint32(data.sampleRate);
    const quint16 bitsPerSample = 16;
    const quint16 blockAlign = channels * bitsPerSample / 8;
    const quint32 byteRate = sampleRate * blockAlign;
    const quint32 dataSize = quint32(data.samples.size() * sizeof(qint16));
    const quint32 riffSize = 36 + dataSize;

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream.writeRawData("RIFF", 4);
    stream << riffSize;
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << audioFormat << channels << sampleRate << byteRate << blockAlign << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataSize;

    for (float sample : data.samples) {
        const float clamped = clampSample(sample);
        const qint16 pcm = qint16(std::lrint(clamped * 32767.0f));
        stream << pcm;
    }

    return true;
}
