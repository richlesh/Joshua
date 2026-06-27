// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "PokerWindow.h"
#include <QPainter>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>

static const int kWinW = 900;
static const int kWinH = 600;
static const int kCardW = 55;
static const int kCardH = 78;
static const int kSheetCardW = 256, kSheetCardH = 356;
static const int suitToRow[] = {2, 1, 3, 0}; // spades=0,hearts=1,diamonds=2,clubs=3 -> row

PokerWindow::PokerWindow(int numOpponents, QWidget *parent) : QWidget(parent) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Texas Hold'em");
  setFixedSize(kWinW, kWinH);

  m_rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
  m_cardSheet = QPixmap(":/cards.png");

  // Setup players
  m_numPlayers = numOpponents + 1;
  m_players.resize(m_numPlayers);
  m_players[0] = {"You", 1000, {}, false, false, true, 0};
  static const char* names[] = {"Alice", "Bob", "Carol", "Dave", "Eve"};
  for (int i = 1; i < m_numPlayers && i <= 5; i++)
    m_players[i] = {names[i-1], 1000, {}, false, false, false, 0};

  m_foldBtn = new QPushButton("Fold", this);
  m_callBtn = new QPushButton("Call", this);
  m_raiseBtn = new QPushButton("Raise", this);
  m_dealBtn = new QPushButton("Deal", this);

  m_foldBtn->setGeometry(250, kWinH - 45, 70, 35);
  m_callBtn->setGeometry(330, kWinH - 45, 90, 35);
  m_raiseBtn->setGeometry(430, kWinH - 45, 90, 35);
  m_dealBtn->setGeometry(380, kWinH - 45, 80, 35);

  m_raiseDownBtn = new QPushButton("-", this);
  m_raiseUpBtn = new QPushButton("+", this);
  m_raiseDownBtn->setGeometry(530, kWinH - 45, 30, 35);
  m_raiseUpBtn->setGeometry(565, kWinH - 45, 30, 35);

  connect(m_foldBtn, &QPushButton::clicked, this, &PokerWindow::onFold);
  connect(m_callBtn, &QPushButton::clicked, this, &PokerWindow::onCall);
  connect(m_raiseBtn, &QPushButton::clicked, this, &PokerWindow::onRaise);
  connect(m_dealBtn, &QPushButton::clicked, this, &PokerWindow::onDeal);
  connect(m_raiseDownBtn, &QPushButton::clicked, this, [this]() {
    if (m_raiseAmount > 25) { m_raiseAmount -= 25; updateButtons(); }
  });
  connect(m_raiseUpBtn, &QPushButton::clicked, this, [this]() {
    int maxRaise = m_players[0].chips - (m_currentBet - m_players[0].currentBet);
    if (m_raiseAmount + 25 <= maxRaise) { m_raiseAmount += 25; updateButtons(); }
  });
  connect(&m_aiTimer, &QTimer::timeout, this, [this]() {
    m_aiTimer.stop();
    aiAction();
  });

  updateButtons();
}

void PokerWindow::initDeck() {
  m_deck.clear();
  for (int s = 0; s < 4; s++)
    for (int r = 1; r <= 13; r++)
      m_deck.push_back({r, s});
}

void PokerWindow::shuffleDeck() {
  initDeck();
  std::shuffle(m_deck.begin(), m_deck.end(), m_rng);
  m_deckPos = 0;
}

PokerWindow::Card PokerWindow::drawCard() {
  return m_deck[m_deckPos++];
}

void PokerWindow::startRound() {
  shuffleDeck();
  m_community.clear();
  m_pot = 0;
  m_currentBet = 0;
  m_phase = PreFlop;
  m_roundActive = true;

  for (auto &p : m_players) {
    p.folded = (p.chips <= 0);
    p.allIn = false;
    p.currentBet = 0;
  }

  postBlinds();
  dealHoleCards();
  startBettingRound();
}

