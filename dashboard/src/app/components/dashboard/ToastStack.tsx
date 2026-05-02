"use client";

import type { LogToast } from "../../hooks/useLogToasts";

interface Props {
  toasts: LogToast[];
}

export function ToastStack({ toasts }: Props) {
  return (
    <div className="toast-stack" aria-live="polite" aria-atomic="false">
      {toasts.map(toast => (
        <div key={toast.id} className="toast-stack__item">
          {toast.message}
        </div>
      ))}
    </div>
  );
}
