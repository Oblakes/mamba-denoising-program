#include "MainWindow.h"

#include "AudioProcessor.h"
#include "SpectrumWidget.h"
#include "WavFile.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <windows.h>
#include <mmsystem.h>

#include <utility>

namespace {

QString withWavSuffix(const QString &path)
{
    return path.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)
        ? path
        : path + QStringLiteral(".wav");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);

    auto *title = new QLabel(QStringLiteral("带噪音音频合成与去噪"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *formLayout = new QFormLayout();

    m_sourceEdit = new QLineEdit(central);
    m_noiseEdit = new QLineEdit(central);
    m_mixOutputEdit = new QLineEdit(central);
    m_denoiseOutputEdit = new QLineEdit(central);

    auto *sourceButton = new QPushButton(QStringLiteral("选择"), central);
    auto *noiseButton = new QPushButton(QStringLiteral("选择"), central);
    auto *mixOutputButton = new QPushButton(QStringLiteral("保存到"), central);
    auto *denoiseOutputButton = new QPushButton(QStringLiteral("保存到"), central);

    auto *playSourceButton = new QPushButton(QStringLiteral("播放"), central);
    auto *playNoiseButton = new QPushButton(QStringLiteral("播放"), central);
    auto *playMixedButton = new QPushButton(QStringLiteral("播放"), central);
    auto *playDenoisedButton = new QPushButton(QStringLiteral("播放"), central);

    auto *sourceRow = new QHBoxLayout();
    sourceRow->addWidget(m_sourceEdit, 1);
    sourceRow->addWidget(sourceButton);
    sourceRow->addWidget(playSourceButton);

    auto *noiseRow = new QHBoxLayout();
    noiseRow->addWidget(m_noiseEdit, 1);
    noiseRow->addWidget(noiseButton);
    noiseRow->addWidget(playNoiseButton);

    auto *mixOutputRow = new QHBoxLayout();
    mixOutputRow->addWidget(m_mixOutputEdit, 1);
    mixOutputRow->addWidget(mixOutputButton);
    mixOutputRow->addWidget(playMixedButton);

    auto *denoiseOutputRow = new QHBoxLayout();
    denoiseOutputRow->addWidget(m_denoiseOutputEdit, 1);
    denoiseOutputRow->addWidget(denoiseOutputButton);
    denoiseOutputRow->addWidget(playDenoisedButton);

    formLayout->addRow(QStringLiteral("原始音频"), sourceRow);
    formLayout->addRow(QStringLiteral("噪声音频"), noiseRow);
    formLayout->addRow(QStringLiteral("混合输出"), mixOutputRow);
    formLayout->addRow(QStringLiteral("去噪输出"), denoiseOutputRow);

    m_strengthSlider = new QSlider(Qt::Horizontal, central);
    m_strengthSlider->setRange(0, 200);
    m_strengthSlider->setValue(100);
    m_strengthLabel = new QLabel(QStringLiteral("1.00"), central);

    auto *strengthRow = new QHBoxLayout();
    strengthRow->addWidget(m_strengthSlider, 1);
    strengthRow->addWidget(m_strengthLabel);
    formLayout->addRow(QStringLiteral("噪声处理强度"), strengthRow);

    m_processButton = new QPushButton(QStringLiteral("合成带噪音音频"), central);
    m_processButton->setMinimumHeight(38);
    m_denoiseButton = new QPushButton(QStringLiteral("去噪"), central);
    m_denoiseButton->setMinimumHeight(38);
    m_stopButton = new QPushButton(QStringLiteral("停止播放"), central);
    m_stopButton->setMinimumHeight(38);
    m_spectrumWidget = new SpectrumWidget(central);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_processButton);
    buttonRow->addWidget(m_denoiseButton);
    buttonRow->addWidget(m_stopButton);

    m_statusLabel = new QLabel(QStringLiteral("请选择原始音频、至少一个噪声音频，并分别设置混合输出和去噪输出路径。"), central);
    m_statusLabel->setWordWrap(true);

    rootLayout->addWidget(title);
    rootLayout->addLayout(formLayout);
    rootLayout->addLayout(buttonRow);
    rootLayout->addWidget(m_spectrumWidget);
    rootLayout->addWidget(m_statusLabel);
    rootLayout->addStretch();

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Qt 带噪音音频合成与去噪"));
    resize(820, 540);

    connect(sourceButton, &QPushButton::clicked, this, &MainWindow::browseSource);
    connect(noiseButton, &QPushButton::clicked, this, &MainWindow::browseNoise);
    connect(mixOutputButton, &QPushButton::clicked, this, &MainWindow::browseMixOutput);
    connect(denoiseOutputButton, &QPushButton::clicked, this, &MainWindow::browseDenoiseOutput);
    connect(playSourceButton, &QPushButton::clicked, this, &MainWindow::playSource);
    connect(playNoiseButton, &QPushButton::clicked, this, &MainWindow::playNoise);
    connect(playMixedButton, &QPushButton::clicked, this, &MainWindow::playMixed);
    connect(playDenoisedButton, &QPushButton::clicked, this, &MainWindow::playDenoised);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopPlayback);
    connect(m_processButton, &QPushButton::clicked, this, &MainWindow::processAudio);
    connect(m_denoiseButton, &QPushButton::clicked, this, &MainWindow::denoiseAudio);
    connect(m_strengthSlider, &QSlider::valueChanged, this, &MainWindow::updateStrengthLabel);
}

