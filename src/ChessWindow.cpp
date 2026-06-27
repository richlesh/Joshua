// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "ChessWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QFile>
#include <cmath>

static const int kSquareSize = 70;
static const int kBoardSize = 8 * kSquareSize;

ChessWindow::ChessWindow(bool userFirst, int elo, bool suggest, QWidget *parent)
  : QWidget(parent), m_elo(elo), m_suggest(suggest) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Chess");
  setFixedSize(kBoardSize, kBoardSize);

  m_userIsWhite = userFirst;
  m_userTurn = userFirst;
  initBoard();

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  startEngine(m_engine, m_engineReady, m_elo);
  if (m_suggest)
    startEngine(m_suggestEngine, m_suggestEngineReady, 3200);

  m_legalMoves = generateLegalMoves();

  if (!m_userTurn)
    QTimer::singleShot(500, this, &ChessWindow::requestComputerMove);
  else if (m_suggest)
    requestSuggestion();
}

ChessWindow::ChessWindow(bool userFirst, int compElo, int humanElo, QWidget *parent)
  : QWidget(parent), m_elo(compElo), m_suggest(false), m_autoPlay(true), m_humanElo(humanElo) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Chess - Computer vs Computer");
  setFixedSize(kBoardSize, kBoardSize);

  m_userIsWhite = userFirst;
  m_userTurn = false;
  initBoard();

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  startEngine(m_engine, m_engineReady, m_elo);
  m_legalMoves = generateLegalMoves();
  QTimer::singleShot(500, this, &ChessWindow::requestComputerMove);
}

void ChessWindow::initBoard() {
  m_board.fill(Empty);
  const int8_t backRank[] = {WRook, WKnight, WBishop, WQueen, WKing, WBishop, WKnight, WRook};
  for (int c = 0; c < 8; c++) {
    m_board[0 * 8 + c] = static_cast<int8_t>(-backRank[c]);
    m_board[1 * 8 + c] = BPawn;
    m_board[6 * 8 + c] = WPawn;
    m_board[7 * 8 + c] = backRank[c];
  }
}

void ChessWindow::closeEvent(QCloseEvent *event) {
  if (m_engine) { m_engine->write("quit\n"); m_engine->waitForFinished(500); }
  if (m_suggestEngine) { m_suggestEngine->write("quit\n"); m_suggestEngine->waitForFinished(500); }
  QWidget::closeEvent(event);
}

// --- Engine Management ---

void ChessWindow::startEngine(QProcess *&proc, bool &readyFlag, int elo) {
  proc = new QProcess(this);
  QString sfPath = "stockfish";
#ifdef Q_OS_MACOS
  QString bundled = QCoreApplication::applicationDirPath() + "/../Resources/stockfish";
  if (QFile::exists(bundled)) sfPath = bundled;
#elif defined(Q_OS_WIN)
  QString bundled2 = QCoreApplication::applicationDirPath() + "/stockfish.exe";
  if (QFile::exists(bundled2)) sfPath = bundled2;
#else
  QString bundled3 = QCoreApplication::applicationDirPath() + "/stockfish";
  if (QFile::exists(bundled3)) sfPath = bundled3;
  else {
    QString installed = "/usr/bin/stockfish-joshua";
    if (QFile::exists(installed)) sfPath = installed;
  }
#endif

  if (proc == m_engine)
    connect(proc, &QProcess::readyReadStandardOutput, this, &ChessWindow::onEngineOutput);
  else
    connect(proc, &QProcess::readyReadStandardOutput, this, &ChessWindow::onSuggestOutput);

  proc->start(sfPath, QStringList());
  if (!proc->waitForStarted(3000)) {
    readyFlag = false;
    m_engineMissing = true;
    return;
  }

  sendToEngine(proc, "uci");
  proc->waitForReadyRead(2000);
  sendToEngine(proc, "setoption name UCI_LimitStrength value true");
  sendToEngine(proc, QString("setoption name UCI_Elo value %1").arg(elo));
  sendToEngine(proc, "isready");
  proc->waitForReadyRead(2000);
  readyFlag = true;
}

