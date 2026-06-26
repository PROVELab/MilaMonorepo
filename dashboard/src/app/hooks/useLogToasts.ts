"use client";

import { useCallback, useEffect, useRef, useState } from "react";

const TOAST_LIMIT = 5;
const TOAST_DURATION_MS = 4200;
const TOAST_DURATION_ALERT_MS = 9800;

type ToastTone = "info" | "warn" | "error";

export interface LogToast {
  id: number;
  message: string;
  tone: ToastTone;
}

export function useLogToasts(logs: string[]) {
  const [toasts, setToasts] = useState<LogToast[]>([]);
  const seenLogsRef = useRef<Set<string>>(new Set());
  const logsPrimedRef = useRef(false);
  const toastSeqRef = useRef(0);
  const toastTimeoutsRef = useRef<number[]>([]);

  const enqueueToast = useCallback((toast: Omit<LogToast, "id">, durationMs: number) => {
    const id = toastSeqRef.current++;
    setToasts(prev => {
      const next = [...prev, { id, ...toast }];
      return next.length > TOAST_LIMIT ? next.slice(next.length - TOAST_LIMIT) : next;
    });

    const timeout = window.setTimeout(() => {
      setToasts(prev => prev.filter(toast => toast.id !== id));
      toastTimeoutsRef.current = toastTimeoutsRef.current.filter(handle => handle !== timeout);
    }, durationMs);
    toastTimeoutsRef.current.push(timeout);
  }, []);

  useEffect(() => {
    const activeLogs = logs.filter(line => line.trim().length > 0);
    const seenLogs = seenLogsRef.current;

    if (!logsPrimedRef.current) {
      activeLogs.forEach(line => seenLogs.add(line));
      logsPrimedRef.current = true;
      return;
    }

    const newLogs = activeLogs.filter(line => !seenLogs.has(line));
    for (const line of newLogs.reverse()) {
      const parsedMcu = line.match(
        /^(?:\[\d{2}:\d{2}:\d{2}\]\s*)?MCU:\s+([IWE])\s+\(\d+\)\s+[^:]+:\s*(.*)$/,
      );

      if (parsedMcu) {
        const severity = parsedMcu[1];
        const cleanedMessage = parsedMcu[2].trim();
        const tone: ToastTone =
          severity === "E" ? "error" : severity === "W" ? "warn" : "info";
        const durationMs = tone === "info" ? TOAST_DURATION_MS : TOAST_DURATION_ALERT_MS;
        enqueueToast({ message: cleanedMessage, tone }, durationMs);
        continue;
      }

      enqueueToast({ message: line, tone: "info" }, TOAST_DURATION_MS);
    }

    seenLogs.clear();
    activeLogs.forEach(line => seenLogs.add(line));
  }, [logs, enqueueToast]);

  useEffect(() => {
    return () => {
      for (const handle of toastTimeoutsRef.current) {
        window.clearTimeout(handle);
      }
      toastTimeoutsRef.current = [];
    };
  }, []);

  return toasts;
}
