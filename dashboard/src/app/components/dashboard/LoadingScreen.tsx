"use client";

import { useEffect, useState } from "react";
import Image from "next/image";

interface Props {
  reason: string;
  isReady: boolean;
  isFadingOut?: boolean;
  onSkip?: () => void;
}

const STATUS_PHRASES = [
  "precharging inverter capacitors",
  "warming MCU coffee filter",
  "calibrating wheel RPM ghosts",
  "convincing BMS cells to cooperate",
  "debouncing the universe",
  "teaching CAN packets to stay in lane",
  "torque vectoring the vibes",
  "counting missed heartbeats on UART",
  "charging sarcasm module to 100%",
  "checking if motor controller is actually listening",
];

const WRITE_PHASE_MS = 4500;
const PULSE_ITERATION_MS = 4000;
const PULSE_ITERATIONS = 3;
const ERASE_PHASE_MS = 9000;

type SignaturePhase = "write" | "pulse" | "erase";

function nextPhraseIndex(previousIndex: number) {
  if (STATUS_PHRASES.length <= 1) {
    return 0;
  }

  let candidate = previousIndex;
  while (candidate === previousIndex) {
    candidate = Math.floor(Math.random() * STATUS_PHRASES.length);
  }
  return candidate;
}

export function LoadingScreen({ reason, isReady, isFadingOut = false, onSkip }: Props) {
  const [phase, setPhase] = useState<SignaturePhase>("write");
  const [phaseIndex, setPhaseIndex] = useState(0);
  const [phraseIndex, setPhraseIndex] = useState(0);

  useEffect(() => {
    const duration =
      phase === "write"
        ? WRITE_PHASE_MS
        : phase === "pulse"
          ? PULSE_ITERATION_MS * PULSE_ITERATIONS
          : ERASE_PHASE_MS;

    const timer = setTimeout(() => {
      setPhase(prev =>
        prev === "write" ? "pulse" : prev === "pulse" ? "erase" : "write",
      );
      setPhaseIndex(index => index + 1);
    }, duration);

    return () => clearTimeout(timer);
  }, [phase]);

  useEffect(() => {
    setPhraseIndex(prev => nextPhraseIndex(prev));
    const timer = setInterval(() => {
      setPhraseIndex(prev => nextPhraseIndex(prev));
    }, 5000);
    return () => clearInterval(timer);
  }, []);

  const baseClass =
    phase === "write"
      ? "signature-text--phase-write"
      : phase === "erase"
        ? "signature-text--phase-erase"
        : "signature-text--phase-static";
  const boldClass = phase === "pulse" ? "signature-text--bold signature-text--bold-pulse" : null;

  return (
    <div
      className={`loading-screen ${isFadingOut ? "loading-screen--fade-out" : ""}`}
      onClick={onSkip}
      style={{ cursor: "pointer" }}
    >
      <div className={`signature-group ${isReady ? "signature-group--ready" : ""}`}>
        <div className="loading-screen__brand">
          <Image src="/prove_logo.png" alt="Prove Logo" width={360} height={120} priority />
        </div>

        <div className="signature-layer-main" style={{ position: "relative" }}>
          <div
            key={`main-${phase}-${phaseIndex}`}
            className={`signature-text signature-text--base ${baseClass}`}
          >
            Mila
          </div>
          {boldClass && <div className={`signature-text ${boldClass}`}>Mila</div>}
        </div>

        {isReady && <div className="loading-screen__ready">Ready to Drive!</div>}

        <div className="signature-reflection">
          <div style={{ position: "relative" }}>
            <div
              key={`reflection-${phase}-${phaseIndex}`}
              className={`signature-text signature-text--base ${baseClass}`}
            >
              Mila
            </div>
            {boldClass && <div className={`signature-text ${boldClass}`}>Mila</div>}
          </div>
        </div>
      </div>

      <div className="loading-screen__status">
        <div className="loading-screen__bar">
          <div className="loading-screen__bar-fill" />
        </div>
        <div className="loading-screen__reason">
          <span className="loading-screen__reason-label">Reason</span>
          <span className="loading-screen__reason-text">{reason}</span>
        </div>
        <span className="loading-screen__phrase">{STATUS_PHRASES[phraseIndex]}</span>
      </div>
    </div>
  );
}
