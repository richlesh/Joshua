// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef CHECKERSENDGAME_H
#define CHECKERSENDGAME_H

#include <array>
#include <cstdint>
#include <atomic>
#include <string>

// Endgame database for checkers using the Chinook 6-piece database.
// Uses the original Chinook C code for position lookup.

class CheckersEndgame {
public:
  enum Result : int8_t { UNKNOWN = 0, WIN = 1, LOSS = 2, DRAW = 3 };

  static CheckersEndgame &instance();

  Result probe(const std::array<int8_t, 64> &board, bool darkToMove) const;
  bool isChinookReady() const { return m_readyChinook.load(); }
  bool chinookCached() const;
  void loadChinook();

private:
  CheckersEndgame() = default;

  int pieceCount(const std::array<int8_t, 64> &board) const;
  std::string chinookDir() const;
  bool downloadChinookDB();
  bool extractChinookDB();

  std::atomic<bool> m_readyChinook{false};
};

#endif // CHECKERSENDGAME_H
