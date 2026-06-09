#pragma once

#include "WavFile.h"

#include <QElapsedTimer>
#include <QTimer>
#include <QVector>
#include <QWidget>

class SpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    void start(const WavData &audio, const QString &title);
    void stop();
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateSpectrum();
    float monoSampleAt(int frame) const;

    WavData m_audio;
    QString m_title;
    QVector<float> m_bars;
    QElapsedTimer m_clock;
    QTimer m_timer;
    bool m_hasAudio = false;
    bool m_running = false;
};
