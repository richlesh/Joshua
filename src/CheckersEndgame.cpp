// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "CheckersEndgame.h"
#include "chinook_db.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <cstdlib>

static constexpr int8_t B_EMPTY = 0;
static constexpr int8_t B_DARK = 1;
static constexpr int8_t B_DARKKING = 2;
static constexpr int8_t B_LIGHT = -1;
static constexpr int8_t B_LIGHTKING = -2;

static bool eg_isDark(int8_t p) { return p > 0; }

// Map 64-square index to Chinook's internal 32-square index
// Chinook's board layout (used by Locbv[] arrays):
//   Row 0: 0, 8, 16, 24   Row 1: 4, 12, 20, 28
//   Row 2: 1, 9, 17, 25   Row 3: 5, 13, 21, 29
//   Row 4: 2, 10, 18, 26  Row 5: 6, 14, 22, 30
//   Row 6: 3, 11, 19, 27  Row 7: 7, 15, 23, 31
static int toDarkSq(int idx) {
  int r = idx / 8, c = idx % 8;
  if ((r + c) % 2 != 1) return -1;
  // Position within the row (0-3)
  int p = (r % 2 == 0) ? (c - 1) / 2 : c / 2;
  return p * 8 + (r / 2) + ((r % 2) ? 4 : 0);
}

CheckersEndgame &CheckersEndgame::instance() {
  static CheckersEndgame inst;
  return inst;
}

int CheckersEndgame::pieceCount(const std::array<int8_t, 64> &board) const {
  int count = 0;
  for (int i = 0; i < 64; i++)
    if (board[i] != B_EMPTY) count++;
  return count;
}

std::string CheckersEndgame::chinookDir() const {
  QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (cacheDir.isEmpty()) cacheDir = QDir::homePath() + "/.cache/Joshua";
  return (cacheDir + "/chinook-db").toStdString();
}

bool CheckersEndgame::chinookCached() const {
  std::string dir = chinookDir();
  struct stat st;
  return (stat((dir + "/DB6").c_str(), &st) == 0 &&
          stat((dir + "/DB6.idx").c_str(), &st) == 0);
}

bool CheckersEndgame::downloadChinookDB() {
  std::string dir = chinookDir();
  QDir().mkpath(QString::fromStdString(dir));

  std::string zipPath = dir + "/DB6.zip";
  std::string dbPath = dir + "/DB6";
  std::string idxPath = dir + "/DB6.idx";

  struct stat st;
  if (stat(dbPath.c_str(), &st) == 0 && stat(idxPath.c_str(), &st) == 0)
    return true;

  // Try bundled DB6.zip from app resources directory first
  QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MAC
  QString bundledZip = appDir + "/../Resources/DB6.zip";
#else
  QString bundledZip = appDir + "/DB6.zip";
#endif
  if (!QFile::exists(bundledZip)) {
    bundledZip = "/usr/share/joshua/DB6.zip";
  }
  if (QFile::exists(bundledZip)) {
    QFile::copy(bundledZip, QString::fromStdString(zipPath));
    if (stat(zipPath.c_str(), &st) == 0 && st.st_size > 20000000)
      return true;
  }

  // Fall back to curl download
  std::string url = "https://webdocs.cs.ualberta.ca/~chinook/DataBases/DB6.zip";
  std::string cmd = "curl -sL -o \"" + zipPath + "\" \"" + url + "\"";
  int ret = system(cmd.c_str());
  if (ret != 0) return false;

  if (stat(zipPath.c_str(), &st) != 0 || st.st_size < 20000000)
    return false;

  return true;
}

bool CheckersEndgame::extractChinookDB() {
  std::string dir = chinookDir();
  std::string zipPath = dir + "/DB6.zip";
  std::string dbPath = dir + "/DB6";
  std::string idxPath = dir + "/DB6.idx";

  struct stat st;
  if (stat(dbPath.c_str(), &st) == 0 && stat(idxPath.c_str(), &st) == 0)
    return true;

  std::string cmd = "unzip -o -q \"" + zipPath + "\" -d \"" + dir + "\"";
  int ret = system(cmd.c_str());
  if (ret != 0) return false;

  unlink(zipPath.c_str());
  return stat(dbPath.c_str(), &st) == 0 && stat(idxPath.c_str(), &st) == 0;
}

