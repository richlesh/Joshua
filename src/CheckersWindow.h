// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef CHECKERSWINDOW_H
#define CHECKERSWINDOW_H

#include <QWidget>
#include <QTimer>
#include <vector>
#include <array>

class CheckersWindow : public QWidget {
  Q_OBJECT
public:
  explicit CheckersWindow(bool userFirst, int depth, bool suggest, QWidget *parent = nullptr);
  explicit CheckersWindow(bool userFirst, int compDepth, int humanDepth, QWidget *parent = nullptr); // computer vs computer

  enum Piece : int8_t { Empty = 0, Dark = 1, DarkKing = 2, Light = -1, LightKing = -2 };

  struct Move {
    int from;
    int to;
    std::vector<int> captured;
    bool kinged = false;
  };

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  // Board: 8x8, indexed row*8+col. Only dark squares (row+col odd) used.
  std::array<int8_t, 64> m_board{};

  bool m_userIsLight; // user plays light (red) pieces
  bool m_userTurn;
  bool m_gameOver = false;
  int m_depth;
  bool m_suggest;
  bool m_autoPlay = false;
  bool m_darkTurn = true;
  int m_humanDepth = 5;
  int m_suggestedFrom = -1;
  int m_suggestedTo = -1;
  int m_effectiveCompDepth = 0;
  int m_effectivePlayerDepth = 0;
  int m_flashCount = 0;
  bool m_flashVisible = false;
  bool m_userWon = false;
  bool m_isDraw = false;
  int m_movesSinceProgress = 0;
  std::vector<std::array<int8_t, 64>> m_positionHistory;
  QTimer m_flashTimer;

  int m_selected = -1; // selected square index
  std::vector<Move> m_validMoves; // current valid moves for selected piece
  std::vector<int> m_highlightSquares;

  // Multi-jump state
  bool m_midJump = false;
  int m_jumpPiece = -1;

  // Drag state
  bool m_dragging = false;
  int m_dragFrom = -1;
  QPoint m_dragPos;

  // Animation state
  bool m_animating = false;
  QTimer m_animTimer;
  QPointF m_animPos;
  QPointF m_animStart;
  QPointF m_animEnd;
  int m_animFrame = 0;
  int m_animFrames = 0;
  int m_animPieceIdx = -1;
  std::vector<int> m_animWaypoints; // sequence of board indices
  int m_animStep = 0;
  Move m_animMove;
  std::vector<int> m_animCapturedSoFar;

  void initBoard();
  void computerMove();

  // Move generation
  std::vector<Move> generateMoves(bool forDark);
  std::vector<Move> generatePieceMoves(int pos);
  std::vector<Move> generatePieceJumps(int pos, std::array<int8_t, 64> &board, std::vector<int> captured);
  bool hasJumps(bool forDark);

  // AI
  int minimax(int depth, int alpha, int beta, bool maximizing);
  int evaluate();

  // Helpers
  bool isDark(int8_t p) { return p > 0; }
  bool isLight(int8_t p) { return p < 0; }
  bool isKing(int8_t p) { return p == DarkKing || p == LightKing; }
  bool onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
  int displayRow(int r) { return m_userIsLight ? r : 7 - r; }
  int displayCol(int c) { return m_userIsLight ? c : 7 - c; }
  void applyMove(const Move &m);
  void applyMoveWithTracking(const Move &m);
  void checkGameOver();
  void endGame(bool userWon);
  void endDraw();
  void startAnimation(const Move &mv);
  void advanceAnimation();
  QPointF squareCenter(int idx);
  void computeSuggestion();
};

#endif // CHECKERSWINDOW_H
