// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef TICTACTOEWINDOW_H
#define TICTACTOEWINDOW_H

#include <QWidget>
#include <array>

class TicTacToeWindow : public QWidget {
  Q_OBJECT
public:
  explicit TicTacToeWindow(bool userFirst, QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  void computerMove();
  int minimax(bool isComp);
  int checkWin(); // returns 1=X wins, 2=O wins, 0=none
  bool boardFull();
  void endGame();

  std::array<int, 9> m_board{}; // 0=empty, 1=X, 2=O
  int m_userMark;   // 1=X or 2=O
  int m_compMark;
  bool m_userTurn;
  bool m_gameOver = false;
  int m_winLine[3] = {-1, -1, -1};
  int m_flashCount = 0;
  bool m_flashVisible = false;
};

#endif // TICTACTOEWINDOW_H
