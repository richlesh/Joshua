// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Settings.h"
#include "TicTacToeWindow.h"
#include "CheckersWindow.h"
#include "FourAcrossWindow.h"
#include "SettingsDialog.h"
#include "AboutDialog.h"
#include "LicenseDialog.h"
#include <QApplication>
#include <QMenuBar>
#include <QTimer>
#include <QProcess>
#include <QResizeEvent>
#include <QFontDatabase>

// --- TerminalWidget ---

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent) {
  setReadOnly(false);
  setLineWrapMode(QPlainTextEdit::WidgetWidth);
  setFont(QFont("JetBrains Mono NL", 14));
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void TerminalWidget::printText(const QString &text) {
  moveCursor(QTextCursor::End);
  insertPlainText(text);
  m_inputStart = textCursor().position();
}

void TerminalWidget::showPrompt(const QString &prompt) {
  printText(prompt);
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
  QTextCursor cursor = textCursor();

  if (cursor.position() < m_inputStart && event->key() != Qt::Key_Return) {
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
  }

  if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Left)
      && cursor.position() <= m_inputStart) {
    return;
  }

  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
    int pos = cursor.position();
    QString input = toPlainText().mid(m_inputStart, pos - m_inputStart);
    insertPlainText("\n");
    m_inputStart = textCursor().position();
    emit lineEntered(input);
    return;
  }

  QPlainTextEdit::keyPressEvent(event);
}

// --- MainWindow ---

static const QStringList yesWords = {"yes", "y", "sure", "ok", "okay", "yep", "yeah", "yup",
  "si", "ja", "oui", "da", "sim", "hai", "ne", "evet", "tak", "ano", "kyllä"};
static const QStringList noWords = {"no", "n", "nope", "nah", "non", "nein", "nie", "nej", "ei"};

static Settings *s_settings = nullptr;

static void speak(const QString &text) {
  if (s_settings && !s_settings->audioEnabled()) return;
  auto *proc = new QProcess;
  proc->connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QProcess::deleteLater);
#ifdef Q_OS_MACOS
  proc->start("say", {"-v", "Fred", text});
#elif defined(Q_OS_WIN)
  proc->start("powershell", {"-Command", QString("Add-Type -AssemblyName System.Speech; "
    "(New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak('%1')").arg(QString(text).replace("'", "''"))});
#else
  // Try espeak-ng first (modern Debian/Ubuntu), fall back to espeak
  auto *check = new QProcess;
  check->start("which", {"espeak-ng"});
  check->waitForFinished(500);
  if (check->exitCode() == 0)
    proc->start("espeak-ng", {text});
  else
    proc->start("espeak", {text});
  delete check;
#endif
}

