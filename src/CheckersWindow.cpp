// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "CheckersWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <cstdlib>

static const int kSquareSize = 70;
static const int kBoardSize = 8 * kSquareSize;
static const int kPieceMargin = 8;

CheckersWindow::CheckersWindow(bool userFirst, int depth, bool suggest, QWidget *parent)
  : QWidget(parent), m_depth(depth), m_suggest(suggest) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Checkers");
  setFixedSize(kBoardSize, kBoardSize);

  m_userIsLight = !userFirst;
  m_userTurn = userFirst;

  initBoard();

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  if (!m_userTurn) {
    QTimer::singleShot(300, this, &CheckersWindow::computerMove);
  } else {
    computeSuggestion();
  }
}

CheckersWindow::CheckersWindow(bool userFirst, int compDepth, int humanDepth, QWidget *parent)
  : QWidget(parent), m_depth(compDepth), m_suggest(false), m_autoPlay(true), m_humanDepth(humanDepth) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Checkers - Computer vs Computer");
  setFixedSize(kBoardSize, kBoardSize);

  m_userIsLight = !userFirst;
  m_userTurn = false;

  initBoard();

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });

  QTimer::singleShot(300, this, &CheckersWindow::computerMove);
}

void CheckersWindow::initBoard() {
  m_board.fill(Empty);
  // Dark pieces in rows 0-2, Light pieces in rows 5-7
  // Dark squares are where (row+col) is odd
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 8; c++)
      if ((r + c) % 2 == 1) m_board[r * 8 + c] = Dark;
  for (int r = 5; r < 8; r++)
    for (int c = 0; c < 8; c++)
      if ((r + c) % 2 == 1) m_board[r * 8 + c] = Light;
}

void CheckersWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Draw board
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      int dr = displayRow(r), dc = displayCol(c);
      QRect sq(dc * kSquareSize, dr * kSquareSize, kSquareSize, kSquareSize);
      if ((r + c) % 2 == 0)
        p.fillRect(sq, QColor(240, 240, 240));
      else
        p.fillRect(sq, QColor(160, 160, 160));
    }
  }

  // Highlight suggested piece
  if (m_suggestedFrom >= 0 && !m_dragging && m_selected < 0) {
    int sr = m_suggestedFrom / 8, sc = m_suggestedFrom % 8;
    int dsr = displayRow(sr), dsc = displayCol(sc);
    QRect sq(dsc * kSquareSize, dsr * kSquareSize, kSquareSize, kSquareSize);
    p.fillRect(sq, QColor(144, 238, 144, 150));

    if (m_suggestedTo >= 0) {
      int tr = m_suggestedTo / 8, tc = m_suggestedTo % 8;
      int dtr = displayRow(tr), dtc = displayCol(tc);
      QRect tq(dtc * kSquareSize, dtr * kSquareSize, kSquareSize, kSquareSize);
      p.fillRect(tq, QColor(144, 238, 144, 150));
    }
  }

  // Highlight selected and valid destinations
  if (m_selected >= 0) {
    int sr = m_selected / 8, sc = m_selected % 8;
    int dsr = displayRow(sr), dsc = displayCol(sc);
    QRect sq(dsc * kSquareSize, dsr * kSquareSize, kSquareSize, kSquareSize);
    p.fillRect(sq, QColor(100, 200, 100, 120));
  }
  for (int idx : m_highlightSquares) {
    int hr = idx / 8, hc = idx % 8;
    int dhr = displayRow(hr), dhc = displayCol(hc);
    QRect sq(dhc * kSquareSize, dhr * kSquareSize, kSquareSize, kSquareSize);
    p.fillRect(sq, QColor(100, 200, 255, 100));
  }

  // Draw pieces
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      int8_t piece = m_board[r * 8 + c];
      if (piece == Empty) continue;
      if (m_dragging && r * 8 + c == m_dragFrom) continue;
      if (m_animating && r * 8 + c == m_animPieceIdx) continue;

      int dr = displayRow(r), dc = displayCol(c);
      QRect sq(dc * kSquareSize + kPieceMargin, dr * kSquareSize + kPieceMargin,
               kSquareSize - 2 * kPieceMargin, kSquareSize - 2 * kPieceMargin);

      if (isDark(piece)) {
        p.setBrush(QColor(30, 30, 30));
        p.setPen(QPen(QColor(60, 60, 60), 2));
      } else {
        p.setBrush(QColor(200, 40, 40));
        p.setPen(QPen(QColor(150, 20, 20), 2));
      }
      p.drawEllipse(sq);

      if (isKing(piece)) {
        p.setPen(QPen(Qt::white, 3));
        QFont f = p.font();
        f.setPixelSize(kSquareSize / 3);
        f.setBold(true);
        p.setFont(f);
        p.drawText(sq, Qt::AlignCenter, "K");
      }
    }
  }

  // Draw animating piece
  if (m_animating && m_animPieceIdx >= 0) {
    int8_t piece = m_board[m_animPieceIdx];
    int sz = kSquareSize - 2 * kPieceMargin;
    QRect sq(m_animPos.x() - sz / 2, m_animPos.y() - sz / 2, sz, sz);

    if (isDark(piece)) {
      p.setBrush(QColor(30, 30, 30));
      p.setPen(QPen(QColor(60, 60, 60), 2));
    } else {
      p.setBrush(QColor(200, 40, 40));
      p.setPen(QPen(QColor(150, 20, 20), 2));
    }
    p.drawEllipse(sq);

    if (isKing(piece)) {
      p.setPen(QPen(Qt::white, 3));
      QFont f = p.font();
      f.setPixelSize(kSquareSize / 3);
      f.setBold(true);
      p.setFont(f);
      p.drawText(sq, Qt::AlignCenter, "K");
    }
  }

  // Draw dragged piece at cursor
  if (m_dragging && m_dragFrom >= 0) {
    int8_t piece = m_board[m_dragFrom];
    int sz = kSquareSize - 2 * kPieceMargin;
    QRect sq(m_dragPos.x() - sz / 2, m_dragPos.y() - sz / 2, sz, sz);

    if (isDark(piece)) {
      p.setBrush(QColor(30, 30, 30));
      p.setPen(QPen(QColor(60, 60, 60), 2));
    } else {
      p.setBrush(QColor(200, 40, 40));
      p.setPen(QPen(QColor(150, 20, 20), 2));
    }
    p.drawEllipse(sq);

    if (isKing(piece)) {
      p.setPen(QPen(Qt::white, 3));
      QFont f = p.font();
      f.setPixelSize(kSquareSize / 3);
      f.setBold(true);
      p.setFont(f);
      p.drawText(sq, Qt::AlignCenter, "K");
    }
  }

  // Flash win/loss message
  if (m_gameOver && m_flashVisible) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRect(rect());
    QFont f = p.font();
    f.setPixelSize(60);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 0));
    QString msg = m_isDraw ? "DRAW!" : (m_userWon ? "YOU WIN!" : "I WIN!");
    p.drawText(rect(), Qt::AlignCenter, msg);
  }
}

void CheckersWindow::mousePressEvent(QMouseEvent *event) {
  if (m_gameOver || !m_userTurn || m_animating) return;

  int dc = event->pos().x() / kSquareSize;
  int dr = event->pos().y() / kSquareSize;
  int c = displayCol(dc);
  int r = displayRow(dr);
  if (!onBoard(r, c)) return;
  int idx = r * 8 + c;

  bool userIsDark = !m_userIsLight;

  // If mid-jump, only the jump piece can be dragged
  if (m_midJump) {
    if (idx == m_selected) {
      m_dragging = true;
      m_dragFrom = idx;
      m_dragPos = event->pos();
      update();
    }
    return;
  }

  // Check if clicking own piece
  int8_t piece = m_board[idx];
  bool ownPiece = (userIsDark && isDark(piece)) || (!userIsDark && isLight(piece));

  if (ownPiece) {
    m_selected = idx;
    m_validMoves.clear();
    m_highlightSquares.clear();

    bool jumpsAvail = hasJumps(userIsDark);
    auto pieceMoves = generatePieceMoves(idx);

    for (auto &mv : pieceMoves) {
      if (jumpsAvail && mv.captured.empty()) continue;
      m_validMoves.push_back(mv);
      m_highlightSquares.push_back(mv.to);
    }

    m_dragging = true;
    m_dragFrom = idx;
    m_dragPos = event->pos();
    update();
  }
}