void CheckersEndgame::loadChinook() {
  if (m_readyChinook.load()) return;

  std::string dir = chinookDir();
  std::string dbPath = dir + "/DB6";
  std::string idxPath = dir + "/DB6.idx";
  struct stat st;

  bool needsDownload = (stat(dbPath.c_str(), &st) != 0 || stat(idxPath.c_str(), &st) != 0);

  if (needsDownload) {
    if (!downloadChinookDB()) { qWarning("Chinook DB: download failed"); return; }
    if (!extractChinookDB()) { qWarning("Chinook DB: extract failed"); return; }
  }

  if (chinook_init(dir.c_str())) {
    m_readyChinook.store(true);
    qDebug("Chinook 6-piece endgame database loaded successfully");
  } else {
    qWarning("Chinook DB: init failed");
  }
}

static bool onBoard(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }

static bool hasCapture(const std::array<int8_t, 64> &board, bool forDark) {
  for (int i = 0; i < 64; i++) {
    int8_t p = board[i];
    if (p == B_EMPTY) continue;
    if (forDark && p < 0) continue;
    if (!forDark && p > 0) continue;
    int r = i / 8, c = i % 8;
    int dirs[][2] = {{1,-1},{1,1},{-1,-1},{-1,1}};
    for (auto &d : dirs) {
      if (p == B_DARK && d[0] != 1) continue;
      if (p == B_LIGHT && d[0] != -1) continue;
      int mr = r+d[0], mc = c+d[1], lr = r+2*d[0], lc = c+2*d[1];
      if (!onBoard(lr, lc)) continue;
      int8_t mid = board[mr*8+mc];
      if (mid == B_EMPTY || board[lr*8+lc] != B_EMPTY) continue;
      if ((forDark && mid < 0) || (!forDark && mid > 0)) return true;
    }
  }
  return false;
}

CheckersEndgame::Result CheckersEndgame::probe(const std::array<int8_t, 64> &board, bool darkToMove) const {
  if (!m_readyChinook.load()) return UNKNOWN;

  int pc = pieceCount(board);
  if (pc < 2 || pc > 6) return UNKNOWN;

  // Chinook DB only returns correct results when neither side has a capture.
  if (hasCapture(board, darkToMove)) return UNKNOWN;
  if (hasCapture(board, !darkToMove)) return UNKNOWN;

  // Convert our board to Chinook's Locbv format
  // Our Dark = Chinook's WHITE (bottom of board, rows 0-2 start)
  // Our Light = Chinook's BLACK (top of board, rows 5-7 start)
  unsigned int whiteBv = 0, blackBv = 0, kingBv = 0;

  for (int i = 0; i < 64; i++) {
    int8_t p = board[i];
    if (p == B_EMPTY) continue;
    int dsq = toDarkSq(i);
    if (dsq < 0) continue;
    unsigned int mask = 1U << dsq;
    if (eg_isDark(p)) {
      whiteBv |= mask;
      if (p == B_DARKKING) kingBv |= mask;
    } else {
      blackBv |= mask;
      if (p == B_LIGHTKING) kingBv |= mask;
    }
  }

  // Chinook: WHITE=0, BLACK=1
  // Our darkToMove = Chinook white to move = turn 0
  int turn = darkToMove ? 0 : 1;

  int result = chinook_lookup(whiteBv, blackBv, kingBv, turn);

  // Result is from side-to-move perspective
  if (result == CHINOOK_WIN) return WIN;
  if (result == CHINOOK_LOSS) return LOSS;
  if (result == CHINOOK_TIE) return DRAW;
  return UNKNOWN;
}