void ChessWindow::sendToEngine(QProcess *proc, const QString &cmd) {
  if (proc && proc->state() == QProcess::Running)
    proc->write((cmd + "\n").toUtf8());
}

QString ChessWindow::positionCommand() {
  if (m_moveHistory.isEmpty())
    return "position startpos";
  return "position startpos moves " + m_moveHistory.join(" ");
}

void ChessWindow::requestComputerMove() {
  if (m_gameOver) return;
  if (!m_engineReady) {
    m_userTurn = true; // let user keep playing both sides
    update();
    return;
  }
  setCursor(Qt::WaitCursor);
  m_waitingForMove = true;
  m_engineOutput.clear();

  // Count pieces to detect endgame
  int pieceCount = 0;
  for (int i = 0; i < 64; i++)
    if (m_board[i] != Empty) pieceCount++;

  // In endgame (<=10 pieces), disable strength limit and increase think time
  int moveTime = 1000;
  if (pieceCount <= 10) {
    sendToEngine(m_engine, "setoption name UCI_LimitStrength value false");
    moveTime = 3000;
  } else if (pieceCount <= 16) {
    sendToEngine(m_engine, "setoption name UCI_LimitStrength value true");
    moveTime = 2000;
  } else {
    sendToEngine(m_engine, "setoption name UCI_LimitStrength value true");
  }

  sendToEngine(m_engine, positionCommand());
  sendToEngine(m_engine, QString("go movetime %1").arg(moveTime));
}

void ChessWindow::requestSuggestion() {
  if (!m_suggest || m_gameOver || !m_userTurn) return;
  QProcess *eng = m_suggestEngine ? m_suggestEngine : m_engine;
  bool ready = m_suggestEngine ? m_suggestEngineReady : m_engineReady;
  if (!ready) return;

  if (m_suggestEngine) {
    m_waitingForSuggestion = true;
    m_suggestOutput.clear();
    sendToEngine(m_suggestEngine, positionCommand());
    sendToEngine(m_suggestEngine, "go movetime 2000");
  } else {
    // Use main engine for suggestion when no separate engine
    m_waitingForSuggestion = true;
    m_engineOutput.clear();
    sendToEngine(m_engine, positionCommand());
    sendToEngine(m_engine, "go movetime 2000");
  }
}

void ChessWindow::onEngineOutput() {
  m_engineOutput += m_engine->readAllStandardOutput();

  if (m_waitingForSuggestion && m_engineOutput.contains("bestmove")) {
    m_waitingForSuggestion = false;
    int idx = m_engineOutput.lastIndexOf("bestmove");
    QStringList parts = m_engineOutput.mid(idx).split(' ', Qt::SkipEmptyParts);
    m_engineOutput.clear();
    if (parts.size() >= 2 && parts[1] != "(none)") {
      m_suggestedFrom = uciToIndex(parts[1].left(2));
      m_suggestedTo = uciToIndex(parts[1].mid(2, 2));
      update();
    }
    return;
  }

  if (m_waitingForMove && m_engineOutput.contains("bestmove")) {
    m_waitingForMove = false;
    int idx = m_engineOutput.lastIndexOf("bestmove");
    QStringList parts = m_engineOutput.mid(idx).split(' ', Qt::SkipEmptyParts);
    m_engineOutput.clear();
    unsetCursor();

    if (parts.size() >= 2 && parts[1] != "(none)") {
      QString move = parts[1];
      m_moveHistory.append(move);
      int from = uciToIndex(move.left(2));
      int to = uciToIndex(move.mid(2, 2));
      int8_t piece = m_board[from];
      int capturedIdx = (m_board[to] != Empty) ? to : -1;
      if ((piece == WPawn || piece == BPawn) && (from % 8 != to % 8) && m_board[to] == Empty)
        capturedIdx = from / 8 * 8 + to % 8;
      startAnimation(from, to, piece, capturedIdx);
    } else {
      // Engine says no move - game is over
      if (inCheck(m_whiteTurn))
        endGame(m_whiteTurn != m_userIsWhite); // checkmate
      else
        endDraw(); // stalemate
    }
  }
}

