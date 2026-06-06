// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef FOURACROSSWINDOW_H
#define FOURACROSSWINDOW_H

#include <QWidget>
#include <QTimer>
#include <array>

class FourAcrossWindow : public QWidget {
  Q_OBJECT
public:
  // Normal play
  explicit FourAcrossWindow(bool userFirst, int depth, bool suggest, QWidget *parent = nullptr);
  // Computer vs computer
  explicit FourAcrossWindow(bool userFirst, int compDepth, int humanDepth, QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  static const int kCols = 7;
  static const int kRows = 6;
  static const int kCellSize = 80;
  static const int kTopMargin = 80; // space above board for sliding piece

  enum Cell : int8_t { Empty = 0, Player1 = 1, Player2 = 2 };

  std::array<int8_t, kCols * kRows> m_board{}; // col-major: board[col * kRows + row], row 0 = bottom
  int8_t m_userPiece;
  int8_t m_compPiece;
  bool m_userTurn;
  bool m_gameOver = false;
  bool m_isDraw = false;
  bool m_userWon = false;
  int m_depth;
  bool m_suggest;
  bool m_autoPlay = false;
  int m_humanDepth = 5;

  // Hover/slide state
  int m_hoverCol = -1;

  // Drop animation
  bool m_animating = false;
  QTimer m_animTimer;
  int m_animCol = -1;
  int m_animTargetRow = -1;
  double m_animY = 0;
  double m_animTargetY = 0;
  int8_t m_animPiece = Empty;

  // Flash state
  int m_flashCount = 0;
  bool m_flashVisible = false;
  QTimer m_flashTimer;
  std::array<int, 4> m_winCells{}; // indices of winning 4

  // Suggestion
  int m_suggestedCol = -1;

  // Helpers
  int cell(int col, int row) { return col * kRows + row; }
  int dropRow(int col);
  bool isFull();
  int checkWin(); // 0=none, 1=P1, 2=P2
  bool findWinLine(int8_t piece);

  // AI
  void computerMove();
  int minimax(int depth, int alpha, int beta, bool maximizing);
  int evaluate();
  void computeSuggestion();

  // Game flow
  void dropPiece(int col, int8_t piece);
  void finishDrop();
  void checkGameOver();
  void endGame(bool userWon);
  void endDraw();
  void nextTurn();
};

#endif // FOURACROSSWINDOW_H
