// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef POKERWINDOW_H
#define POKERWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QPixmap>
#include <vector>
#include <array>
#include <random>
#include <QString>

class PokerWindow : public QWidget {
  Q_OBJECT
public:
  explicit PokerWindow(int numOpponents = 4, QWidget *parent = nullptr);

  struct Card { int rank; int suit; }; // rank 1-13, suit 0-3

  enum HandRank { HighCard=0, OnePair, TwoPair, ThreeOfAKind, Straight, Flush,
                  FullHouse, FourOfAKind, StraightFlush, RoyalFlush };

  struct Player {
    QString name;
    int chips = 1000;
    Card hole[2];
    bool folded = false;
    bool allIn = false;
    bool isHuman = false;
    int currentBet = 0;
  };

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void onFold();
  void onCall();
  void onRaise();
  void onDeal();

private:
  // Players (0 = human, 1-4 = AI)
  std::vector<Player> m_players;
  int m_dealerIdx = 0;
  int m_currentPlayer = 0;
  int m_numPlayers = 5;

  // Community cards
  std::vector<Card> m_community;

  // Deck
  std::vector<Card> m_deck;
  int m_deckPos = 0;
  std::mt19937 m_rng;

  // Pot and betting
  int m_pot = 0;
  int m_currentBet = 0;
  int m_minRaise = 25;
  int m_smallBlind = 25;
  int m_bigBlind = 50;
  int m_numActed = 0; // how many players have acted this betting round

  // Round state
  enum Phase { PreFlop, Flop, Turn, River, Showdown, WaitingToDeal };
  Phase m_phase = WaitingToDeal;
  bool m_roundActive = false;

  // AI timer
  QTimer m_aiTimer;

  // Buttons
  QPushButton *m_foldBtn;
  QPushButton *m_callBtn;
  QPushButton *m_raiseBtn;
  QPushButton *m_dealBtn;
  QPushButton *m_raiseUpBtn;
  QPushButton *m_raiseDownBtn;
  int m_raiseAmount = 25;

  // Card sprite sheet
  QPixmap m_cardSheet;

  void initDeck();
  void shuffleDeck();
  Card drawCard();
  void startRound();
  void postBlinds();
  void dealHoleCards();
  void nextPhase();
  void dealCommunity(int count);
  void startBettingRound();
  void advanceToNextPlayer();
  void aiAction();
  void playerAction(int playerIdx, int action, int amount = 0); // 0=fold,1=call,2=raise
  bool bettingComplete();
  void showdown();
  void awardPot();
  void updateButtons();
  void removeEliminatedPlayers();

  // Hand evaluation
  HandRank evaluateHand(const std::vector<Card> &cards);
  int handScore(const std::vector<Card> &cards);
  int bestHandScore(int playerIdx);
  double handStrength(int playerIdx);
  QString handRankName(HandRank rank);

  // AI heuristic
  int aiDecide(int playerIdx);

  // Drawing helpers
  void drawCardAt(QPainter &p, const Card &c, int x, int y, bool faceDown);
  QString rankStr(int rank);
};

#endif // POKERWINDOW_H
