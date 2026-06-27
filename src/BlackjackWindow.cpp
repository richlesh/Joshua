// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "BlackjackWindow.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <chrono>
#include <cmath>

static const int kWindowW = 750;
static const int kWindowH = 520;
static const int kCardW = 90;
static const int kCardH = 128;

BlackjackWindow::BlackjackWindow(QWidget *parent) : QWidget(parent) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Blackjack");
  setFixedSize(kWindowW, kWindowH);

  m_rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
  initShoe();
  m_cardSheet = QPixmap(":/cards.png");

  // Buttons at bottom
  m_hitBtn = new QPushButton("Hit", this);
  m_standBtn = new QPushButton("Stand", this);
  m_doubleBtn = new QPushButton("Double", this);
  m_dealBtn = new QPushButton("Deal", this);

  m_hitBtn->setGeometry(130, kWindowH - 45, 70, 35);
  m_standBtn->setGeometry(210, kWindowH - 45, 70, 35);
  m_doubleBtn->setGeometry(290, kWindowH - 45, 70, 35);
  m_dealBtn->setGeometry(380, kWindowH - 45, 70, 35);

  m_betDownBtn = new QPushButton("-$25", this);
  m_betUpBtn = new QPushButton("+$25", this);
  m_betDownBtn->setGeometry(130, kWindowH - 45, 70, 35);
  m_betUpBtn->setGeometry(210, kWindowH - 45, 70, 35);

  connect(m_hitBtn, &QPushButton::clicked, this, &BlackjackWindow::onHit);
  connect(m_standBtn, &QPushButton::clicked, this, &BlackjackWindow::onStand);
  connect(m_doubleBtn, &QPushButton::clicked, this, &BlackjackWindow::onDoubleDown);
  connect(m_dealBtn, &QPushButton::clicked, this, &BlackjackWindow::onDeal);
  connect(m_betDownBtn, &QPushButton::clicked, this, [this]() {
    if (m_bet > 25) { m_bet -= 25; update(); }
  });
  connect(m_betUpBtn, &QPushButton::clicked, this, [this]() {
    if (m_bet < std::min(500, m_balance)) { m_bet += 25; update(); }
  });

  connect(&m_dealerTimer, &QTimer::timeout, this, &BlackjackWindow::advanceDealerPlay);

  updateButtons();
}

void BlackjackWindow::initShoe() {
  m_shoe.clear();
  for (int deck = 0; deck < 6; deck++)
    for (int suit = 0; suit < 4; suit++)
      for (int rank = 1; rank <= 13; rank++)
        m_shoe.push_back({rank, suit});
  shuffleShoe();
}

void BlackjackWindow::shuffleShoe() {
  std::shuffle(m_shoe.begin(), m_shoe.end(), m_rng);
  m_shoePos = 0;
}

BlackjackWindow::Card BlackjackWindow::drawCard() {
  // Reshuffle at 75% penetration
  if (m_shoePos >= (int)(m_shoe.size() * 0.75))
    shuffleShoe();
  return m_shoe[m_shoePos++];
}

int BlackjackWindow::handValue(const std::vector<Card> &hand) const {
  int value = 0, aces = 0;
  for (auto &c : hand) {
    if (c.rank == 1) { value += 11; aces++; }
    else if (c.rank >= 10) value += 10;
    else value += c.rank;
  }
  while (value > 21 && aces > 0) { value -= 10; aces--; }
  return value;
}

bool BlackjackWindow::isSoft(const std::vector<Card> &hand) const {
  int value = 0, aces = 0;
  for (auto &c : hand) {
    if (c.rank == 1) { value += 11; aces++; }
    else if (c.rank >= 10) value += 10;
    else value += c.rank;
  }
  while (value > 21 && aces > 0) { value -= 10; aces--; }
  return aces > 0;
}

QString BlackjackWindow::suitChar(int suit) const {
  switch (suit) {
  case 0: return QStringLiteral("\u2660"); // ♠
  case 1: return QStringLiteral("\u2665"); // ♥
  case 2: return QStringLiteral("\u2666"); // ♦
  case 3: return QStringLiteral("\u2663"); // ♣
  default: return "?";
  }
}

QString BlackjackWindow::cardStr(const Card &c) const {
  QString r;
  if (c.rank == 1) r = "A";
  else if (c.rank == 11) r = "J";
  else if (c.rank == 12) r = "Q";
  else if (c.rank == 13) r = "K";
  else r = QString::number(c.rank);
  return r + suitChar(c.suit);
}

// --- Actions ---