void CheckersWindow::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging) {
    m_dragPos = event->pos();
    update();
  }
}

void CheckersWindow::mouseReleaseEvent(QMouseEvent *event) {
  if (!m_dragging) return;
  m_dragging = false;

  int dc = event->pos().x() / kSquareSize;
  int dr = event->pos().y() / kSquareSize;
  int c = displayCol(dc);
  int r = displayRow(dr);
  if (!onBoard(r, c)) { update(); return; }
  int idx = r * 8 + c;

  // Try to find a matching valid move
  for (auto &mv : m_validMoves) {
    if (mv.to == idx) {
      applyMoveWithTracking(mv);

      // Check for multi-jump
      if (!mv.captured.empty() && !mv.kinged) {
        auto furtherJumps = generatePieceJumps(mv.to, m_board, {});
        if (!furtherJumps.empty()) {
          m_midJump = true;
          m_jumpPiece = mv.to;
          m_selected = mv.to;
          m_validMoves = furtherJumps;
          m_highlightSquares.clear();
          for (auto &fm : m_validMoves) m_highlightSquares.push_back(fm.to);
          update();
          return;
        }
      }

      // Turn over
      m_midJump = false;
      m_jumpPiece = -1;
      m_selected = -1;
      m_validMoves.clear();
      m_highlightSquares.clear();
      m_suggestedFrom = -1;
      m_suggestedTo = -1;
      m_userTurn = false;
      update();
      checkGameOver();
      if (!m_gameOver)
        QTimer::singleShot(300, this, &CheckersWindow::computerMove);
      return;
    }
  }

  // Dropped on invalid square - just redraw
  update();
}

void CheckersWindow::applyMove(const Move &m) {
  m_board[m.to] = m_board[m.from];
  m_board[m.from] = Empty;
  bool captured = !m.captured.empty();
  for (int cap : m.captured) m_board[cap] = Empty;

  // King promotion
  bool promoted = false;
  int row = m.to / 8;
  if (m_board[m.to] == Dark && row == 7) { m_board[m.to] = DarkKing; promoted = true; }
  if (m_board[m.to] == Light && row == 0) { m_board[m.to] = LightKing; promoted = true; }
}

void CheckersWindow::applyMoveWithTracking(const Move &m) {
  applyMove(m);

  bool captured = !m.captured.empty();
  int row = m.to / 8;
  bool promoted = (m_board[m.to] == DarkKing && m.kinged) || (m_board[m.to] == LightKing && m.kinged);

  if (captured || promoted)
    m_movesSinceProgress = 0;
  else
    m_movesSinceProgress++;

  m_positionHistory.push_back(m_board);
}

std::vector<CheckersWindow::Move> CheckersWindow::generateMoves(bool forDark) {
  std::vector<Move> moves;
  bool jumpsExist = hasJumps(forDark);

  for (int i = 0; i < 64; i++) {
    int8_t piece = m_board[i];
    if (piece == Empty) continue;
    if (forDark && !isDark(piece)) continue;
    if (!forDark && !isLight(piece)) continue;

    auto pm = generatePieceMoves(i);
    for (auto &m : pm) {
      if (jumpsExist && m.captured.empty()) continue;
      moves.push_back(m);
    }
  }
  return moves;
}

std::vector<CheckersWindow::Move> CheckersWindow::generatePieceMoves(int pos) {
  std::vector<Move> moves;
  int r = pos / 8, c = pos % 8;
  int8_t piece = m_board[pos];

  // Determine directions
  std::vector<std::pair<int,int>> dirs;
  if (piece == Dark || piece == DarkKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }
  if (piece == Light || piece == LightKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == DarkKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == LightKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }

  // Remove duplicates for kings
  std::sort(dirs.begin(), dirs.end());
  dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());

  // Jumps
  auto jumps = generatePieceJumps(pos, m_board, {});
  for (auto &j : jumps) moves.push_back(j);

  // Simple moves (only if no jumps for this piece - but caller filters)
  for (auto [dr, dc] : dirs) {
    int nr = r + dr, nc = c + dc;
    if (!onBoard(nr, nc)) continue;
    int nIdx = nr * 8 + nc;
    if (m_board[nIdx] == Empty) {
      Move m;
      m.from = pos;
      m.to = nIdx;
      if ((piece == Dark && nr == 7) || (piece == Light && nr == 0)) m.kinged = true;
      moves.push_back(m);
    }
  }

  return moves;
}

