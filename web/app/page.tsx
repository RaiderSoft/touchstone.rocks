import styles from "./page.module.css";

const FEATURES = [
  {
    icon: "◉",
    name: "Camera Vision",
    desc: "A webcam pointed at a physical Go board detects every stone in real time. Calibrate four corners, and OpenCV handles perspective correction, temporal smoothing, and confidence scoring — so only deliberate placements register as moves.",
  },
  {
    icon: "⬡",
    name: "Board & Game Play",
    desc: "Full rule enforcement on 9×9, 13×13, and 19×19 boards. Five difficulty levels from 20k Novice to 1k Advanced using KataGo's human-like play profiles. Set up any position and play from there.",
  },
  {
    icon: "◇",
    name: "AI Analysis",
    desc: "KataGo evaluates every move: win rate, score lead, top suggestions, territory ownership. Each move is graded — blunder, inaccuracy, good, or great — with color-coded indicators on the board.",
  },
  {
    icon: "△",
    name: "Puzzles",
    desc: "Capture, Defend, Life & Death, and Tesuji decks with hints and multiple correct solutions. Track your progress per puzzle. Place positions on a real board and solve them with the camera.",
  },
  {
    icon: "◎",
    name: "Voice Interaction",
    desc: "Speak to the app hands-free during play. Gemini-powered speech-to-text captures your words, and text-to-speech reads responses back — so you never have to look away from the board.",
  },
  {
    icon: "▣",
    name: "LLM Chat",
    desc: "An in-game chat overlay powered by Claude and Gemini. A coaching mode grounds advice in your current position. An opponent persona mode lets the AI analyze as your sitting opponent.",
  },
];

export default function Home() {
  return (
    <>
      <div className={styles.gridBg} />
      <div className={styles.page}>
        {/* ── Hero ── */}
        <section className={styles.hero}>
          <div className={styles.heroGlow} />
          <div className={styles.stones}>
            <div className={`${styles.stone} ${styles.stoneBlack}`} />
            <div className={`${styles.stone} ${styles.stoneWhite}`} />
            <div className={`${styles.stone} ${styles.stoneBlack}`} />
          </div>
          <h1 className={styles.title}>
            touchstone<span className={styles.dot}>.</span>rocks
          </h1>
          <p className={styles.tagline}>
            Play Go against a computer on a real board.
          </p>
          <p className={styles.heroDesc}>
            A webcam watches your physical Go board, detects your stones, and
            KataGo plays back. AI analysis, puzzles, voice interaction, and LLM
            coaching — Touch real Stone Rocks!
          </p>
          <div className={styles.heroCta}>
            <a
              href="https://github.com/RaiderSoft/touchstone.rocks"
              className={styles.btnPrimary}
              target="_blank"
              rel="noopener noreferrer"
            >
              View on GitHub
            </a>
            <a href="#get-started" className={styles.btnSecondary}>
              Get Started
            </a>
          </div>
          <div className={styles.scrollHint}>
            <div className={styles.scrollLine} />
          </div>
        </section>

        {/* ── Divider ── */}
        <div className={styles.divider}>
          <div className={styles.dividerLine} />
          <div className={styles.dividerStone} />
          <div className={styles.dividerLine} />
        </div>

        {/* ── Features ── */}
        <section className={styles.features}>
          <p className={styles.sectionLabel}>Capabilities</p>
          <h2 className={styles.sectionTitle}>
            Everything you need at the board
          </h2>
          <div className={styles.featureGrid}>
            {FEATURES.map((f, i) => (
              <div key={f.name} className={styles.featureCard}>
                <span className={styles.featureNumber}>
                  {String(i + 1).padStart(2, "0")}
                </span>
                <span className={styles.featureIcon}>{f.icon}</span>
                <h3 className={styles.featureName}>{f.name}</h3>
                <p className={styles.featureDesc}>{f.desc}</p>
              </div>
            ))}
          </div>
        </section>

        {/* ── Divider ── */}
        <div className={styles.divider}>
          <div className={styles.dividerLine} />
          <div className={styles.dividerStone} />
          <div className={styles.dividerLine} />
        </div>

        {/* ── Getting Started ── */}
        <section id="get-started" className={styles.gettingStarted}>
          <p className={styles.sectionLabel}>Getting Started</p>
          <h2 className={styles.gettingStartedTitle}>
            Build from source
          </h2>
          <p className={styles.gettingStartedDesc}>
            touchstone.rocks is a C++ desktop application built with CMake. You
            need a C++17 compiler, Raylib, OpenCV, and KataGo installed.
          </p>
          <div className={styles.codeBlock}>
            <code>
              <span className={styles.codeComment}>
                # Clone and build
              </span>
              <br />
              <span className={styles.codeCmd}>git clone</span>{" "}
              https://github.com/RaiderSoft/touchstone.rocks.git
              <br />
              <span className={styles.codeCmd}>cd</span> touchstone.rocks
              <br />
              <span className={styles.codeCmd}>mkdir</span> build &&{" "}
              <span className={styles.codeCmd}>cd</span> build
              <br />
              <span className={styles.codeCmd}>cmake</span> ..
              <br />
              <span className={styles.codeCmd}>make</span> -j$(nproc)
              <br />
              <br />
              <span className={styles.codeComment}># Run</span>
              <br />
              <span className={styles.codeCmd}>./touchstone</span>
            </code>
          </div>
          <a
            href="https://github.com/RaiderSoft/touchstone.rocks#readme"
            className={styles.btnSecondary}
            target="_blank"
            rel="noopener noreferrer"
          >
            Full instructions on GitHub
          </a>
        </section>

        {/* ── Footer ── */}
        <footer className={styles.footer}>
          <div className={styles.footerInner}>
            <span className={styles.footerName}>touchstone.rocks</span>
            <div className={styles.footerLinks}>
              <a
                href="https://github.com/RaiderSoft/touchstone.rocks"
                className={styles.footerLink}
                target="_blank"
                rel="noopener noreferrer"
              >
                GitHub
              </a>
              <a
                href="https://github.com/RaiderSoft/touchstone.rocks/issues"
                className={styles.footerLink}
                target="_blank"
                rel="noopener noreferrer"
              >
                Issues
              </a>
            </div>
          </div>
        </footer>
      </div>
    </>
  );
}