void ChessWindow::onSuggestOutput() {
  m_suggestOutput += m_suggestEngine->readAllStandardOutput();
  if (m_waitingForSuggestion && m_suggestOutput.contains("bestmove")) {
    m_waitingForSuggestion = false;
    int idx = m_suggestOutput.lastIndexOf("bestmove");
    QStringList parts = m_suggestOutput.mid(idx).split(' ', Qt::SkipEmptyParts);
    m_suggestOutput.clear();
    if (parts.size() >= 2 && parts[1] != "(none)") {
      m_suggestedFrom = uciToIndex(parts[1].left(2));
      m_suggestedTo = uciToIndex(parts[1].mid(2, 2));
      update();
    }
  }
}

// --- Local Move Generation ---

static bool onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

QStringList ChessWindow::generatePseudoMoves(bool forWhite) {
  QStringList moves;
  for (int idx = 0; idx < 64; idx++) {
    int8_t p = m_board[idx];
    if (p == Empty) continue;
    if (forWhite && p < 0) continue;
    if (!forWhite && p > 0) continue;
    int r = idx / 8, c = idx % 8;
    int absP = std::abs(p);

    if (absP == 1) { // Pawn
      int dir = forWhite ? -1 : 1;
      int startRow = forWhite ? 6 : 1;
      int promoRow = forWhite ? 0 : 7;
      // Forward
      int nr = r + dir;
      if (onBoard(nr, c) && m_board[nr * 8 + c] == Empty) {
        if (nr == promoRow) {
          for (QChar pr : {'q','r','b','n'})
            moves.append(indexToUCI(idx) + indexToUCI(nr * 8 + c) + pr);
        } else {
          moves.append(indexToUCI(idx) + indexToUCI(nr * 8 + c));
          // Double push
          if (r == startRow && m_board[(r + 2 * dir) * 8 + c] == Empty)
            moves.append(indexToUCI(idx) + indexToUCI((r + 2 * dir) * 8 + c));
        }
      }
      // Captures
      for (int dc : {-1, 1}) {
        int nc = c + dc;
        if (!onBoard(nr, nc)) continue;
        int tgt = nr * 8 + nc;
        bool canCapture = (m_board[tgt] != Empty && ((forWhite && m_board[tgt] < 0) || (!forWhite && m_board[tgt] > 0)));
        bool isEP = (tgt == m_epSquare);
        if (canCapture || isEP) {
          if (nr == promoRow) {
            for (QChar pr : {'q','r','b','n'})
              moves.append(indexToUCI(idx) + indexToUCI(tgt) + pr);
          } else {
            moves.append(indexToUCI(idx) + indexToUCI(tgt));
          }
        }
      }
    } else if (absP == 2) { // Knight
      static const int kd[][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
      for (auto &d : kd) {
        int nr = r + d[0], nc = c + d[1];
        if (!onBoard(nr, nc)) continue;
        int8_t t = m_board[nr * 8 + nc];
        if (t == Empty || (forWhite && t < 0) || (!forWhite && t > 0))
          moves.append(indexToUCI(idx) + indexToUCI(nr * 8 + nc));
      }
    } else if (absP == 3 || absP == 4 || absP == 5) { // Bishop, Rook, Queen
      static const int dirs[][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
      int start = 0, end = 8;
      if (absP == 3) { start = 0; end = 4; } // diagonals only (indices 0,2,5,7 won't work - use proper)
      // Actually let's just use the right dirs
      std::vector<std::pair<int,int>> slideDir;
      if (absP == 3) slideDir = {{-1,-1},{-1,1},{1,-1},{1,1}};
      else if (absP == 4) slideDir = {{-1,0},{1,0},{0,-1},{0,1}};
      else slideDir = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
      for (auto [dr, dcc] : slideDir) {
        int nr = r + dr, nc = c + dcc;
        while (onBoard(nr, nc)) {
          int8_t t = m_board[nr * 8 + nc];
          if (t == Empty) {
            moves.append(indexToUCI(idx) + indexToUCI(nr * 8 + nc));
          } else {
            if ((forWhite && t < 0) || (!forWhite && t > 0))
              moves.append(indexToUCI(idx) + indexToUCI(nr * 8 + nc));
            break;
          }
          nr += dr; nc += dcc;
        }
      }
    } else if (absP == 6) { // King
      static const int kd[][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
      for (auto &d : kd) {
        int nr = r + d[0], nc = c + d[1];
        if (!onBoard(nr, nc)) continue;
        int8_t t = m_board[nr * 8 + nc];
        if (t == Empty || (forWhite && t < 0) || (!forWhite && t > 0))
          moves.append(indexToUCI(idx) + indexToUCI(nr * 8 + nc));
      }
      // Castling
      if (forWhite && r == 7 && c == 4) {
        if (m_whiteKingSide && m_board[7*8+5] == Empty && m_board[7*8+6] == Empty &&
            !isSquareAttacked(7*8+4, false) && !isSquareAttacked(7*8+5, false) && !isSquareAttacked(7*8+6, false))
          moves.append("e1g1");
        if (m_whiteQueenSide && m_board[7*8+3] == Empty && m_board[7*8+2] == Empty && m_board[7*8+1] == Empty &&
            !isSquareAttacked(7*8+4, false) && !isSquareAttacked(7*8+3, false) && !isSquareAttacked(7*8+2, false))
          moves.append("e1c1");
      } else if (!forWhite && r == 0 && c == 4) {
        if (m_blackKingSide && m_board[0*8+5] == Empty && m_board[0*8+6] == Empty &&
            !isSquareAttacked(0*8+4, true) && !isSquareAttacked(0*8+5, true) && !isSquareAttacked(0*8+6, true))
          moves.append("e8g8");
        if (m_blackQueenSide && m_board[0*8+3] == Empty && m_board[0*8+2] == Empty && m_board[0*8+1] == Empty &&
            !isSquareAttacked(0*8+4, true) && !isSquareAttacked(0*8+3, true) && !isSquareAttacked(0*8+2, true))
          moves.append("e8c8");
      }
    }
  }
  return moves;
}

bool ChessWindow::isSquareAttacked(int sq, bool byWhite) {
  int r = sq / 8, c = sq % 8;
  // Knight attacks
  static const int kd[][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
  for (auto &d : kd) {
    int nr = r + d[0], nc = c + d[1];
    if (!onBoard(nr, nc)) continue;
    int8_t p = m_board[nr * 8 + nc];
    if (byWhite && p == WKnight) return true;
    if (!byWhite && p == BKnight) return true;
  }
  // King attacks
  static const int kk[][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
  for (auto &d : kk) {
    int nr = r + d[0], nc = c + d[1];
    if (!onBoard(nr, nc)) continue;
    int8_t p = m_board[nr * 8 + nc];
    if (byWhite && p == WKing) return true;
    if (!byWhite && p == BKing) return true;
  }
  // Pawn attacks
  int pawnDir = byWhite ? 1 : -1; // white pawns attack upward (lower row numbers)
  for (int dc : {-1, 1}) {
    int nr = r + pawnDir, nc = c + dc;
    if (!onBoard(nr, nc)) continue;
    int8_t p = m_board[nr * 8 + nc];
    if (byWhite && p == WPawn) return true;
    if (!byWhite && p == BPawn) return true;
  }
  // Sliding pieces (bishop/queen diagonals, rook/queen straights)
  static const int diag[][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
  for (auto &d : diag) {
    int nr = r + d[0], nc = c + d[1];
    while (onBoard(nr, nc)) {
      int8_t p = m_board[nr * 8 + nc];
      if (p != Empty) {
        if (byWhite && (p == WBishop || p == WQueen)) return true;
        if (!byWhite && (p == BBishop || p == BQueen)) return true;
        break;
      }
      nr += d[0]; nc += d[1];
    }
  }
  static const int straight[][2] = {{-1,0},{1,0},{0,-1},{0,1}};
  for (auto &d : straight) {
    int nr = r + d[0], nc = c + d[1];
    while (onBoard(nr, nc)) {
      int8_t p = m_board[nr * 8 + nc];
      if (p != Empty) {
        if (byWhite && (p == WRook || p == WQueen)) return true;
        if (!byWhite && (p == BRook || p == BQueen)) return true;
        break;
      }
      nr += d[0]; nc += d[1];
    }
  }
  return false;
}

int ChessWindow::findKing(bool white) {
  int8_t king = white ? WKing : BKing;
  for (int i = 0; i < 64; i++)
    if (m_board[i] == king) return i;
  return -1;
}

bool ChessWindow::inCheck(bool white) {
  int kingSq = findKing(white);
  if (kingSq < 0) return true;
  return isSquareAttacked(kingSq, !white);
}

bool ChessWindow::moveLeavesSafe(const QString &uci, bool forWhite) {
  // Save state
  auto savedBoard = m_board;
  int savedEP = m_epSquare;
  bool savedWK = m_whiteKingSide, savedWQ = m_whiteQueenSide;
  bool savedBK = m_blackKingSide, savedBQ = m_blackQueenSide;
  int savedClock = m_halfMoveClock;
  auto savedHistory = m_positionHistory.size();

  applyUCIMove(uci);
  bool safe = !inCheck(forWhite);

  // Restore state
  m_board = savedBoard;
  m_epSquare = savedEP;
  m_whiteKingSide = savedWK; m_whiteQueenSide = savedWQ;
  m_blackKingSide = savedBK; m_blackQueenSide = savedBQ;
  m_halfMoveClock = savedClock;
  m_positionHistory.resize(savedHistory);
  return safe;
}

QStringList ChessWindow::generateLegalMoves() {
  QStringList pseudo = generatePseudoMoves(m_whiteTurn);
  QStringList legal;
  for (const QString &mv : pseudo) {
    if (moveLeavesSafe(mv, m_whiteTurn))
      legal.append(mv);
  }
  return legal;
}

// --- Move Application ---

void ChessWindow::applyUCIMove(const QString &uci) {
  int from = uciToIndex(uci.left(2));
  int to = uciToIndex(uci.mid(2, 2));
  int8_t piece = m_board[from];
  bool isCapture = (m_board[to] != Empty);
  bool isPawnMove = (std::abs(piece) == 1);

  // En passant is also a capture
  if (isPawnMove && from % 8 != to % 8 && m_board[to] == Empty)
    isCapture = true;

  // Update castling rights
  if (piece == WKing) { m_whiteKingSide = false; m_whiteQueenSide = false; }
  if (piece == BKing) { m_blackKingSide = false; m_blackQueenSide = false; }
  if (piece == WRook && from == 63) m_whiteKingSide = false;
  if (piece == WRook && from == 56) m_whiteQueenSide = false;
  if (piece == BRook && from == 7) m_blackKingSide = false;
  if (piece == BRook && from == 0) m_blackQueenSide = false;
  // Rook captured
  if (to == 63) m_whiteKingSide = false;
  if (to == 56) m_whiteQueenSide = false;
  if (to == 7) m_blackKingSide = false;
  if (to == 0) m_blackQueenSide = false;

  // Handle castling (king moves 2 squares)
  if ((piece == WKing || piece == BKing) && std::abs(from % 8 - to % 8) == 2) {
    int rookFrom, rookTo;
    if (to % 8 == 6) { rookFrom = from - from % 8 + 7; rookTo = from - from % 8 + 5; }
    else { rookFrom = from - from % 8; rookTo = from - from % 8 + 3; }
    m_board[rookTo] = m_board[rookFrom];
    m_board[rookFrom] = Empty;
  }

  // Handle en passant capture
  if ((piece == WPawn || piece == BPawn) && (from % 8 != to % 8) && m_board[to] == Empty) {
    int epCapture = from / 8 * 8 + to % 8;
    m_board[epCapture] = Empty;
  }

  // Update en passant square
  m_epSquare = -1;
  if ((piece == WPawn || piece == BPawn) && std::abs(from / 8 - to / 8) == 2) {
    m_epSquare = (from + to) / 2; // square between from and to
  }

  m_board[to] = piece;
  m_board[from] = Empty;

  // Promotion
  if (uci.size() == 5) {
    QChar promo = uci[4];
    bool white = isWhite(piece);
    if (promo == 'q') m_board[to] = white ? WQueen : BQueen;
    else if (promo == 'r') m_board[to] = white ? WRook : BRook;
    else if (promo == 'b') m_board[to] = white ? WBishop : BBishop;
    else if (promo == 'n') m_board[to] = white ? WKnight : BKnight;
  }

  // Update half-move clock (reset on pawn move or capture)
  if (isPawnMove || isCapture) {
    m_halfMoveClock = 0;
    m_positionHistory.clear(); // irreversible move resets repetition tracking
  } else {
    m_halfMoveClock++;
  }

  m_positionHistory.push_back(m_board);
}

// --- Animation ---

void ChessWindow::startAnimation(int from, int to, int8_t piece, int capturedIdx) {
  m_animating = true;
  m_animFrom = from;
  m_animTo = to;
  m_animPiece = piece;
  m_animCapturedIdx = capturedIdx;
  m_animFrame = 0;
  m_animStart = squareCenter(from);
  m_animEnd = squareCenter(to);
  m_animPos = m_animStart;
  connect(&m_animTimer, &QTimer::timeout, this, &ChessWindow::advanceAnimation);
  m_animTimer.start(20);
}

void ChessWindow::advanceAnimation() {
  m_animFrame++;
  double t = (double)m_animFrame / m_animFrames;
  if (t >= 1.0) t = 1.0;
  m_animPos = m_animStart + t * (m_animEnd - m_animStart);
  update();

  if (t >= 1.0) {
    m_animTimer.stop();
    disconnect(&m_animTimer, &QTimer::timeout, this, &ChessWindow::advanceAnimation);
    m_animating = false;

    applyUCIMove(m_moveHistory.last());
    m_whiteTurn = !m_whiteTurn;
    m_legalMoves = generateLegalMoves();

    // Check game over
    if (m_legalMoves.isEmpty()) {
      if (inCheck(m_whiteTurn)) {
        bool whiteJustMoved = !m_whiteTurn;
        endGame(whiteJustMoved == m_userIsWhite);
      } else {
        endDraw();
      }
      return;
    }
    if (checkDrawConditions()) return;

    if (m_autoPlay) {
      m_userTurn = false;
      update();
      QTimer::singleShot(300, this, &ChessWindow::requestComputerMove);
    } else {
      m_userTurn = !m_userTurn;
      m_suggestedFrom = -1;
      m_suggestedTo = -1;
      update();
      if (!m_userTurn)
        QTimer::singleShot(300, this, &ChessWindow::requestComputerMove);
      else if (m_suggest)
        requestSuggestion();
    }
  }
}

bool ChessWindow::checkDrawConditions() {
  // 50-move rule (100 half-moves)
  if (m_halfMoveClock >= 100) { endDraw(); return true; }

  // Threefold repetition (only positions with same side to move)
  int count = 0;
  int histSize = (int)m_positionHistory.size();
  // Current position is at index histSize-1. Same side to move at histSize-3, histSize-5, etc.
  for (int i = histSize - 1; i >= 0; i -= 2)
    if (m_positionHistory[i] == m_board && ++count >= 3) { endDraw(); return true; }

  // Insufficient material
  int wN = 0, wB = 0, bN = 0, bB = 0, others = 0;
  for (int i = 0; i < 64; i++) {
    int8_t p = m_board[i];
    if (p == Empty || p == WKing || p == BKing) continue;
    if (p == WKnight) wN++;
    else if (p == WBishop) wB++;
    else if (p == BKnight) bN++;
    else if (p == BBishop) bB++;
    else others++;
  }
  if (others == 0) {
    int wMinor = wN + wB, bMinor = bN + bB;
    // K vs K, K+B vs K, K+N vs K, K+B vs K+B (same color - skip that complexity)
    if (wMinor + bMinor == 0) { endDraw(); return true; }
    if (wMinor == 1 && bMinor == 0) { endDraw(); return true; }
    if (wMinor == 0 && bMinor == 1) { endDraw(); return true; }
  }

  return false;
}

void ChessWindow::endGame(bool userWon) {
  m_gameOver = true;
  m_userWon = userWon;
  m_isDraw = false;
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

void ChessWindow::endDraw() {
  m_gameOver = true;
  m_isDraw = true;
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

// --- Painting ---

void ChessWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      int dr = displayRow(r), dc = displayCol(c);
      QRect sq(dc * kSquareSize, dr * kSquareSize, kSquareSize, kSquareSize);
      p.fillRect(sq, ((r + c) % 2 == 0) ? QColor(240, 217, 181) : QColor(181, 136, 99));
    }
  }

  // Highlight suggestion
  if (m_suggestedFrom >= 0 && !m_dragging && m_selected < 0) {
    int sr = m_suggestedFrom / 8, sc = m_suggestedFrom % 8;
    p.fillRect(QRect(displayCol(sc) * kSquareSize, displayRow(sr) * kSquareSize, kSquareSize, kSquareSize),
               QColor(144, 238, 144, 150));
    if (m_suggestedTo >= 0) {
      int tr = m_suggestedTo / 8, tc = m_suggestedTo % 8;
      p.fillRect(QRect(displayCol(tc) * kSquareSize, displayRow(tr) * kSquareSize, kSquareSize, kSquareSize),
                 QColor(144, 238, 144, 150));
    }
  }

  // Highlight selected and valid targets
  if (m_selected >= 0) {
    int sr = m_selected / 8, sc = m_selected % 8;
    p.fillRect(QRect(displayCol(sc) * kSquareSize, displayRow(sr) * kSquareSize, kSquareSize, kSquareSize),
               QColor(100, 200, 100, 120));
  }
  for (int idx : m_validTargets) {
    int hr = idx / 8, hc = idx % 8;
    p.fillRect(QRect(displayCol(hc) * kSquareSize, displayRow(hr) * kSquareSize, kSquareSize, kSquareSize),
               QColor(100, 200, 255, 100));
  }

  // Draw pieces
  QFont f;
  f.setPixelSize(kSquareSize - 16);
  p.setFont(f);
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      int idx = r * 8 + c;
      int8_t piece = m_board[idx];
      if (piece == Empty) continue;
      if (m_dragging && idx == m_dragFrom) continue;
      if (m_animating && idx == m_animFrom) continue;
      if (m_animating && idx == m_animCapturedIdx) continue;
      int dr = displayRow(r), dc = displayCol(c);
      QRect sq(dc * kSquareSize, dr * kSquareSize, kSquareSize, kSquareSize);
      p.setPen(Qt::black);
      p.drawText(sq, Qt::AlignCenter, pieceChar(piece));
    }
  }

  // Animating piece
  if (m_animating) {
    QRect sq(m_animPos.x() - kSquareSize / 2, m_animPos.y() - kSquareSize / 2, kSquareSize, kSquareSize);
    p.setPen(Qt::black);
    p.drawText(sq, Qt::AlignCenter, pieceChar(m_animPiece));
  }

  // Dragged piece
  if (m_dragging && m_dragFrom >= 0) {
    QRect sq(m_dragPos.x() - kSquareSize / 2, m_dragPos.y() - kSquareSize / 2, kSquareSize, kSquareSize);
    p.setPen(Qt::black);
    p.drawText(sq, Qt::AlignCenter, pieceChar(m_board[m_dragFrom]));
  }

  // Flash game-over message
  if (m_gameOver && m_flashVisible) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRect(rect());
    QFont gf;
    gf.setPixelSize(60);
    gf.setBold(true);
    p.setFont(gf);
    p.setPen(QColor(255, 255, 0));
    p.drawText(rect(), Qt::AlignCenter, m_isDraw ? "DRAW!" : (m_userWon ? "YOU WIN!" : "I WIN!"));
  }

  // Engine missing warning
  if (m_engineMissing && !m_gameOver) {
    QFont wf;
    wf.setPixelSize(12);
    wf.setBold(true);
    p.setFont(wf);
    p.setPen(QColor(255, 80, 80));
    p.drawText(QRect(0, kBoardSize - 18, kBoardSize, 18), Qt::AlignCenter,
               "Stockfish not found - install with: brew install stockfish");
  }
}

// --- Mouse Events ---

void ChessWindow::mousePressEvent(QMouseEvent *event) {
  if (m_gameOver || !m_userTurn || m_animating) return;
  int dc = event->pos().x() / kSquareSize;
  int dr = event->pos().y() / kSquareSize;
  int c = displayCol(dc), r = displayRow(dr);
  if (r < 0 || r > 7 || c < 0 || c > 7) return;
  int idx = r * 8 + c;
  int8_t piece = m_board[idx];
  bool ownPiece = (m_userIsWhite && isWhite(piece)) || (!m_userIsWhite && isBlack(piece));
  if (ownPiece) {
    m_selected = idx;
    m_dragging = true;
    m_dragFrom = idx;
    m_dragPos = event->pos();
    m_validTargets.clear();
    QString fromSq = indexToUCI(idx);
    for (const QString &mv : m_legalMoves)
      if (mv.startsWith(fromSq))
        m_validTargets.push_back(uciToIndex(mv.mid(2, 2)));
    update();
  }
}

void ChessWindow::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging) { m_dragPos = event->pos(); update(); }
}

