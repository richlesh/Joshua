// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "GoWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QThread>
#include <cmath>

GoWindow::GoWindow(bool userFirst, int maxVisits, bool suggest, int boardSize, QWidget *parent)
  : QWidget(parent), m_boardSize(boardSize), m_maxVisits(maxVisits), m_suggest(suggest) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Go");
  int sz = m_boardSize <= 9 ? 500 : (m_boardSize <= 13 ? 600 : 700);
  setFixedSize(sz, sz);
  m_board.resize(m_boardSize * m_boardSize, Empty);
  m_userIsBlack = userFirst;
  m_userTurn = userFirst;
  setMouseTracking(true);

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  startEngine();
  if (!m_userTurn)
    QTimer::singleShot(500, this, &GoWindow::requestComputerMove);
  else if (m_suggest)
    requestSuggestion();
}

GoWindow::GoWindow(bool userFirst, int compVisits, int humanVisits, int boardSize, QWidget *parent)
  : QWidget(parent), m_boardSize(boardSize), m_maxVisits(compVisits), m_suggest(false), m_autoPlay(true), m_humanVisits(humanVisits) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Go - Computer vs Computer");
  int sz = m_boardSize <= 9 ? 500 : (m_boardSize <= 13 ? 600 : 700);
  setFixedSize(sz, sz);
  m_board.resize(m_boardSize * m_boardSize, Empty);
  m_userIsBlack = userFirst;
  m_userTurn = false;
  setMouseTracking(true);

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  startEngine();
  QTimer::singleShot(500, this, &GoWindow::requestComputerMove);
}

void GoWindow::closeEvent(QCloseEvent *event) {
  if (m_engine) { sendGTP("quit"); m_engine->waitForFinished(500); }
  QWidget::closeEvent(event);
}

