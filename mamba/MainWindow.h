#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class SpectrumWidget;

struct WavData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void browseSource();
    void browseNoise();
    void browseMixOutput();
    void browseDenoiseOutput();
    void processAudio();
    void denoiseAudio();
    void playSource();
    void playNoise();
    void playMixed();
    void playDenoised();
    void stopPlayback();
    void updateStrengthLabel(int value);

    QStringList selectedNoisePaths() const;
    bool loadInputs(WavData &source, QVector<WavData> &noises, QString *errorMessage) const;
    void playAudioFile(const QString &path);
    void setProcessingEnabled(bool enabled);

    QLineEdit *m_sourceEdit = nullptr;
    QLineEdit *m_noiseEdit = nullptr;
    QLineEdit *m_mixOutputEdit = nullptr;
    QLineEdit *m_denoiseOutputEdit = nullptr;
    QSlider *m_strengthSlider = nullptr;
    QLabel *m_strengthLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_processButton = nullptr;
    QPushButton *m_denoiseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    SpectrumWidget *m_spectrumWidget = nullptr;
};
