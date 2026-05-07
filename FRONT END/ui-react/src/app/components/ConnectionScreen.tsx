import { useState, useEffect } from 'react';
import { Usb, Bluetooth, RefreshCw, Cable, AlertCircle } from 'lucide-react';

type ConnectionType = 'usb' | 'bluetooth' | 'unknown';

interface SerialPort {
  name: string;
  type: ConnectionType;
  status: 'available' | 'unavailable';
}

interface ConnectionScreenProps {
  availablePorts: string[];
  onConnect: (port: string) => Promise<void>;
  isLoading?: boolean;
  error?: string | null;
}

export default function ConnectionScreen({
  availablePorts,
  onConnect,
  isLoading = false,
  error = null
}: ConnectionScreenProps) {
  const [selectedPort, setSelectedPort] = useState<string>('');
  const [isConnecting, setIsConnecting] = useState(false);
  const [ports, setPorts] = useState<SerialPort[]>([]);

  // Update ports when availablePorts change
  useEffect(() => {
    const formattedPorts: SerialPort[] = availablePorts.map(portName => {
      let type: ConnectionType = 'unknown';

      // Detect port type based on name
      if (portName.includes('usbmodem') || portName.includes('usbserial') || portName.startsWith('COM')) {
        type = 'usb';
      } else if (portName.toUpperCase().includes('BT') || portName.includes('bluetooth')) {
        type = 'bluetooth';
      }

      return {
        name: portName,
        type,
        status: 'available' as const,
      };
    });

    setPorts(formattedPorts);

    // Auto-select first port if available and none selected
    if (formattedPorts.length > 0 && !selectedPort) {
      setSelectedPort(formattedPorts[0].name);
    }
  }, [availablePorts, selectedPort]);

  const handleRefresh = async () => {
    // This will trigger a refresh of ports from the parent component
    // For now, just reset selection to force re-fetch
    setSelectedPort('');
  };

  const handleConnect = async () => {
    if (!selectedPort) return;

    const port = ports.find(p => p.name === selectedPort);
    if (!port || port.status !== 'available') return;

    setIsConnecting(true);
    try {
      await onConnect(selectedPort);
    } catch (err) {
      console.error('Connection error:', err);
    } finally {
      setIsConnecting(false);
    }
  };

  return (
    <div className="min-h-screen bg-slate-900 text-white flex items-center justify-center p-6">
      <div className="w-full max-w-2xl bg-slate-800 rounded-lg p-8 shadow-xl">
        <div className="flex items-center gap-3 mb-6">
          <Cable className="w-8 h-8 text-blue-400" />
          <h1 className="text-3xl font-bold">Machine Connection</h1>
        </div>

        {/* Error Display */}
        {error && (
          <div className="mb-6 flex items-center gap-3 p-4 bg-red-900 rounded-lg border border-red-700">
            <AlertCircle className="w-5 h-5 text-red-400 flex-shrink-0" />
            <p className="text-red-200 text-sm">{error}</p>
          </div>
        )}

        {/* Loading State */}
        {isLoading && (
          <div className="mb-6 flex items-center justify-center p-4 bg-slate-700 rounded-lg">
            <div className="animate-spin rounded-full h-5 w-5 border-2 border-blue-400 border-t-blue-600 mr-3"></div>
            <p className="text-slate-300">Loading ports...</p>
          </div>
        )}

        <div className="mb-6">
          <div className="flex items-center justify-between mb-4">
            <label className="text-lg font-semibold">Available Ports</label>
            <button
              onClick={handleRefresh}
              disabled={isLoading}
              className="flex items-center gap-2 px-3 py-2 bg-slate-700 hover:bg-slate-600 disabled:opacity-50 disabled:cursor-not-allowed rounded-lg transition-colors"
            >
              <RefreshCw className={`w-4 h-4 ${isLoading ? 'animate-spin' : ''}`} />
              Refresh
            </button>
          </div>

          <div className="space-y-2 max-h-64 overflow-y-auto">
            {ports.length === 0 ? (
              <div className="text-center py-8 text-slate-400">
                <p className="mb-2">No ports detected</p>
                <p className="text-xs">Make sure your Arduino is connected and click Refresh</p>
              </div>
            ) : (
              ports.map((port) => (
                <button
                  key={port.name}
                  onClick={() => setSelectedPort(port.name)}
                  disabled={port.status !== 'available' || isConnecting}
                  className={`w-full flex items-center justify-between p-4 rounded-lg transition-all ${
                    selectedPort === port.name
                      ? 'bg-blue-600 border-2 border-blue-400'
                      : port.status === 'available'
                      ? 'bg-slate-700 hover:bg-slate-600 border-2 border-transparent cursor-pointer'
                      : 'bg-slate-700 opacity-50 cursor-not-allowed border-2 border-transparent'
                  }`}
                >
                  <div className="flex items-center gap-3">
                    {port.type === 'usb' ? (
                      <Usb className="w-5 h-5 text-green-400" />
                    ) : port.type === 'bluetooth' ? (
                      <Bluetooth className="w-5 h-5 text-blue-400" />
                    ) : (
                      <Cable className="w-5 h-5 text-slate-400" />
                    )}
                    <div className="text-left">
                      <div className="font-mono font-semibold text-sm">{port.name}</div>
                      <div className="text-xs text-slate-400">
                        {port.type === 'usb' ? 'USB Serial' : port.type === 'bluetooth' ? 'Bluetooth' : 'Unknown'}
                      </div>
                    </div>
                  </div>
                  <div>
                    <span className="text-xs font-semibold px-2 py-1 rounded bg-green-600 text-green-100">
                      available
                    </span>
                  </div>
                </button>
              ))
            )}
          </div>
        </div>

        <div className="mb-4">
          <div className={`text-sm px-3 py-2 rounded ${
            selectedPort ? 'text-green-400 bg-green-950' : 'text-slate-400 bg-slate-700'
          }`}>
            {isConnecting ? 'Connecting...' :
             selectedPort ? `Port selected: ${selectedPort}` :
             'No port selected'}
          </div>
        </div>

        <button
          onClick={handleConnect}
          disabled={!selectedPort || isConnecting || isLoading}
          className="w-full px-6 py-4 bg-blue-600 hover:bg-blue-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg font-semibold text-lg transition-colors"
        >
          {isConnecting ? 'Connecting...' : 'Connect'}
        </button>

        <p className="text-xs text-slate-400 text-center mt-4">
          {ports.length === 0 && !isLoading ? 'No ports available' : `${ports.length} port${ports.length !== 1 ? 's' : ''} available`}
        </p>
      </div>
    </div>
  );
}