void GoWindow::startEngine() {
  m_engine = new QProcess(this);
  QString katagoPath = "katago";
  QString modelPath, configPath;

#ifdef Q_OS_MACOS
  QString resDir = QCoreApplication::applicationDirPath() + "/../Resources";
  QString bundled = resDir + "/katago";
  if (QFile::exists(bundled)) katagoPath = bundled;
  else if (QFile::exists("/opt/homebrew/bin/katago")) katagoPath = "/opt/homebrew/bin/katago";
  else if (QFile::exists("/usr/local/bin/katago")) katagoPath = "/usr/local/bin/katago";
  modelPath = resDir + "/katago-model.bin.gz";
  configPath = resDir + "/katago-gtp.cfg";
  // Fallback: check Homebrew locations for model and config
  if (!QFile::exists(configPath)) {
    QStringList cfgPaths = {
      "/opt/homebrew/opt/katago/share/katago/configs/gtp_example.cfg",
      "/usr/local/opt/katago/share/katago/configs/gtp_example.cfg",
      QDir::homePath() + "/.katago/default_gtp.cfg"
    };
    QDir brewDir2("/opt/homebrew/Cellar/katago");
    if (brewDir2.exists()) {
      auto versions = brewDir2.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
      for (auto &v : versions)
        cfgPaths.append(brewDir2.filePath(v + "/share/katago/configs/gtp_example.cfg"));
    }
    for (auto &cp : cfgPaths)
      if (QFile::exists(cp)) { configPath = cp; break; }
  }
  // Fallback: check Homebrew locations for model
  if (!QFile::exists(modelPath)) {
    QStringList brewPaths = {
      "/opt/homebrew/opt/katago/share/katago/kata1-b18c384nbt-s9996604416-d4316597426.bin.gz",
      "/usr/local/opt/katago/share/katago/kata1-b18c384nbt-s9996604416-d4316597426.bin.gz",
      "/opt/homebrew/Cellar/katago/1.16.5/share/katago/kata1-b18c384nbt-s9996604416-d4316597426.bin.gz",
      QDir::homePath() + "/.katago/default_model.bin.gz"
    };
    // Also search dynamically
    QDir brewDir("/opt/homebrew/Cellar/katago");
    if (brewDir.exists()) {
      auto versions = brewDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
      for (auto &v : versions) {
        QString p = brewDir.filePath(v + "/share/katago");
        QDir shareDir(p);
        auto models = shareDir.entryList({"*.bin.gz"}, QDir::Files);
        for (auto &m2 : models) brewPaths.prepend(shareDir.filePath(m2));
      }
    }
    for (auto &p : brewPaths) {
      if (QFile::exists(p)) { modelPath = p; break; }
    }
  }
#elif defined(Q_OS_WIN)
  QString appDir = QCoreApplication::applicationDirPath();
  QString bundled2 = appDir + "/katago.exe";
  if (QFile::exists(bundled2)) katagoPath = bundled2;
  modelPath = appDir + "/katago-model.bin.gz";
  configPath = appDir + "/katago-gtp.cfg";
#else
  QString appDir = QCoreApplication::applicationDirPath();
  QString bundled3 = appDir + "/katago";
  if (QFile::exists(bundled3)) katagoPath = bundled3;
  else if (QFile::exists("/usr/bin/katago-joshua")) katagoPath = "/usr/bin/katago-joshua";
  modelPath = appDir + "/katago-model.bin.gz";
  configPath = appDir + "/katago-gtp.cfg";
#endif

  QStringList args;
  args << "gtp";
  if (QFile::exists(modelPath)) args << "-model" << modelPath;
  if (QFile::exists(configPath)) args << "-config" << configPath;

  // Debug: write paths to temp file
  QFile dbg("/tmp/katago-debug.txt");
  dbg.open(QIODevice::WriteOnly);
  dbg.write(QString("katagoPath: %1 (exists: %2)\nmodelPath: %3 (exists: %4)\nconfigPath: %5 (exists: %6)\nfull args: %7\n")
    .arg(katagoPath).arg(QFile::exists(katagoPath))
    .arg(modelPath).arg(QFile::exists(modelPath))
    .arg(configPath).arg(QFile::exists(configPath))
    .arg(args.join(" ")).toUtf8());
  dbg.close();

  m_engine->start(katagoPath, args);
  if (!m_engine->waitForStarted(5000)) {
    m_engineMissing = true;
    m_engineReady = false;
    return;
  }

  // KataGo takes time to load the neural network - wait for it to be ready
  // Give it a moment to not crash immediately
  QThread::msleep(500);
  if (m_engine->state() != QProcess::Running) {
    m_engineMissing = true;
    m_engineReady = false;
    return;
  }

  sendGTP("name");
  bool gotResponse = false;
  for (int i = 0; i < 60 && !gotResponse; i++) {
    if (m_engine->state() != QProcess::Running) break;
    m_engine->waitForReadyRead(500);
    m_engineBuffer += m_engine->readAllStandardOutput();
    if (m_engineBuffer.contains("\n\n")) gotResponse = true;
  }
  if (!gotResponse) {
    m_engineMissing = true;
    m_engineReady = false;
    return;
  }
  m_engineBuffer.clear();

  sendGTP(QString("boardsize %1").arg(m_boardSize));
  m_engine->waitForReadyRead(3000);
  m_engineBuffer += m_engine->readAllStandardOutput();
  m_engineBuffer.clear();

  sendGTP("clear_board");
  m_engine->waitForReadyRead(2000);
  m_engineBuffer += m_engine->readAllStandardOutput();
  m_engineBuffer.clear();

  sendGTP("komi 6.5");
  m_engine->waitForReadyRead(2000);
  m_engineBuffer += m_engine->readAllStandardOutput();
  m_engineBuffer.clear();

  // Only resign when 99% certain of losing
  sendGTP("kata-set-param resignThreshold -0.99");
  m_engine->waitForReadyRead(1000);
  m_engineBuffer += m_engine->readAllStandardOutput();
  m_engineBuffer.clear();

  connect(m_engine, &QProcess::readyReadStandardOutput, this, &GoWindow::onEngineOutput);
  m_engineReady = true;
}

void GoWindow::sendGTP(const QString &cmd) {
  if (m_engine && m_engine->state() == QProcess::Running)
    m_engine->write((cmd + "\n").toUtf8());
}

void GoWindow::onEngineOutput() {
  m_engineBuffer += m_engine->readAllStandardOutput();

  // GTP responses end with double newline
  while (m_engineBuffer.contains("\n\n")) {
    int idx = m_engineBuffer.indexOf("\n\n");
    QString response = m_engineBuffer.left(idx).trimmed();
    m_engineBuffer = m_engineBuffer.mid(idx + 2);

    if (m_waitingForMove && response.startsWith("=")) {
      m_waitingForMove = false;
      QString move = response.mid(1).trimmed().split(' ').first();
      unsetCursor();
      handleEngineMove(move);
    } else if (m_waitingForSuggestion && response.startsWith("=")) {
      m_waitingForSuggestion = false;
      QString move = response.mid(1).trimmed().split(' ').first();
      // Undo the genmove so engine state is restored
      sendGTP("undo");
      int r, c;
      if (gtpToCoord(move, r, c)) {
        m_suggestedRow = r;
        m_suggestedCol = c;
        update();
      }
    }
  }
}

