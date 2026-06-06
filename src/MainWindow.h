// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QKeyEvent>

class Settings;

namespace Ui { class MainWindow; }

class TerminalWidget : public QPlainTextEdit {
  Q_OBJECT
public:
  explicit TerminalWidget(QWidget *parent = nullptr);
  void printText(const QString &text);
  void showPrompt(const QString &prompt);

signals:
  void lineEntered(const QString &line);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  int m_inputStart = 0;
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(Settings &settings, bool licensed, QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  void handleInput(const QString &line);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void applySettings();
  enum State { LogonPrompt, GreetingPrompt, GameMenu, GoFirstPrompt, CheckersGoFirstPrompt, CheckersDifficultyPrompt, CheckersSuggestPrompt, CheckersAutoPlayPrompt, CheckersAutoPlayLevel };

  Ui::MainWindow *ui;
  Settings &m_settings;
  bool m_licensed;
  TerminalWidget *m_terminal;
  State m_state = LogonPrompt;
  bool m_checkersUserFirst = true;
  bool m_checkersSuggest = false;
  int m_checkersDepth = 5;
};

#endif // MAINWINDOW_H
