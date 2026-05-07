import { useState, useEffect } from 'react';
import { AlertTriangle } from 'lucide-react';
import ConnectionScreen from './components/ConnectionScreen';
import MotionControl from './components/MotionControl';
import StatusPanel from './components/StatusPanel';
import SerialMonitor from './components/SerialMonitor';
import { useMachineController } from './hooks/useMachineController';

interface SerialLog {
  timestamp: string;
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;
}

export default function App() {
  const {
    isConnected,
    machineState,
    logs,
    isLoading,
    error,
    getAvailablePorts,
    connect,
    disconnect,
    home,
    start,
    stop,
    setFrequency,
    setSpeed,
    applyPreset,
  } = useMachineController();

  const [availablePorts, setAvailablePorts] = useState<string[]>([]);
  const [loadingPorts, setLoadingPorts] = useState(true);
  const [portsError, setPortsError] = useState<string | null>(null);
  const [showConnection, setShowConnection] = useState(!isConnected);
  const [serialLogs, setSerialLogs] = useState<SerialLog[]>([]);
  const [selectedPreset, setSelectedPreset] = useState('custom');

  // Load available ports on mount and when needed
  useEffect(() => {
    const loadPorts = async () => {
      setLoadingPorts(true);
      setPortsError(null);
      try {
        const ports = await getAvailablePorts();
        setAvailablePorts(ports);
        if (ports.length === 0) {
          setPortsError('No ports detected. Make sure your device is connected.');
        }
      } catch (err) {
        setPortsError('Failed to load ports. Make sure the API is running.');
        console.error('Error loading ports:', err);
      } finally {
        setLoadingPorts(false);
      }
    };
    loadPorts();
  }, [getAvailablePorts]);

  // Map API logs to serial logs with timestamps
  useEffect(() => {
    const newLogs = logs.map((log: any, idx: number) => ({
      timestamp: new Date(Date.now() - (logs.length - idx) * 100)
        .toLocaleTimeString(),
      type: log.type || 'state',
      message: log.message,
    }));
    setSerialLogs(newLogs);
  }, [logs]);

  const handleConnect = async (port: string) => {
    await connect(port);
    setShowConnection(false);
  };

  const handleDisconnect = async () => {
    await disconnect();
    setShowConnection(true);
  };

  const handleFrequencyChange = (freq: number) => {
    setFrequency(freq);
    setSelectedPreset('custom');
  };

  const handlePresetChange = (presetKey: string) => {
    setSelectedPreset(presetKey);
    applyPreset(presetKey);
  };

  const handleCommand = (command: string) => {
    if (command === 'HOME') home();
    else if (command === 'START') start();
    else if (command === 'STOP') stop();
  };

  if (showConnection) {
    return (
      <ConnectionScreen
        availablePorts={availablePorts}
        onConnect={handleConnect}
        isLoading={loadingPorts}
        error={portsError || error}
      />
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-900 via-slate-800 to-slate-900 text-white p-6">
      {/* Header */}
      <div className="mb-6 flex justify-between items-center">
        <div>
          <h1 className="text-3xl font-bold">Control Panel</h1>
          <p className="text-slate-400">Machine Control Interface</p>
        </div>

        {/* Connection Status */}
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2 px-4 py-2 bg-slate-800 rounded-lg">
            <div
              className={`w-2 h-2 rounded-full ${
                isConnected ? 'bg-green-500' : 'bg-red-500'
              }`}
            ></div>
            <span
              className={`font-semibold ${
                isConnected ? 'text-green-500' : 'text-red-500'
              }`}
            >
              {isConnected ? 'Connected' : 'Disconnected'}
            </span>
          </div>

          {error && (
            <div className="flex items-center gap-2 px-4 py-2 bg-red-900 rounded-lg">
              <AlertTriangle size={18} />
              <span className="text-sm">{error}</span>
            </div>
          )}

          <button
            onClick={handleDisconnect}
            className="px-4 py-2 bg-red-600 hover:bg-red-700 rounded-lg transition-colors font-semibold"
          >
            Disconnect
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div className="grid grid-cols-1 xl:grid-cols-4 gap-6">
        {/* Status Panel */}
        <div className="xl:col-span-1">
          <StatusPanel
            isConnected={isConnected}
            machineState={machineState}
            onCommand={handleCommand}
          />
        </div>

        {/* Motion Control */}
        <div className="xl:col-span-2 space-y-6">
          <div className="bg-slate-800 rounded-lg p-6">
            <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
              <div className="w-3 h-3 bg-purple-500 rounded-full"></div>
              Motion Control
            </h2>
            <MotionControl
              frequency={machineState?.frequency_hz || 0}
              speed={machineState?.t_speed_percent || 100}
              onChange={handleFrequencyChange}
              isConnected={isConnected}
              selectedPreset={selectedPreset}
              onPresetChange={handlePresetChange}
              onSpeedChange={setSpeed}
              machineState={machineState?.machine_status || 'DISCONNECTED'}
            />
          </div>
        </div>

        {/* Right Spacer */}
        <div className="xl:col-span-1"></div>
      </div>

      {/* Serial Monitor */}
      <div className="mt-6">
        <SerialMonitor logs={serialLogs} onClear={() => setSerialLogs([])} />
      </div>
    </div>
  );
}
