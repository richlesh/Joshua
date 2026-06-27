// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "ReversiWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <algorithm>
#include <limits>

static const int kSquareSize = 70;
static const int kBoardSize = 8 * kSquareSize;

ReversiWindow::ReversiWindow(bool userFirst, int depth, bool suggest, QWidget *parent)
  : QWidget(parent), m_depth(depth), m_suggest(suggest) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Reversi");
  setFixedSize(kBoardSize, kBoardSize);
  setMouseTracking(true);
  m_userIsBlack = userFirst;
  m_userTurn = userFirst;
  initBoard();

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  if (!m_userTurn)
    QTimer::singleShot(300, this, &ReversiWindow::computerMove);
  else
    computeSuggestion();
}

ReversiWindow::ReversiWindow(bool userFirst, int compDepth, int humanDepth, QWidget *parent)
  : QWidget(parent), m_depth(compDepth), m_suggest(false), m_autoPlay(true), m_humanDepth(humanDepth) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Reversi - Computer vs Computer");
  setFixedSize(kBoardSize, kBoardSize);
  setMouseTracking(true);
  m_userIsBlack = userFirst;
  m_userTurn = false;
  initBoard();

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  QTimer::singleShot(300, this, &ReversiWindow::computerMove);
}

void ReversiWindow::initBoard() {
  m_board.fill(Empty);
  m_board[3 * 8 + 3] = White; m_board[3 * 8 + 4] = Black;
  m_board[4 * 8 + 3] = Black; m_board[4 * 8 + 4] = White;
}

std::vector<int> ReversiWindow::getFlipsDir(int idx, int8_t disc, int dr, int dc) const {
  std::vector<int> flips;
  int r = idx / 8 + dr, c = idx % 8 + dc;
  while (r >= 0 && r < 8 && c >= 0 && c < 8) {
    int8_t val = m_board[r * 8 + c];
    if (val == Empty) return {};
    if (val == disc) return flips;
    flips.push_back(r * 8 + c);
    r += dr; c += dc;
  }
  return {};
}

std::vector<int> ReversiWindow::getFlips(int idx, int8_t disc) const {
  if (m_board[idx] != Empty) return {};
  std::vector<int> all;
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      if (dr == 0 && dc == 0) continue;
      auto f = getFlipsDir(idx, disc, dr, dc);
      all.insert(all.end(), f.begin(), f.end());
    }
  return all;
}

std::vector<int> ReversiWindow::validMoves(int8_t disc) const {
  std::vector<int> moves;
  for (int i = 0; i < 64; i++)
    if (!getFlips(i, disc).empty()) moves.push_back(i);
  return moves;
}

void ReversiWindow::makeMove(int idx, int8_t disc) {
  auto flips = getFlips(idx, disc);
  m_board[idx] = disc;
  for (int f : flips) m_board[f] = disc;
  m_lastPlaced = idx;
  m_flipping = flips;
  // Start flip animation
  m_animating = true;
  m_animFrame = 0;
  connect(&m_animTimer, &QTimer::timeout, this, &ReversiWindow::advanceFlipAnim);
  m_animTimer.start(30);
}

void ReversiWindow::advanceFlipAnim() {
  m_animFrame++;
  update();
  if (m_animFrame >= 8) {
    m_animTimer.stop();
    disconnect(&m_animTimer, &QTimer::timeout, this, &ReversiWindow::advanceFlipAnim);
    m_animating = false;
    m_flipping.clear();
    m_blackTurn = !m_blackTurn;

    // Check for pass/game over
    auto moves = validMoves(m_blackTurn ? Black : White);
    if (moves.empty()) {
      auto oppMoves = validMoves(m_blackTurn ? White : Black);
      if (oppMoves.empty()) { endGame(); return; }
      // Current player passes, switch back
      m_blackTurn = !m_blackTurn;
    }

    if (m_autoPlay) {
      m_userTurn = false;
      update();
      QTimer::singleShot(300, this, &ReversiWindow::computerMove);
    } else {
      // Determine whose turn it is based on m_blackTurn
      bool userPlays = (m_userIsBlack && m_blackTurn) || (!m_userIsBlack && !m_blackTurn);
      m_userTurn = userPlays;
      m_suggestedMove = -1;
      update();
      if (!m_userTurn)
        QTimer::singleShot(300, this, &ReversiWindow::computerMove);
      else
        computeSuggestion();
    }
  }
}

