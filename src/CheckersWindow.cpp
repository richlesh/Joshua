// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "CheckersWindow.h"
#include "CheckersEndgame.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>

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

  // Draw depth labels
  if (m_effectiveCompDepth > 0 || m_effectivePlayerDepth > 0) {
    QFont df = p.font();
    df.setPixelSize(14);
    df.setBold(true);
    p.setFont(df);
    QString compText = QString("Computer Depth: %1").arg(m_effectiveCompDepth);
    QString playerText = QString("Player Depth: %1").arg(m_effectivePlayerDepth);
    // Shadow
    p.setPen(QColor(0, 0, 0));
    p.drawText(QRect(1, 5, kBoardSize, 20), Qt::AlignCenter, compText);
    p.drawText(QRect(1, kBoardSize - 21, kBoardSize, 20), Qt::AlignCenter, playerText);
    // Foreground
    p.setPen(QColor(255, 255, 0));
    p.drawText(QRect(0, 4, kBoardSize, 20), Qt::AlignCenter, compText);
    p.drawText(QRect(0, kBoardSize - 22, kBoardSize, 20), Qt::AlignCenter, playerText);
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

  if (captured || promoted) {
    m_movesSinceProgress = 0;
  } else {
    m_movesSinceProgress++;
  }

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

// --- Thread-safe static helpers for parallel search ---

static constexpr int8_t s_Empty = CheckersWindow::Empty;

static bool s_isDark(int8_t p) { return p > 0; }
static bool s_isLight(int8_t p) { return p < 0; }
static bool s_isKing(int8_t p) { return p == CheckersWindow::DarkKing || p == CheckersWindow::LightKing; }
static bool s_onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

static std::vector<CheckersWindow::Move> s_generatePieceJumps(int pos, std::array<int8_t, 64> &board, std::vector<int> captured) {
  using Move = CheckersWindow::Move;
  std::vector<Move> jumps;
  int r = pos / 8, c = pos % 8;
  int8_t piece = board[pos];

  std::vector<std::pair<int,int>> dirs;
  if (piece == CheckersWindow::Dark || piece == CheckersWindow::DarkKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }
  if (piece == CheckersWindow::Light || piece == CheckersWindow::LightKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == CheckersWindow::DarkKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == CheckersWindow::LightKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }
  std::sort(dirs.begin(), dirs.end());
  dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());

  for (auto [dr, dc] : dirs) {
    int mr = r + dr, mc = c + dc;
    int lr = r + 2 * dr, lc = c + 2 * dc;
    if (!s_onBoard(lr, lc)) continue;
    int midIdx = mr * 8 + mc;
    int landIdx = lr * 8 + lc;
    if (board[landIdx] != s_Empty) continue;
    int8_t mid = board[midIdx];
    if (mid == s_Empty) continue;
    if (s_isDark(piece) && s_isDark(mid)) continue;
    if (s_isLight(piece) && s_isLight(mid)) continue;
    if (std::find(captured.begin(), captured.end(), midIdx) != captured.end()) continue;

    auto newCaptured = captured;
    newCaptured.push_back(midIdx);
    bool willKing = (piece == CheckersWindow::Dark && lr == 7) || (piece == CheckersWindow::Light && lr == 0);

    auto savedBoard = board;
    board[landIdx] = willKing ? (s_isDark(piece) ? CheckersWindow::DarkKing : CheckersWindow::LightKing) : piece;
    board[pos] = s_Empty;
    board[midIdx] = s_Empty;

    if (!willKing) {
      auto further = s_generatePieceJumps(landIdx, board, newCaptured);
      if (!further.empty()) {
        for (auto &fj : further) {
          Move m;
          m.from = pos;
          m.to = fj.to;
          m.captured = newCaptured;
          for (int fc : fj.captured)
            if (std::find(m.captured.begin(), m.captured.end(), fc) == m.captured.end())
              m.captured.push_back(fc);
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

static std::vector<CheckersWindow::Move> s_generatePieceMoves(int pos, std::array<int8_t, 64> &board) {
  using Move = CheckersWindow::Move;
  std::vector<Move> moves;
  int r = pos / 8, c = pos % 8;
  int8_t piece = board[pos];

  std::vector<std::pair<int,int>> dirs;
  if (piece == CheckersWindow::Dark || piece == CheckersWindow::DarkKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }
  if (piece == CheckersWindow::Light || piece == CheckersWindow::LightKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == CheckersWindow::DarkKing) { dirs.push_back({-1, -1}); dirs.push_back({-1, 1}); }
  if (piece == CheckersWindow::LightKing) { dirs.push_back({1, -1}); dirs.push_back({1, 1}); }
  std::sort(dirs.begin(), dirs.end());
  dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());

  auto jumps = s_generatePieceJumps(pos, board, {});
  for (auto &j : jumps) moves.push_back(j);

  for (auto [dr, dc] : dirs) {
    int nr = r + dr, nc = c + dc;
    if (!s_onBoard(nr, nc)) continue;
    int nIdx = nr * 8 + nc;
    if (board[nIdx] == s_Empty) {
      Move m;
      m.from = pos;
      m.to = nIdx;
      if ((piece == CheckersWindow::Dark && nr == 7) || (piece == CheckersWindow::Light && nr == 0)) m.kinged = true;
      moves.push_back(m);
    }
  }
  return moves;
}

static bool s_hasJumps(bool forDark, const std::array<int8_t, 64> &board) {
  for (int i = 0; i < 64; i++) {
    int8_t piece = board[i];
    if (piece == s_Empty) continue;
    if (forDark && !s_isDark(piece)) continue;
    if (!forDark && !s_isLight(piece)) continue;
    auto boardCopy = board;
    auto jumps = s_generatePieceJumps(i, boardCopy, {});
    if (!jumps.empty()) return true;
  }
  return false;
}

static std::vector<CheckersWindow::Move> s_generateMoves(bool forDark, const std::array<int8_t, 64> &board) {
  using Move = CheckersWindow::Move;
  std::vector<Move> moves;
  bool jumpsExist = s_hasJumps(forDark, board);

  for (int i = 0; i < 64; i++) {
    int8_t piece = board[i];
    if (piece == s_Empty) continue;
    if (forDark && !s_isDark(piece)) continue;
    if (!forDark && !s_isLight(piece)) continue;
    auto boardCopy = board;
    auto pm = s_generatePieceMoves(i, boardCopy);
    for (auto &m : pm) {
      if (jumpsExist && m.captured.empty()) continue;
      moves.push_back(m);
    }
  }
  return moves;
}

static void s_applyMove(const CheckersWindow::Move &m, std::array<int8_t, 64> &board) {
  board[m.to] = board[m.from];
  board[m.from] = s_Empty;
  for (int cap : m.captured) board[cap] = s_Empty;
  int row = m.to / 8;
  if (board[m.to] == CheckersWindow::Dark && row == 7) board[m.to] = CheckersWindow::DarkKing;
  if (board[m.to] == CheckersWindow::Light && row == 0) board[m.to] = CheckersWindow::LightKing;
}

static int s_evaluate(const std::array<int8_t, 64> &board, bool compIsDark) {
  int compMen = 0, compKings = 0, oppMen = 0, oppKings = 0;
  int compPositional = 0, oppPositional = 0;

  // First pass: count material
  for (int i = 0; i < 64; i++) {
    int8_t p = board[i];
    if (p == s_Empty) continue;
    bool isComp = (compIsDark && s_isDark(p)) || (!compIsDark && s_isLight(p));
    if (s_isKing(p)) { if (isComp) compKings++; else oppKings++; }
    else { if (isComp) compMen++; else oppMen++; }
  }

  int compTotal = compMen + compKings;
  int oppTotal = oppMen + oppKings;
  int totalPieces = compTotal + oppTotal;
  bool isEndgame = totalPieces <= 8;
  bool isLateEndgame = totalPieces <= 5;

  // Second pass: positional scoring
  for (int i = 0; i < 64; i++) {
    int8_t p = board[i];
    if (p == s_Empty) continue;
    int r = i / 8, c = i % 8;
    bool isComp = (compIsDark && s_isDark(p)) || (!compIsDark && s_isLight(p));
    int val = 0;

    if (!s_isKing(p)) {
      val = 100;
      // Advancement bonus
      int advance = s_isDark(p) ? r : (7 - r);
      val += advance * 10;

      // Back rank defense: pieces on home row protect against promotion
      if (!isEndgame) {
        bool onHomeRank = (s_isDark(p) && r == 0) || (s_isLight(p) && r == 7);
        if (onHomeRank) val += 15;
      }

      // Runaway piece detection: unblockable path to promotion
      if (isEndgame) {
        int promoRow = s_isDark(p) ? 7 : 0;
        int distToPromo = std::abs(promoRow - r);
        bool runaway = true;
        int dir = s_isDark(p) ? 1 : -1;
        for (int step = 1; step <= distToPromo && runaway; step++) {
          int checkR = r + dir * step;
          // Opponent must be closer or at same diagonal to block
          for (int j = 0; j < 64; j++) {
            int8_t q = board[j];
            if (q == s_Empty) continue;
            if (s_isDark(p) == s_isDark(q)) continue;
            int qr = j / 8;
            int oppDistToIntercept = std::abs(qr - checkR);
            int ourDist = step;
            if (oppDistToIntercept < ourDist) { runaway = false; break; }
            // Kings can move any direction
            if (s_isKing(q) && std::abs(qr - checkR) + std::abs(j % 8 - c) <= step * 2) {
              runaway = false; break;
            }
          }
        }
        if (runaway && distToPromo <= 4) val += (5 - distToPromo) * 40;
      }
    } else {
      // King
      val = 300;

      int centerDist = std::abs(r - 3) + std::abs(c - 3);
      bool allKings = (compMen == 0 && oppMen == 0);

      if (isComp) {
        // Computer king: pursue opponent pieces aggressively
        int minDist = 14;
        for (int j = 0; j < 64; j++) {
          int8_t q = board[j];
          if (q == s_Empty) continue;
          if (s_isDark(p) == s_isDark(q)) continue;
          int dist = std::abs(r - j / 8) + std::abs(c - j % 8);
          if (dist < minDist) minDist = dist;
        }
        if (minDist < 14) {
          int pursuitBonus = allKings ? 25 : (isLateEndgame ? 15 : (isEndgame ? 8 : 5));
          val += (14 - minDist) * pursuitBonus;
        }
        // Center control for the computer's kings (better mobility)
        if (isEndgame) val += (7 - centerDist) * (allKings ? 15 : 5);
      } else {
        // Opponent king: heavily penalize edges/corners (trapped = losing)
        if (isEndgame) {
          // Distance from center = bad for opponent
          int edgePenalty = allKings ? 40 : 15;
          val -= centerDist * edgePenalty;

          bool onEdge = (r == 0 || r == 7 || c == 0 || c == 7);
          bool inCorner = ((r == 0 || r == 7) && (c == 0 || c == 7));
          bool inDoubleCorner = (r == 0 && c == 1) || (r == 0 && c == 7) ||
                                (r == 7 && c == 0) || (r == 7 && c == 6);
          if (allKings) {
            if (inCorner) val -= 150;
            else if (inDoubleCorner) val -= 100;
            else if (onEdge) val -= 60;
          } else {
            if (inCorner) val -= 40;
            else if (onEdge) val -= 20;
          }
        }
      }
    }

    if (isComp) compPositional += val;
    else oppPositional += val;
  }

  int score = compPositional - oppPositional;

  // Material advantage amplifier: when ahead, incentivize trades
  int materialDiff = (compMen + compKings * 3) - (oppMen + oppKings * 3);
  if (materialDiff > 0 && isEndgame) score += materialDiff * 20;

  // Winning bonus: heavily reward positions where opponent has few pieces
  if (oppTotal == 1 && compTotal >= 2) score += 500;
  if (oppTotal == 0) score += 10000;
  if (compTotal == 0) score -= 10000;

  return score;
}

static int s_minimax(std::array<int8_t, 64> board, int depth, int alpha, int beta, bool maximizing, bool compIsDark) {
  // Probe Chinook endgame database for perfect play with few pieces
  if (CheckersEndgame::instance().isChinookReady()) {
    int pc = 0;
    for (int i = 0; i < 64; i++) if (board[i] != s_Empty) pc++;
    if (pc >= 2 && pc <= 6) {
      bool darkToMove = maximizing ? compIsDark : !compIsDark;
      auto result = CheckersEndgame::instance().probe(board, darkToMove);
      if (result != CheckersEndgame::UNKNOWN) {
        // DRAW: return immediately
        if (result == CheckersEndgame::DRAW) return 0;
        // LOSS for side to move: avoid this line
        if (result == CheckersEndgame::LOSS)
          return maximizing ? -(9000 + depth) : (9000 + depth);
        // WIN for side to move: only use as leaf eval; otherwise let search
        // continue so positional evaluation guides toward trapping/capturing.
        if (result == CheckersEndgame::WIN && depth == 0) {
          int eval = s_evaluate(board, compIsDark);
          // Boost eval to ensure it's clearly winning but preserve relative ordering
          return maximizing ? std::max(eval, 5000) : std::min(eval, -5000);
        }
      }
    }
  }

  if (depth == 0) return s_evaluate(board, compIsDark);

  if (maximizing) {
    auto moves = s_generateMoves(compIsDark, board);
    if (moves.empty()) return -10000;
    std::sort(moves.begin(), moves.end(), [](const CheckersWindow::Move &a, const CheckersWindow::Move &b) {
      if (!a.captured.empty() != !b.captured.empty()) return !a.captured.empty();
      return a.captured.size() > b.captured.size();
    });
    int maxEval = std::numeric_limits<int>::min();
    for (auto &mv : moves) {
      auto child = board;
      s_applyMove(mv, child);
      int eval = s_minimax(child, depth - 1, alpha, beta, false, compIsDark);
      maxEval = std::max(maxEval, eval);
      alpha = std::max(alpha, eval);
      if (beta <= alpha) break;
    }
    return maxEval;
  } else {
    auto moves = s_generateMoves(!compIsDark, board);
    if (moves.empty()) return 10000;
    std::sort(moves.begin(), moves.end(), [](const CheckersWindow::Move &a, const CheckersWindow::Move &b) {
      if (!a.captured.empty() != !b.captured.empty()) return !a.captured.empty();
      return a.captured.size() > b.captured.size();
    });
    int minEval = std::numeric_limits<int>::max();
    for (auto &mv : moves) {
      auto child = board;
      s_applyMove(mv, child);
      int eval = s_minimax(child, depth - 1, alpha, beta, true, compIsDark);
      minEval = std::min(minEval, eval);
      beta = std::min(beta, eval);
      if (beta <= alpha) break;
    }
    return minEval;
  }
}

// --- End thread-safe helpers ---

void CheckersWindow::computerMove() {
  if (m_gameOver) return;
  setCursor(Qt::WaitCursor);

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

  int pieceCount = 0;
  for (int i = 0; i < 64; i++)
    if (m_board[i] != Empty) pieceCount++;

  // If the endgame DB covers this position, use it for perfect play
  bool dbCovers = (pieceCount <= 6 && CheckersEndgame::instance().isChinookReady());
  m_effectiveCompDepth = m_depth;
  m_effectivePlayerDepth = m_autoPlay ? m_humanDepth : m_depth;

  bool compIsDark = movingDark;

  // Parallel YBWC root search: first move sequential, rest in parallel
  std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
    if (!a.captured.empty() != !b.captured.empty()) return !a.captured.empty();
    return a.captured.size() > b.captured.size();
  });

  auto boardSnapshot = m_board;
  bool isHumanProxy = m_autoPlay && (movingDark != m_userIsLight);
  int searchDepth = isHumanProxy ? m_humanDepth : m_depth;

  // Search first move sequentially for a good bound
  {
    auto child = boardSnapshot;
    s_applyMove(moves[0], child);
    int score;
    if (isHumanProxy)
      score = -s_minimax(child, searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true, !compIsDark);
    else
      score = s_minimax(child, searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, compIsDark);
    scores.push_back(score);
    bestScore = score;
  }

  // Search remaining moves in parallel
  if (moves.size() > 1) {
    std::vector<std::future<int>> futures;
    for (size_t i = 1; i < moves.size(); i++) {
      futures.push_back(std::async(std::launch::async, [&, i]() {
        auto child = boardSnapshot;
        s_applyMove(moves[i], child);
        if (isHumanProxy)
          return -s_minimax(child, searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true, !compIsDark);
        else
          return s_minimax(child, searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, compIsDark);
      }));
    }
    for (auto &f : futures)
      scores.push_back(f.get());
  }

  for (size_t i = 0; i < scores.size(); i++) {
    if (scores[i] > bestScore) {
      bestScore = scores[i];
      bestMove = moves[i];
    }
  }

  // Only randomize among truly equal moves (not "close" moves)
  std::vector<size_t> topIndices;
  for (size_t i = 0; i < scores.size(); i++)
    if (scores[i] == bestScore) topIndices.push_back(i);
  if (topIndices.size() > 1)
    bestMove = moves[topIndices[rand() % topIndices.size()]];

  unsetCursor();
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
      if (captured || promoted) {
        m_movesSinceProgress = 0;
      } else {
        m_movesSinceProgress++;
      }
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
    // Move ordering: try captures first, then sort by quick eval
    std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
      if (!a.captured.empty() != !b.captured.empty()) return !a.captured.empty();
      return a.captured.size() > b.captured.size();
    });
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
    std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
      if (!a.captured.empty() != !b.captured.empty()) return !a.captured.empty();
      return a.captured.size() > b.captured.size();
    });
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
  // Delegate to static version for consistency
  bool compIsDark = m_userIsLight;
  return s_evaluate(m_board, compIsDark);
}

