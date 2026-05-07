import { useState, useCallback } from 'react';

const API_BASE = 'http://localhost:8000/api';

export interface MachineState {
  preset_name: string;
  frequency_hz: number | null;
  t_speed_percent: number;
  position: string | null;
  motor_current: string | null;
  force_sensor: string | null;
  errors: string;
  slave_status: string;
  machine_status: string;
}

export interface SerialLog {
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;
}

export const useMachineController = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [machineState, setMachineState] = useState<MachineState | null>(null);
  const [logs, setLogs] = useState<SerialLog[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Get available ports
  const getAvailablePorts = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/ports`);
      const data = await response.json();
      return data.ports as string[];
    } catch (err) {
      setError('Failed to get ports: ' + String(err));
      return [];
    }
  }, []);

  // Connect to port
  const connect = useCallback(async (port: string) => {
    setIsLoading(true);
    setError(null);
    try {
      const response = await fetch(`${API_BASE}/connect`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ port }),
      });

      if (!response.ok) throw new Error('Connection failed');

      setIsConnected(true);
      // Get initial state
      await getStatus();
    } catch (err) {
      setError('Connection failed: ' + String(err));
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

    try {
      const response = await fetch(`${API_BASE}/status`);
      if (response.ok) {
        const state = await response.json();
        setMachineState(state);
      }
    } catch (err) {
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
        const err = await response.text();
        throw new Error(err);
      }

      const data = await response.json();
      setMachineState(data.state || data);

      // Refresh logs
      await refreshLogs();

      return data;
    } catch (err) {
      setError('Command failed: ' + String(err));
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

  const applyPreset = useCallback((preset: string) =>
    sendCommand('/command/preset', { preset }),
    [sendCommand]
  );

  const sendManualCommand = useCallback((command: string) =>
    sendCommand('/command/manual', { command }),
    [sendCommand]
  );

  // Refresh logs
  const refreshLogs = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/logs?limit=100`);
      if (response.ok) {
        const data = await response.json();
        setLogs(data.logs || []);
      }
    } catch (err) {
      console.error('Failed to refresh logs:', err);
    }
  }, []);

  return {
    // State
    isConnected,
    machineState,
    logs,
    isLoading,
    error,

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
    applyPreset,
    sendManualCommand,

    // Utils
    refreshLogs,
  };
};
