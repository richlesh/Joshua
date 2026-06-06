// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "FourAcrossWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <limits>
#include <cstdlib>

FourAcrossWindow::FourAcrossWindow(bool userFirst, int depth, bool suggest, QWidget *parent)
  : QWidget(parent), m_depth(depth), m_suggest(suggest) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Four Across");
  setFixedSize(kCols * kCellSize, kRows * kCellSize + kTopMargin);
  setMouseTracking(true);

  m_userPiece = userFirst ? Player1 : Player2;
  m_compPiece = userFirst ? Player2 : Player1;
  m_userTurn = userFirst;
  m_board.fill(Empty);
  m_winCells.fill(-1);

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });
  connect(&m_animTimer, &QTimer::timeout, this, [this]() {
    m_animY += 12;
    if (m_animY >= m_animTargetY) {
      m_animY = m_animTargetY;
      m_animTimer.stop();
      finishDrop();
    }
    update();
  });

  if (!m_userTurn) {
    QTimer::singleShot(300, this, &FourAcrossWindow::computerMove);
  } else {
    computeSuggestion();
  }
}

FourAcrossWindow::FourAcrossWindow(bool userFirst, int compDepth, int humanDepth, QWidget *parent)
  : QWidget(parent), m_depth(compDepth), m_suggest(false), m_autoPlay(true), m_humanDepth(humanDepth) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Four Across - Computer vs Computer");
  setFixedSize(kCols * kCellSize, kRows * kCellSize + kTopMargin);
  setMouseTracking(true);

  m_userPiece = userFirst ? Player1 : Player2;
  m_compPiece = userFirst ? Player2 : Player1;
  m_userTurn = false;
  m_board.fill(Empty);
  m_winCells.fill(-1);

  connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
    m_flashVisible = !m_flashVisible;
    m_flashCount++;
    update();
    if (m_flashCount >= 10) m_flashTimer.stop();
  });
  connect(&m_animTimer, &QTimer::timeout, this, [this]() {
    m_animY += 12;
    if (m_animY >= m_animTargetY) {
      m_animY = m_animTargetY;
      m_animTimer.stop();
      finishDrop();
    }
    update();
  });

  QTimer::singleShot(300, this, &FourAcrossWindow::computerMove);
}

void FourAcrossWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int boardTop = kTopMargin;

  // Draw board background
  p.fillRect(0, boardTop, kCols * kCellSize, kRows * kCellSize, QColor(0, 0, 180));

  // Draw cells (empty circles)
  for (int col = 0; col < kCols; col++) {
    for (int row = 0; row < kRows; row++) {
      int dispRow = kRows - 1 - row; // row 0 is bottom, display top-to-bottom
      int x = col * kCellSize + 8;
      int y = boardTop + dispRow * kCellSize + 8;
      int sz = kCellSize - 16;

      int8_t piece = m_board[cell(col, row)];
      // Skip animating piece's final position during animation
      if (m_animating && col == m_animCol && row == m_animTargetRow) piece = Empty;

      if (piece == Player1)
        p.setBrush(QColor(220, 20, 20));
      else if (piece == Player2)
        p.setBrush(QColor(220, 220, 20));
      else
        p.setBrush(QColor(240, 240, 240));

      p.setPen(Qt::NoPen);
      p.drawEllipse(x, y, sz, sz);
    }
  }

  // Highlight suggested column
  if (m_suggestedCol >= 0 && !m_animating && !m_gameOver && m_userTurn) {
    p.setBrush(QColor(144, 238, 144, 80));
    p.setPen(Qt::NoPen);
    p.drawRect(m_suggestedCol * kCellSize, boardTop, kCellSize, kRows * kCellSize);
  }

  // Highlight win cells
  if (m_gameOver && !m_isDraw && m_winCells[0] >= 0) {
    p.setPen(QPen(Qt::white, 4));
    p.setBrush(Qt::NoBrush);
    for (int idx : m_winCells) {
      if (idx < 0) break;
      int col = idx / kRows, row = idx % kRows;
      int dispRow = kRows - 1 - row;
      int x = col * kCellSize + 8;
      int y = boardTop + dispRow * kCellSize + 8;
      int sz = kCellSize - 16;
      p.drawEllipse(x, y, sz, sz);
    }
  }

  // Draw hover piece at top (sliding)
  if (m_hoverCol >= 0 && !m_animating && !m_gameOver && m_userTurn && !m_autoPlay) {
    int x = m_hoverCol * kCellSize + 8;
    int y = (kTopMargin - kCellSize) / 2 + 8;
    int sz = kCellSize - 16;
    p.setBrush(m_userPiece == Player1 ? QColor(220, 20, 20) : QColor(220, 220, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(x, y, sz, sz);
  }

  // Draw dropping piece
  if (m_animating) {
    int x = m_animCol * kCellSize + 8;
    int y = (int)m_animY + 8;
    int sz = kCellSize - 16;
    p.setBrush(m_animPiece == Player1 ? QColor(220, 20, 20) : QColor(220, 220, 20));
    p.setPen(Qt::NoPen);
    p.drawEllipse(x, y, sz, sz);
  }

  // Flash message
  if (m_gameOver && m_flashVisible) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRect(rect());
    QFont f = p.font();
    f.setPixelSize(50);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 0));
    QString msg = m_isDraw ? "DRAW!" : (m_userWon ? "YOU WIN!" : "I WIN!");
    p.drawText(rect(), Qt::AlignCenter, msg);
  }
}