void ChessWindow::mouseReleaseEvent(QMouseEvent *event) {
  if (!m_dragging) return;
  m_dragging = false;
  int dc = event->pos().x() / kSquareSize;
  int dr = event->pos().y() / kSquareSize;
  int c = displayCol(dc), r = displayRow(dr);
  if (r < 0 || r > 7 || c < 0 || c > 7) {
    m_selected = -1; m_validTargets.clear(); update(); return;
  }
  int idx = r * 8 + c;
  QString fromSq = indexToUCI(m_dragFrom);
  QString toSq = indexToUCI(idx);
  QString moveUCI;

  // Check for promotion (pick queen by default)
  int8_t piece = m_board[m_dragFrom];
  bool isPromotion = (piece == WPawn && r == 0) || (piece == BPawn && r == 7);

  for (const QString &mv : m_legalMoves) {
    if (mv.startsWith(fromSq + toSq)) {
      moveUCI = mv;
      if (isPromotion && mv.size() == 5 && mv[4] == 'q')
        break; // prefer queen promotion
      if (!isPromotion)
        break;
    }
  }

  if (!moveUCI.isEmpty()) {
    m_moveHistory.append(moveUCI);
    m_suggestedFrom = -1;
    m_suggestedTo = -1;
    m_selected = -1;
    m_validTargets.clear();
    applyUCIMove(moveUCI);
    m_whiteTurn = !m_whiteTurn;
    m_legalMoves = generateLegalMoves();
    m_userTurn = false;
    update();

    if (m_legalMoves.isEmpty()) {
      if (inCheck(m_whiteTurn))
        endGame(true); // user just checkmated the computer
      else
        endDraw();
    } else if (!checkDrawConditions()) {
      QTimer::singleShot(300, this, &ChessWindow::requestComputerMove);
    }
  } else {
    m_selected = -1;
    m_validTargets.clear();
    update();
  }
}