MainWindow::MainWindow(Settings &settings, bool licensed, QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow), m_settings(settings), m_licensed(licensed) {
  s_settings = &m_settings;
  ui->setupUi(this);
  setWindowTitle("Joshua");
  resize(m_settings.windowSize());

  m_terminal = new TerminalWidget(this);
  setCentralWidget(m_terminal);

  auto *appMenu = menuBar()->addMenu(tr("Joshua"));
  auto *aboutAction = appMenu->addAction(tr("About Joshua"));
  aboutAction->setMenuRole(QAction::AboutRole);
  connect(aboutAction, &QAction::triggered, this, [this]() {
    auto *dlg = new AboutDialog(m_licensed, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
  });
  appMenu->addSeparator();
  auto *settingsAction = appMenu->addAction(tr("Settings..."));
  settingsAction->setMenuRole(QAction::ApplicationSpecificRole);
  connect(settingsAction, &QAction::triggered, this, [this]() {
    auto *dlg = new SettingsDialog(m_settings, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    if (dlg->exec() == QDialog::Accepted)
      applySettings();
  });
  auto *licenseAction = appMenu->addAction(tr("License Key..."));
  licenseAction->setMenuRole(QAction::ApplicationSpecificRole);
  connect(licenseAction, &QAction::triggered, this, [this]() {
    auto *dlg = new LicenseDialog(m_settings, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
  });
  if (ui->statusBar) ui->statusBar->hide();

  connect(m_terminal, &TerminalWidget::lineEntered, this, &MainWindow::handleInput);

  QFontDatabase::addApplicationFont(":/fonts/JetBrainsMonoNL-Regular.ttf");
  applySettings();
  m_state = LogonPrompt;
  m_terminal->showPrompt("LOGON: ");
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  m_settings.setWindowSize(event->size());
  m_settings.save();
}

void MainWindow::applySettings() {
  int basePt = 14;
  QString fs = m_settings.fontSize();
  if (fs == "Medium") basePt = 21;
  else if (fs == "Large") basePt = 28;

  QFont font("JetBrains Mono NL", basePt);
  font.setStyleHint(QFont::Monospace);
  font.setFamilies({"JetBrains Mono NL", "Courier"});
  m_terminal->setFont(font);
  m_terminal->setStyleSheet(
    QString("QPlainTextEdit { background-color: %1; color: %2; border: none; }")
      .arg(m_settings.backgroundColor().name(), m_settings.fontColor().name()));
}

void MainWindow::handleInput(const QString &line) {
  QString input = line.trimmed().toLower();

  switch (m_state) {
  case LogonPrompt:
    if (line.trimmed().compare("Joshua", Qt::CaseInsensitive) == 0) {
      m_terminal->printText("\nGreetings!  Shall we play a game? ");
      speak("Greetings! Shall we play a game?");
      m_state = GreetingPrompt;
    } else {
      m_terminal->showPrompt("LOGON: ");
    }
    break;

  case GreetingPrompt:
    if (noWords.contains(input)) {
      m_terminal->printText("Goodbye.\n");
      speak("Goodbye.");
      QTimer::singleShot(5000, qApp, &QApplication::quit);
    } else if (yesWords.contains(input)) {
      m_terminal->printText("\nGame List:\n");
      m_terminal->printText("1. Tic-Tac-Toe\n2. Checkers\n3. Four Across\n4. Chess\n5. Go\n6. Blackjack\n7. Poker\n8. Global Thermonuclear War\n0. Quit\n\n");
      m_terminal->showPrompt("Which game would you like to play? ");
      speak("Which game would you like to play?");
      m_state = GameMenu;
    } else {
      m_terminal->printText("Shall we play a game? ");
      speak("Shall we play a game?");
    }
    break;

  case GameMenu: {
    bool ok;
    int choice = line.trimmed().toInt(&ok);
    if (ok && choice == 0) {
      m_terminal->printText("Goodbye.\n");
      speak("Goodbye.");
      QTimer::singleShot(5000, qApp, &QApplication::quit);
    } else if (ok && choice == 1) {
      m_terminal->printText("Do you want to go first? ");
      speak("Do you want to go first?");
      m_state = GoFirstPrompt;
    } else if (ok && choice == 2) {
      m_terminal->printText("Do you want to go first? ");
      speak("Do you want to go first?");
      m_state = CheckersGoFirstPrompt;
    } else if (ok && choice == 3) {
      m_terminal->printText("Do you want to go first? ");
      speak("Do you want to go first?");
      m_state = C4GoFirstPrompt;
    } else if (ok && choice == 8) {
      m_terminal->printText("Nuclear war. A strange game, the only winning move is not to play.\nHow about a nice game of chess?\n\n");
      speak("Nuclear war. A strange game, the only winning move is not to play. How about a nice game of chess?");
      m_terminal->showPrompt("Which game would you like to play? ");
    } else if (ok && choice >= 4 && choice <= 7) {
      m_terminal->printText("That game is not currently available.\n\n");
      speak("That game is not currently available.");
      m_terminal->showPrompt("Which game would you like to play? ");
    } else {
      m_terminal->printText("\nGame List:\n");
      m_terminal->printText("1. Tic-Tac-Toe\n2. Checkers\n3. Four Across\n4. Chess\n5. Go\n6. Blackjack\n7. Poker\n8. Global Thermonuclear War\n0. Quit\n\n");
      m_terminal->showPrompt("Which game would you like to play? ");
    }
    break;
  }

  case GoFirstPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      bool userFirst = yesWords.contains(input);
      auto *game = new TicTacToeWindow(userFirst, this);
      game->setAttribute(Qt::WA_DeleteOnClose);
      game->show();
      m_terminal->printText("\n");
      m_terminal->showPrompt("Which game would you like to play? ");
      m_state = GameMenu;
    } else {
      m_terminal->printText("Do you want to go first? ");
      speak("Do you want to go first?");
    }
    break;

  case CheckersGoFirstPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      m_checkersUserFirst = yesWords.contains(input);
      m_terminal->printText("Difficulty (Really Easy, Easy, Medium, Hard)? ");
      speak("Difficulty?");
      m_state = CheckersDifficultyPrompt;
    } else {
      m_terminal->printText("Do you want to go first? ");
      speak("Do you want to go first?");
    }
    break;

  case CheckersDifficultyPrompt: {
    int depth = 0;
    if (input == "really easy" || input == "re" || input == "r") depth = 1;
    else if (input == "easy" || input == "e") depth = 3;
    else if (input == "medium" || input == "m") depth = 5;
    else if (input == "hard" || input == "h") depth = 7;
    if (depth > 0) {
      m_checkersDepth = depth;
      m_terminal->printText("Do you want the computer to play for you? ");
      speak("Do you want the computer to play for you?");
      m_state = CheckersAutoPlayPrompt;
    } else {
      m_terminal->printText("Difficulty (Really Easy, Easy, Medium, Hard)? ");
    }
    break;
  }

  case CheckersAutoPlayPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      if (yesWords.contains(input)) {
        m_terminal->printText("Play level for your player (1-9)? ");
        speak("Play level for your player?");
        m_state = CheckersAutoPlayLevel;
      } else {
        m_terminal->printText("Do you want computer suggestions? ");
        speak("Do you want computer suggestions?");
        m_state = CheckersSuggestPrompt;
      }
    } else {
      m_terminal->printText("Do you want the computer to play for you? ");
      speak("Do you want the computer to play for you?");
    }
    break;

  case CheckersSuggestPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      m_checkersSuggest = yesWords.contains(input);
      auto *game = new CheckersWindow(m_checkersUserFirst, m_checkersDepth, m_checkersSuggest, nullptr);
      game->setAttribute(Qt::WA_DeleteOnClose);
      game->show();
      m_terminal->printText("\n");
      m_terminal->showPrompt("Which game would you like to play? ");
      m_state = GameMenu;
    } else {
      m_terminal->printText("Do you want computer suggestions? ");
      speak("Do you want computer suggestions?");
    }
    break;

  case CheckersAutoPlayLevel: {
    bool ok;
    int level = line.trimmed().toInt(&ok);
    if (ok && level >= 1 && level <= 9) {
      auto *game = new CheckersWindow(m_checkersUserFirst, m_checkersDepth, level, nullptr);
      game->setAttribute(Qt::WA_DeleteOnClose);
      game->show();
      m_terminal->printText("\n");
      m_terminal->showPrompt("Which game would you like to play? ");
      m_state = GameMenu;
    } else {
      m_terminal->printText("Play level for your player (1-9)? ");
    }
    break;
  }

  case C4GoFirstPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      m_c4UserFirst = yesWords.contains(input);
      m_terminal->printText("Difficulty (Easy, Medium, Hard)? ");
      speak("Difficulty?");
      m_state = C4DifficultyPrompt;
    } else {
      m_terminal->printText("Do you want to go first? ");
      speak("Do you want to go first?");
    }
    break;

  case C4DifficultyPrompt: {
    int depth = 0;
    if (input == "easy" || input == "e") depth = 4;
    else if (input == "medium" || input == "m") depth = 6;
    else if (input == "hard" || input == "h") depth = 8;
    if (depth > 0) {
      m_c4Depth = depth;
      m_terminal->printText("Do you want the computer to play for you? ");
      speak("Do you want the computer to play for you?");
      m_state = C4AutoPlayPrompt;
    } else {
      m_terminal->printText("Difficulty (Easy, Medium, Hard)? ");
    }
    break;
  }

  case C4AutoPlayPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      if (yesWords.contains(input)) {
        m_terminal->printText("Play level for your player (1-9)? ");
        speak("Play level for your player?");
        m_state = C4AutoPlayLevel;
      } else {
        m_terminal->printText("Do you want computer suggestions? ");
        speak("Do you want computer suggestions?");
        m_state = C4SuggestPrompt;
      }
    } else {
      m_terminal->printText("Do you want the computer to play for you? ");
      speak("Do you want the computer to play for you?");
    }
    break;

  case C4SuggestPrompt:
    if (yesWords.contains(input) || noWords.contains(input)) {
      m_c4Suggest = yesWords.contains(input);
      auto *game = new FourAcrossWindow(m_c4UserFirst, m_c4Depth, m_c4Suggest, nullptr);
      game->setAttribute(Qt::WA_DeleteOnClose);
      game->show();
      m_terminal->printText("\n");
      m_terminal->showPrompt("Which game would you like to play? ");
      m_state = GameMenu;
    } else {
      m_terminal->printText("Do you want computer suggestions? ");
      speak("Do you want computer suggestions?");
    }
    break;

  case C4AutoPlayLevel: {
    bool ok;
    int level = line.trimmed().toInt(&ok);
    if (ok && level >= 1 && level <= 9) {
      auto *game = new FourAcrossWindow(m_c4UserFirst, m_c4Depth, level, nullptr);
      game->setAttribute(Qt::WA_DeleteOnClose);
      game->show();
      m_terminal->printText("\n");
      m_terminal->showPrompt("Which game would you like to play? ");
      m_state = GameMenu;
    } else {
      m_terminal->printText("Play level for your player (1-9)? ");
    }
    break;
  }
  }
}
