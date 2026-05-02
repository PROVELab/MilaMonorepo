"use client";

import { useCallback, useEffect, useRef, useState } from "react";

const TOAST_LIMIT = 5;
const TOAST_DURATION_MS = 4200;

export interface LogToast {
  id: number;
  message: string;
}

export function useLogToasts(logs: string[]) {
  const [toasts, setToasts] = useState<LogToast[]>([]);
  const seenLogsRef = useRef<Set<string>>(new Set());
  const logsPrimedRef = useRef(false);
  const toastSeqRef = useRef(0);
  const toastTimeoutsRef = useRef<number[]>([]);

  const enqueueToast = useCallback((message: string) => {
    const id = toastSeqRef.current++;
    setToasts(prev => {
      const next = [...prev, { id, message }];
      return next.length > TOAST_LIMIT ? next.slice(next.length - TOAST_LIMIT) : next;
    });

    const timeout = window.setTimeout(() => {
      setToasts(prev => prev.filter(toast => toast.id !== id));
      toastTimeoutsRef.current = toastTimeoutsRef.current.filter(handle => handle !== timeout);
    }, TOAST_DURATION_MS);
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
      enqueueToast(line);
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