void PokerWindow::postBlinds() {
  // Find next active players for blinds
  auto nextActive = [&](int from) {
    int i = (from + 1) % m_numPlayers;
    while (m_players[i].chips <= 0) i = (i + 1) % m_numPlayers;
    return i;
  };
  int sb = nextActive(m_dealerIdx);
  int bb = nextActive(sb);

  int sbAmt = std::min(m_smallBlind, m_players[sb].chips);
  m_players[sb].chips -= sbAmt;
  m_players[sb].currentBet = sbAmt;
  m_pot += sbAmt;

  int bbAmt = std::min(m_bigBlind, m_players[bb].chips);
  m_players[bb].chips -= bbAmt;
  m_players[bb].currentBet = bbAmt;
  m_pot += bbAmt;

  m_currentBet = m_bigBlind;
  m_currentPlayer = nextActive(bb);
}

void PokerWindow::dealHoleCards() {
  for (auto &p : m_players) {
    if (!p.folded) {
      p.hole[0] = drawCard();
      p.hole[1] = drawCard();
    }
  }
}

void PokerWindow::dealCommunity(int count) {
  for (int i = 0; i < count; i++)
    m_community.push_back(drawCard());
}

void PokerWindow::nextPhase() {
  // Reset bets for new round
  for (auto &p : m_players) p.currentBet = 0;
  m_currentBet = 0;
  m_numActed = 0;

  if (m_phase == PreFlop) { m_phase = Flop; dealCommunity(3); }
  else if (m_phase == Flop) { m_phase = Turn; dealCommunity(1); }
  else if (m_phase == Turn) { m_phase = River; dealCommunity(1); }
  else if (m_phase == River) { showdown(); return; }

  update(); // repaint to show new community cards
  startBettingRound();
}

// --- Betting ---

void PokerWindow::startBettingRound() {
  if (m_phase != PreFlop) {
    // Start from first active player after dealer
    int i = (m_dealerIdx + 1) % m_numPlayers;
    int attempts = 0;
    while ((m_players[i].folded || m_players[i].allIn || m_players[i].chips <= 0) && attempts < m_numPlayers) {
      i = (i + 1) % m_numPlayers;
      attempts++;
    }
    if (attempts >= m_numPlayers) { nextPhase(); return; } // everyone all-in or folded
    m_currentPlayer = i;
  }
  advanceToNextPlayer();
}

void PokerWindow::advanceToNextPlayer() {
  // Check if only one player remains
  int active = 0, lastActive = 0;
  for (int i = 0; i < m_numPlayers; i++)
    if (!m_players[i].folded && m_players[i].chips >= 0) { active++; lastActive = i; }
  if (active <= 1) { awardPot(); return; }

  if (bettingComplete()) { nextPhase(); update(); return; }

  // Skip folded/all-in players
  int attempts = 0;
  while ((m_players[m_currentPlayer].folded || m_players[m_currentPlayer].allIn) && attempts < m_numPlayers) {
    m_currentPlayer = (m_currentPlayer + 1) % m_numPlayers;
    attempts++;
  }
  if (attempts >= m_numPlayers) { nextPhase(); return; }

  updateButtons();
  update();

  if (!m_players[m_currentPlayer].isHuman) {
    m_aiTimer.start(800);
  }
}

bool PokerWindow::bettingComplete() {
  // Count active players who can still act
  int activePlayers = 0;
  for (int i = 0; i < m_numPlayers; i++) {
    if (m_players[i].folded || m_players[i].allIn || m_players[i].chips <= 0) continue;
    activePlayers++;
    if (m_players[i].currentBet < m_currentBet) return false;
  }
  // Everyone must have acted at least once
  return m_numActed >= activePlayers;
}

void PokerWindow::playerAction(int playerIdx, int action, int amount) {
  Player &p = m_players[playerIdx];
  m_numActed++;
  if (action == 0) { // fold
    p.folded = true;
  } else if (action == 1) { // call
    int toCall = m_currentBet - p.currentBet;
    toCall = std::min(toCall, p.chips);
    p.chips -= toCall;
    p.currentBet += toCall;
    m_pot += toCall;
    if (p.chips <= 0) p.allIn = true;
  } else if (action == 2) { // raise
    int toCall = m_currentBet - p.currentBet;
    int raiseAmt = std::max(amount, m_bigBlind);
    int total = toCall + raiseAmt;
    total = std::min(total, p.chips);
    p.chips -= total;
    p.currentBet += total;
    m_pot += total;
    m_currentBet = p.currentBet;
    if (p.chips <= 0) p.allIn = true;
  }

  m_currentPlayer = (m_currentPlayer + 1) % m_numPlayers;
  advanceToNextPlayer();
}

