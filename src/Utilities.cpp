// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "Settings.h"
#include "Utilities.h"

#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QApplication>
#include <QImage>
#include <QProcess>
#include <QFile>

QPixmap roundedPixmap(const QPixmap &src, int radius) {
  QPixmap result(src.size());
  result.fill(Qt::transparent);
  QPainter p(&result);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(result.rect(), radius, radius);
  p.setClipPath(path);
  p.drawPixmap(0, 0, src);
  return result;
}

QIcon themedIcon(const QString &path) {
  QPixmap px(path);
  bool dark = qApp->palette().window().color().lightness() < 128;
  if (dark) {
    QImage img = px.toImage();
    img.invertPixels(QImage::InvertRgb);
    px = QPixmap::fromImage(img);
  }
  return QIcon(px);
}

void speak(const QString &text) {
  // Speak the lesson if audio is enabled
  Settings settings;
  settings.load();
  if (!settings.audioEnabled()) return;
  auto *proc = new QProcess;
  proc->connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QProcess::deleteLater);
  QString voice = (settings.voiceName().isEmpty()) ? settings.voiceName() : QString();
#ifdef Q_OS_MACOS
  QStringList args;
  args << "-v" << (voice.isEmpty() ? "Fred" : voice) << text;
  proc->start("say", args);
#elif defined(Q_OS_WIN)
  QString voiceSelect;
  if (voice.isEmpty()) voice = "Microsoft David Desktop";
  voiceSelect = QString("$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
    "$s.SelectVoice('%1'); $s.Speak('%2')").arg(voice, QString(text).replace("'", "''"));
  proc->start("powershell", {"-Command", "Add-Type -AssemblyName System.Speech; " + voiceSelect});
#else
  QStringList args;
  if (voice.isEmpty()) voice = "en";
  args << "-v" << voice;
  args << text;
  if (QFile::exists("/usr/bin/espeak-ng")) {
    proc->start("/usr/bin/espeak-ng", args);
  } else if (QFile::exists("/usr/bin/espeak")) {
    proc->start("/usr/bin/espeak", args);
  } else {
    proc->start("espeak-ng", args);
  }
#endif
}
