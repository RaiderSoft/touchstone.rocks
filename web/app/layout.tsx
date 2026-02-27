import type { Metadata } from "next";
import { DM_Serif_Display, Outfit } from "next/font/google";
import "./globals.css";

const serif = DM_Serif_Display({
  weight: "400",
  subsets: ["latin"],
  variable: "--font-serif",
  display: "swap",
});

const sans = Outfit({
  subsets: ["latin"],
  variable: "--font-sans",
  display: "swap",
});

export const metadata: Metadata = {
  title: "touchstone.rocks — Play Go on a Real Board",
  description:
    "Play Go against KataGo on a physical board with webcam vision, AI analysis, puzzles, voice interaction, and LLM coaching.",
  openGraph: {
    title: "touchstone.rocks",
    description: "Play Go against a computer on a real board.",
    url: "https://touchstone.rocks",
    siteName: "touchstone.rocks",
    type: "website",
  },
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en" className={`${serif.variable} ${sans.variable}`}>
      <body>{children}</body>
    </html>
  );
}
