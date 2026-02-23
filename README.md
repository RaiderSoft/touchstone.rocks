# touchstone.rocks

Play Go against a computer on a real board. A webcam watches the board, detects your stones, and KataGo plays back. Also works as a regular desktop app.

## Features

### Camera Vision

A webcam pointed at a physical Go board detects the full board state in real time.

- **Stone detection** using OpenCV: identifies black, white, and empty intersections from a live camera feed
- **Interactive calibration**: set 4 corner points of the board, compute perspective transform, persist calibration data across sessions
- **Temporal smoothing** and confidence scoring to filter noise (5-frame confirmation threshold before registering a move)
- **Mismatch highlighting** when the physical board diverges from expected state
- **Setup verification** for placing puzzle positions on a real board

### Board & Game Play

- **9x9, 13x13, and 19x19 boards** with full rule enforcement (captures, ko, suicide prevention)
- **Play vs Computer** at 5 difficulty levels using KataGo's human-like play profiles (20k Novice through 1k Advanced)
- **Play from Position** setup mode: place black/white stones or erase to create arbitrary board positions, then play from there
- **Undo/Redo** full move history navigation
- **Chinese scoring** (Tromp-Taylor) with automatic territory detection, configurable komi

### AI Analysis

- **KataGo integration** via GTP with async position analysis
- **Win rate and score lead** tracked per move across the entire game
- **Move quality assessment** for every move (blunder / inaccuracy / good / great) shown as color-coded rings
- **Top move suggestions** overlay showing KataGo's top 3 recommended moves
- **Territory ownership heatmap** visualizing who controls which regions
- **Statistics panel** with detailed analysis: win rates, score lead, move quality history

### Puzzles (under construction)

- **Four puzzle categories**: Capture, Defend, Life & Death, Tesuji
- **Deck of predefined positions** with hints, multiple correct solutions, and explanations
- **Immediate feedback** on correct/incorrect moves
- **Progress tracking** per puzzle (solved / attempted / untried)
- **Vision mode support**: place puzzle positions on a physical board and solve them there

The puzzle infrastructure is in place but the puzzle content itself needs work.

### Voice Interaction

- **Speech-to-text** via microphone recording, WAV encoding, and Gemini API transcription
- **Text-to-speech** for AI responses with async PCM audio playback
- **Visual indicators** showing recording and transcription state

### LLM Chat Overlay

- **Claude and Gemini** support with Tab to switch models
- **Game coach mode**: system prompt provides Go theory and strategic advice grounded in the current board state
- **Opponent persona mode**: AI analyzes the position as your sitting opponent
- **Scrollable message history** with fade animation when chat is closed
- **Voice input** for hands-free conversation during play

### Save / Load

- **Named saves** with auto-captured metadata (board size, move count, timestamp)
- **Full state persistence**: move history, per-move analysis records (win rates, score leads, top suggestions, quality), and UI state
- **Browse, load, and delete** saved games from the pause menu

### Display & Controls

| Key | Action |
|-----|--------|
| Click | Place stone |
| `P` | Pass |
| `Backspace` | Undo |
| `R` | Redo |
| `O` | Toggle territory ownership overlay |
| `A` | Toggle top move suggestions |
| `K` | Toggle KataGo statistics panel |
| `T` | Toggle atari indicator (stones with 1 liberty) |
| `G` | Toggle liberty count display |
| `H` | Toggle help screen |
| `Enter` | Open chat overlay |
| `Tab` | Switch LLM model (in chat) |
| `ESC` | Pause / quit |

### Pre-Game Configuration

- **Color selection**: play as Black (first) or White
- **Board size selection**: 9x9, 13x13, or 19x19
- **Difficulty presets**: 5 levels with descriptive labels and rank indicators

## Dependencies

- CMake 3.14+
- C++17 compiler
- libcurl (system-installed on macOS)
- Raylib (graphics, input, audio)
- OpenCV (core, imgproc, videoio) for vision mode
- KataGo (required for game play)

### Installing KataGo (macOS)

```bash
brew install katago
```

This installs the binary at `/opt/homebrew/bin/katago`. You also need a neural network model:

```bash
# Download a model (smaller = faster, larger = stronger)
# 9x9 is fine with a small model
katago genconfig  # interactive setup, or manually download:

# Example: download the default model
brew info katago  # shows where models are stored
```

Models can be downloaded from https://katagotraining.org/networks/.

### KataGo models used

These are not checked into git (`.gitignore`d). Download them and place in the project root or configure paths via `.env`.

| File | Description | Source |
|------|-------------|--------|
| `b18c384nbt-humanv0.bin.gz` | 18-block human-like play model. Used for humanSL difficulty profiles (20k through 1k). | [katagotraining.org](https://katagotraining.org/networks/) |
| `kata1-b6c96-s152505856-d23152636.txt.gz` | 6-block model config/weights. Small and fast, suitable for 9x9 and 13x13. | [katagotraining.org](https://katagotraining.org/networks/) |

## Configuration

Copy `.env.example` to `.env` and fill in the required values:

```bash
cp .env.example .env
```

### Required environment variables

| Variable | Description | Example |
|----------|-------------|---------|
| `KATAGO_MODEL` | Path to KataGo neural network model file (.bin.gz) | `~/.katago/kata1-b18c384.bin.gz` |

### Optional environment variables

| Variable | Description | Default |
|----------|-------------|---------|
| `KATAGO_PATH` | Path to katago binary | `/opt/homebrew/bin/katago` |
| `KATAGO_HUMAN_MODEL` | Path to human-like play model for difficulty profiles | (uses KATAGO_MODEL) |
| `KATAGO_CONFIG` | Path to analysis config file | (KataGo defaults) |
| `KATAGO_ANALYSIS_VISITS` | Default analysis visits | `200` |
| `CLAUDE_API_KEY` | Anthropic API key for chat overlay | (chat disabled) |
| `GEMINI_API_KEY` | Google Gemini API key for chat, STT, and TTS | (chat/voice disabled) |

### Example .env

```
KATAGO_MODEL=/opt/homebrew/share/katago/g170-b6c96.bin.gz
KATAGO_PATH=/opt/homebrew/bin/katago
CLAUDE_API_KEY=sk-ant-...
GEMINI_API_KEY=AIza...
```

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

For vision (camera) support, OpenCV must be installed (`brew install opencv`).

## Running

```bash
./build/touchstone
```

The app will fail to start if `KATAGO_MODEL` is not set or KataGo cannot be launched.

For the vision calibration/dev tool:

```bash
./build/touchstone vision
```

## Tests

```bash
cd build
ctest
```

### Tromp-Taylor scoring tests

The `tromp_tests` target validates our internal scoring against John Tromp's reference Tromp-Taylor implementation. It requires GHC (Haskell compiler) to build the reference scorer:

```bash
brew install ghc               # if not already installed
ghc -o tools/tromp_score tools/tromp_score.hs
```

The test binary looks for `tools/tromp_score` relative to the build directory. If not found, the tests skip gracefully.