void PokerWindow::aiAction() {
  int decision = aiDecide(m_currentPlayer);
  if (decision == 0) playerAction(m_currentPlayer, 0);
  else if (decision == 1) playerAction(m_currentPlayer, 1);
  else playerAction(m_currentPlayer, 2, 25 * (1 + m_rng() % 4)); // $25-$100 in $25 increments
}

int PokerWindow::aiDecide(int playerIdx) {
  double strength = handStrength(playerIdx);
  int toCall = m_currentBet - m_players[playerIdx].currentBet;
  double potOdds = (toCall > 0) ? (double)toCall / (m_pot + toCall) : 0;

  // Pre-flop: stay in more often to see the flop
  if (m_phase == PreFlop) {
    if (strength > 0.7) return 2; // raise
    if (strength > 0.3) return 1; // call
    if (toCall <= m_bigBlind) return 1; // call the blind
    if ((m_rng() % 100) < 30) return 1; // call 30% of the time anyway
    return 0;
  }

  // Post-flop strategy
  if (strength > 0.8) return 2; // raise with strong hand
  if (strength > 0.5) return 1; // call with decent hand
  if (strength > 0.3 && potOdds < 0.3) return 1; // call with ok hand + good odds
  if (toCall == 0) return 1; // check (free)
  // Bluff ~15% of the time
  if ((m_rng() % 100) < 15) return (m_rng() % 2 == 0) ? 2 : 1;
  return 0; // fold
}

double PokerWindow::handStrength(int playerIdx) {
  // Estimate hand strength via quick evaluation
  std::vector<Card> cards;
  cards.push_back(m_players[playerIdx].hole[0]);
  cards.push_back(m_players[playerIdx].hole[1]);
  for (auto &c : m_community) cards.push_back(c);

  if (cards.size() < 3) {
    // Pre-flop: use simple hand ranking
    int r1 = m_players[playerIdx].hole[0].rank;
    int r2 = m_players[playerIdx].hole[1].rank;
    if (r1 == 1) r1 = 14; if (r2 == 1) r2 = 14;
    bool suited = m_players[playerIdx].hole[0].suit == m_players[playerIdx].hole[1].suit;
    bool pair = (r1 == r2);
    int high = std::max(r1, r2);
    double s = 0.2;
    if (pair) s = 0.5 + high * 0.03;
    else {
      s = (high - 2) * 0.03 + (suited ? 0.05 : 0.0);
      if (std::abs(r1 - r2) == 1) s += 0.05; // connected
    }
    if (high >= 13) s += 0.1;
    return std::min(s, 1.0);
  }

  HandRank rank = evaluateHand(cards);
  // Map rank to strength 0-1
  return std::min(0.3 + rank * 0.08, 1.0);
}

// --- Showdown and pot ---

void PokerWindow::showdown() {
  m_phase = Showdown;
  awardPot();
}

void PokerWindow::awardPot() {
  // Find best hand among non-folded
  int bestScore = -1, winner = -1;
  for (int i = 0; i < m_numPlayers; i++) {
    if (m_players[i].folded) continue;
    int score = bestHandScore(i);
    if (score > bestScore) { bestScore = score; winner = i; }
  }
  if (winner >= 0) m_players[winner].chips += m_pot;
  m_pot = 0;
  m_roundActive = false;
  m_phase = WaitingToDeal;
  m_dealerIdx = (m_dealerIdx + 1) % m_numPlayers;
  removeEliminatedPlayers();
  updateButtons();
  update();
}

void PokerWindow::removeEliminatedPlayers() {
  // Just mark them - don't resize vector
  for (auto &p : m_players)
    if (p.chips <= 0 && !p.isHuman) p.folded = true;
}

// --- Hand Evaluation ---

