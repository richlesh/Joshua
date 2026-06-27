// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef REVERSIWINDOW_H
#define REVERSIWINDOW_H

#include <QWidget>
#include <QTimer>
#include <array>
#include <vector>

class ReversiWindow : public QWidget {
  Q_OBJECT
public:
  explicit ReversiWindow(bool userFirst, int depth, bool suggest, QWidget *parent = nullptr);
  explicit ReversiWindow(bool userFirst, int compDepth, int humanDepth, QWidget *parent = nullptr);

  enum Disc : int8_t { Empty = 0, Black = 1, White = -1 };

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  std::array<int8_t, 64> m_board{};
  bool m_userIsBlack;
  bool m_userTurn;
  bool m_gameOver = false;
  bool m_blackTurn = true;
  int m_depth;
  bool m_suggest;
  bool m_autoPlay = false;
  int m_humanDepth = 5;
  int m_suggestedMove = -1;
  int m_hoverIdx = -1;
  int m_flashCount = 0;
  bool m_flashVisible = false;
  bool m_userWon = false;
  bool m_isDraw = false;
  QTimer m_flashTimer;

  // Flip animation
  bool m_animating = false;
  QTimer m_animTimer;
  int m_animFrame = 0;
  std::vector<int> m_flipping; // indices being flipped
  int m_lastPlaced = -1;

  void initBoard();
  void computerMove();
  void makeMove(int idx, int8_t disc);
  std::vector<int> getFlips(int idx, int8_t disc) const;
  std::vector<int> getFlipsDir(int idx, int8_t disc, int dr, int dc) const;
  std::vector<int> validMoves(int8_t disc) const;
  int minimax(int depth, int alpha, int beta, bool maximizing);
  int evaluate() const;
  void checkGameOver();
  void endGame();
  void computeSuggestion();
  void advanceFlipAnim();
};

#endif // REVERSIWINDOW_H
