# Joshua v1.8.0

A WarGames-inspired AI terminal game application.

*by Richard Lesh*

---

## Features

- Classic green-on-black terminal interface (WarGames WOPR style)
- Tic-Tac-Toe with unbeatable minimax AI
- Checkers with minimax + alpha-beta pruning AI
  - Adjustable difficulty (Really Easy, Easy, Medium, Hard)
  - Drag-and-drop piece movement
  - Animated computer moves (including multi-jump sequences)
  - Computer suggestions highlighting best move
  - Computer-vs-computer spectator mode with configurable levels
  - Draw detection (no-progress and position repetition)
- Four Across with minimax + alpha-beta pruning AI
  - Adjustable difficulty (Easy, Medium, Hard)
  - Hover-to-select column with animated piece drop
  - Computer suggestions highlighting best column
  - Computer-vs-computer spectator mode with configurable levels
- Reversi with minimax + alpha-beta pruning AI
  - Adjustable difficulty (Really Easy, Easy, Medium, Hard, Expert)
  - Click-to-place with legal move indicators
  - Animated disc flipping
  - Computer suggestions highlighting best move
  - Computer-vs-computer spectator mode with configurable levels
- Chess powered by bundled Stockfish engine
  - Adjustable difficulty (Really Easy, Easy, Medium, Hard, Expert)
  - Drag-and-drop piece movement
  - Animated computer moves
  - Computer suggestions highlighting best move
  - Computer-vs-computer spectator mode with configurable levels
  - Full strength endgame play
  - Draw detection (50-move rule, threefold repetition, insufficient material)
- Go powered by bundled KataGo engine
  - Adjustable difficulty (Really Easy, Easy, Medium, Hard, Expert)
  - Board sizes: 9×9, 13×13, 19×19
  - Click-to-place stones with hover preview
  - Computer suggestions highlighting best move
  - Computer-vs-computer spectator mode with configurable levels
  - Automatic capture detection
  - Pass and territory scoring
- Blackjack with 6-deck shoe
  - Hit, Stand, Double Down
  - Adjustable bet ($25–$500 in $25 increments)
  - Dealer hits on soft 17
  - Blackjack pays 3:2
  - Animated dealer card draws
  - Balance tracking with auto-reset
- Texas Hold'em Poker with heuristic AI
  - 5 players (you + 4 AI opponents)
  - Pre-flop, flop, turn, river betting rounds
  - AI uses hand strength, pot odds, and bluffing
  - Full hand evaluation (High Card through Royal Flush)
  - Fold, Call/Check, Raise actions
  - Rotating dealer with blinds
- Global Thermonuclear War simulation
  - Choose side: United States, Soviet Union, or China
  - World map with Natural Earth coastline data
  - Select enemy cities to target
  - Animated missile trajectories with retaliation and counter-strike
  - Four rounds of futile attempts at different strategies
  - "The only winning move is not to play"
- Configurable terminal font size, font color, and background color
- Text-to-speech for all game prompts (macOS/Windows/Linux)
- Audio on/off setting
- JetBrains Mono NL font with Courier fallback
- Splash screen with donation link
- About dialog
- License key validation (HMAC-SHA256 based)
- Window size persistence
- Settings saved to `~/.joshua-settings.json`
- macOS, Windows, and Linux builds via GitHub Actions
- Code signing and notarization for macOS
- Azure Artifact Signing for Windows

---

## Installation

