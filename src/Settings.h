// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QByteArray>
#include <QColor>
#include <QSize>
#include <QString>

class Settings {
public:
  Settings();

  void load();
  void save();

  QColor fontColor() const;
  void setFontColor(const QColor &color);
  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);
  QString theme() const;
  void setTheme(const QString &theme);
  QString fontSize() const;
  void setFontSize(const QString &size);
  QString userName() const;
  void setUserName(const QString &name);
  QString licenseKey() const;
  void setLicenseKey(const QString &key);
  QSize windowSize() const;
  void setWindowSize(const QSize &size);
  QByteArray headerState() const;
  void setHeaderState(const QByteArray &state);
  bool audioEnabled() const;
  void setAudioEnabled(bool enabled);

private:
  QString settingsFilePath() const;

  QColor m_fontColor{0, 255, 0};
  QColor m_backgroundColor{0, 0, 0};
  QString m_theme{"System"};
  QString m_fontSize{"Small"};
  QString m_userName;
  QString m_licenseKey;
  QSize m_windowSize{800, 600};
  QByteArray m_headerState;
  bool m_audioEnabled{true};
};

#endif // SETTINGS_H
