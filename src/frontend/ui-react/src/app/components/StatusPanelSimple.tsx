import { CheckCircle2, XCircle, AlertCircle, Zap, Signal } from 'lucide-react';

interface MachineState {
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

interface StatusPanelSimpleProps {
  isConnected: boolean;
  machineState: MachineState | null;
  onCommand: (command: string) => void;
}

export default function StatusPanelSimple({
  isConnected,
  machineState,
  onCommand,
}: StatusPanelSimpleProps) {
  if (!machineState) {
    return (
      <div className="bg-slate-800 rounded-lg p-6">
        <p className="text-slate-400">Loading machine state...</p>
      </div>
    );
  }

  const hasError = machineState.errors !== 'NONE' && machineState.errors !== 'None';

  return (
    <div className="bg-slate-800 rounded-lg p-6 space-y-4">
      <h3 className="text-lg font-semibold flex items-center gap-2">
        <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`}></div>
        Status
      </h3>

      {/* Machine State */}
      <div className="space-y-2 text-sm">
        <div className="flex justify-between">
          <span className="text-slate-400">State:</span>
          <span className="font-semibold">{machineState.machine_status}</span>
        </div>

        <div className="flex justify-between">
          <span className="text-slate-400">Preset:</span>
          <span className="font-semibold">{machineState.preset_name}</span>
        </div>

        <div className="flex justify-between">
          <span className="text-slate-400">Frequency:</span>
          <span className="font-semibold">{machineState.frequency_hz?.toFixed(2) || 'N/A'} Hz</span>
        </div>

        <div className="flex justify-between">
          <span className="text-slate-400">Speed:</span>
          <span className="font-semibold">{machineState.t_speed_percent}%</span>
        </div>
      </div>

      {/* Position & Sensors */}
      {(machineState.position || machineState.motor_current || machineState.force_sensor) && (
        <div className="border-t border-slate-700 pt-2 space-y-2 text-sm">
          {machineState.position && (
            <div className="flex justify-between">
              <span className="text-slate-400">Position:</span>
              <span className="font-mono">{machineState.position}</span>
            </div>
          )}

          {machineState.motor_current && (
            <div className="flex justify-between">
              <span className="text-slate-400">Motor Current:</span>
              <span className="font-mono">{machineState.motor_current}</span>
            </div>
          )}

          {machineState.force_sensor && (
            <div className="flex justify-between">
              <span className="text-slate-400">Force:</span>
              <span className="font-mono">{machineState.force_sensor}</span>
            </div>
          )}
        </div>
      )}

      {/* Slave Status */}
      <div className="border-t border-slate-700 pt-2 flex items-center gap-2 text-sm">
        <Signal className="w-4 h-4" />
        <span className="text-slate-400">Slave:</span>
        <span className={`font-semibold ${machineState.slave_status === 'UNKNOWN' ? 'text-yellow-500' : 'text-green-500'}`}>
          {machineState.slave_status}
        </span>
      </div>

      {/* Errors */}
      {hasError && (
        <div className="border-t border-slate-700 pt-2 flex items-start gap-2 bg-red-900 bg-opacity-30 p-2 rounded border border-red-700">
          <AlertCircle className="w-4 h-4 text-red-500 flex-shrink-0 mt-0.5" />
          <div className="text-xs">
            <p className="font-semibold text-red-500">Error</p>
            <p className="text-red-300">{machineState.errors}</p>
          </div>
        </div>
      )}

      {/* Quick Commands */}
      <div className="border-t border-slate-700 pt-3 space-y-2">
        <p className="text-xs text-slate-400 uppercase font-semibold">Quick Actions</p>
        <button
          onClick={() => onCommand('HOME')}
          className="w-full px-3 py-2 bg-blue-600 hover:bg-blue-700 rounded text-sm font-semibold transition-colors"
        >
          🏠 Home
        </button>
        <button
          onClick={() => onCommand('START')}
          className="w-full px-3 py-2 bg-green-600 hover:bg-green-700 rounded text-sm font-semibold transition-colors"
        >
          ▶️ Start
        </button>
        <button
          onClick={() => onCommand('STOP')}
          className="w-full px-3 py-2 bg-red-600 hover:bg-red-700 rounded text-sm font-semibold transition-colors"
        >
          ⏹️ Stop
        </button>
      </div>
    </div>
  );
}
