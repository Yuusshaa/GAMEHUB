# 🎮 Game Hub

A local two-player arcade game hub built with **C++17** and **SFML 2.x**, featuring three fully playable games, a persistent leaderboard, animated backgrounds, and background music — all launched from a single polished hub menu.

---

## Games

### ⚡ GRIDLOCK — Tron Light Cycles
A synthwave-styled Tron clone. Two players race across a grid leaving light trails behind them. The first player to crash into a wall or a trail loses the round.

### 🐦 Flappy Bird — PvP Edition
Both players control their own bird simultaneously, competing to survive the longest through an endless stream of pipes. The last bird flying wins.

### 🏓 Neon Pong — Pro Arcade Edition
Classic paddle-and-ball action with a neon aesthetic. First player to reach **7 points** wins the match.

---

## Controls

### Hub Navigation
| Key | Action |
|-----|--------|
| `←` / `→` | Navigate between game tiles |
| `Enter` / `Space` | Launch selected game |
| `L` or `X` | Open leaderboard for selected game |
| `M` | Mute / unmute background music |
| `Esc` | Exit hub |

### In-Game Controls
| Game | Player 1 | Player 2 | Pause |
|------|----------|----------|-------|
| GRIDLOCK | `W A S D` | `↑ ↓ ← →` | `Esc` |
| Flappy Bird | `Space` | `Enter` | `Esc` |
| Neon Pong | `W` / `S` | `↑` / `↓` | `Esc` |

---

## Features

- **Game Hub menu** with animated tile cards and a particle background system
- **Persistent leaderboard** — scores are saved to a binary file (`scores.dat`) and survive between sessions; stores up to 300 records across all three games
- **Per-game leaderboard view** — browse top 10 scores for each game; navigate with `←` / `→`
- **Best score badge** displayed on each game tile in the hub
- **Player name entry** on startup — names are used for leaderboard records throughout the session
- **Background music** — place `bgg.mac` next to the executable; music loops, pauses when a game launches, and fades back in on return
- **Custom background image** — drop any of the following files next to the executable and it will auto-load and fill the window: `hub_bg.png`, `hub_bg.jpg`, `background.png`, `background.jpg` (fallback: animated procedural background)
- **Mute toggle** (`M`) with smooth volume fade in/out

---

## Project Structure

```
GameHub/
├── main.cpp          # Hub menu, leaderboard manager, music system, name entry
├── tron.cpp          # GRIDLOCK — Tron light cycles game
├── flappy.cpp        # Flappy Bird PvP game
├── pong.cpp          # Neon Pong game
├── font.ttf          # Required font (or arial.ttf as fallback)
├── bgg.mac           # (Optional) Background music file
├── hub_bg.png        # (Optional) Hub background image
└── scores.dat        # Auto-generated leaderboard save file
```

---

## Building

### Requirements
- C++17 compiler (GCC, Clang, or MSVC)
- [SFML 2.x](https://www.sfml-dev.org/) (Graphics + Audio modules)

### Linux / macOS (g++)
```bash
g++ -std=c++17 -o GameHub main.cpp tron.cpp flappy.cpp pong.cpp \
    -lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system
./GameHub
```

### Windows (MinGW)
```bash
g++ -std=c++17 -o GameHub.exe main.cpp tron.cpp flappy.cpp pong.cpp \
    -lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system
GameHub.exe
```

### Windows (MSVC)
Add all four `.cpp` files to a Visual Studio project, link the SFML libraries, set the C++ standard to C++17, and build.

> **Note:** Make sure `font.ttf` (or `arial.ttf`) is in the same directory as the executable, or text will not render. SFML DLLs must also be present on Windows.

---

## Leaderboard

Scores are recorded automatically after every game session. The `LeaderboardManager` class handles everything:

- Scores of 0 are ignored
- Records are sorted by game, then by score (descending)
- Data is written to `scores.dat` in binary format after every new entry and on exit
- Up to 300 total records are stored across all three games
- Top 10 entries per game are shown in the leaderboard screen

---

## Window

| Setting | Value |
|---------|-------|
| Resolution | 1200 × 730 |
| Frame rate cap | 60 FPS |
| Anti-aliasing | 4× |

---

## License

This project was built for educational / personal use. Feel free to fork and build on it.