void GoWindow::requestComputerMove() {
  if (m_gameOver) return;
  if (!m_engineReady) { m_userTurn = true; update(); return; }
  setCursor(Qt::WaitCursor);
  m_waitingForMove = true;
  m_engineBuffer.clear();

  QString color = m_blackTurn ? "black" : "white";
  sendGTP(QString("genmove %1").arg(color));
}

void GoWindow::requestSuggestion() {
  if (!m_suggest || m_gameOver || !m_userTurn || !m_engineReady) return;
  m_waitingForSuggestion = true;
  m_engineBuffer.clear();
  QString color = m_blackTurn ? "black" : "white";
  sendGTP(QString("genmove %1").arg(color));
}

void GoWindow::handleEngineMove(const QString &move) {
  // Handle final_score response (e.g. "B+2.5", "W+0.5", "0")
  if (move.contains("+") || move == "0") {
    endGame(move);
    return;
  }
  if (move.toLower() == "resign") {
    bool computerIsBlack = !m_userIsBlack;
    if (m_autoPlay)
      endGame(m_blackTurn ? "W+Resign" : "B+Resign");
    else
      endGame(m_userIsBlack ? "B+Resign" : "W+Resign"); // computer resigned, user wins
    return;
  }
  if (move.toLower() == "pass") {
    m_consecutivePasses++;
    m_computerPasses++;
    m_lastMoveRow = -1;
    m_lastMoveCol = -1;
    m_blackTurn = !m_blackTurn;
    // End game if both passed consecutively OR computer has passed twice
    if (m_consecutivePasses >= 2 || m_computerPasses >= 2) {
      sendGTP("final_score");
      m_waitingForMove = true;
      return;
    }
    // Inform user the computer passed
    if (!m_autoPlay) {
      QMessageBox::information(this, "Go", "The computer has passed.\nPress P to pass and end the game,\nor continue playing.");
    }
  } else {
    m_consecutivePasses = 0;
    int r, c;
    if (gtpToCoord(move, r, c)) {
      Stone s = m_blackTurn ? Black : White;
      applyMove(r, c, s);
      m_lastMoveRow = r;
      m_lastMoveCol = c;
      m_blackTurn = !m_blackTurn;
    }
  }

  if (m_autoPlay) {
    m_userTurn = false;
    update();
    QTimer::singleShot(300, this, &GoWindow::requestComputerMove);
  } else {
    m_userTurn = !m_userTurn;
    m_suggestedRow = -1;
    m_suggestedCol = -1;
    update();
    if (!m_userTurn)
      QTimer::singleShot(300, this, &GoWindow::requestComputerMove);
    else if (m_suggest)
      requestSuggestion();
  }
}

