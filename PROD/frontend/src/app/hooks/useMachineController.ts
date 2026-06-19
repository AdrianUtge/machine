/**
 * ===========================================================================
 * FILE: useMachineController.ts
 * ROLE:
 *   Hook central : tout l'état machine côté frontend + tous les appels à
 *   l'API REST du backend (port 8000). Unique point de contact réseau de l'UI.
 *
 * ARCHITECTURE:
 *   composants React -> useMachineController() --fetch--> FastAPI (:8000)
 *                       (polling : /api/status à 5 Hz, /api/logs à 2 Hz)
 *
 * RESPONSIBILITIES:
 *   - connect/disconnect, commandes (home/start/stop/frequency/force/goto/torque),
 *     presets personnalisés (CRUD), récupération logs & statut.
 *   - Mesure de latence (EMA) du lien de bout en bout via /api/status.
 *
 * MAINTAINER NOTES:
 *   - API_BASE est figé sur localhost:8000 (backend local). Le backend, lui,
 *     parle à l'ESP/OpenRB ; le frontend ne connaît QUE le backend.
 *   - Les types (MachineState, SerialLog, CustomPreset) doivent rester alignés
 *     sur MachineStateResponse d'api.py.
 * ===========================================================================
 */
import { useState, useCallback, useEffect, useRef } from 'react';

const API_BASE = 'http://localhost:8000/api';

export interface MachineState {
  preset_name: string;
  frequency_hz: number | null;
  t_speed_percent: number;
  force_target?: number | null;
  force_targets?: number[];
  cycle_start?: number | null;  // epoch ms, start of the running cycle
  positions: number[];  // 4 table positions
  sensors: number[];    // 4 force sensors (N, calibrated) — deprecated
  cell_volts_mv?: number[];  // 4 raw cell voltages (mV) — for calibration
  cell_forces_N?: number[];  // 4 forces in Newton (calibrated)
  motor_current: string | null;
  errors: string;
  slave_status: string;
  machine_status: string;
}

export interface SerialLog {
  timestamp: string;
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;
}

export interface CustomPreset {
  frequency: number;
  force: number;
  forces?: number[];  // per-cell forces (4 sensors), optional
}

export type CustomPresets = Record<string, CustomPreset>;

const STATUS_REQUEST_TIMEOUT = 3000; // 3 seconds - if no data, request status