### Prerequisites
- [Qt 6](https://www.qt.io) (6.x)
- CMake 3.21+
- C++17 compiler

### Setup
```bash
git clone https://github.com/richlesh/Joshua.git
cd Joshua
cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build
```

### Running
```bash
./build/Joshua.app/Contents/MacOS/Joshua   # macOS
./build/Joshua                              # Linux
build\Release\Joshua.exe                    # Windows
```

---

## Building Distribution Packages

Distribution builds are handled via GitHub Actions workflows:

- `build-cpp-mac.yml` — macOS DMG (x64 + arm64), signed and notarized
- `build-cpp-windows.yml` — Windows NSIS installer (x64 + arm64), Azure signed
- `build-cpp-linux.yml` — Linux DEB + RPM (x64 + arm64)

Trigger manually with a release tag via `workflow_dispatch`.

---

## Project Structure

```
Joshua/
├── CMakeLists.txt           # Build configuration
├── Info.plist.in            # macOS bundle info
├── src/
│   ├── main.cpp             # Entry point
│   ├── MainWindow.cpp/h     # Terminal UI and game menu
│   ├── TicTacToeWindow.cpp/h # Tic-Tac-Toe game
│   ├── CheckersWindow.cpp/h  # Checkers game with AI
│   ├── FourAcrossWindow.cpp/h # Four Across game with AI
│   ├── ChessWindow.cpp/h     # Chess game with Stockfish AI
│   ├── ReversiWindow.cpp/h   # Reversi game with AI
│   ├── GoWindow.cpp/h        # Go game with KataGo AI
│   ├── BlackjackWindow.cpp/h # Blackjack card game
│   ├── PokerWindow.cpp/h    # Texas Hold'em poker
│   ├── Settings.cpp/h       # Settings persistence
│   ├── SettingsDialog.cpp/h  # Settings UI
│   ├── LicenseDialog.cpp/h   # License key entry
│   ├── LicenseValidator.cpp/h # License validation
│   ├── License.cpp/h         # License salt
│   ├── SplashScreen.cpp/h    # Splash screen
│   ├── AboutDialog.cpp/h     # About dialog
│   ├── BouncingProgressBar.cpp/h # Progress bar widget
│   └── Utilities.cpp/h       # Helper functions
├── ui/                       # Qt Designer UI files
├── resources/
│   ├── icons/               # App icons (png, icns, ico)
│   ├── fonts/               # JetBrains Mono NL TTF
│   ├── resources.qrc        # Qt resource file
│   ├── app_icon.rc          # Windows icon resource
│   ├── joshua.desktop       # Linux desktop entry
│   └── joshua-mime.xml      # MIME type definitions
├── .github/workflows/       # CI/CD workflows
└── generate_license_key.py  # License key generator
```

---

## Games

### Tic-Tac-Toe
Classic 3×3 grid. The AI uses full minimax search and is unbeatable.

### Checkers
American checkers (8×8 board) with:
- Minimax + alpha-beta pruning (depth 1–9)
- Mandatory jumps and multi-jump sequences
- King promotion (turn ends on promotion)
- Drag-and-drop interface
- Optional move suggestions (depth 9 analysis)
- Computer-vs-computer mode with independent difficulty per side

### Four Across
Classic 7×6 vertical drop game with:
- Minimax + alpha-beta pruning (depth 4–9)
- Hover-to-select column with animated piece drop
- Optional move suggestions (depth 9 analysis)
- Computer-vs-computer mode with independent difficulty per side

### Chess
Standard chess powered by the bundled [Stockfish](https://stockfishchess.org) engine (GPL 3.0) with:
- UCI protocol communication via QProcess
- Adjustable difficulty via UCI_Elo (1320–3190)
- Local legal move generation for instant move validation
- Drag-and-drop piece movement with legal move highlighting
- Animated computer moves
- Optional best-move suggestions (full-strength analysis)
- Computer-vs-computer spectator mode (levels 1–9)
- Full-strength endgame play (ELO limit removed when ≤10 pieces)
- Draw detection (50-move rule, threefold repetition, insufficient material)
- Unicode chess piece rendering

### Go
The game of Go powered by the bundled [KataGo](https://github.com/lightvector/KataGo) engine (MIT license) with:
- GTP protocol communication via QProcess
- Adjustable difficulty via maxVisits (1–2000)
- Board sizes: 9×9, 13×13, 19×19
- Click-to-place stones with hover preview
- Automatic capture detection (liberty counting)
- Optional best-move suggestions (full-strength analysis)
- Computer-vs-computer spectator mode (levels 1–9)
- Pass detection and territory scoring via engine
- Star points (hoshi) and last-move marker

---

## License Key

Generate a license key for a user:
```bash
python3 generate_license_key.py user@example.com
```

---

## Tech Stack

- [Qt 6](https://www.qt.io) — GUI framework
- C++17
- CMake — build system
- GitHub Actions — CI/CD

---

## License

GPL 3.0
