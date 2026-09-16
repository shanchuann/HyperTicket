import { useEffect, useState } from 'react';

export function useCountdown() {
  const [seconds, setSeconds] = useState(0);

  useEffect(() => {
    if (seconds <= 0) return;
    const timer = window.setInterval(() => setSeconds(value => Math.max(0, value - 1)), 1000);
    return () => window.clearInterval(timer);
  }, [seconds > 0]);

  return { seconds, start: setSeconds, reset: () => setSeconds(0) };
}