void BlackjackWindow::onDeal() {
  if (m_balance <= 0) { m_balance = 1000; m_bet = 100; } // reset if broke
  if (m_bet > m_balance) m_bet = m_balance;
  if (m_bet < 25) m_bet = 25;
  m_playerHand.clear();
  m_dealerHand.clear();
  m_dealerHoleRevealed = false;
  m_resultText.clear();

  m_playerHand.push_back(drawCard());
  m_dealerHand.push_back(drawCard());
  m_playerHand.push_back(drawCard());
  m_dealerHand.push_back(drawCard());

  m_state = Playing;

  // Check for natural blackjack
  if (handValue(m_playerHand) == 21) {
    m_dealerHoleRevealed = true;
    if (handValue(m_dealerHand) == 21) {
      m_resultText = "Push! Both have Blackjack.";
      m_state = RoundOver;
    } else {
      m_resultText = "Blackjack! You win!";
      m_balance += (int)(m_bet * 1.5);
      m_state = RoundOver;
    }
  } else if (handValue(m_dealerHand) == 21) {
    m_dealerHoleRevealed = true;
    m_resultText = "Dealer has Blackjack. You lose.";
    m_balance -= m_bet;
    m_state = RoundOver;
  }

  updateButtons();
  update();
}

void BlackjackWindow::onHit() {
  if (m_state != Playing) return;
  m_playerHand.push_back(drawCard());
  if (handValue(m_playerHand) > 21) {
    m_resultText = "Bust! You lose.";
    m_balance -= m_bet;
    m_dealerHoleRevealed = true;
    m_state = RoundOver;
  } else if (handValue(m_playerHand) == 21) {
    onStand(); // auto-stand on 21
    return;
  }
  updateButtons();
  update();
}

void BlackjackWindow::onStand() {
  if (m_state != Playing) return;
  m_state = DealerTurn;
  m_dealerHoleRevealed = true;
  updateButtons();
  update();
  m_dealerTimer.start(600);
}

void BlackjackWindow::onDoubleDown() {
  if (m_state != Playing || m_playerHand.size() != 2) return;
  if (m_balance < m_bet * 2) return; // can't afford
  m_bet *= 2;
  m_playerHand.push_back(drawCard());
  if (handValue(m_playerHand) > 21) {
    m_resultText = "Bust! You lose.";
    m_balance -= m_bet;
    m_dealerHoleRevealed = true;
    m_state = RoundOver;
    updateButtons();
    update();
  } else {
    onStand();
  }
}

void BlackjackWindow::advanceDealerPlay() {
  int dv = handValue(m_dealerHand);
  // Dealer hits on 16 or less, and soft 17
  if (dv < 17 || (dv == 17 && isSoft(m_dealerHand))) {
    m_dealerHand.push_back(drawCard());
    update();
  } else {
    m_dealerTimer.stop();
    resolveRound();
  }
}

void BlackjackWindow::resolveRound() {
  int pv = handValue(m_playerHand);
  int dv = handValue(m_dealerHand);

  if (dv > 21) {
    m_resultText = "Dealer busts! You win!";
    m_balance += m_bet;
  } else if (pv > dv) {
    m_resultText = "You win!";
    m_balance += m_bet;
  } else if (dv > pv) {
    m_resultText = "Dealer wins.";
    m_balance -= m_bet;
  } else {
    m_resultText = "Push!";
  }
  m_state = RoundOver;
  updateButtons();
  update();
}

void BlackjackWindow::updateButtons() {
  m_hitBtn->setVisible(m_state == Playing);
  m_standBtn->setVisible(m_state == Playing);
  m_doubleBtn->setVisible(m_state == Playing && m_playerHand.size() == 2 && m_balance >= m_bet * 2);
  bool canBet = (m_state == RoundOver || m_state == Betting);
  m_dealBtn->setVisible(canBet);
  m_betDownBtn->setVisible(canBet);
  m_betUpBtn->setVisible(canBet);
  // Position bet buttons next to deal
  if (canBet) {
    m_betDownBtn->setGeometry(130, kWindowH - 45, 70, 35);
    m_betUpBtn->setGeometry(210, kWindowH - 45, 70, 35);
    m_dealBtn->setGeometry(290, kWindowH - 45, 70, 35);
  }
}

// --- Painting ---

void BlackjackWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Green felt background
  p.fillRect(rect(), QColor(0, 100, 0));

  // Card sprite sheet: 13 columns (A,2..K) x 4 rows (Clubs,Hearts,Spades,Diamonds)
  static const int kSheetCardW = 256, kSheetCardH = 356;
  // Map suit enum (0=spades,1=hearts,2=diamonds,3=clubs) to sprite row
  static const int suitToRow[] = {2, 1, 3, 0};

  auto drawCardAt = [&](const Card &c, int x, int y, bool faceDown) {
    QRect cr(x, y, kCardW, kCardH);
    if (!faceDown && !m_cardSheet.isNull()) {
      int col = c.rank - 1; // A=0, 2=1, ... K=12
      int row = suitToRow[c.suit];
      QRect src(col * kSheetCardW, row * kSheetCardH, kSheetCardW, kSheetCardH);
      p.setRenderHint(QPainter::SmoothPixmapTransform, true);
      p.drawPixmap(cr, m_cardSheet, src);
    } else {
      p.setPen(QPen(Qt::black, 1));
      p.setBrush(QColor(50, 50, 200));
      p.drawRoundedRect(cr, 6, 6);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(30, 30, 180));
      p.drawRoundedRect(cr.adjusted(4, 4, -4, -4), 4, 4);
    }
  };

  // Dealer label
  QFont labelFont;
  labelFont.setPixelSize(14);
  labelFont.setBold(true);
  p.setFont(labelFont);
  p.setPen(Qt::white);
  p.drawText(20, 30, "Dealer");

  // Dealer hand
  for (int i = 0; i < (int)m_dealerHand.size(); i++) {
    bool faceDown = (i == 1 && !m_dealerHoleRevealed);
    drawCardAt(m_dealerHand[i], 20 + i * (kCardW + 10), 40, faceDown);
  }

  // Dealer value
  if (m_dealerHoleRevealed && !m_dealerHand.empty()) {
    p.setFont(labelFont);
    p.setPen(Qt::yellow);
    int dv = handValue(m_dealerHand);
    p.drawText(20 + (int)m_dealerHand.size() * (kCardW + 10) + 10, 80,
               QString("(%1)").arg(dv));
  }

  // Player label
  p.setFont(labelFont);
  p.setPen(Qt::white);
  p.drawText(20, 200, "You");

  // Player hand
  for (int i = 0; i < (int)m_playerHand.size(); i++)
    drawCardAt(m_playerHand[i], 20 + i * (kCardW + 10), 210, false);

  // Player value
  if (!m_playerHand.empty()) {
    p.setFont(labelFont);
    p.setPen(Qt::yellow);
    int pv = handValue(m_playerHand);
    p.drawText(20 + (int)m_playerHand.size() * (kCardW + 10) + 10, 250,
               QString("(%1)").arg(pv));
  }

  // Balance and bet
  p.setFont(labelFont);
  p.setPen(Qt::white);
  p.drawText(20, kWindowH - 60, QString("Balance: $%1    Bet: $%2").arg(m_balance).arg(m_bet));

  // Chip display - break balance into $100, $50, $25 chips
  {
    int remaining = m_balance;
    struct ChipStack { int value; QColor color; QColor edge; int count; };
    ChipStack stacks[] = {
      {100, QColor(40, 40, 40), QColor(80, 80, 80), 0},    // black $100
      {50, QColor(0, 120, 0), QColor(0, 180, 0), 0},       // green $50
      {25, QColor(180, 0, 0), QColor(220, 60, 60), 0}      // red $25
    };
    for (auto &s : stacks) {
      s.count = remaining / s.value;
      remaining -= s.count * s.value;
    }

    int chipR = 16;
    int stackSpacing = chipR * 2 + 12;
    int baseY = kWindowH - 90;
    int chipVOffset = 4; // vertical offset per chip in stack
    int startX = 30;
    int stackIdx = 0;

    for (auto &s : stacks) {
      int chipsLeft = s.count;
      while (chipsLeft > 0) {
        int inStack = std::min(chipsLeft, 10);
        int sx = startX + stackIdx * stackSpacing;
        for (int i = 0; i < inStack; i++) {
          int cy = baseY - i * chipVOffset;
          // Chip edge
          p.setPen(QPen(s.edge, 1));
          p.setBrush(s.color);
          p.drawEllipse(QPoint(sx, cy), chipR, chipR / 2);
          // Dashes around edge
          p.setPen(QPen(Qt::white, 1));
          for (int d = 0; d < 6; d++) {
            double angle = d * 3.14159 / 3.0;
            int dx = (int)(chipR * 0.75 * cos(angle));
            int dy = (int)(chipR * 0.35 * sin(angle));
            p.drawPoint(sx + dx, cy + dy);
          }
        }
        // Value label on top chip
        QFont chipFont;
        chipFont.setPixelSize(9);
        chipFont.setBold(true);
        p.setFont(chipFont);
        p.setPen(Qt::white);
        int topY = baseY - (inStack - 1) * chipVOffset;
        p.drawText(QRect(sx - chipR, topY - 5, chipR * 2, 10), Qt::AlignCenter,
                   QString("$%1").arg(s.value));
        chipsLeft -= inStack;
        stackIdx++;
      }
    }
  }

  // Result text
  if (!m_resultText.isEmpty()) {
    QFont rf;
    rf.setPixelSize(28);
    rf.setBold(true);
    p.setFont(rf);
    p.setPen(QColor(255, 255, 0));
    p.drawText(QRect(0, 340, kWindowW, 50), Qt::AlignCenter, m_resultText);
  }

  // Instructions when in betting state
  if (m_state == Betting && m_playerHand.empty()) {
    QFont inf;
    inf.setPixelSize(20);
    p.setFont(inf);
    p.setPen(QColor(200, 200, 200));
    p.drawText(QRect(0, 200, kWindowW, 50), Qt::AlignCenter, "Press Deal to start");
  }
}