std::vector<CheckersWindow::Move> CheckersWindow::generatePieceJumps(int pos, std::array<int8_t, 64> &board, std::vector<int> captured) {
  std::vector<Move> jumps;
  int r = pos / 8, c = pos % 8;
  int8_t piece = board[pos];

  std::vector<std::pair<int,int>> dirs;
  if (piece == Dark || piece == DarkKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }
  if (piece == Light || piece == LightKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == DarkKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == LightKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }

  std::sort(dirs.begin(), dirs.end());
  dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());

  bool found = false;
  for (auto [dr, dc] : dirs) {
    int mr = r + dr, mc = c + dc; // middle
    int lr = r + 2 * dr, lc = c + 2 * dc; // landing
    if (!onBoard(lr, lc)) continue;
    int midIdx = mr * 8 + mc;
    int landIdx = lr * 8 + lc;
    if (board[landIdx] != Empty) continue;

    int8_t mid = board[midIdx];
    if (mid == Empty) continue;
    // Can't jump own piece
    if (isDark(piece) && isDark(mid)) continue;
    if (isLight(piece) && isLight(mid)) continue;
    // Can't jump already captured
    if (std::find(captured.begin(), captured.end(), midIdx) != captured.end()) continue;

    // Valid jump
    found = true;
    auto newCaptured = captured;
    newCaptured.push_back(midIdx);

    // Check for king promotion on landing
    bool willKing = (piece == Dark && lr == 7) || (piece == Light && lr == 0);

    // Try further jumps (not if piece just kinged - American rules: turn ends on promotion)
    auto savedBoard = board;
    board[landIdx] = willKing ? (isDark(piece) ? DarkKing : LightKing) : piece;
    board[pos] = Empty;
    board[midIdx] = Empty;

    if (!willKing) {
      auto further = generatePieceJumps(landIdx, board, newCaptured);
      if (!further.empty()) {
        for (auto &fj : further) {
          Move m;
          m.from = pos;
          m.to = fj.to;
          m.captured = fj.captured;
          m.captured.insert(m.captured.begin(), newCaptured.begin(), newCaptured.end() - (fj.captured.size() > 0 ? 0 : 0));
          // Actually, the further jump already has the full chain from landIdx
          // We need: captured from here + further captured
          m.captured = newCaptured;
          for (int fc : fj.captured) {
            if (std::find(m.captured.begin(), m.captured.end(), fc) == m.captured.end())
              m.captured.push_back(fc);
          }
          m.kinged = fj.kinged;
          jumps.push_back(m);
        }
        board = savedBoard;
        continue;
      }
    }

    board = savedBoard;

    Move m;
    m.from = pos;
    m.to = landIdx;
    m.captured = newCaptured;
    m.kinged = willKing;
    jumps.push_back(m);
  }

  return jumps;
}

bool CheckersWindow::hasJumps(bool forDark) {
  for (int i = 0; i < 64; i++) {
    int8_t piece = m_board[i];
    if (piece == Empty) continue;
    if (forDark && !isDark(piece)) continue;
    if (!forDark && !isLight(piece)) continue;
    auto jumps = generatePieceJumps(i, m_board, {});
    if (!jumps.empty()) return true;
  }
  return false;
}

void CheckersWindow::computerMove() {
  if (m_gameOver) return;

  bool movingDark;
  if (m_autoPlay) {
    movingDark = m_darkTurn;
    m_darkTurn = !m_darkTurn;
  } else {
    movingDark = m_userIsLight; // computer is dark when user is light
  }

  auto moves = generateMoves(movingDark);
  if (moves.empty()) {
    if (m_autoPlay)
      endGame(false); // show "I WIN!" (the other side won)
    else
      endGame(true); // user wins
    return;
  }

  Move bestMove = moves[0];
  int bestScore = std::numeric_limits<int>::min();
  std::vector<int> scores;

  for (auto &mv : moves) {
    auto saved = m_board;
    applyMove(mv);
    bool isHumanProxy = m_autoPlay && (movingDark != m_userIsLight);
    int searchDepth = isHumanProxy ? m_humanDepth : m_depth;
    int score;
    if (isHumanProxy) {
      score = -minimax(searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true);
    } else {
      score = minimax(searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false);
    }
    m_board = saved;
    scores.push_back(score);
    if (score > bestScore) {
      bestScore = score;
      bestMove = mv;
    }
  }

  // Pick randomly among moves within a small margin of best to avoid repetition
  std::vector<size_t> topIndices;
  for (size_t i = 0; i < scores.size(); i++)
    if (scores[i] >= bestScore - 10) topIndices.push_back(i);
  if (!topIndices.empty())
    bestMove = moves[topIndices[rand() % topIndices.size()]];

  startAnimation(bestMove);
}