void FourAcrossWindow::mouseMoveEvent(QMouseEvent *event) {
  int col = event->pos().x() / kCellSize;
  if (col >= 0 && col < kCols)
    m_hoverCol = col;
  else
    m_hoverCol = -1;
  update();
}

void FourAcrossWindow::mousePressEvent(QMouseEvent *event) {
  if (m_gameOver || !m_userTurn || m_animating || m_autoPlay) return;

  int col = event->pos().x() / kCellSize;
  if (col < 0 || col >= kCols) return;
  if (dropRow(col) < 0) return; // column full

  dropPiece(col, m_userPiece);
}

int FourAcrossWindow::dropRow(int col) {
  for (int row = 0; row < kRows; row++)
    if (m_board[cell(col, row)] == Empty) return row;
  return -1; // full
}

bool FourAcrossWindow::isFull() {
  for (int col = 0; col < kCols; col++)
    if (m_board[cell(col, kRows - 1)] == Empty) return false;
  return true;
}

void FourAcrossWindow::dropPiece(int col, int8_t piece) {
  int row = dropRow(col);
  if (row < 0) return;

  m_animating = true;
  m_animCol = col;
  m_animTargetRow = row;
  m_animPiece = piece;
  m_animY = (kTopMargin - kCellSize) / 2.0;
  int dispRow = kRows - 1 - row;
  m_animTargetY = kTopMargin + dispRow * kCellSize;

  // Place piece in board immediately (but hide it in paint until anim done)
  m_board[cell(col, row)] = piece;

  m_animTimer.start(16);
}

void FourAcrossWindow::finishDrop() {
  m_animating = false;
  update();
  checkGameOver();
  if (!m_gameOver) nextTurn();
}

void FourAcrossWindow::nextTurn() {
  if (m_autoPlay) {
    QTimer::singleShot(300, this, &FourAcrossWindow::computerMove);
  } else {
    m_userTurn = !m_userTurn;
    if (!m_userTurn) {
      QTimer::singleShot(300, this, &FourAcrossWindow::computerMove);
    } else {
      m_suggestedCol = -1;
      computeSuggestion();
      update();
    }
  }
}

