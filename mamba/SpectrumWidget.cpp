#include "SpectrumWidget.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kBarCount = 64;
constexpr int kWindowSize = 2048;
constexpr float kMinFrequency = 40.0f;
constexpr float kPi = 3.14159265358979323846f;

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

} // namespace

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent),
      m_bars(kBarCount, 0.0f)
{
    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_timer.setInterval(33);
    connect(&m_timer, &QTimer::timeout, this, &SpectrumWidget::updateSpectrum);
}

void SpectrumWidget::start(const WavData &audio, const QString &title)
{
    m_audio = audio;
    m_title = title;
    std::fill(m_bars.begin(), m_bars.end(), 0.0f);
    m_hasAudio = audio.sampleRate > 0 && audio.channels > 0 && !audio.samples.isEmpty();
    m_running = m_hasAudio;

    if (m_running) {
        m_clock.restart();
        m_timer.start();
        updateSpectrum();
    } else {
        m_timer.stop();
        update();
    }
}

void SpectrumWidget::stop()
{
    m_timer.stop();
    m_running = false;
    std::fill(m_bars.begin(), m_bars.end(), 0.0f);
    update();
}

void SpectrumWidget::clear()
{
    m_timer.stop();
    m_audio = {};
    m_title.clear();
    m_hasAudio = false;
    m_running = false;
    std::fill(m_bars.begin(), m_bars.end(), 0.0f);
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect area = rect().adjusted(10, 10, -10, -10);
    painter.fillRect(rect(), QColor(24, 26, 29));
    painter.setPen(QColor(72, 76, 82));
    painter.drawRect(area);

    painter.setPen(QColor(220, 224, 230));
    const QString title = m_title.isEmpty()
        ? QStringLiteral("实时频谱")
        : QStringLiteral("实时频谱 - %1").arg(m_title);
    painter.drawText(area.adjusted(10, 6, -10, -6), Qt::AlignLeft | Qt::AlignTop, title);

    const QRect plot = area.adjusted(12, 32, -12, -24);
    painter.setPen(QColor(52, 56, 62));
    for (int i = 1; i < 4; ++i) {
        const int y = plot.top() + plot.height() * i / 4;
        painter.drawLine(plot.left(), y, plot.right(), y);
    }

    if (!m_hasAudio) {
        painter.setPen(QColor(150, 156, 164));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("播放 WAV 后显示频率"));
        return;
    }

    const float gap = 2.0f;
    const float barWidth = std::max(2.0f, (float(plot.width()) - gap * float(kBarCount - 1)) / float(kBarCount));

    for (int i = 0; i < kBarCount; ++i) {
        const float value = clamp01(m_bars[i]);
        const float x = float(plot.left()) + float(i) * (barWidth + gap);
        const float h = std::max(1.0f, value * float(plot.height()));
        const QRectF bar(x, float(plot.bottom()) - h, barWidth, h);

        QColor color;
        color.setHsvF(0.52 - 0.34 * value, 0.72, 0.92);
        painter.fillRect(bar, color);
    }

    painter.setPen(QColor(150, 156, 164));
    painter.drawText(area.adjusted(12, 0, -12, -6), Qt::AlignLeft | Qt::AlignBottom, QStringLiteral("40 Hz"));
    painter.drawText(area.adjusted(12, 0, -12, -6), Qt::AlignCenter | Qt::AlignBottom, QStringLiteral("1 kHz"));
    painter.drawText(area.adjusted(12, 0, -12, -6), Qt::AlignRight | Qt::AlignBottom,
                     QStringLiteral("%1 kHz").arg(m_audio.sampleRate / 2000));
}

void SpectrumWidget::updateSpectrum()
{
    if (!m_hasAudio) {
        return;
    }

    const int totalFrames = m_audio.samples.size() / m_audio.channels;
    const qint64 elapsedMs = m_clock.elapsed();
    const int centerFrame = int((elapsedMs * qint64(m_audio.sampleRate)) / 1000);
    if (centerFrame >= totalFrames) {
        stop();
        return;
    }

    const int startFrame = std::max(0, centerFrame - kWindowSize / 2);
    const float nyquist = float(m_audio.sampleRate) * 0.5f;
    const float maxFrequency = std::max(kMinFrequency + 1.0f, nyquist);

    QVector<float> nextBars(kBarCount, 0.0f);
    for (int bar = 0; bar < kBarCount; ++bar) {
        const float ratio = float(bar) / float(kBarCount - 1);
        const float frequency = kMinFrequency * std::pow(maxFrequency / kMinFrequency, ratio);
        const double angular = 2.0 * double(kPi) * double(frequency) / double(m_audio.sampleRate);

        double real = 0.0;
        double imag = 0.0;
        for (int n = 0; n < kWindowSize; ++n) {
            const int frame = startFrame + n;
            const float window = 0.5f - 0.5f * std::cos(2.0f * kPi * float(n) / float(kWindowSize - 1));
            const double sample = double(monoSampleAt(frame)) * double(window);
            const double phase = angular * double(n);
            real += sample * std::cos(phase);
            imag -= sample * std::sin(phase);
        }

        const double magnitude = std::sqrt(real * real + imag * imag) / double(kWindowSize);
        const double db = 20.0 * std::log10(magnitude + 1.0e-7);
        nextBars[bar] = clamp01(float((db + 70.0) / 60.0));
    }

    for (int i = 0; i < kBarCount; ++i) {
        const float falling = m_bars[i] * 0.82f;
        m_bars[i] = std::max(nextBars[i], falling);
    }

    update();
}

float SpectrumWidget::monoSampleAt(int frame) const
{
    const int totalFrames = m_audio.samples.size() / m_audio.channels;
    if (totalFrames <= 0) {
        return 0.0f;
    }

    const int clampedFrame = std::max(0, std::min(totalFrames - 1, frame));
    float sum = 0.0f;
    for (int ch = 0; ch < m_audio.channels; ++ch) {
        sum += m_audio.samples[clampedFrame * m_audio.channels + ch];
    }
    return sum / float(m_audio.channels);
}
