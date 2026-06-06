// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "TicTacToeWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>

TicTacToeWindow::TicTacToeWindow(bool userFirst, QWidget *parent)
  : QWidget(parent, Qt::Window) {
  setWindowTitle("Tic-Tac-Toe");
  setFixedSize(400, 400);

  // X always goes first. If user goes first, user is X.
  m_userMark = userFirst ? 1 : 2;
  m_compMark = userFirst ? 2 : 1;
  m_userTurn = userFirst;
  m_board.fill(0);

  if (!userFirst)
    QTimer::singleShot(500, this, &TicTacToeWindow::computerMove);
}

void TicTacToeWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), Qt::black);

  int w = width(), h = height();
  int cellW = w / 3, cellH = h / 3;

  // Draw grid
  QPen gridPen(Qt::white, 4);
  p.setPen(gridPen);
  p.drawLine(cellW, 0, cellW, h);
  p.drawLine(cellW * 2, 0, cellW * 2, h);
  p.drawLine(0, cellH, w, cellH);
  p.drawLine(0, cellH * 2, w, cellH * 2);

  // Draw marks
  int pad = 20;
  for (int i = 0; i < 9; ++i) {
    if (m_board[i] == 0) continue;
    int col = i % 3, row = i / 3;
    int x = col * cellW, y = row * cellH;

    if (m_board[i] == 1) { // X - green
      QPen xPen(QColor("#00ff00"), 6);
      p.setPen(xPen);
      p.drawLine(x + pad, y + pad, x + cellW - pad, y + cellH - pad);
      p.drawLine(x + cellW - pad, y + pad, x + pad, y + cellH - pad);
    } else { // O - red
      QPen oPen(Qt::red, 6);
      p.setPen(oPen);
      p.drawEllipse(x + pad, y + pad, cellW - 2 * pad, cellH - 2 * pad);
    }
  }

  // Draw win line
  if (m_gameOver && m_winLine[0] >= 0) {
    auto cellCenterX = [&](int idx) { return (idx % 3) * cellW + cellW / 2; };
    auto cellCenterY = [&](int idx) { return (idx / 3) * cellH + cellH / 2; };
    QPen winPen(QColor("#ffff00"), 5);
    p.setPen(winPen);
    p.drawLine(cellCenterX(m_winLine[0]), cellCenterY(m_winLine[0]),
               cellCenterX(m_winLine[2]), cellCenterY(m_winLine[2]));
  }

  // Draw flash
  if (m_gameOver && m_winLine[0] < 0 && m_flashVisible) {
    QFont f("Courier", 48, QFont::Bold);
    p.setFont(f);
    p.setPen(QColor("#ffff00"));
    p.drawText(rect(), Qt::AlignCenter, "DRAW");
  }
}

void TicTacToeWindow::mousePressEvent(QMouseEvent *event) {
  if (m_gameOver || !m_userTurn) return;

  int col = event->pos().x() / (width() / 3);
  int row = event->pos().y() / (height() / 3);
  int idx = row * 3 + col;

  if (idx < 0 || idx > 8 || m_board[idx] != 0) return;

  m_board[idx] = m_userMark;
  m_userTurn = false;
  update();

  if (checkWin() || boardFull()) {
    endGame();
    return;
  }

  QTimer::singleShot(300, this, &TicTacToeWindow::computerMove);
}

int TicTacToeWindow::minimax(bool isComp) {
  int w = checkWin();
  if (w == m_compMark) return 1;
  if (w == m_userMark) return -1;
  if (boardFull()) return 0;

  int best = isComp ? -1000 : 1000;
  for (int i = 0; i < 9; ++i) {
    if (m_board[i] != 0) continue;
    m_board[i] = isComp ? m_compMark : m_userMark;
    int score = minimax(!isComp);
    m_board[i] = 0;
    best = isComp ? std::max(best, score) : std::min(best, score);
  }
  return best;
}

void TicTacToeWindow::computerMove() {
  if (m_gameOver) return;

  int bestScore = -1000;
  int bestIdx = -1;
  for (int i = 0; i < 9; ++i) {
    if (m_board[i] != 0) continue;
    m_board[i] = m_compMark;
    int score = minimax(false);
    m_board[i] = 0;
    if (score > bestScore) { bestScore = score; bestIdx = i; }
  }

  if (bestIdx < 0) return;
  m_board[bestIdx] = m_compMark;
  m_userTurn = true;
  update();

  if (checkWin() || boardFull())
    endGame();
}

int TicTacToeWindow::checkWin() {
  static const int lines[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},
    {0,3,6},{1,4,7},{2,5,8},
    {0,4,8},{2,4,6}
  };
  for (auto &l : lines) {
    if (m_board[l[0]] != 0 && m_board[l[0]] == m_board[l[1]] && m_board[l[1]] == m_board[l[2]]) {
      return m_board[l[0]];
    }
  }
  return 0;
}

bool TicTacToeWindow::boardFull() {
  for (int v : m_board)
    if (v == 0) return false;
  return true;
}

void TicTacToeWindow::endGame() {
  m_gameOver = true;

  // Find winning line if any
  static const int lines[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},
    {0,3,6},{1,4,7},{2,5,8},
    {0,4,8},{2,4,6}
  };
  for (auto &l : lines) {
    if (m_board[l[0]] != 0 && m_board[l[0]] == m_board[l[1]] && m_board[l[1]] == m_board[l[2]]) {
      m_winLine[0] = l[0]; m_winLine[1] = l[1]; m_winLine[2] = l[2];
      break;
    }
  }

  if (m_winLine[0] >= 0) {
    // Win - draw line and close after 5s
    update();
    QTimer::singleShot(5000, this, &QWidget::close);
  } else {
    // Draw - flash 5 times (on 300ms, off 300ms)
    m_flashCount = 0;
    m_flashVisible = true;
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
      m_flashVisible = !m_flashVisible;
      update();
      if (!m_flashVisible) m_flashCount++;
      if (m_flashCount >= 5) {
        timer->stop();
        close();
      }
    });
    timer->start(300);
    update();
  }
}