void FourAcrossWindow::computerMove() {
  if (m_gameOver) return;
  setCursor(Qt::WaitCursor);

  // Determine which piece is moving
  int8_t movingPiece;
  if (m_autoPlay) {
    // Alternate: count pieces to determine whose turn
    int p1 = 0, p2 = 0;
    for (auto c : m_board) { if (c == Player1) p1++; if (c == Player2) p2++; }
    movingPiece = (p1 <= p2) ? Player1 : Player2;
  } else {
    movingPiece = m_compPiece;
  }

  bool isHumanProxy = m_autoPlay && (movingPiece == m_userPiece);
  int searchDepth = isHumanProxy ? m_humanDepth : m_depth;

  int bestCol = -1;
  int bestScore = std::numeric_limits<int>::min();
  std::vector<std::pair<int,int>> scores; // col, score

  for (int col = 0; col < kCols; col++) {
    int row = dropRow(col);
    if (row < 0) continue;

    m_board[cell(col, row)] = movingPiece;
    int score;
    if (isHumanProxy) {
      score = -minimax(searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true);
    } else {
      score = minimax(searchDepth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false);
    }
    m_board[cell(col, row)] = Empty;

    scores.push_back({col, score});
    if (score > bestScore) {
      bestScore = score;
      bestCol = col;
    }
  }

  // Randomize among near-best to avoid repetition
  std::vector<int> topCols;
  for (auto &[col, score] : scores)
    if (score >= bestScore - 5) topCols.push_back(col);
  if (!topCols.empty())
    bestCol = topCols[rand() % topCols.size()];

  unsetCursor();
  if (bestCol >= 0) dropPiece(bestCol, movingPiece);
}

int FourAcrossWindow::minimax(int depth, int alpha, int beta, bool maximizing) {
  int win = checkWin();
  if (win == m_compPiece) return 10000 + depth;
  if (win == m_userPiece) return -10000 - depth;
  if (isFull()) return 0;
  if (depth == 0) return evaluate();

  if (maximizing) {
    int maxEval = std::numeric_limits<int>::min();
    for (int col = 0; col < kCols; col++) {
      int row = dropRow(col);
      if (row < 0) continue;
      m_board[cell(col, row)] = m_compPiece;
      int eval = minimax(depth - 1, alpha, beta, false);
      m_board[cell(col, row)] = Empty;
      maxEval = std::max(maxEval, eval);
      alpha = std::max(alpha, eval);
      if (beta <= alpha) break;
    }
    return maxEval;
  } else {
    int minEval = std::numeric_limits<int>::max();
    for (int col = 0; col < kCols; col++) {
      int row = dropRow(col);
      if (row < 0) continue;
      m_board[cell(col, row)] = m_userPiece;
      int eval = minimax(depth - 1, alpha, beta, true);
      m_board[cell(col, row)] = Empty;
      minEval = std::min(minEval, eval);
      beta = std::min(beta, eval);
      if (beta <= alpha) break;
    }
    return minEval;
  }
}

int FourAcrossWindow::evaluate() {
  int score = 0;
  // Score all windows of 4
  auto scoreWindow = [&](int8_t a, int8_t b, int8_t c, int8_t d) {
    int comp = 0, user = 0, empty = 0;
    for (int8_t v : {a, b, c, d}) {
      if (v == m_compPiece) comp++;
      else if (v == m_userPiece) user++;
      else empty++;
    }
    if (comp == 4) score += 10000;
    else if (comp == 3 && empty == 1) score += 50;
    else if (comp == 2 && empty == 2) score += 10;
    if (user == 4) score -= 10000;
    else if (user == 3 && empty == 1) score -= 50;
    else if (user == 2 && empty == 2) score -= 10;
  };

  // Horizontal
  for (int row = 0; row < kRows; row++)
    for (int col = 0; col <= kCols - 4; col++)
      scoreWindow(m_board[cell(col,row)], m_board[cell(col+1,row)], m_board[cell(col+2,row)], m_board[cell(col+3,row)]);
  // Vertical
  for (int col = 0; col < kCols; col++)
    for (int row = 0; row <= kRows - 4; row++)
      scoreWindow(m_board[cell(col,row)], m_board[cell(col,row+1)], m_board[cell(col,row+2)], m_board[cell(col,row+3)]);
  // Diagonal /
  for (int col = 0; col <= kCols - 4; col++)
    for (int row = 0; row <= kRows - 4; row++)
      scoreWindow(m_board[cell(col,row)], m_board[cell(col+1,row+1)], m_board[cell(col+2,row+2)], m_board[cell(col+3,row+3)]);
  // Diagonal '\'
  for (int col = 0; col <= kCols - 4; col++)
    for (int row = 3; row < kRows; row++)
      scoreWindow(m_board[cell(col,row)], m_board[cell(col+1,row-1)], m_board[cell(col+2,row-2)], m_board[cell(col+3,row-3)]);

  // Center column preference
  for (int row = 0; row < kRows; row++) {
    if (m_board[cell(3, row)] == m_compPiece) score += 6;
    else if (m_board[cell(3, row)] == m_userPiece) score -= 6;
  }

  return score;
}

