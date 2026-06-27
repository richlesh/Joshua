// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef GOWINDOW_H
#define GOWINDOW_H

#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <vector>

class GoWindow : public QWidget {
  Q_OBJECT
public:
  explicit GoWindow(bool userFirst, int maxVisits, bool suggest, int boardSize = 19, QWidget *parent = nullptr);
  explicit GoWindow(bool userFirst, int compVisits, int humanVisits, int boardSize = 19, QWidget *parent = nullptr);

  enum Stone : int8_t { Empty = 0, Black = 1, White = -1 };

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  int m_boardSize;
  std::vector<int8_t> m_board;

  bool m_userIsBlack;
  bool m_userTurn;
  bool m_gameOver = false;
  bool m_blackTurn = true;
  int m_maxVisits;
  bool m_suggest;
  bool m_autoPlay = false;
  int m_humanVisits = 100;
  int m_suggestedRow = -1;
  int m_suggestedCol = -1;
  int m_hoverRow = -1;
  int m_hoverCol = -1;
  int m_lastMoveRow = -1;
  int m_lastMoveCol = -1;
  int m_flashCount = 0;
  bool m_flashVisible = false;
  bool m_userWon = false;
  bool m_isDraw = false;
  QString m_resultText;
  QTimer m_flashTimer;
  int m_consecutivePasses = 0;
  int m_computerPasses = 0;

  // Engine
  QProcess *m_engine = nullptr;
  bool m_engineReady = false;
  bool m_engineMissing = false;
  bool m_waitingForMove = false;
  bool m_waitingForSuggestion = false;
  QString m_engineBuffer;

  void startEngine();
  void sendGTP(const QString &cmd);
  void onEngineOutput();
  void requestComputerMove();
  void requestSuggestion();
  void handleEngineMove(const QString &move);
  void applyMove(int row, int col, Stone stone);
  void pass();
  void endGame(const QString &result);

  // Helpers
  int margin();
  int cellSize();
  QPoint stoneCenter(int row, int col);
  bool posFromPixel(const QPoint &pos, int &row, int &col);
  QString coordToGTP(int row, int col);
  bool gtpToCoord(const QString &s, int &row, int &col);
};

#endif // GOWINDOW_H
