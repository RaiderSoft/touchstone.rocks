# touchstone.rocks

9x9 Go game with KataGo AI opponent, camera-based board vision, and LLM chat overlay.

## Dependencies

- CMake 3.14+
- C++17 compiler
- libcurl (system-installed on macOS)
- OpenCV (core, imgproc, videoio)
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
| `KATAGO_CONFIG` | Path to analysis config file | (KataGo defaults) |
| `KATAGO_ANALYSIS_VISITS` | Default analysis visits | `200` |
| `CLAUDE_API_KEY` | Anthropic API key for chat overlay | (chat disabled) |
| `GEMINI_API_KEY` | Google Gemini API key for chat overlay | (chat disabled) |

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

## Game Controls

| Key | Action |
|-----|--------|
| Click | Place stone |
| `P` | Pass |
| `O` | Toggle ownership overlay (KataGo) |
| `A` | Toggle top move suggestions (KataGo) |
| `K` | Toggle KataGo statistics panel |
| `ESC` | Pause / quit |
| `ENTER` | Open chat overlay |
| `TAB` | Switch LLM model (in chat) |

## Vision Mode

If a calibrated camera is detected, the app enters vision mode where stones are placed on a physical board and detected via camera. Run `./build/touchstone vision` for the vision calibration/dev tool.

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
