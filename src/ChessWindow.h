// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef CHESSWINDOW_H
#define CHESSWINDOW_H

#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <array>
#include <vector>

class ChessWindow : public QWidget {
  Q_OBJECT
public:
  explicit ChessWindow(bool userFirst, int elo, bool suggest, QWidget *parent = nullptr);
  explicit ChessWindow(bool userFirst, int compElo, int humanElo, QWidget *parent = nullptr);

  enum Piece : int8_t {
    Empty = 0,
    WPawn = 1, WKnight = 2, WBishop = 3, WRook = 4, WQueen = 5, WKing = 6,
    BPawn = -1, BKnight = -2, BBishop = -3, BRook = -4, BQueen = -5, BKing = -6
  };

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private:
  std::array<int8_t, 64> m_board{};

  bool m_userIsWhite;
  bool m_userTurn;
  bool m_gameOver = false;
  bool m_whiteTurn = true;
  int m_elo;
  bool m_suggest;
  bool m_autoPlay = false;
  int m_humanElo = 1500;
  int m_suggestedFrom = -1;
  int m_suggestedTo = -1;
  int m_flashCount = 0;
  bool m_flashVisible = false;
  bool m_userWon = false;
  bool m_isDraw = false;
  QTimer m_flashTimer;

  // Castling rights
  bool m_whiteKingSide = true;
  bool m_whiteQueenSide = true;
  bool m_blackKingSide = true;
  bool m_blackQueenSide = true;

  // En passant target square (-1 if none)
  int m_epSquare = -1;

  // Move history in UCI format
  QStringList m_moveHistory;

  // Draw detection
  int m_halfMoveClock = 0; // moves since last pawn move or capture (50-move rule)
  std::vector<std::array<int8_t, 64>> m_positionHistory;

  // Drag state
  bool m_dragging = false;
  int m_dragFrom = -1;
  QPoint m_dragPos;

  // Valid move targets for dragged piece
  std::vector<int> m_validTargets;
  int m_selected = -1;

  // All legal moves in UCI format for current position
  QStringList m_legalMoves;

  // Animation state
  bool m_animating = false;
  QTimer m_animTimer;
  QPointF m_animPos;
  QPointF m_animStart;
  QPointF m_animEnd;
  int m_animFrame = 0;
  int m_animFrames = 15;
  int m_animFrom = -1;
  int m_animTo = -1;
  int8_t m_animPiece = Empty;
  int m_animCapturedIdx = -1;

  // Stockfish engine
  QProcess *m_engine = nullptr;
  QProcess *m_suggestEngine = nullptr;
  bool m_engineReady = false;
  bool m_suggestEngineReady = false;
  bool m_engineMissing = false;
  bool m_waitingForMove = false;
  bool m_waitingForSuggestion = false;
  QString m_engineOutput;
  QString m_suggestOutput;

  void initBoard();
  void startEngine(QProcess *&proc, bool &readyFlag, int elo);
  void sendToEngine(QProcess *proc, const QString &cmd);
  void requestComputerMove();
  void requestSuggestion();
  void onEngineOutput();
  void onSuggestOutput();
  void applyUCIMove(const QString &uci);
  void startAnimation(int from, int to, int8_t piece, int capturedIdx = -1);
  void advanceAnimation();
  void endGame(bool userWon);
  void endDraw();
  bool checkDrawConditions();

  // Local move generation
  QStringList generateLegalMoves();
  QStringList generatePseudoMoves(bool forWhite);
  bool isSquareAttacked(int sq, bool byWhite);
  int findKing(bool white);
  bool inCheck(bool white);
  bool moveLeavesSafe(const QString &uci, bool forWhite);

  // Helpers
  bool isWhite(int8_t p) { return p > 0; }
  bool isBlack(int8_t p) { return p < 0; }
  int displayRow(int r) { return m_userIsWhite ? r : 7 - r; }
  int displayCol(int c) { return m_userIsWhite ? c : 7 - c; }
  QPointF squareCenter(int idx);
  QString positionCommand();
  int uciToIndex(const QString &sq);
  QString indexToUCI(int idx);
  QString pieceChar(int8_t p);
};

#endif // CHESSWINDOW_H