void GoWindow::applyMove(int row, int col, Stone stone) {
  m_board[row * m_boardSize + col] = stone;
  // Remove captured opponent groups (groups with no liberties)
  Stone opp = (stone == Black) ? White : Black;
  int dirs[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
  for (auto &d : dirs) {
    int nr = row + d[0], nc = col + d[1];
    if (nr < 0 || nr >= m_boardSize || nc < 0 || nc >= m_boardSize) continue;
    if (m_board[nr * m_boardSize + nc] != opp) continue;
    // BFS to find entire group and check for liberties
    std::vector<int> group;
    std::vector<bool> visited(m_boardSize * m_boardSize, false);
    std::vector<int> queue;
    int startIdx = nr * m_boardSize + nc;
    queue.push_back(startIdx);
    visited[startIdx] = true;
    bool hasLiberty = false;
    while (!queue.empty()) {
      int pos = queue.back(); queue.pop_back();
      group.push_back(pos);
      int pr = pos / m_boardSize, pc = pos % m_boardSize;
      for (auto &dd : dirs) {
        int ar = pr + dd[0], ac = pc + dd[1];
        if (ar < 0 || ar >= m_boardSize || ac < 0 || ac >= m_boardSize) continue;
        int ai = ar * m_boardSize + ac;
        if (visited[ai]) continue;
        visited[ai] = true;
        if (m_board[ai] == Empty) hasLiberty = true;
        else if (m_board[ai] == opp) queue.push_back(ai);
      }
    }
    if (!hasLiberty)
      for (int pos2 : group) m_board[pos2] = Empty;
  }
}

void GoWindow::pass() {
  if (m_gameOver || !m_userTurn) return;
  m_consecutivePasses++;
  QString color = m_blackTurn ? "black" : "white";
  sendGTP(QString("play %1 pass").arg(color));
  m_lastMoveRow = -1;
  m_lastMoveCol = -1;
  m_blackTurn = !m_blackTurn;

  if (m_consecutivePasses >= 2) {
    sendGTP("final_score");
    m_waitingForMove = true;
    return;
  }

  m_userTurn = false;
  update();
  QTimer::singleShot(300, this, &GoWindow::requestComputerMove);
}

void GoWindow::endGame(const QString &result) {
  m_gameOver = true;
  // Determine if user won
  bool blackWon = result.startsWith("B+");
  bool whiteWon = result.startsWith("W+");
  m_userWon = (m_userIsBlack && blackWon) || (!m_userIsBlack && whiteWon);
  m_isDraw = result.startsWith("0") || result.contains("Draw");

  // Build friendly message
  if (m_isDraw) {
    m_resultText = "DRAW!";
  } else if (m_autoPlay) {
    m_resultText = blackWon ? "Black wins!" : "White wins!";
  } else if (m_userWon) {
    m_resultText = "YOU WIN!";
  } else {
    m_resultText = "I WIN!";
  }
  // Append score details if available
  if (result.contains("Resign"))
    m_resultText += "\n(by resignation)";
  else if (result.contains("+") && !result.contains("Resign"))
    m_resultText += "\n(" + result + ")";

  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

// --- Painting ---

int GoWindow::margin() { return 30; }
int GoWindow::cellSize() { return (width() - 2 * margin()) / (m_boardSize - 1); }

QPoint GoWindow::stoneCenter(int row, int col) {
  int m = margin(), cs = cellSize();
  return QPoint(m + col * cs, m + row * cs);
}

bool GoWindow::posFromPixel(const QPoint &pos, int &row, int &col) {
  int m = margin(), cs = cellSize();
  col = qRound((pos.x() - m) / (double)cs);
  row = qRound((pos.y() - m) / (double)cs);
  return row >= 0 && row < m_boardSize && col >= 0 && col < m_boardSize;
}

void GoWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Board background
  p.fillRect(rect(), QColor(220, 179, 92));

  int m = margin(), cs = cellSize();

  // Grid lines
  p.setPen(QPen(Qt::black, 1));
  for (int i = 0; i < m_boardSize; i++) {
    p.drawLine(m, m + i * cs, m + (m_boardSize - 1) * cs, m + i * cs);
    p.drawLine(m + i * cs, m, m + i * cs, m + (m_boardSize - 1) * cs);
  }

  // Star points (hoshi)
  int stoneR = cs * 0.43;
  QVector<QPoint> hoshi;
  if (m_boardSize == 19) {
    for (int r : {3, 9, 15}) for (int c : {3, 9, 15}) hoshi.append({r, c});
  } else if (m_boardSize == 13) {
    for (int r : {3, 6, 9}) for (int c : {3, 6, 9}) hoshi.append({r, c});
  } else if (m_boardSize == 9) {
    hoshi = {{2,2},{2,6},{4,4},{6,2},{6,6}};
  }
  for (auto &h : hoshi) {
    QPoint ctr = stoneCenter(h.x(), h.y());
    p.setBrush(Qt::black);
    p.setPen(Qt::NoPen);
    p.drawEllipse(ctr, 4, 4);
  }

  // Highlight suggestion
  if (m_suggestedRow >= 0 && m_suggestedCol >= 0 && !m_gameOver) {
    QPoint ctr = stoneCenter(m_suggestedRow, m_suggestedCol);
    p.setBrush(QColor(144, 238, 144, 150));
    p.setPen(Qt::NoPen);
    p.drawEllipse(ctr, stoneR, stoneR);
  }

  // Stones
  for (int r = 0; r < m_boardSize; r++) {
    for (int c = 0; c < m_boardSize; c++) {
      int8_t s = m_board[r * m_boardSize + c];
      if (s == Empty) continue;
      QPoint ctr = stoneCenter(r, c);
      p.setPen(QPen(Qt::black, 1));
      p.setBrush(s == Black ? Qt::black : Qt::white);
      p.drawEllipse(ctr, stoneR, stoneR);
    }
  }

  // Last move marker
  if (m_lastMoveRow >= 0 && m_lastMoveCol >= 0) {
    QPoint ctr = stoneCenter(m_lastMoveRow, m_lastMoveCol);
    int8_t s = m_board[m_lastMoveRow * m_boardSize + m_lastMoveCol];
    p.setPen(QPen(s == Black ? Qt::white : Qt::black, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ctr, stoneR / 3, stoneR / 3);
  }

  // Hover ghost stone
  if (m_hoverRow >= 0 && m_hoverCol >= 0 && m_userTurn && !m_gameOver) {
    if (m_board[m_hoverRow * m_boardSize + m_hoverCol] == Empty) {
      QPoint ctr = stoneCenter(m_hoverRow, m_hoverCol);
      QColor ghost = m_blackTurn ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 80);
      p.setBrush(ghost);
      p.setPen(Qt::NoPen);
      p.drawEllipse(ctr, stoneR, stoneR);
    }
  }

  // Game-over message
  if (m_gameOver && m_flashVisible) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRect(rect());
    QFont gf;
    gf.setPixelSize(40);
    gf.setBold(true);
    p.setFont(gf);
    p.setPen(QColor(255, 255, 0));
    QString msg = m_resultText.isEmpty() ? (m_userWon ? "YOU WIN!" : "I WIN!") : m_resultText;
    p.drawText(rect(), Qt::AlignCenter, msg);
  }

  // Engine missing warning
  if (m_engineMissing && !m_gameOver) {
    QFont wf;
    wf.setPixelSize(12);
    wf.setBold(true);
    p.setFont(wf);
    p.setPen(QColor(255, 80, 80));
    p.drawText(QRect(0, height() - 18, width(), 18), Qt::AlignCenter,
               "KataGo not found - install with: brew install katago");
  }

  // Pass hint and computer-passed indicator
  if (!m_gameOver && !m_engineMissing && m_userTurn) {
    QFont hf;
    hf.setPixelSize(13);
    hf.setBold(true);
    p.setFont(hf);
    if (m_consecutivePasses > 0) {
      p.setPen(QColor(255, 200, 0));
      p.drawText(QRect(0, 2, width(), 18), Qt::AlignCenter, "Computer passed! Press P to pass and end game");
    } else {
      p.setPen(QColor(255, 255, 255, 180));
      p.drawText(QRect(0, 2, width(), 18), Qt::AlignCenter, "Press P to pass");
    }
  }
}

