import { useState } from 'react';
import { Usb, Bluetooth, RefreshCw, Cable } from 'lucide-react';

type ConnectionType = 'bluetooth' | 'usb' | 'disconnected';

interface SerialPort {
  name: string;
  type: ConnectionType;
  status: 'available' | 'busy' | 'unavailable';
}

interface ConnectionScreenProps {
  onConnect: (port: string, type: ConnectionType) => void;
}

export default function ConnectionScreen({ onConnect }: ConnectionScreenProps) {
  const [selectedPort, setSelectedPort] = useState<string>('');
  const [isConnecting, setIsConnecting] = useState(false);

  // Mock serial ports
  const [ports, setPorts] = useState<SerialPort[]>([
    { name: '/dev/cu.usbmodem101', type: 'usb', status: 'available' },
    { name: '/dev/cu.usbserial-14210', type: 'usb', status: 'available' },
    { name: 'COM3', type: 'usb', status: 'busy' },
    { name: 'BT-MACHINE-01', type: 'bluetooth', status: 'available' },
  ]);

  const handleRefresh = () => {
    // Simulate rescanning ports
    setPorts([...ports]);
  };

  const handleConnect = () => {
    if (!selectedPort) return;

    const port = ports.find(p => p.name === selectedPort);
    if (!port || port.status !== 'available') return;

    setIsConnecting(true);
    setTimeout(() => {
      onConnect(selectedPort, port.type);
      setIsConnecting(false);
    }, 1500);
  };

  return (
    <div className="min-h-screen bg-slate-900 text-white flex items-center justify-center p-6">
      <div className="w-full max-w-2xl bg-slate-800 rounded-lg p-8 shadow-xl">
        <div className="flex items-center gap-3 mb-6">
          <Cable className="w-8 h-8 text-blue-400" />
          <h1 className="text-3xl font-bold">Machine Connection</h1>
        </div>

        <div className="mb-6">
          <div className="flex items-center justify-between mb-4">
            <label className="text-lg font-semibold">Available Ports</label>
            <button
              onClick={handleRefresh}
              className="flex items-center gap-2 px-3 py-2 bg-slate-700 hover:bg-slate-600 rounded-lg transition-colors"
            >
              <RefreshCw className="w-4 h-4" />
              Refresh
            </button>
          </div>

          <div className="space-y-2 max-h-64 overflow-y-auto">
            {ports.length === 0 ? (
              <div className="text-center py-8 text-slate-400">
                No ports detected
              </div>
            ) : (
              ports.map((port) => (
                <button
                  key={port.name}
                  onClick={() => setSelectedPort(port.name)}
                  disabled={port.status !== 'available'}
                  className={`w-full flex items-center justify-between p-4 rounded-lg transition-all ${
                    selectedPort === port.name
                      ? 'bg-blue-600 border-2 border-blue-400'
                      : port.status === 'available'
                      ? 'bg-slate-700 hover:bg-slate-600 border-2 border-transparent'
                      : 'bg-slate-700 opacity-50 cursor-not-allowed border-2 border-transparent'
                  }`}
                >
                  <div className="flex items-center gap-3">
                    {port.type === 'usb' ? (
                      <Usb className="w-5 h-5 text-green-400" />
                    ) : (
                      <Bluetooth className="w-5 h-5 text-blue-400" />
                    )}
                    <div className="text-left">
                      <div className="font-mono font-semibold">{port.name}</div>
                      <div className="text-xs text-slate-400">
                        {port.type === 'usb' ? 'USB' : 'Bluetooth'}
                      </div>
                    </div>
                  </div>
                  <div>
                    <span className={`text-xs font-semibold px-2 py-1 rounded ${
                      port.status === 'available' ? 'bg-green-600 text-green-100' :
                      port.status === 'busy' ? 'bg-yellow-600 text-yellow-100' :
                      'bg-red-600 text-red-100'
                    }`}>
                      {port.status}
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
          disabled={!selectedPort || isConnecting}
          className="w-full px-6 py-4 bg-blue-600 hover:bg-blue-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg font-semibold text-lg transition-colors"
        >
          {isConnecting ? 'Connecting...' : 'Connect'}
        </button>
      </div>
    </div>
  );
}
