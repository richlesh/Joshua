// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "Settings.h"

#include <QColorDialog>
#include <QPushButton>
#include <QComboBox>
#include <QProcess>
#include <QRegularExpression>
#include <QStyleFactory>

SettingsDialog::SettingsDialog(Settings &settings, QWidget *parent)
  : QDialog(parent), ui(new Ui::SettingsDialog), m_settings(settings) {
  ui->setupUi(this);
  setWindowTitle(tr("Settings"));
  setModal(true);

  ui->fontSizeCombo->setStyle(QStyleFactory::create("Fusion"));
  ui->fontSizeCombo->addItems({tr("Small"), tr("Medium"), tr("Large")});
  int fsIdx = ui->fontSizeCombo->findText(m_settings.fontSize());
  if (fsIdx >= 0) ui->fontSizeCombo->setCurrentIndex(fsIdx);

  m_buttonColor = m_settings.fontColor();
  m_treeColor = m_settings.backgroundColor();

  updateSwatch(ui->btnColorPicker, m_buttonColor);
  updateSwatch(ui->treeColorPicker, m_treeColor);

  connect(ui->btnColorPicker, &QPushButton::clicked, this, &SettingsDialog::pickButtonColor);
  connect(ui->treeColorPicker, &QPushButton::clicked, this, &SettingsDialog::pickTreeColor);

  ui->audioCombo->setStyle(QStyleFactory::create("Fusion"));
  ui->audioCombo->addItems({tr("On"), tr("Off")});
  ui->audioCombo->setCurrentIndex(m_settings.audioEnabled() ? 0 : 1);

  ui->voiceCombo->setStyle(QStyleFactory::create("Fusion"));
  populateVoices();

  setFixedSize(sizeHint());
}

SettingsDialog::~SettingsDialog() {
  delete ui;
}

void SettingsDialog::updateSwatch(QPushButton *btn, const QColor &color) {
  btn->setStyleSheet(
    QString("background-color: %1; border: 1px solid #888; min-width: 60px;").arg(color.name()));
  btn->setText(color.name());
}

void SettingsDialog::pickButtonColor() {
  QColor c = QColorDialog::getColor(m_buttonColor, this, tr("Font Color"));
  if (c.isValid()) {
    m_buttonColor = c;
    updateSwatch(ui->btnColorPicker, c);
  }
}

void SettingsDialog::pickTreeColor() {
  QColor c = QColorDialog::getColor(m_treeColor, this, tr("Background Color"));
  if (c.isValid()) {
    m_treeColor = c;
    updateSwatch(ui->treeColorPicker, c);
  }
}

void SettingsDialog::populateVoices() {
  QProcess proc;
#ifdef Q_OS_MACOS
  proc.start("say", {"-v", "?"});
#elif defined(Q_OS_WIN)
  proc.start("powershell", {"-Command",
    "Add-Type -AssemblyName System.Speech; "
    "(New-Object System.Speech.Synthesis.SpeechSynthesizer).GetInstalledVoices() | "
    "ForEach-Object { $_.VoiceInfo.Name }"});
#else
  proc.start("espeak-ng", {"--voices"});
#endif
  proc.waitForFinished(3000);
  QString output = QString::fromUtf8(proc.readAllStandardOutput());
  QStringList voices;
  for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
#ifdef Q_OS_MACOS
    QString name = line.section(' ', 0, 0).trimmed();
    // Multi-word voice names: everything before the language code column
    // Format: "Voice Name    lang_CODE  ..."
    int idx = line.indexOf(QRegularExpression("\\s[a-z]{2}[_-]"));
    if (idx > 0) name = line.left(idx).trimmed();
#elif defined(Q_OS_WIN)
    QString name = line.trimmed();
#else
    // espeak-ng --voices format: columns, voice name is column 4 (index 3)
    QStringList cols = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (cols.size() < 4 || cols[0] == "Pty") continue;
    QString name = cols[3];
#endif
    if (!name.isEmpty()) voices << name;
  }
  if (voices.isEmpty()) voices << "Default";
  ui->voiceCombo->addItems(voices);
  int idx = ui->voiceCombo->findText(m_settings.voiceName());
  if (idx >= 0) ui->voiceCombo->setCurrentIndex(idx);
}

void SettingsDialog::accept() {
  m_settings.setFontSize(ui->fontSizeCombo->currentText());
  m_settings.setFontColor(m_buttonColor);
  m_settings.setBackgroundColor(m_treeColor);
  m_settings.setAudioEnabled(ui->audioCombo->currentIndex() == 0);
  m_settings.setVoiceName(ui->voiceCombo->currentText());
  m_settings.save();
  QDialog::accept();
}