// --- Mouse Events ---

void GoWindow::mousePressEvent(QMouseEvent *event) {
  if (m_gameOver || !m_userTurn) return;
  int r, c;
  if (!posFromPixel(event->pos(), r, c)) return;
  if (m_board[r * m_boardSize + c] != Empty) return;

  // Play the move
  m_consecutivePasses = 0;
  Stone s = m_blackTurn ? Black : White;
  QString color = m_blackTurn ? "black" : "white";
  sendGTP(QString("play %1 %2").arg(color, coordToGTP(r, c)));

  applyMove(r, c, s);
  m_lastMoveRow = r;
  m_lastMoveCol = c;
  m_blackTurn = !m_blackTurn;
  m_userTurn = false;
  m_suggestedRow = -1;
  m_suggestedCol = -1;
  update();
  QTimer::singleShot(300, this, &GoWindow::requestComputerMove);
}

void GoWindow::mouseMoveEvent(QMouseEvent *event) {
  int r, c;
  if (posFromPixel(event->pos(), r, c)) {
    if (r != m_hoverRow || c != m_hoverCol) {
      m_hoverRow = r;
      m_hoverCol = c;
      update();
    }
  } else {
    if (m_hoverRow >= 0) { m_hoverRow = -1; m_hoverCol = -1; update(); }
  }
}

void GoWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_P && m_userTurn && !m_gameOver)
    pass();
  else
    QWidget::keyPressEvent(event);
}

// --- Helpers ---

QString GoWindow::coordToGTP(int row, int col) {
  // GTP uses columns A-T (skipping I), rows 1-19 from bottom
  char colChar = 'A' + col;
  if (colChar >= 'I') colChar++;
  int gtpRow = m_boardSize - row;
  return QString(QChar(colChar)) + QString::number(gtpRow);
}

bool GoWindow::gtpToCoord(const QString &s, int &row, int &col) {
  if (s.size() < 2) return false;
  char colChar = s[0].toUpper().toLatin1();
  if (colChar >= 'J') colChar--; // GTP skips I
  col = colChar - 'A';
  int gtpRow = s.mid(1).toInt();
  row = m_boardSize - gtpRow;
  return row >= 0 && row < m_boardSize && col >= 0 && col < m_boardSize;
}