PokerWindow::HandRank PokerWindow::evaluateHand(const std::vector<Card> &cards) {
  if (cards.size() < 5) {
    // Not enough cards for full eval, estimate
    std::array<int, 15> ranks{};
    for (auto &c : cards) { int r = c.rank == 1 ? 14 : c.rank; ranks[r]++; }
    for (int i = 2; i <= 14; i++) { if (ranks[i] >= 3) return ThreeOfAKind; }
    int pairs = 0;
    for (int i = 2; i <= 14; i++) { if (ranks[i] >= 2) pairs++; }
    if (pairs >= 2) return TwoPair;
    if (pairs == 1) return OnePair;
    return HighCard;
  }

  // Evaluate best 5-card hand from available cards
  // For simplicity, check properties of all cards
  std::array<int, 15> ranks{};
  std::array<int, 4> suits{};
  for (auto &c : cards) {
    int r = c.rank == 1 ? 14 : c.rank;
    ranks[r]++;
    suits[c.suit]++;
  }

  bool flush = false;
  int flushSuit = -1;
  for (int s = 0; s < 4; s++) if (suits[s] >= 5) { flush = true; flushSuit = s; }

  // Check straight
  bool straight = false;
  int straightHigh = 0;
  // Include ace-low: A2345
  ranks[1] = ranks[14]; // copy ace to low position too
  for (int i = 14; i >= 5; i--) {
    if (ranks[i] && ranks[i-1] && ranks[i-2] && ranks[i-3] && ranks[i-4]) {
      straight = true; straightHigh = i; break;
    }
  }

  int four = 0, three = 0, pairs = 0;
  for (int i = 2; i <= 14; i++) {
    if (ranks[i] == 4) four++;
    else if (ranks[i] == 3) three++;
    else if (ranks[i] == 2) pairs++;
  }

  if (flush && straight) {
    // Check if straight flush
    std::vector<int> flushRanks;
    for (auto &c : cards) if (c.suit == flushSuit) flushRanks.push_back(c.rank == 1 ? 14 : c.rank);
    std::sort(flushRanks.begin(), flushRanks.end());
    for (int i = (int)flushRanks.size() - 1; i >= 4; i--) {
      if (flushRanks[i] - flushRanks[i-4] == 4) {
        if (flushRanks[i] == 14) return RoyalFlush;
        return StraightFlush;
      }
    }
  }
  if (four) return FourOfAKind;
  if (three && pairs) return FullHouse;
  if (three && cards.size() >= 6) { /* check for second three */ }
  if (flush) return Flush;
  if (straight) return Straight;
  if (three) return ThreeOfAKind;
  if (pairs >= 2) return TwoPair;
  if (pairs == 1) return OnePair;
  return HighCard;
}

int PokerWindow::handScore(const std::vector<Card> &cards) {
  HandRank rank = evaluateHand(cards);
  int high = 0;
  for (auto &c : cards) { int r = c.rank == 1 ? 14 : c.rank; high = std::max(high, r); }
  return rank * 100 + high;
}

int PokerWindow::bestHandScore(int playerIdx) {
  std::vector<Card> all;
  all.push_back(m_players[playerIdx].hole[0]);
  all.push_back(m_players[playerIdx].hole[1]);
  for (auto &c : m_community) all.push_back(c);
  return handScore(all);
}

QString PokerWindow::handRankName(HandRank rank) {
  switch (rank) {
  case RoyalFlush: return "Royal Flush";
  case StraightFlush: return "Straight Flush";
  case FourOfAKind: return "Four of a Kind";
  case FullHouse: return "Full House";
  case Flush: return "Flush";
  case Straight: return "Straight";
  case ThreeOfAKind: return "Three of a Kind";
  case TwoPair: return "Two Pair";
  case OnePair: return "One Pair";
  default: return "High Card";
  }
}

// --- UI Slots ---

void PokerWindow::onFold() { if (m_currentPlayer == 0) playerAction(0, 0); }
void PokerWindow::onCall() { if (m_currentPlayer == 0) playerAction(0, 1); }
void PokerWindow::onRaise() { if (m_currentPlayer == 0) playerAction(0, 2, m_raiseAmount); }
void PokerWindow::onDeal() { startRound(); update(); }

