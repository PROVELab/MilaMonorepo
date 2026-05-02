"use client";

import { useEffect, useState } from "react";

interface Props {
  onSkip?: () => void;
}

export function LoadingScreen({ onSkip }: Props) {
  const [isDrawn, setIsDrawn] = useState(false);

  useEffect(() => {
    // 4.5s matches the signature-write animation in CSS
    const timer = setTimeout(() => {
      setIsDrawn(true);
    }, 4500);
    return () => clearTimeout(timer);
  }, []);

  return (
    <div className="loading-screen" onClick={onSkip} style={{ cursor: 'pointer' }}>
      <div className="signature-group">
        {/* Main "Mila" Text */}
        <div className="signature-text signature-text--base">
          Mila
        </div>
        
        {/* Animated Bolding Layer */}
        <div className={`signature-text signature-text--bold ${isDrawn ? 'is-visible' : ''}`}>
          Mila
        </div>
        
        {/* High-Fidelity Reflection */}
        <div className="signature-reflection">
           <div className="signature-text">
            Mila
          </div>
        </div>
      </div>

      {/* Initialization Status */}
      <div className="loading-screen__status">
        <div className="loading-screen__bar">
          <div className="loading-screen__bar-fill" />
        </div>
        <span>Initializing VSR Link</span>
      </div>
    </div>
  );
}