QPointF CheckersWindow::squareCenter(int idx) {
  int r = idx / 8, c = idx % 8;
  int dr = displayRow(r), dc = displayCol(c);
  return QPointF(dc * kSquareSize + kSquareSize / 2.0, dr * kSquareSize + kSquareSize / 2.0);
}

void CheckersWindow::startAnimation(const Move &mv) {
  m_animating = true;
  m_animMove = mv;
  m_animPieceIdx = mv.from;
  m_animCapturedSoFar.clear();

  // Build waypoints: from -> each jump landing -> final to
  m_animWaypoints.clear();
  m_animWaypoints.push_back(mv.from);
  if (!mv.captured.empty()) {
    // For each captured piece, compute intermediate landing
    int cur = mv.from;
    for (int cap : mv.captured) {
      int cr = cap / 8, cc = cap % 8;
      int sr = cur / 8, sc = cur % 8;
      int lr = cr + (cr - sr), lc = cc + (cc - sc);
      int land = lr * 8 + lc;
      m_animWaypoints.push_back(land);
      cur = land;
    }
  } else {
    m_animWaypoints.push_back(mv.to);
  }

  m_animStep = 0;
  m_animFrame = 0;
  m_animFrames = 12; // frames per hop
  m_animStart = squareCenter(m_animWaypoints[0]);
  m_animEnd = squareCenter(m_animWaypoints[1]);
  m_animPos = m_animStart;

  connect(&m_animTimer, &QTimer::timeout, this, &CheckersWindow::advanceAnimation);
  m_animTimer.start(20);
}

void CheckersWindow::advanceAnimation() {
  m_animFrame++;
  double t = (double)m_animFrame / m_animFrames;
  if (t >= 1.0) t = 1.0;
  m_animPos = m_animStart + t * (m_animEnd - m_animStart);
  update();

  if (t >= 1.0) {
    // Remove captured piece for this hop
    if (m_animStep < (int)m_animMove.captured.size()) {
      m_animCapturedSoFar.push_back(m_animMove.captured[m_animStep]);
      m_board[m_animMove.captured[m_animStep]] = Empty;
    }

    m_animStep++;
    if (m_animStep + 1 < (int)m_animWaypoints.size()) {
      // Next hop
      m_animFrame = 0;
      m_animStart = squareCenter(m_animWaypoints[m_animStep]);
      m_animEnd = squareCenter(m_animWaypoints[m_animStep + 1]);
      m_animPos = m_animStart;
    } else {
      // Animation complete - apply the full move
      m_animTimer.stop();
      disconnect(&m_animTimer, &QTimer::timeout, this, &CheckersWindow::advanceAnimation);
      m_animating = false;

      // Restore board to pre-animation state and apply properly
      m_board[m_animMove.from] = m_board[m_animPieceIdx]; // piece is still logically at from
      // Clear any mid-animation artifacts
      for (int cap : m_animCapturedSoFar) m_board[cap] = Empty;

      // Board is now: piece at from, captured pieces removed during animation
      // Re-add captured for clean apply
      // Actually the board during animation: piece stays at m_animPieceIdx (from),
      // captured are removed per-hop. So board already has piece at from and captured removed.
      // Just do the final move + tracking:
      m_board[m_animMove.to] = m_board[m_animMove.from];
      m_board[m_animMove.from] = Empty;

      // King promotion
      int row = m_animMove.to / 8;
      if (m_board[m_animMove.to] == Dark && row == 7) m_board[m_animMove.to] = DarkKing;
      if (m_board[m_animMove.to] == Light && row == 0) m_board[m_animMove.to] = LightKing;

      // Draw tracking
      bool captured = !m_animMove.captured.empty();
      bool promoted = (m_board[m_animMove.to] == DarkKing || m_board[m_animMove.to] == LightKing) && m_animMove.kinged;
      if (captured || promoted)
        m_movesSinceProgress = 0;
      else
        m_movesSinceProgress++;
      m_positionHistory.push_back(m_board);

      m_userTurn = !m_autoPlay;
      update();
      checkGameOver();
      if (!m_gameOver) {
        if (m_autoPlay)
          QTimer::singleShot(300, this, &CheckersWindow::computerMove);
        else
          computeSuggestion();
      }
    }
  }
}