// --- AI ---

static const int kWeights[64] = {
  100, -20, 10,  5,  5, 10, -20, 100,
  -20, -50, -2, -2, -2, -2, -50, -20,
   10,  -2,  1,  1,  1,  1,  -2,  10,
    5,  -2,  1,  0,  0,  1,  -2,   5,
    5,  -2,  1,  0,  0,  1,  -2,   5,
   10,  -2,  1,  1,  1,  1,  -2,  10,
  -20, -50, -2, -2, -2, -2, -50, -20,
  100, -20, 10,  5,  5, 10, -20, 100
};

int ReversiWindow::evaluate() const {
  bool compIsBlack = !m_userIsBlack;
  if (m_autoPlay) compIsBlack = true; // arbitrary for autoplay

  int score = 0;
  int compDiscs = 0, oppDiscs = 0;
  for (int i = 0; i < 64; i++) {
    if (m_board[i] == Empty) continue;
    bool isComp = (compIsBlack && m_board[i] == Black) || (!compIsBlack && m_board[i] == White);
    if (isComp) { score += kWeights[i]; compDiscs++; }
    else { score -= kWeights[i]; oppDiscs++; }
  }

  // Mobility bonus
  int8_t compDisc = compIsBlack ? Black : White;
  int8_t oppDisc = compIsBlack ? White : Black;
  int compMobility = (int)validMoves(compDisc).size();
  int oppMobility = (int)validMoves(oppDisc).size();
  score += (compMobility - oppMobility) * 5;

  // Late game: disc count matters more
  int total = compDiscs + oppDiscs;
  if (total > 52) score += (compDiscs - oppDiscs) * 10;

  return score;
}

int ReversiWindow::minimax(int depth, int alpha, int beta, bool maximizing) {
  bool compIsBlack = !m_userIsBlack;
  if (m_autoPlay) compIsBlack = m_blackTurn; // current mover maximizes

  int8_t disc = maximizing ? (compIsBlack ? Black : White) : (compIsBlack ? White : Black);
  auto moves = validMoves(disc);

  if (depth == 0 || moves.empty()) {
    if (moves.empty()) {
      auto oppMoves = validMoves(disc == Black ? White : Black);
      if (oppMoves.empty()) {
        // Terminal: count discs
        int b = 0, w = 0;
        for (int i = 0; i < 64; i++) {
          if (m_board[i] == Black) b++;
          else if (m_board[i] == White) w++;
        }
        int diff = compIsBlack ? (b - w) : (w - b);
        return diff * 1000;
      }
    }
    return evaluate();
  }

  if (maximizing) {
    int maxEval = std::numeric_limits<int>::min();
    for (int mv : moves) {
      auto saved = m_board;
      auto flips = getFlips(mv, disc);
      m_board[mv] = disc;
      for (int f : flips) m_board[f] = disc;
      int eval = minimax(depth - 1, alpha, beta, false);
      m_board = saved;
      maxEval = std::max(maxEval, eval);
      alpha = std::max(alpha, eval);
      if (beta <= alpha) break;
    }
    return maxEval;
  } else {
    int minEval = std::numeric_limits<int>::max();
    for (int mv : moves) {
      auto saved = m_board;
      auto flips = getFlips(mv, disc);
      m_board[mv] = disc;
      for (int f : flips) m_board[f] = disc;
      int eval = minimax(depth - 1, alpha, beta, true);
      m_board = saved;
      minEval = std::min(minEval, eval);
      beta = std::min(beta, eval);
      if (beta <= alpha) break;
    }
    return minEval;
  }
}

