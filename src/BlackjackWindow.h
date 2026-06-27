// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef BLACKJACKWINDOW_H
#define BLACKJACKWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QPixmap>
#include <vector>
#include <random>

class BlackjackWindow : public QWidget {
  Q_OBJECT
public:
  explicit BlackjackWindow(QWidget *parent = nullptr);

  struct Card {
    int rank; // 1=Ace, 2-10, 11=J, 12=Q, 13=K
    int suit; // 0=spades, 1=hearts, 2=diamonds, 3=clubs
  };

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void onHit();
  void onStand();
  void onDoubleDown();
  void onDeal();

private:
  // Shoe
  std::vector<Card> m_shoe;
  int m_shoePos = 0;
  std::mt19937 m_rng;

  // Hands
  std::vector<Card> m_playerHand;
  std::vector<Card> m_dealerHand;

  // State
  enum State { Betting, Playing, DealerTurn, RoundOver };
  State m_state = Betting;
  int m_balance = 1000;
  int m_bet = 100;
  bool m_dealerHoleRevealed = false;
  QString m_resultText;

  // Buttons
  QPushButton *m_hitBtn;
  QPushButton *m_standBtn;
  QPushButton *m_doubleBtn;
  QPushButton *m_dealBtn;
  QPushButton *m_betUpBtn;
  QPushButton *m_betDownBtn;

  // Dealer animation
  QTimer m_dealerTimer;
  void dealerPlay();
  void advanceDealerPlay();

  void initShoe();
  void shuffleShoe();
  Card drawCard();

  // Card sprite sheet
  QPixmap m_cardSheet;
  int handValue(const std::vector<Card> &hand) const;
  bool isSoft(const std::vector<Card> &hand) const;
  QString cardStr(const Card &c) const;
  QString suitChar(int suit) const;
  void updateButtons();
  void resolveRound();
};

#endif // BLACKJACKWINDOW_H