export const useMachineController = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [machineState, setMachineState] = useState<MachineState | null>(null);
  const [logs, setLogs] = useState<SerialLog[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [lastLogCount, setLastLogCount] = useState(0);
  const [customPresets, setCustomPresets] = useState<CustomPresets>({});
  const [connectedSince, setConnectedSince] = useState<number | null>(null);
  // Latence aller-retour du poll /api/status (ms). Sur WiFi, le backend interroge
  // l'ESP -> l'OpenRB dans ce même appel, donc ça mesure tout le lien de bout en bout.
  // null = pas encore de mesure, -1 = dernier poll en échec (lien coupé).
  const [latencyMs, setLatencyMs] = useState<number | null>(null);
  const lastLogTimeRef = useRef<number>(Date.now());
  // Stable log list: timestamps are assigned once on first arrival, not re-computed on each poll.
  const stableLogsRef = useRef<SerialLog[]>([]);

  // Track connection start time for runtime display
  useEffect(() => {
    setConnectedSince(isConnected ? Date.now() : null);
  }, [isConnected]);

  // Get available ports
  const getAvailablePorts = useCallback(async () => {
    try {
      console.log('🔍 FETCHING AVAILABLE PORTS...');
      console.log('📤 GET', `${API_BASE}/ports`);

      const response = await fetch(`${API_BASE}/ports`);
      console.log('📥 Response:', response.status, response.statusText);

      const data = await response.json();
      console.log('✅ Available Ports:', data.ports);

      return data.ports as string[];
    } catch (err) {
      const errorMsg = 'Failed to get ports: ' + String(err);
      console.error('❌', errorMsg, err);
      setError(errorMsg);
      return [];
    }
  }, []);

  // Connect to port
  const connect = useCallback(async (port: string) => {
    console.log('\n' + '='.repeat(60));
    console.log('🔗 CONNECTING TO PORT:', port);
    console.log('='.repeat(60));

    setIsLoading(true);
    setError(null);

    try {
      console.log('📤 POST', `${API_BASE}/connect`, '| Body:', JSON.stringify({ port }));

      const response = await fetch(`${API_BASE}/connect`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ port }),
      });

      console.log('📥 Response Status:', response.status, response.statusText);
      console.log('📝 Response Headers:', {
        'content-type': response.headers.get('content-type'),
      });

      if (!response.ok) {
        const errorText = await response.text();
        console.error('❌ ERROR Response Body:', errorText);

        let errorMessage = `Connection failed (${response.status})`;
        try {
          const errorData = JSON.parse(errorText);
          errorMessage = errorData.detail || errorMessage;
        } catch (e) {
          errorMessage = errorText || errorMessage;
        }

        console.error('❌ ERROR MESSAGE:', errorMessage);
        throw new Error(errorMessage);
      }

      const responseData = await response.json();
      console.log('✅ Connection Success:', responseData);

      setIsConnected(true);

      // Get initial machine state after successful connection
      try {
        console.log('\n🔄 LOADING MACHINE STATE...');
        console.log('📤 GET', `${API_BASE}/status`);

        const statusResponse = await fetch(`${API_BASE}/status`);
        console.log('📥 Status Response:', statusResponse.status, statusResponse.statusText);

        if (!statusResponse.ok) {
          const errorText = await statusResponse.text();
          console.error('❌ Status API Error:', errorText);
          throw new Error(`Status API returned ${statusResponse.status}`);
        }

        const responseText = await statusResponse.text();
        console.log('📦 Raw Response:', responseText);

        const state = JSON.parse(responseText);
        console.log('✅ Machine State Loaded:', state);

        // Validate state has required fields
        if (state && typeof state === 'object') {
          setMachineState(state);
          console.log('✅ STATE UPDATED');
        } else {
          throw new Error('Invalid state format');
        }
      } catch (err) {
        const errorMsg = err instanceof Error ? err.message : String(err);
        console.error('⚠️ Failed to get initial status:', errorMsg, err);
        // Don't fail the connection just because status fetch failed
        setMachineState({
          preset_name: 'UNKNOWN',
          frequency_hz: null,
          t_speed_percent: 100,
          positions: [0, 0, 0, 0],
          sensors: [0, 0, 0, 0],
          motor_current: null,
          errors: 'Status unavailable',
          slave_status: 'UNKNOWN',
          machine_status: 'CONNECTED',
        });
      }
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : String(err);
      console.error('\n❌ CONNECTION ERROR:', errorMessage);
      console.error('Full Error Object:', err);
      console.log('='.repeat(60) + '\n');

      setError(errorMessage);
      setIsConnected(false);
    } finally {
      setIsLoading(false);
    }
  }, []);

  // Disconnect
  const disconnect = useCallback(async () => {
    try {
      await fetch(`${API_BASE}/disconnect`, { method: 'POST' });
      setIsConnected(false);
      setMachineState(null);
    } catch (err) {
      setError('Disconnect failed: ' + String(err));
    }
  }, []);

  // Get current status
  const getStatus = useCallback(async () => {
    if (!isConnected) return;

    const t0 = performance.now();
    try {
      const response = await fetch(`${API_BASE}/status`);
      if (response.ok) {
        const state = await response.json();
        setMachineState(state);
        // Latence lissée (EMA) pour un affichage stable malgré la jitter du lien.
        const sample = Math.round(performance.now() - t0);
        setLatencyMs((prev) =>
          prev == null || prev < 0 ? sample : Math.round(prev * 0.7 + sample * 0.3)
        );
      } else {
        setLatencyMs(-1);
      }
    } catch (err) {
      setLatencyMs(-1);  // lien coupé / backend injoignable
      setError('Failed to get status: ' + String(err));
    }
  }, [isConnected]);

  // Send command helper
  const sendCommand = useCallback(async (endpoint: string, body?: any) => {
    if (!isConnected) {
      setError('Not connected');
      return null;
    }

    setIsLoading(true);
    try {
      const response = await fetch(`${API_BASE}${endpoint}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: body ? JSON.stringify(body) : undefined,
      });

      if (!response.ok) {
        // Message clair : endpoint + code HTTP + détail backend (FastAPI renvoie {"detail": ...}).
        const raw = await response.text();
        let detail = raw;
        try { detail = JSON.parse(raw).detail ?? raw; } catch { /* texte brut */ }
        if (response.status === 404) {
          throw new Error(
            `POST ${endpoint} → 404 (endpoint absent côté backend). ` +
            `Vérifie que api.py expose cette route et que le serveur a redémarré.`
          );
        }
        throw new Error(`POST ${endpoint} → ${response.status}: ${detail}`);
      }

      const data = await response.json();
      setMachineState(data.state || data);

      // Refresh logs
      await refreshLogs();

      return data;
    } catch (err) {
      // Erreur réseau (fetch rejette) = backend injoignable, pas une 4xx/5xx.
      const msg = err instanceof Error ? err.message : String(err);
      const networkHint = msg.includes('Failed to fetch')
        ? ` — backend injoignable (${API_BASE}). Le serveur uvicorn tourne-t-il ?`
        : '';
      setError(`Command failed: ${msg}${networkHint}`);
      console.error(`[sendCommand] ${endpoint}`, body ?? '', '→', msg);
      return null;
    } finally {
      setIsLoading(false);
    }
  }, [isConnected]);

  // Commands
  const home = useCallback(() => sendCommand('/command/home'), [sendCommand]);
  const start = useCallback(() => sendCommand('/command/start'), [sendCommand]);
  const stop = useCallback(() => sendCommand('/command/stop'), [sendCommand]);
  const hardReset = useCallback(() => sendCommand('/command/hard-reset'), [sendCommand]);

  const setFrequency = useCallback((frequency: number) =>
    sendCommand('/command/frequency', { frequency }),
    [sendCommand]
  );

  const setSpeed = useCallback((speed: number) =>
    sendCommand('/command/speed', { speed }),
    [sendCommand]
  );

  // sensor: undefined/null = global (4 cells), 1-4 = per-cell
  const setForce = useCallback((force: number, sensor?: number | null) =>
    sendCommand('/command/force', sensor ? { force, sensor } : { force }),
    [sendCommand]
  );

  const goto = useCallback((table: number, position: number) =>
    sendCommand('/command/goto', { table, position }),
    [sendCommand]
  );

  const torqueOff = useCallback(() =>
    sendCommand('/command/torque', { on: false }),
    [sendCommand]
  );

  const torqueOn = useCallback(() =>
    sendCommand('/command/torque', { on: true }),
    [sendCommand]
  );

  const applyPreset = useCallback((preset: string) =>
    sendCommand('/command/preset', { preset }),
    [sendCommand]
  );

  const sendManualCommand = useCallback((command: string) =>
    sendCommand('/command/manual', { command }),
    [sendCommand]
  );

  const blinkMotor = useCallback((motorId: number, durationMs: number = 500) =>
    sendCommand('/settings/blink-motor', { motor_id: motorId, duration_ms: durationMs }),
    [sendCommand]
  );

  const setResistance = useCallback((resistanceOhm: number) =>
    sendCommand('/settings/set-resistance', { resistance_ohm: resistanceOhm }),
    [sendCommand]
  );

  // --- Custom presets (frequency + force), persisted server-side ---------

  const loadPresets = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/presets`);
      if (response.ok) {
        const data = await response.json();
        setCustomPresets(data.presets || {});
        return data.presets as CustomPresets;
      }
    } catch (err) {
      console.error('Failed to load presets:', err);
    }
    return {} as CustomPresets;
  }, []);

  const savePreset = useCallback(async (name: string, frequency: number, force: number, forces?: number[]) => {
    try {
      const response = await fetch(`${API_BASE}/presets`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(forces ? { name, frequency, force, forces } : { name, frequency, force }),
      });
      if (!response.ok) {
        const err = await response.text();
        throw new Error(err);
      }
      const data = await response.json();
      setCustomPresets(data.presets || {});
      return true;
    } catch (err) {
      setError('Failed to save preset: ' + String(err));
      return false;
    }
  }, []);

  const deletePreset = useCallback(async (name: string) => {
    try {
      const response = await fetch(`${API_BASE}/presets/${encodeURIComponent(name)}`, {
        method: 'DELETE',
      });
      if (response.ok) {
        const data = await response.json();
        setCustomPresets(data.presets || {});
      }
    } catch (err) {
      setError('Failed to delete preset: ' + String(err));
    }
  }, []);

  const applyCustomPreset = useCallback(async (name: string) => {
    if (!isConnected) {
      setError('Not connected');
      return null;
    }
    setIsLoading(true);
    try {
      const response = await fetch(`${API_BASE}/presets/${encodeURIComponent(name)}/apply`, {
        method: 'POST',
      });
      if (!response.ok) {
        const err = await response.text();
        throw new Error(err);
      }
      const data = await response.json();
      setMachineState(data.state || data);
      return data;
    } catch (err) {
      setError('Failed to apply preset: ' + String(err));
      return null;
    } finally {
      setIsLoading(false);
    }
  }, [isConnected]);

  // Load saved presets once on mount (independent of connection)
  useEffect(() => {
    loadPresets();
  }, [loadPresets]);

  // Clear the displayed log list (frontend only — backend buffer persists).
  // On next poll, entries reappear with fresh timestamps.
  const clearLogs = useCallback(() => {
    stableLogsRef.current = [];
    setLogs([]);
  }, []);

  // Refresh logs
  const refreshLogs = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/logs?limit=200`);
      if (response.ok) {
        const data = await response.json();
        const incoming: any[] = data.logs || [];
        const prev = stableLogsRef.current;
        const now = new Date().toLocaleTimeString();

        // Preserve timestamps for entries that already existed at the same index;
        // only stamp genuinely new entries (appended at the end).
        const logArray: SerialLog[] = incoming.map((log: any, idx: number) => {
          if (
            idx < prev.length &&
            prev[idx].message === log.message &&
            prev[idx].type === (log.type || 'state')
          ) {
            return prev[idx];           // already seen — keep original timestamp
          }
          return {
            timestamp: now,
            type: log.type || 'state',
            message: log.message,
          };
        });

        stableLogsRef.current = logArray;
        setLogs(logArray);
        setLastLogCount(logArray.length);

        // Update last log time if we got new logs
        if (logArray.length > 0) {
          lastLogTimeRef.current = Date.now();
        } else {
          // If no logs for a while, request status to get data
          const timeSinceLastLog = Date.now() - lastLogTimeRef.current;
          if (timeSinceLastLog > STATUS_REQUEST_TIMEOUT && isConnected) {
            console.log('No serial data for', timeSinceLastLog, 'ms - requesting status');
            // Request status directly
            try {
              const statusResponse = await fetch(`${API_BASE}/status`);
              if (statusResponse.ok) {
                const state = await statusResponse.json();
                setMachineState(state);
              }
            } catch (err) {
              console.error('Failed to auto-request status:', err);
            }
          }
        }
      }
    } catch (err) {
      console.error('Failed to refresh logs:', err);
    }
  }, [isConnected]);

  // Setup automatic polling of logs when connected
  useEffect(() => {
    if (!isConnected) return;

    // Initial load
    refreshLogs();

    // Logs n'ont pas besoin du 5 Hz : 500 ms réduit de moitié le volume de
    // requêtes (le statut, lui, reste à 200 ms pour le graphe temps réel).
    const pollInterval = setInterval(() => {
      refreshLogs();
    }, 500);

    return () => clearInterval(pollInterval);
  }, [isConnected, refreshLogs]);

  // Poll machine status (positions / forces / voltages / state) so the live
  // data from the OpenRB keeps flowing to the panel.
  useEffect(() => {
    if (!isConnected) return;
    getStatus();
    const statusInterval = setInterval(() => { getStatus(); }, 200);
    return () => clearInterval(statusInterval);
  }, [isConnected, getStatus]);

  return {
    // State
    isConnected,
    connectedSince,
    machineState,
    logs,
    isLoading,
    error,
    latencyMs,

    // Connection
    getAvailablePorts,
    connect,
    disconnect,
    getStatus,

    // Commands
    home,
    start,
    stop,
    hardReset,
    setFrequency,
    setSpeed,
    setForce,
    goto,
    applyPreset,
    sendManualCommand,
    torqueOff,
    torqueOn,
    blinkMotor,
    setResistance,

    // Custom presets (frequency + force)
    customPresets,
    loadPresets,
    savePreset,
    deletePreset,
    applyCustomPreset,

    // Utils
    refreshLogs,
    clearLogs,
  };
};