void ReversiWindow::computerMove() {
  if (m_gameOver || m_animating) return;
  setCursor(Qt::WaitCursor);

  int8_t disc = m_blackTurn ? Black : White;
  auto moves = validMoves(disc);
  if (moves.empty()) {
    // Pass
    m_blackTurn = !m_blackTurn;
    auto nextMoves = validMoves(m_blackTurn ? Black : White);
    if (nextMoves.empty()) { endGame(); unsetCursor(); return; }
    if (m_autoPlay)
      QTimer::singleShot(300, this, &ReversiWindow::computerMove);
    else {
      m_userTurn = true;
      unsetCursor();
      QMessageBox::information(this, "Reversi", "The computer has no legal moves and must pass.\nIt's your turn.");
      computeSuggestion();
    }
    update();
    return;
  }

  int searchDepth = m_depth;
  if (m_autoPlay) {
    bool isHumanProxy = (m_userIsBlack && m_blackTurn) || (!m_userIsBlack && !m_blackTurn);
    if (isHumanProxy) searchDepth = m_humanDepth;
  }

  int bestMove = moves[0];
  int bestScore = std::numeric_limits<int>::min();
  for (int mv : moves) {
    auto saved = m_board;
    auto flips = getFlips(mv, disc);
    m_board[mv] = disc;
    for (int f : flips) m_board[f] = disc;
    int score = minimax(searchDepth - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false);
    m_board = saved;
    if (score > bestScore) { bestScore = score; bestMove = mv; }
  }

  unsetCursor();
  makeMove(bestMove, disc);
  update();
}

void ReversiWindow::computeSuggestion() {
  m_suggestedMove = -1;
  if (!m_suggest || !m_userTurn || m_gameOver) return;

  int8_t disc = m_blackTurn ? Black : White;
  auto moves = validMoves(disc);
  if (moves.empty()) return;

  int bestMove = moves[0];
  int bestScore = std::numeric_limits<int>::min();
  for (int mv : moves) {
    auto saved = m_board;
    auto flips = getFlips(mv, disc);
    m_board[mv] = disc;
    for (int f : flips) m_board[f] = disc;
    int score = minimax(8, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false);
    m_board = saved;
    if (score > bestScore) { bestScore = score; bestMove = mv; }
  }
  m_suggestedMove = bestMove;
  update();
}

void ReversiWindow::checkGameOver() {
  auto bMoves = validMoves(Black);
  auto wMoves = validMoves(White);
  if (!bMoves.empty() || !wMoves.empty()) return;
  endGame();
}