void CheckersWindow::checkGameOver() {
  bool compIsDark = m_userIsLight;
  bool userIsDark = !m_userIsLight;

  if (m_autoPlay) {
    // In auto-play, check if the next side to move (m_darkTurn) has moves
    auto nextMoves = generateMoves(m_darkTurn);
    if (nextMoves.empty()) {
      // The side that just moved wins (opposite of m_darkTurn)
      bool darkJustMoved = !m_darkTurn;
      endGame(darkJustMoved == userIsDark); // "YOU WIN" if the human-proxy side won
      return;
    }
  } else {
    auto userMoves = generateMoves(userIsDark);
    auto compMoves = generateMoves(compIsDark);

    if (m_userTurn && userMoves.empty()) { endGame(false); return; }
    if (!m_userTurn && compMoves.empty()) { endGame(true); return; }
  }

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
  setCursor(Qt::WaitCursor);

  bool userIsDark = !m_userIsLight;
  auto moves = generateMoves(userIsDark);
  if (moves.empty()) return;

  // Sort for better move ordering
  std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
    if (!a.captured.empty() != !b.captured.empty()) return !a.captured.empty();
    return a.captured.size() > b.captured.size();
  });

  auto boardSnapshot = m_board;
  bool compIsDark = !userIsDark; // from user's perspective, computer is the opponent

  Move bestMove = moves[0];
  int bestScore = std::numeric_limits<int>::min();

  // Search first move sequentially
  {
    auto child = boardSnapshot;
    s_applyMove(moves[0], child);
    bestScore = -s_minimax(child, 9, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true, compIsDark);
  }

  // Search remaining in parallel
  if (moves.size() > 1) {
    std::vector<std::future<int>> futures;
    for (size_t i = 1; i < moves.size(); i++) {
      futures.push_back(std::async(std::launch::async, [&, i]() {
        auto child = boardSnapshot;
        s_applyMove(moves[i], child);
        return -s_minimax(child, 9, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true, compIsDark);
      }));
    }
    std::vector<int> scores;
    scores.push_back(bestScore);
    for (auto &f : futures)
      scores.push_back(f.get());
    for (size_t i = 1; i < scores.size(); i++) {
      if (scores[i] > bestScore) {
        bestScore = scores[i];
        bestMove = moves[i];
      }
    }
  }

  m_suggestedFrom = bestMove.from;
  m_suggestedTo = bestMove.to;
  unsetCursor();
}
