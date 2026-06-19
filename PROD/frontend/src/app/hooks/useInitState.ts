import { useState, useEffect } from "react";

interface InitStatus {
  running: boolean;
  phase: string;
  progress_percent: number;
  elapsed_ms: number;
  force_peaks: [number, number, number, number];
  complete_motors: [boolean, boolean, boolean, boolean];
  error_code: number;
}

interface InitConfig {
  target_position_mm: number;
  descent_rate_mm_per_min: number;
  max_duration_s: number;
  auto_init_interval: number;
}

// Get API base URL from window location or fallback to localhost
const getApiBase = () => {
  const hostname = window.location.hostname;
  const port = window.location.port ? `:${window.location.port}` : '';
  // If on localhost:3000 (dev), API is on localhost:8000
  if (hostname === 'localhost' || hostname === '127.0.0.1') {
    return `http://${hostname}:8000/api`;
  }
  // Otherwise use same host (production setup with reverse proxy)
  return `http://${hostname}${port}/api`;
};

const API_BASE = getApiBase();

export const useInitState = () => {
  const [initStatus, setInitStatus] = useState<InitStatus | null>(null);
  const [initConfig, setInitConfig] = useState<InitConfig | null>(null);
  const [isInitRunning, setIsInitRunning] = useState(false);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Poll /api/init/status every 500ms while running
  useEffect(() => {
    let interval: NodeJS.Timeout | null = null;

    if (isInitRunning) {
      interval = setInterval(async () => {
        try {
          const resp = await fetch(`${API_BASE}/init/status`);
          const data = await resp.json();
          setInitStatus(data);

          // Auto-stop when init completes or errors
          if (data.phase === "COMPLETE" || data.phase === "ERROR") {
            setIsInitRunning(false);
          }
        } catch (e) {
          console.error("Failed to poll init status:", e);
          setError("Failed to fetch init status");
        }
      }, 500);
    }

    return () => {
      if (interval) clearInterval(interval);
    };
  }, [isInitRunning]);

  // Fetch initial config on mount
  useEffect(() => {
    const fetchConfig = async () => {
      try {
        const resp = await fetch(`${API_BASE}/init/config`);
        const data = await resp.json();
        setInitConfig(data);
      } catch (e) {
        console.error("Failed to load init config:", e);
        setError("Failed to load init config");
      }
    };

    fetchConfig();
  }, []);

  const startInit = async (overrides?: Partial<InitConfig>) => {
    setLoading(true);
    setError(null);

    try {
      const body = overrides || {};
      const resp = await fetch(`${API_BASE}/init/start`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
      });

      if (!resp.ok) {
        throw new Error(`Failed to start init: ${resp.statusText}`);
      }

      setIsInitRunning(true);
    } catch (e) {
      const msg = e instanceof Error ? e.message : "Unknown error";
      setError(msg);
      console.error("Failed to start init:", e);
    } finally {
      setLoading(false);
    }
  };

  const stopInit = async () => {
    setLoading(true);
    setError(null);

    try {
      const resp = await fetch(`${API_BASE}/init/stop`, {
        method: "POST",
      });

      if (!resp.ok) {
        throw new Error(`Failed to stop init: ${resp.statusText}`);
      }

      setIsInitRunning(false);
    } catch (e) {
      const msg = e instanceof Error ? e.message : "Unknown error";
      setError(msg);
      console.error("Failed to stop init:", e);
    } finally {
      setLoading(false);
    }
  };

  const updateConfig = async (config: InitConfig) => {
    setLoading(true);
    setError(null);

    try {
      const resp = await fetch(`${API_BASE}/init/config`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(config),
      });

      if (!resp.ok) {
        throw new Error(`Failed to update config: ${resp.statusText}`);
      }

      setInitConfig(config);
    } catch (e) {
      const msg = e instanceof Error ? e.message : "Unknown error";
      setError(msg);
      console.error("Failed to update config:", e);
    } finally {
      setLoading(false);
    }
  };

  return {
    initStatus,
    initConfig,
    isInitRunning,
    loading,
    error,
    startInit,
    stopInit,
    updateConfig,
  };
};