void MainWindow::browseSource()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择原始音频"), QString(), QStringLiteral("WAV 音频 (*.wav)"));
    if (!path.isEmpty()) {
        m_sourceEdit->setText(path);
    }
}

void MainWindow::browseNoise()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择一个或多个噪声音频"), QString(), QStringLiteral("WAV 音频 (*.wav)"));
    if (!paths.isEmpty()) {
        m_noiseEdit->setText(paths.join(QStringLiteral(";")));
    }
}

void MainWindow::browseMixOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存混合输出音频"), QStringLiteral("noisy_mix.wav"), QStringLiteral("WAV 音频 (*.wav)"));
    if (!path.isEmpty()) {
        m_mixOutputEdit->setText(withWavSuffix(path));
    }
}

void MainWindow::browseDenoiseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存去噪输出音频"), QStringLiteral("denoised.wav"), QStringLiteral("WAV 音频 (*.wav)"));
    if (!path.isEmpty()) {
        m_denoiseOutputEdit->setText(withWavSuffix(path));
    }
}

void MainWindow::processAudio()
{
    const QString mixOutputPath = m_mixOutputEdit->text().trimmed();
    if (m_sourceEdit->text().trimmed().isEmpty() || selectedNoisePaths().isEmpty() || mixOutputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入不完整"),
                             QStringLiteral("请先选择原始音频、至少一个噪声音频和混合输出路径。"));
        return;
    }

    setProcessingEnabled(false);
    m_statusLabel->setText(QStringLiteral("正在读取音频..."));

    WavData source;
    QVector<WavData> noises;
    WavData mixed;
    QString error;

    if (!loadInputs(source, noises, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        m_statusLabel->setText(QStringLiteral("音频读取失败。"));
        return;
    }

    AudioProcessor::Options options;
    options.noiseGain = float(m_strengthSlider->value()) / 100.0f;

    m_statusLabel->setText(QStringLiteral("正在合成带噪音音频..."));
    if (!AudioProcessor::mixWithNoise(source, noises, options, mixed, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("处理失败"), error);
        m_statusLabel->setText(QStringLiteral("合成失败。"));
        return;
    }

    if (!WavFile::save16BitPcm(mixOutputPath, mixed, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        m_statusLabel->setText(QStringLiteral("混合音频保存失败。"));
        return;
    }

    setProcessingEnabled(true);
    m_statusLabel->setText(QStringLiteral("混合完成：%1").arg(mixOutputPath));
    QMessageBox::information(this, QStringLiteral("完成"),
                             QStringLiteral("已保存混合带噪音音频：\n%1").arg(mixOutputPath));
}

void MainWindow::denoiseAudio()
{
    const QString mixOutputPath = m_mixOutputEdit->text().trimmed();
    const QString denoiseOutputPath = m_denoiseOutputEdit->text().trimmed();
    if (m_sourceEdit->text().trimmed().isEmpty() || selectedNoisePaths().isEmpty() ||
        mixOutputPath.isEmpty() || denoiseOutputPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入不完整"),
                             QStringLiteral("请先选择原始音频、至少一个噪声音频、混合输出路径和去噪输出路径。"));
        return;
    }

    setProcessingEnabled(false);
    m_statusLabel->setText(QStringLiteral("正在读取音频..."));

    WavData source;
    QVector<WavData> noises;
    WavData mixed;
    WavData denoised;
    QString error;

    if (!loadInputs(source, noises, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("读取失败"), error);
        m_statusLabel->setText(QStringLiteral("音频读取失败。"));
        return;
    }

    AudioProcessor::Options options;
    options.noiseGain = float(m_strengthSlider->value()) / 100.0f;
    options.denoiseStrength = float(m_strengthSlider->value()) / 100.0f;

    m_statusLabel->setText(QStringLiteral("正在生成混合音频..."));
    if (!AudioProcessor::mixWithNoise(source, noises, options, mixed, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("处理失败"), error);
        m_statusLabel->setText(QStringLiteral("混合失败。"));
        return;
    }

    if (!WavFile::save16BitPcm(mixOutputPath, mixed, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        m_statusLabel->setText(QStringLiteral("混合音频保存失败。"));
        return;
    }

    m_statusLabel->setText(QStringLiteral("正在以混合音频为音源去噪..."));
    if (!AudioProcessor::denoiseWithNlms(mixed, noises, options, denoised, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("处理失败"), error);
        m_statusLabel->setText(QStringLiteral("去噪失败。"));
        return;
    }

    if (!WavFile::save16BitPcm(denoiseOutputPath, denoised, &error)) {
        setProcessingEnabled(true);
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        m_statusLabel->setText(QStringLiteral("去噪音频保存失败。"));
        return;
    }

    setProcessingEnabled(true);
    m_statusLabel->setText(QStringLiteral("已分别保存混合音频和去噪音频。"));
    QMessageBox::information(this, QStringLiteral("完成"),
                             QStringLiteral("混合音频：\n%1\n\n去噪音频：\n%2")
                                 .arg(mixOutputPath, denoiseOutputPath));
}

void MainWindow::playSource()
{
    playAudioFile(m_sourceEdit->text().trimmed());
}

void MainWindow::playNoise()
{
    const QStringList paths = selectedNoisePaths();
    if (paths.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法播放"), QStringLiteral("请先选择噪声音频。"));
        return;
    }

    if (paths.size() > 1) {
        m_statusLabel->setText(QStringLiteral("已选择多个噪声音频，正在播放第一个。"));
    }
    playAudioFile(paths.first());
}

void MainWindow::playMixed()
{
    playAudioFile(m_mixOutputEdit->text().trimmed());
}

void MainWindow::playDenoised()
{
    playAudioFile(m_denoiseOutputEdit->text().trimmed());
}

void MainWindow::stopPlayback()
{
    PlaySoundW(nullptr, nullptr, 0);
    m_spectrumWidget->stop();
    m_statusLabel->setText(QStringLiteral("已停止播放。"));
}

void MainWindow::updateStrengthLabel(int value)
{
    m_strengthLabel->setText(QString::number(double(value) / 100.0, 'f', 2));
}

QStringList MainWindow::selectedNoisePaths() const
{
    QStringList paths;
    for (const QString &path : m_noiseEdit->text().split(QStringLiteral(";"), Qt::SkipEmptyParts)) {
        const QString trimmed = path.trimmed();
        if (!trimmed.isEmpty()) {
            paths.push_back(trimmed);
        }
    }
    return paths;
}

bool MainWindow::loadInputs(WavData &source, QVector<WavData> &noises, QString *errorMessage) const
{
    if (!WavFile::load(m_sourceEdit->text().trimmed(), source, errorMessage)) {
        return false;
    }

    const QStringList paths = selectedNoisePaths();
    noises.clear();
    noises.reserve(paths.size());
    for (const QString &path : paths) {
        WavData noise;
        if (!WavFile::load(path, noise, errorMessage)) {
            return false;
        }
        noises.push_back(std::move(noise));
    }

    return true;
}

void MainWindow::playAudioFile(const QString &path)
{
    if (path.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法播放"), QStringLiteral("请先选择或生成音频文件。"));
        return;
    }

    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("无法播放"), QStringLiteral("文件不存在：\n%1").arg(path));
        return;
    }

    const std::wstring nativePath = path.toStdWString();
    if (!PlaySoundW(nativePath.c_str(), nullptr, SND_FILENAME | SND_ASYNC)) {
        QMessageBox::warning(this, QStringLiteral("无法播放"), QStringLiteral("播放失败：\n%1").arg(path));
        return;
    }

    WavData audio;
    QString error;
    if (WavFile::load(path, audio, &error)) {
        m_spectrumWidget->start(audio, QFileInfo(path).fileName());
    } else {
        m_spectrumWidget->clear();
    }

    m_statusLabel->setText(QStringLiteral("正在播放：%1").arg(path));
}

void MainWindow::setProcessingEnabled(bool enabled)
{
    m_processButton->setEnabled(enabled);
    m_denoiseButton->setEnabled(enabled);
}