int FourAcrossWindow::checkWin() {
  // Horizontal
  for (int row = 0; row < kRows; row++)
    for (int col = 0; col <= kCols - 4; col++) {
      int8_t p = m_board[cell(col, row)];
      if (p != Empty && p == m_board[cell(col+1,row)] && p == m_board[cell(col+2,row)] && p == m_board[cell(col+3,row)])
        return p;
    }
  // Vertical
  for (int col = 0; col < kCols; col++)
    for (int row = 0; row <= kRows - 4; row++) {
      int8_t p = m_board[cell(col, row)];
      if (p != Empty && p == m_board[cell(col,row+1)] && p == m_board[cell(col,row+2)] && p == m_board[cell(col,row+3)])
        return p;
    }
  // Diagonal /
  for (int col = 0; col <= kCols - 4; col++)
    for (int row = 0; row <= kRows - 4; row++) {
      int8_t p = m_board[cell(col, row)];
      if (p != Empty && p == m_board[cell(col+1,row+1)] && p == m_board[cell(col+2,row+2)] && p == m_board[cell(col+3,row+3)])
        return p;
    }
  // Diagonal '\'
  for (int col = 0; col <= kCols - 4; col++)
    for (int row = 3; row < kRows; row++) {
      int8_t p = m_board[cell(col, row)];
      if (p != Empty && p == m_board[cell(col+1,row-1)] && p == m_board[cell(col+2,row-2)] && p == m_board[cell(col+3,row-3)])
        return p;
    }
  return 0;
}

bool FourAcrossWindow::findWinLine(int8_t piece) {
  // Find and store the winning 4 cells
  auto check = [&](int a, int b, int c, int d) {
    if (m_board[a] == piece && m_board[b] == piece && m_board[c] == piece && m_board[d] == piece) {
      m_winCells = {a, b, c, d};
      return true;
    }
    return false;
  };
  for (int row = 0; row < kRows; row++)
    for (int col = 0; col <= kCols - 4; col++)
      if (check(cell(col,row), cell(col+1,row), cell(col+2,row), cell(col+3,row))) return true;
  for (int col = 0; col < kCols; col++)
    for (int row = 0; row <= kRows - 4; row++)
      if (check(cell(col,row), cell(col,row+1), cell(col,row+2), cell(col,row+3))) return true;
  for (int col = 0; col <= kCols - 4; col++)
    for (int row = 0; row <= kRows - 4; row++)
      if (check(cell(col,row), cell(col+1,row+1), cell(col+2,row+2), cell(col+3,row+3))) return true;
  for (int col = 0; col <= kCols - 4; col++)
    for (int row = 3; row < kRows; row++)
      if (check(cell(col,row), cell(col+1,row-1), cell(col+2,row-2), cell(col+3,row-3))) return true;
  return false;
}

void FourAcrossWindow::checkGameOver() {
  int win = checkWin();
  if (win) {
    findWinLine(win);
    endGame(win == m_userPiece);
  } else if (isFull()) {
    endDraw();
  }
}

void FourAcrossWindow::endGame(bool userWon) {
  m_gameOver = true;
  m_userWon = userWon;
  m_isDraw = false;
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

void FourAcrossWindow::endDraw() {
  m_gameOver = true;
  m_isDraw = true;
  m_flashCount = 0;
  m_flashVisible = true;
  m_flashTimer.start(400);
  update();
}

void FourAcrossWindow::computeSuggestion() {
  m_suggestedCol = -1;
  if (!m_suggest || !m_userTurn || m_gameOver) return;
  setCursor(Qt::WaitCursor);

  int bestCol = -1;
  int bestScore = std::numeric_limits<int>::min();

  for (int col = 0; col < kCols; col++) {
    int row = dropRow(col);
    if (row < 0) continue;
    m_board[cell(col, row)] = m_userPiece;
    int score = -minimax(9, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), true);
    m_board[cell(col, row)] = Empty;
    if (score > bestScore) {
      bestScore = score;
      bestCol = col;
    }
  }
  m_suggestedCol = bestCol;
  unsetCursor();
}
