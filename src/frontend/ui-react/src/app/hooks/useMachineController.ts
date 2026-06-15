import { useState, useCallback, useEffect, useRef } from 'react';

const API_BASE = 'http://localhost:8000/api';

export interface MachineState {
  preset_name: string;
  frequency_hz: number | null;
  t_speed_percent: number;
  positions: number[];  // 4 table positions
  sensors: number[];    // 4 force sensors
  motor_current: string | null;
  errors: string;
  slave_status: string;
  machine_status: string;
}

export interface SerialLog {
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;
}

const STATUS_REQUEST_TIMEOUT = 3000; // 3 seconds - if no data, request status

export const useMachineController = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [machineState, setMachineState] = useState<MachineState | null>(null);
  const [logs, setLogs] = useState<SerialLog[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [lastLogCount, setLastLogCount] = useState(0);
  const lastLogTimeRef = useRef<number>(Date.now());

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
      const response = await fetch(`${API_BASE}/logs?limit=200`);
      if (response.ok) {
        const data = await response.json();
        const logArray = (data.logs || []).map((log: any, idx: number) => ({
          timestamp: new Date().toLocaleTimeString(),
          type: log.type || 'state',
          message: log.message,
        }));
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

    // Setup polling interval - refresh every 200ms for real-time feel
    const pollInterval = setInterval(() => {
      refreshLogs();
    }, 200);

    return () => clearInterval(pollInterval);
  }, [isConnected, refreshLogs]);

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
