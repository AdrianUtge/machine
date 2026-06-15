import { CheckCircle2, XCircle, AlertCircle, Settings, Activity, Radio } from 'lucide-react';

type ConnectionType = 'bluetooth' | 'usb' | 'disconnected';
type MachineState = 'IDLE' | 'HOMING' | 'RUNNING' | 'ERROR' | 'SHUTDOWN';
type CommandState = 'idle' | 'pending' | 'completed' | 'error';

interface LoadCellData {
  force: number;
  mass: number;
  status: 'OK' | 'noisy' | 'error' | 'disconnected';
}

interface StatusPanelProps {
  isConnected: boolean;
  isSlaveConnected: boolean;
  connectionType: ConnectionType;
  machineState: MachineState;
  positions: number[];
  currentPosition: number;
  loadCells: LoadCellData[];
  frequency: number;
  speed: number;
  lastCommand: string;
  lastCompletedCommand: string;
  slaveMessage: string;
  slaveFrequency: number;
  slaveRunning: boolean;
  onCommand: (command: string) => void;
  pendingCommands: Record<string, CommandState>;
}

export default function StatusPanel({
  isConnected,
  isSlaveConnected,
  connectionType,
  machineState,
  currentPosition,
  frequency,
  speed,
  lastCommand,
  lastCompletedCommand,
  slaveMessage,
  slaveFrequency,
  slaveRunning,
}: StatusPanelProps) {
  const handleSettings = () => {
    alert('Settings panel would open here');
  };

  const getMachineStateColor = () => {
    switch (machineState) {
      case 'IDLE':
        return 'text-slate-300';
      case 'HOMING':
        return 'text-yellow-500';
      case 'RUNNING':
        return 'text-green-500';
      case 'ERROR':
        return 'text-red-500';
      case 'SHUTDOWN':
        return 'text-slate-500';
      default:
        return 'text-slate-400';
    }
  };

  return (
    <div className="bg-slate-800 rounded-lg p-6 h-full">
      <div className="flex items-center justify-between mb-4">
        <h2 className="text-xl font-semibold flex items-center gap-2">
          <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`}></div>
          System Status
        </h2>
        <button
          onClick={handleSettings}
          className="p-2 bg-slate-700 hover:bg-slate-600 rounded-lg transition-colors"
          title="Settings"
        >
          <Settings className="w-5 h-5 text-slate-300" />
        </button>
      </div>

      {/* Connection Info */}
      <div className="mb-4 pb-4 border-b border-slate-700">
        <div className="space-y-2 text-sm">
          <div className="flex justify-between">
            <span className="text-slate-400">Main Board</span>
            <span className={`font-semibold ${isConnected ? 'text-green-500' : 'text-red-500'}`}>
              {connectionType === 'usb' ? 'USB' : connectionType === 'bluetooth' ? 'Bluetooth' : 'Offline'}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-400">Slave Board</span>
            <span className={`font-semibold ${isSlaveConnected ? 'text-green-500' : 'text-red-500'}`}>
              {isSlaveConnected ? 'Connected' : 'Offline'}
            </span>
          </div>
        </div>
      </div>

      {/* Machine State */}
      <div className="mb-4 pb-4 border-b border-slate-700">
        <div className="flex items-center gap-2 mb-2">
          <Activity className="w-4 h-4 text-blue-400" />
          <h3 className="text-sm font-semibold text-slate-400 uppercase">Machine State</h3>
        </div>
        <div className={`text-2xl font-bold font-mono ${getMachineStateColor()}`}>
          {machineState}
        </div>
      </div>

      {/* Command Status */}
      <div className="mb-4 pb-4 border-b border-slate-700 space-y-2">
        <h3 className="text-xs font-semibold text-slate-400 uppercase">Commands</h3>
        <div className="text-sm space-y-1">
          <div className="flex justify-between">
            <span className="text-slate-400">Last Sent:</span>
            <span className="font-mono text-yellow-400">{lastCommand || 'None'}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-400">Last Completed:</span>
            <span className="font-mono text-green-400">{lastCompletedCommand || 'None'}</span>
          </div>
        </div>
      </div>

      {/* Machine Parameters */}
      <div className="mb-4 pb-4 border-b border-slate-700 space-y-2">
        <h3 className="text-xs font-semibold text-slate-400 uppercase">Parameters</h3>
        <div className="space-y-2 text-sm">
          <div className="flex justify-between">
            <span className="text-slate-400">Position:</span>
            <span className="font-mono font-semibold">{currentPosition.toFixed(2)} mm</span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-400">Frequency:</span>
            <span className="font-mono font-semibold">{frequency.toFixed(1)} Hz</span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-400">Speed:</span>
            <span className="font-mono font-semibold">{speed} units</span>
          </div>
        </div>
      </div>

      {/* Slave Status */}
      <div className="mb-4 pb-4 border-b border-slate-700">
        <div className="flex items-center gap-2 mb-2">
          <Radio className="w-4 h-4 text-purple-400" />
          <h3 className="text-sm font-semibold text-slate-400 uppercase">Slave Status</h3>
        </div>
        <div className="space-y-2 text-sm">
          <div className="flex items-center justify-between">
            <span className="text-slate-400">Motor State:</span>
            <span className={`font-semibold ${slaveRunning ? 'text-green-500' : 'text-slate-400'}`}>
              {slaveRunning ? 'Running' : 'Stopped'}
            </span>
          </div>
          <div className="flex justify-between">
            <span className="text-slate-400">Excitation Freq:</span>
            <span className="font-mono font-semibold">{slaveFrequency.toFixed(1)} Hz</span>
          </div>
          <div className="mt-2 p-2 bg-slate-900 rounded text-xs font-mono text-purple-400 break-all">
            {slaveMessage || 'No message'}
          </div>
        </div>
      </div>

      {/* Quick Stats */}
      <div className="space-y-2">
        <h3 className="text-xs font-semibold text-slate-400 uppercase">Quick Stats</h3>
        <div className="grid grid-cols-1 gap-2">
          <div className="bg-slate-700 rounded p-2">
            <div className="text-xs text-slate-400">Status</div>
            <div className="flex items-center gap-2 mt-1">
              {isConnected ? (
                <CheckCircle2 className="w-4 h-4 text-green-500" />
              ) : (
                <XCircle className="w-4 h-4 text-red-500" />
              )}
              <span className={`text-sm font-semibold ${isConnected ? 'text-green-500' : 'text-red-500'}`}>
                {isConnected ? 'Online' : 'Offline'}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