void ReversiWindow::endGame() {
  m_gameOver = true;
  int b = 0, w = 0;
  for (int i = 0; i < 64; i++) {
    if (m_board[i] == Black) b++;
    else if (m_board[i] == White) w++;
  }
  if (b == w) m_isDraw = true;
  else m_userWon = (m_userIsBlack && b > w) || (!m_userIsBlack && w > b);
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

// --- Painting ---

void ReversiWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Board background
  p.fillRect(rect(), QColor(0, 100, 0));

  // Grid
  p.setPen(QPen(Qt::black, 1));
  for (int i = 0; i <= 8; i++) {
    p.drawLine(i * kSquareSize, 0, i * kSquareSize, kBoardSize);
    p.drawLine(0, i * kSquareSize, kBoardSize, i * kSquareSize);
  }

  int8_t userDisc = m_userIsBlack ? Black : White;
  auto moves = validMoves(m_blackTurn ? Black : White);

  // Highlight suggested move
  if (m_suggestedMove >= 0 && m_userTurn && !m_gameOver) {
    int sr = m_suggestedMove / 8, sc = m_suggestedMove % 8;
    QRect sq(sc * kSquareSize, sr * kSquareSize, kSquareSize, kSquareSize);
    p.fillRect(sq, QColor(144, 238, 144, 100));
  }

  // Show valid moves as dots
  if (m_userTurn && !m_gameOver && !m_animating) {
    for (int mv : moves) {
      int r = mv / 8, c = mv % 8;
      QPoint ctr(c * kSquareSize + kSquareSize / 2, r * kSquareSize + kSquareSize / 2);
      p.setBrush(QColor(255, 255, 255, 60));
      p.setPen(Qt::NoPen);
      p.drawEllipse(ctr, 8, 8);
    }
  }

  // Draw discs
  int margin = 6;
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      int idx = r * 8 + c;
      int8_t disc = m_board[idx];
      if (disc == Empty) continue;

      QRect sq(c * kSquareSize + margin, r * kSquareSize + margin,
               kSquareSize - 2 * margin, kSquareSize - 2 * margin);

      // Flip animation: scale horizontally
      if (m_animating && std::find(m_flipping.begin(), m_flipping.end(), idx) != m_flipping.end()) {
        double t = m_animFrame / 8.0;
        double scaleX = std::abs(1.0 - 2.0 * t); // shrink to 0 then grow back
        int cx = sq.center().x();
        int newW = (int)(sq.width() * scaleX);
        sq = QRect(cx - newW / 2, sq.y(), newW, sq.height());
      }

      p.setPen(QPen(QColor(40, 40, 40), 1));
      p.setBrush(disc == Black ? Qt::black : Qt::white);
      p.drawEllipse(sq);
    }
  }

  // Hover preview
  if (m_hoverIdx >= 0 && m_userTurn && !m_gameOver && !m_animating) {
    if (std::find(moves.begin(), moves.end(), m_hoverIdx) != moves.end()) {
      int r = m_hoverIdx / 8, c = m_hoverIdx % 8;
      QRect sq(c * kSquareSize + margin, r * kSquareSize + margin,
               kSquareSize - 2 * margin, kSquareSize - 2 * margin);
      QColor ghost = (m_blackTurn) ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 80);
      p.setBrush(ghost);
      p.setPen(Qt::NoPen);
      p.drawEllipse(sq);
    }
  }

  // Disc count
  int b = 0, w = 0;
  for (int i = 0; i < 64; i++) {
    if (m_board[i] == Black) b++;
    else if (m_board[i] == White) w++;
  }
  QFont cf;
  cf.setPixelSize(14);
  cf.setBold(true);
  p.setFont(cf);
  p.setPen(Qt::white);
  p.drawText(QRect(4, kBoardSize - 20, 100, 20), Qt::AlignLeft,
             QString("Black: %1").arg(b));
  p.drawText(QRect(kBoardSize - 104, kBoardSize - 20, 100, 20), Qt::AlignRight,
             QString("White: %1").arg(w));

  // Flash win/loss/draw
  if (m_gameOver && m_flashVisible) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRect(rect());
    QFont gf;
    gf.setPixelSize(60);
    gf.setBold(true);
    p.setFont(gf);
    p.setPen(QColor(255, 255, 0));
    QString msg = m_isDraw ? "DRAW!" : (m_userWon ? "YOU WIN!" : "I WIN!");
    p.drawText(rect(), Qt::AlignCenter, msg);
  }
}

// --- Mouse Events ---

void ReversiWindow::mousePressEvent(QMouseEvent *event) {
  if (m_gameOver || !m_userTurn || m_animating) return;
  int c = event->pos().x() / kSquareSize;
  int r = event->pos().y() / kSquareSize;
  if (r < 0 || r >= 8 || c < 0 || c >= 8) return;
  int idx = r * 8 + c;

  int8_t disc = m_blackTurn ? Black : White;
  auto flips = getFlips(idx, disc);
  if (flips.empty()) return;

  m_suggestedMove = -1;
  makeMove(idx, disc);
  update();
}

void ReversiWindow::mouseMoveEvent(QMouseEvent *event) {
  int c = event->pos().x() / kSquareSize;
  int r = event->pos().y() / kSquareSize;
  int idx = (r >= 0 && r < 8 && c >= 0 && c < 8) ? r * 8 + c : -1;
  if (idx != m_hoverIdx) { m_hoverIdx = idx; update(); }
}

void ReversiWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_P && m_userTurn && !m_gameOver && !m_animating) {
    int8_t disc = m_blackTurn ? Black : White;
    if (validMoves(disc).empty()) {
      // User passes
      m_blackTurn = !m_blackTurn;
      auto nextMoves = validMoves(m_blackTurn ? Black : White);
      if (nextMoves.empty()) { endGame(); return; }
      m_userTurn = false;
      m_suggestedMove = -1;
      update();
      QTimer::singleShot(300, this, &ReversiWindow::computerMove);
    }
  } else {
    QWidget::keyPressEvent(event);
  }
}