int CheckersWindow::minimax(int depth, int alpha, int beta, bool maximizing) {
  bool compIsDark = m_userIsLight;

  if (depth == 0) return evaluate();

  if (maximizing) {
    auto moves = generateMoves(compIsDark);
    if (moves.empty()) return -10000;
    int maxEval = std::numeric_limits<int>::min();
    for (auto &mv : moves) {
      auto saved = m_board;
      applyMove(mv);
      int eval = minimax(depth - 1, alpha, beta, false);
      m_board = saved;
      maxEval = std::max(maxEval, eval);
      alpha = std::max(alpha, eval);
      if (beta <= alpha) break;
    }
    return maxEval;
  } else {
    auto moves = generateMoves(!compIsDark);
    if (moves.empty()) return 10000;
    int minEval = std::numeric_limits<int>::max();
    for (auto &mv : moves) {
      auto saved = m_board;
      applyMove(mv);
      int eval = minimax(depth - 1, alpha, beta, true);
      m_board = saved;
      minEval = std::min(minEval, eval);
      beta = std::min(beta, eval);
      if (beta <= alpha) break;
    }
    return minEval;
  }
}

int CheckersWindow::evaluate() {
  // Positive = good for computer, negative = good for user
  bool compIsDark = m_userIsLight;
  int score = 0;
  for (int i = 0; i < 64; i++) {
    int8_t p = m_board[i];
    if (p == Empty) continue;
    int val = 0;
    if (p == Dark || p == Light) val = 100;
    else val = 175; // kings worth more

    // Positional bonus: advance toward promotion
    int r = i / 8;
    if (p == Dark) val += r * 5;
    if (p == Light) val += (7 - r) * 5;

    if ((compIsDark && isDark(p)) || (!compIsDark && isLight(p)))
      score += val;
    else
      score -= val;
  }
  return score;
}

void CheckersWindow::checkGameOver() {
  bool compIsDark = m_userIsLight;
  bool userIsDark = !m_userIsLight;

  auto userMoves = generateMoves(userIsDark);
  auto compMoves = generateMoves(compIsDark);

  if (m_userTurn && userMoves.empty()) { endGame(false); return; }
  if (!m_userTurn && compMoves.empty()) { endGame(true); return; }

  // Draw: 80 half-moves without progress
  if (m_movesSinceProgress >= 80) { endDraw(); return; }

  // Draw: threefold repetition
  int count = 0;
  for (auto &pos : m_positionHistory)
    if (pos == m_board && ++count >= 6) { endDraw(); return; }
}

void CheckersWindow::endGame(bool userWon) {
  m_gameOver = true;
  m_userWon = userWon;
  m_isDraw = false;
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

void CheckersWindow::endDraw() {
  m_gameOver = true;
  m_isDraw = true;
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

void CheckersWindow::computeSuggestion() {
  m_suggestedFrom = -1;
  m_suggestedTo = -1;
  if (!m_suggest || !m_userTurn || m_gameOver) return;

  bool userIsDark = !m_userIsLight;
  auto moves = generateMoves(userIsDark);
  if (moves.empty()) return;

  Move bestMove = moves[0];
  int bestScore = std::numeric_limits<int>::min();

  for (auto &mv : moves) {
    auto saved = m_board;
    applyMove(mv);
    int score = -minimax(9, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true);
    m_board = saved;
    if (score > bestScore) {
      bestScore = score;
      bestMove = mv;
    }
  }
  m_suggestedFrom = bestMove.from;
  m_suggestedTo = bestMove.to;
}