// --- Helpers ---

QPointF ChessWindow::squareCenter(int idx) {
  int r = idx / 8, c = idx % 8;
  return QPointF(displayCol(c) * kSquareSize + kSquareSize / 2.0,
                 displayRow(r) * kSquareSize + kSquareSize / 2.0);
}

int ChessWindow::uciToIndex(const QString &sq) {
  if (sq.size() < 2) return -1;
  int c = sq[0].toLatin1() - 'a';
  int r = 8 - (sq[1].toLatin1() - '0');
  return r * 8 + c;
}

QString ChessWindow::indexToUCI(int idx) {
  int r = idx / 8, c = idx % 8;
  return QString(QChar('a' + c)) + QString::number(8 - r);
}

QString ChessWindow::pieceChar(int8_t p) {
  switch (p) {
  case WKing:   return QStringLiteral("\u2654");
  case WQueen:  return QStringLiteral("\u2655");
  case WRook:   return QStringLiteral("\u2656");
  case WBishop: return QStringLiteral("\u2657");
  case WKnight: return QStringLiteral("\u2658");
  case WPawn:   return QStringLiteral("\u2659");
  case BKing:   return QStringLiteral("\u265A");
  case BQueen:  return QStringLiteral("\u265B");
  case BRook:   return QStringLiteral("\u265C");
  case BBishop: return QStringLiteral("\u265D");
  case BKnight: return QStringLiteral("\u265E");
  case BPawn:   return QStringLiteral("\u265F");
  default:      return QString();
  }
}
