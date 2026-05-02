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
        {/* Prove Logo Branding */}
        <div className="loading-screen__brand">
          <img src="/prove_logo.png" alt="Prove Logo" />
        </div>

        {/* Main "Mila" Text Group */}
        <div className="signature-layer-main" style={{ position: 'relative' }}>
          <div className="signature-text signature-text--base">
            Mila
          </div>
          <div className={`signature-text signature-text--bold ${isDrawn ? 'is-visible' : ''}`}>
            Mila
          </div>
        </div>
        
        {/* Synchronized Reflection Group */}
        <div className="signature-reflection">
           <div style={{ position: 'relative' }}>
            <div className="signature-text signature-text--base">
              Mila
            </div>
            <div className={`signature-text signature-text--bold ${isDrawn ? 'is-visible' : ''}`}>
              Mila
            </div>
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