void PokerWindow::updateButtons() {
  bool humanTurn = m_roundActive && m_currentPlayer == 0 && !m_players[0].folded;
  bool canRaise = humanTurn && m_players[0].chips > (m_currentBet - m_players[0].currentBet);
  m_foldBtn->setVisible(humanTurn);
  m_callBtn->setVisible(humanTurn);
  m_raiseBtn->setVisible(canRaise);
  m_raiseDownBtn->setVisible(canRaise);
  m_raiseUpBtn->setVisible(canRaise);
  m_dealBtn->setVisible(!m_roundActive && m_players[0].chips > 0);

  if (humanTurn) {
    int toCall = m_currentBet - m_players[0].currentBet;
    m_callBtn->setText(toCall > 0 ? QString("Call $%1").arg(toCall) : "Check");
    int maxRaise = m_players[0].chips - toCall;
    if (m_raiseAmount > maxRaise) m_raiseAmount = std::max(25, maxRaise);
    m_raiseBtn->setText(QString("Raise $%1").arg(m_raiseAmount));
  }
}

QString PokerWindow::rankStr(int rank) {
  if (rank == 1) return "A";
  if (rank == 11) return "J";
  if (rank == 12) return "Q";
  if (rank == 13) return "K";
  return QString::number(rank);
}

void PokerWindow::drawCardAt(QPainter &p, const Card &c, int x, int y, bool faceDown) {
  QRect cr(x, y, kCardW, kCardH);
  if (!faceDown && !m_cardSheet.isNull()) {
    int col = c.rank - 1;
    int row = suitToRow[c.suit];
    QRect src(col * kSheetCardW, row * kSheetCardH, kSheetCardW, kSheetCardH);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(cr, m_cardSheet, src);
  } else {
    p.setPen(QPen(Qt::black, 1));
    p.setBrush(QColor(50, 50, 200));
    p.drawRoundedRect(cr, 4, 4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 180));
    p.drawRoundedRect(cr.adjusted(3, 3, -3, -3), 3, 3);
  }
}

// --- Paint ---

void PokerWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), QColor(0, 80, 0));

  QFont labelFont;
  labelFont.setPixelSize(12);
  labelFont.setBold(true);

  // Community cards (center)
  int commX = (kWinW - (int)m_community.size() * (kCardW + 5)) / 2;
  for (int i = 0; i < (int)m_community.size(); i++)
    drawCardAt(p, m_community[i], commX + i * (kCardW + 5), 220, false);

  // Pot
  p.setFont(labelFont);
  p.setPen(Qt::yellow);
  p.drawText(QRect(0, 305, kWinW, 20), Qt::AlignCenter, QString("Pot: $%1").arg(m_pot));

  // Players arranged around table
  // Player 0 (human) at bottom center
  // Players 1+ evenly distributed across the top
  int humanX = kWinW / 2 - 60;
  int humanY = 420;

  for (int i = 0; i < m_numPlayers; i++) {
    auto &pl = m_players[i];
    int px, py;
    if (i == 0) {
      px = humanX; py = humanY;
    } else {
      // Distribute opponents evenly across top
      int numOpp = m_numPlayers - 1;
      int spacing = (kWinW - 140) / numOpp;
      px = 40 + (i - 1) * spacing;
      py = 40;
    }

    // Name and chips
    p.setFont(labelFont);
    p.setPen(pl.folded ? QColor(100, 100, 100) : Qt::white);
    p.drawText(px, py, QString("%1 ($%2)").arg(pl.name).arg(pl.chips));

    // Dealer button
    if (i == m_dealerIdx) {
      p.setPen(Qt::NoPen);
      p.setBrush(Qt::white);
      p.drawEllipse(px - 15, py - 10, 12, 12);
      p.setPen(Qt::black);
      QFont df; df.setPixelSize(8); df.setBold(true);
      p.setFont(df);
      p.drawText(QRect(px - 15, py - 10, 12, 12), Qt::AlignCenter, "D");
    }

    if (pl.folded && !m_roundActive) continue;
    if (pl.folded) {
      p.setFont(labelFont);
      p.setPen(QColor(150, 150, 150));
      p.drawText(px, py + 15, "Folded");
      continue;
    }

    // Cards
    bool show = pl.isHuman || m_phase == Showdown || m_phase == WaitingToDeal;
    if (m_roundActive || m_phase == Showdown || m_phase == WaitingToDeal) {
      drawCardAt(p, pl.hole[0], px, py + 18, !show);
      drawCardAt(p, pl.hole[1], px + kCardW + 4, py + 18, !show);
    }

    // Current bet
    if (pl.currentBet > 0 && m_roundActive) {
      p.setFont(labelFont);
      p.setPen(QColor(200, 200, 0));
      p.drawText(px, py + kCardH + 22, QString("Bet: $%1").arg(pl.currentBet));
    }

    // Active indicator
    if (i == m_currentPlayer && m_roundActive) {
      p.setPen(QPen(Qt::yellow, 2));
      p.setBrush(Qt::NoBrush);
      p.drawRect(px - 3, py - 3, kCardW * 2 + 12, kCardH + 30);
    }
  }

  // Show winner info at showdown - to the right of player's hand
  if (m_phase == WaitingToDeal && !m_roundActive) {
    int bestScore2 = -1, winner = -1;
    for (int i = 0; i < m_numPlayers; i++) {
      if (m_players[i].folded) continue;
      int score = bestHandScore(i);
      if (score > bestScore2) { bestScore2 = score; winner = i; }
    }
    if (winner >= 0) {
      std::vector<Card> all;
      all.push_back(m_players[winner].hole[0]);
      all.push_back(m_players[winner].hole[1]);
      for (auto &c : m_community) all.push_back(c);
      HandRank hr = evaluateHand(all);
      p.setFont(labelFont);
      p.setPen(QColor(255, 255, 0));
      int msgX = kWinW / 2 - 60 + kCardW * 2 + 20;
      p.drawText(msgX, 440, QString("%1 win%2!").arg(m_players[winner].name, winner == 0 ? "" : "s"));
      p.drawText(msgX, 455, handRankName(hr));
    }
  }

  // Human player's hand info - to the right of cards
  if (m_roundActive && !m_players[0].folded && m_community.size() >= 3) {
    std::vector<Card> all;
    all.push_back(m_players[0].hole[0]);
    all.push_back(m_players[0].hole[1]);
    for (auto &c : m_community) all.push_back(c);
    HandRank hr = evaluateHand(all);
    p.setFont(labelFont);
    p.setPen(Qt::white);
    int msgX = kWinW / 2 - 60 + kCardW * 2 + 20;
    p.drawText(msgX, 455, handRankName(hr));
  }

  // Chip display (bottom right) for human player
  {
    int remaining = m_players[0].chips;
    struct ChipInfo { int value; QColor color; int count; };
    ChipInfo chips[] = {{100, QColor(40,40,40), 0}, {50, QColor(0,120,0), 0}, {25, QColor(180,0,0), 0}};
    for (auto &ch : chips) { ch.count = remaining / ch.value; remaining -= ch.count * ch.value; }

    int chipR = 12;
    int stackSpacing = chipR * 2 + 8;
    int baseY = kWinH - 70;
    int startX = kWinW - 180;
    int stackIdx = 0;

    for (auto &ch : chips) {
      int left = ch.count;
      while (left > 0) {
        int inStack = std::min(left, 10);
        int sx = startX + stackIdx * stackSpacing;
        for (int i = 0; i < inStack; i++) {
          int cy = baseY - i * 3;
          p.setPen(QPen(Qt::white, 1));
          p.setBrush(ch.color);
          p.drawEllipse(QPoint(sx, cy), chipR, chipR / 2);
        }
        QFont cf; cf.setPixelSize(8); cf.setBold(true);
        p.setFont(cf);
        p.setPen(Qt::white);
        p.drawText(QRect(sx - chipR, baseY - (inStack-1)*3 - 6, chipR*2, 10), Qt::AlignCenter,
                   QString("$%1").arg(ch.value));
        left -= inStack;
        stackIdx++;
      }
    }
    p.setFont(labelFont);
    p.setPen(Qt::white);
    p.drawText(startX - 10, kWinH - 45, QString("$%1").arg(m_players[0].chips));
  }

  if (!m_roundActive && m_players[0].chips <= 0) {
    QFont gf; gf.setPixelSize(24); gf.setBold(true);
    p.setFont(gf);
    p.setPen(Qt::red);
    p.drawText(QRect(0, 350, kWinW, 40), Qt::AlignCenter, "You're bust! Out of chips.");
  }
}
