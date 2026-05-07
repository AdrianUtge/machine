import { useState } from 'react';
import { Gauge, Play, Square, ChevronDown, Home, Power, RotateCcw } from 'lucide-react';

type MachineState = 'IDLE' | 'HOMING' | 'RUNNING' | 'ERROR' | 'SHUTDOWN';
type CommandState = 'idle' | 'pending' | 'completed' | 'error';

interface MotionControlProps {
  frequency: number;
  onChange: (value: number) => void;
  isConnected: boolean;
  selectedPreset: string;
  onPresetChange: (preset: string) => void;
  onCommand: (command: string) => void;
  pendingCommands: Record<string, CommandState>;
  machineState: MachineState;
}

export default function MotionControl({
  frequency,
  onChange,
  isConnected,
  selectedPreset,
  onPresetChange,
  onCommand,
  pendingCommands,
  machineState
}: MotionControlProps) {
  const [targetFrequency, setTargetFrequency] = useState(frequency);
  const [gotoPosition, setGotoPosition] = useState(0);

  const presets = [
    { value: 'custom', label: 'Custom', hz: null },
    { value: 'low', label: 'Low Speed', hz: 30 },
    { value: 'medium', label: 'Medium Speed', hz: 60 },
    { value: 'high', label: 'High Speed', hz: 120 },
    { value: 'max', label: 'Max Speed', hz: 180 },
  ];

  const handleFrequencyChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value);
    setTargetFrequency(newValue);
  };

  const handleApplyFrequency = () => {
    onChange(targetFrequency);
    onPresetChange('custom');
  };

  const handleFrequencyInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value);
    setTargetFrequency(newValue);
    // Send immediately (responsive)
    onChange(newValue);
    onPresetChange('custom');
  };

  const handlePresetSelect = (e: React.ChangeEvent<HTMLSelectElement>) => {
    const presetValue = e.target.value;
    const selectedPresetData = presets.find(p => p.value === presetValue);

    if (selectedPresetData && selectedPresetData.hz !== null) {
      // Send SET_FREQ with the preset frequency value
      onChange(selectedPresetData.hz);
      onPresetChange(presetValue);
    } else if (presetValue === 'custom') {
      // Custom preset - don't send anything, just update selection
      onPresetChange('custom');
    }
  };

  const handleGoto = () => {
    onCommand(`GOTO:${gotoPosition}`);
  };

  const isCommandPending = (cmd: string) => pendingCommands[cmd] === 'pending';

  return (
    <div className="space-y-6">
      {/* Command Buttons */}
      <div className="grid grid-cols-2 md:grid-cols-3 gap-3">
        <button
          onClick={() => onCommand('HOME')}
          disabled={!isConnected || isCommandPending('HOME') || machineState === 'HOMING'}
          className="flex items-center justify-center gap-2 px-4 py-3 bg-blue-600 hover:bg-blue-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold"
        >
          <Home className="w-5 h-5" />
          {isCommandPending('HOME') ? 'Homing...' : 'HOME'}
        </button>

        <button
          onClick={() => onCommand('START')}
          disabled={!isConnected || isCommandPending('START') || machineState === 'RUNNING'}
          className="flex items-center justify-center gap-2 px-4 py-3 bg-green-600 hover:bg-green-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold"
        >
          <Play className="w-5 h-5" />
          {isCommandPending('START') ? 'Starting...' : 'START'}
        </button>

        <button
          onClick={() => onCommand('STOP')}
          disabled={!isConnected || isCommandPending('STOP')}
          className="flex items-center justify-center gap-2 px-4 py-3 bg-yellow-600 hover:bg-yellow-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold"
        >
          <Square className="w-5 h-5" />
          {isCommandPending('STOP') ? 'Stopping...' : 'STOP'}
        </button>

        <button
          onClick={() => onCommand('HARD_RESET')}
          disabled={!isConnected || isCommandPending('HARD_RESET')}
          className="flex items-center justify-center gap-2 px-4 py-3 bg-red-600 hover:bg-red-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold border-2 border-red-400"
        >
          <RotateCcw className="w-5 h-5" />
          HARD RESET
        </button>

        <button
          onClick={() => onCommand('SHUTDOWN')}
          disabled={!isConnected || isCommandPending('SHUTDOWN')}
          className="flex items-center justify-center gap-2 px-4 py-3 bg-red-800 hover:bg-red-900 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold border-2 border-red-600"
        >
          <Power className="w-5 h-5" />
          SHUTDOWN
        </button>
      </div>

      {/* GOTO Command */}
      <div className="bg-slate-700 rounded-lg p-4">
        <label className="block text-sm font-semibold text-slate-300 mb-2">GOTO Position</label>
        <div className="flex gap-2">
          <input
            type="number"
            min="0"
            max="1000"
            step="0.1"
            value={gotoPosition}
            onChange={(e) => setGotoPosition(parseFloat(e.target.value))}
            disabled={!isConnected}
            className="flex-1 px-4 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-blue-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono"
          />
          <span className="px-3 py-2 bg-slate-800 rounded-lg text-slate-400">mm</span>
          <button
            onClick={handleGoto}
            disabled={!isConnected || isCommandPending('GOTO')}
            className="px-6 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold"
          >
            {isCommandPending('GOTO') ? 'Moving...' : 'Send'}
          </button>
        </div>
      </div>

      {/* Frequency Control */}
      <div className="bg-slate-700 rounded-lg p-4 space-y-3">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Gauge className="w-5 h-5 text-purple-400" />
            <label className="font-semibold">Frequency Control</label>
          </div>
          <div className="flex items-baseline gap-2">
            <span className="text-2xl font-mono font-bold">{frequency.toFixed(1)}</span>
            <span className="text-slate-400">Hz</span>
          </div>
        </div>

        <div>
          <label className="block text-sm text-slate-400 mb-2">Preset</label>
          <div className="relative">
            <select
              value={selectedPreset}
              onChange={handlePresetSelect}
              disabled={!isConnected}
              className="w-full px-4 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-purple-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed appearance-none cursor-pointer"
            >
              {presets.map((preset) => (
                <option key={preset.value} value={preset.value}>
                  {preset.label} {preset.hz ? `(${preset.hz} Hz)` : ''}
                </option>
              ))}
            </select>
            <ChevronDown className="absolute right-3 top-1/2 -translate-y-1/2 w-5 h-5 text-slate-400 pointer-events-none" />
          </div>
        </div>

        <div>
          <label className="block text-sm text-slate-400 mb-2">Target Frequency (Auto-Send)</label>
          <div className="flex gap-2">
            <input
              type="number"
              min="0"
              max="200"
              step="0.1"
              value={targetFrequency}
              onChange={handleFrequencyInputChange}
              disabled={!isConnected}
              className="flex-1 px-4 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-purple-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono"
            />
            <span className="px-3 py-2 bg-slate-800 rounded-lg text-slate-400">Hz</span>
          </div>
          <div className="mt-2 text-xs text-slate-400">Changes sent automatically</div>
        </div>
      </div>

    </div>
  );
}
