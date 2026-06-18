import { useState } from 'react';
import { Gauge, Play, Square, ChevronDown, Home, Weight, Save, Trash2, BookMarked, Layers } from 'lucide-react';
import type { CustomPresets } from '../hooks/useMachineController';

type MachineState = 'IDLE' | 'HOMING' | 'RUNNING' | 'ERROR' | 'SHUTDOWN';
type CommandState = 'idle' | 'pending' | 'completed' | 'error';

interface MotionControlProps {
  frequency: number;
  onChange: (value: number) => void;                          // frequency
  forceTarget: number;
  forceTargets: number[];                                     // per-cell forces (4)
  onForceChange: (value: number, sensor?: number) => void;    // sensor undefined = global, 1-4 = per-cell
  isConnected: boolean;
  onCommand: (command: string) => void;
  pendingCommands: Record<string, CommandState>;
  machineState: MachineState;
  customPresets: CustomPresets;
  onSavePreset: (name: string, frequency: number, force: number, forces?: number[]) => void;
  onDeletePreset: (name: string) => void;
  advanced: boolean;
  selectedSensors: number[];                                  // indices 0-3 selected (right panel or here)
  onToggleSensor: (idx: number) => void;
}

export default function MotionControl({
  frequency,
  onChange,
  forceTarget,
  forceTargets,
  onForceChange,
  isConnected,
  onCommand,
  pendingCommands,
  machineState,
  customPresets,
  onSavePreset,
  onDeletePreset,
  advanced,
  selectedSensors,
  onToggleSensor
}: MotionControlProps) {
  const [targetFrequency, setTargetFrequency] = useState(frequency);
  const [targetForce, setTargetForce] = useState(forceTarget);
  const [perCellForce, setPerCellForce] = useState(0);
  const [presetName, setPresetName] = useState('');
  const [selectedPresetName, setSelectedPresetName] = useState('');

  const presetNames = Object.keys(customPresets);
  const selectedPreset = selectedPresetName ? customPresets[selectedPresetName] : null;

  const isCommandPending = (cmd: string) => pendingCommands[cmd] === 'pending';

  // --- Frequency / Force inputs (auto-send) ----------------------------

  const handleFrequencyInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const v = parseFloat(e.target.value);
    setTargetFrequency(v);
    if (!Number.isNaN(v)) onChange(v);
  };

  const handleGlobalForceChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const v = parseFloat(e.target.value);
    setTargetForce(v);
    if (!Number.isNaN(v)) onForceChange(v);  // global (all 4)
  };

  const applyPerCellForce = () => {
    if (Number.isNaN(perCellForce) || selectedSensors.length === 0) return;
    // sensors are 0-indexed in the UI, 1-4 on the wire
    selectedSensors.forEach((idx) => onForceChange(perCellForce, idx + 1));
  };

  // --- Presets ---------------------------------------------------------

  const handleSavePreset = () => {
    const name = presetName.trim();
    if (!name) return;
    const freq = Number.isNaN(targetFrequency) ? 0 : targetFrequency;
    const force = Number.isNaN(targetForce) ? 0 : targetForce;
    // In advanced mode, also persist the per-cell forces
    const forces = advanced ? [...forceTargets].slice(0, 4) : undefined;
    onSavePreset(name, freq, force, forces);
    setPresetName('');
  };

  const callFrequency = () => {
    if (!selectedPreset) return;
    setTargetFrequency(selectedPreset.frequency);
    onChange(selectedPreset.frequency);
  };

  const callForce = () => {
    if (!selectedPreset) return;
    if (advanced && selectedPreset.forces && selectedPreset.forces.length === 4) {
      // Per-cell forces -> send each sensor
      selectedPreset.forces.forEach((f, i) => onForceChange(f, i + 1));
    } else {
      setTargetForce(selectedPreset.force);
      onForceChange(selectedPreset.force);  // global
    }
  };

  const callGlobal = () => {
    callFrequency();
    callForce();
  };

  // "1 N / 3 N / 0.25 N / 12 N" - per-cell forces (falls back to global force x4)
  const formatPresetForces = (p: { force: number; forces?: number[] }) => {
    const forces = p.forces && p.forces.length === 4
      ? p.forces
      : [p.force, p.force, p.force, p.force];
    return forces.map((f) => `${f} N`).join(' / ');
  };

  return (
    <div className="space-y-6">
      {/* Command Buttons */}
      <div className="grid grid-cols-3 gap-3">
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
      </div>

      {/* Presets (Frequency + Force) - on top */}
      <div className="bg-slate-700 rounded-lg p-4 space-y-3">
        <div className="flex items-center gap-2">
          <BookMarked className="w-5 h-5 text-amber-400" />
          <label className="font-semibold">Presets (Frequency + Force)</label>
        </div>

        {/* Select a saved preset */}
        <div className="relative">
          <select
            value={selectedPresetName}
            onChange={(e) => setSelectedPresetName(e.target.value)}
            className="w-full px-4 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-amber-500 focus:outline-none appearance-none cursor-pointer"
          >
            <option value="">— Select a preset —</option>
            {presetNames.map((name) => (
              <option key={name} value={name}>
                {name} ({customPresets[name].frequency} Hz / {formatPresetForces(customPresets[name])})
              </option>
            ))}
          </select>
          <ChevronDown className="absolute right-3 top-1/2 -translate-y-1/2 w-5 h-5 text-slate-400 pointer-events-none" />
        </div>

        {/* Three call buttons */}
        <div className="grid grid-cols-3 gap-2">
          <button
            onClick={callFrequency}
            disabled={!isConnected || !selectedPreset}
            className="px-3 py-2 bg-purple-600 hover:bg-purple-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors text-sm font-semibold"
          >
            Call Frequency
          </button>
          <button
            onClick={callForce}
            disabled={!isConnected || !selectedPreset}
            className="px-3 py-2 bg-emerald-600 hover:bg-emerald-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors text-sm font-semibold"
          >
            Call Force
          </button>
          <button
            onClick={callGlobal}
            disabled={!isConnected || !selectedPreset}
            className="px-3 py-2 bg-sky-600 hover:bg-sky-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors text-sm font-semibold"
          >
            Call Global
          </button>
        </div>

        {/* Save current as a preset + delete selected */}
        <div className="flex gap-2">
          <input
            type="text"
            value={presetName}
            onChange={(e) => setPresetName(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter') handleSavePreset(); }}
            placeholder={`Save current (${Number.isNaN(targetFrequency) ? 0 : targetFrequency} Hz / ${Number.isNaN(targetForce) ? 0 : targetForce} N)`}
            className="flex-1 px-4 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-amber-500 focus:outline-none text-sm"
          />
          <button
            onClick={handleSavePreset}
            disabled={!presetName.trim()}
            className="flex items-center gap-2 px-4 py-2 bg-amber-600 hover:bg-amber-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold"
          >
            <Save className="w-4 h-4" />
            Save
          </button>
          <button
            onClick={() => { if (selectedPresetName) { onDeletePreset(selectedPresetName); setSelectedPresetName(''); } }}
            disabled={!selectedPresetName}
            title="Delete selected preset"
            className="flex items-center justify-center p-2 bg-red-700 hover:bg-red-800 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
        {advanced && (
          <div className="text-xs text-slate-400">Advanced: presets also store per-cell forces.</div>
        )}
      </div>

      {/* Frequency + Force side by side */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* Frequency */}
        <div className="bg-slate-700 rounded-lg p-4 space-y-3">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Gauge className="w-5 h-5 text-purple-400" />
              <label className="font-semibold">Frequency</label>
            </div>
            <div className="flex items-baseline gap-1">
              <span className="text-xl font-mono font-bold">{frequency.toFixed(1)}</span>
              <span className="text-slate-400 text-sm">Hz</span>
            </div>
          </div>
          <div className="flex gap-2">
            <input
              type="number"
              min="0"
              max="200"
              step="0.1"
              value={Number.isNaN(targetFrequency) ? '' : targetFrequency}
              onChange={handleFrequencyInputChange}
              disabled={!isConnected}
              className="flex-1 min-w-0 px-3 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-purple-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono"
            />
            <span className="px-3 py-2 bg-slate-800 rounded-lg text-slate-400">Hz</span>
          </div>
          <div className="text-xs text-slate-400">Auto-sent</div>
        </div>

        {/* Force (global - all 4 cells) */}
        <div className="bg-slate-700 rounded-lg p-4 space-y-3">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Weight className="w-5 h-5 text-emerald-400" />
              <label className="font-semibold">Force (Global)</label>
            </div>
            <div className="flex items-baseline gap-1">
              <span className="text-xl font-mono font-bold">{(Number.isNaN(targetForce) ? 0 : targetForce).toFixed(1)}</span>
              <span className="text-slate-400 text-sm">N</span>
            </div>
          </div>
          <div className="flex gap-2">
            <input
              type="number"
              min="0"
              step="0.1"
              value={Number.isNaN(targetForce) ? '' : targetForce}
              onChange={handleGlobalForceChange}
              disabled={!isConnected}
              className="flex-1 min-w-0 px-3 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-emerald-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono"
            />
            <span className="px-3 py-2 bg-slate-800 rounded-lg text-slate-400">N</span>
          </div>
          <div className="text-xs text-slate-400">Applies to all 4 cells</div>
        </div>
      </div>

      {/* Per-cell force (advanced only) */}
      {advanced && (
        <div className="bg-slate-700 rounded-lg p-4 space-y-3 border border-emerald-700/50">
          <div className="flex items-center gap-2">
            <Layers className="w-5 h-5 text-emerald-400" />
            <label className="font-semibold">Per-cell Force</label>
          </div>

          {/* Current per-cell targets - click to select (same as right panel) */}
          <div className="grid grid-cols-4 gap-2">
            {[0, 1, 2, 3].map((i) => (
              <button
                key={i}
                onClick={() => onToggleSensor(i)}
                className={`rounded-lg p-2 text-center transition-all ${selectedSensors.includes(i) ? 'bg-emerald-900/50 ring-1 ring-emerald-500' : 'bg-slate-800 hover:bg-slate-700'}`}
              >
                <div className="text-xs text-slate-400">Cell {i + 1}</div>
                <div className="font-mono font-bold">{(forceTargets?.[i] ?? 0).toFixed(1)}<span className="text-xs text-slate-400"> N</span></div>
              </button>
            ))}
          </div>

          <div>
            <label className="block text-sm text-slate-400 mb-2">
              {selectedSensors.length > 0
                ? `Set force on cell(s) ${selectedSensors.map((i) => i + 1).join(', ')}`
                : 'Select cell(s) above'}
            </label>
            <div className="flex gap-2">
              <input
                type="number"
                min="0"
                step="0.1"
                value={Number.isNaN(perCellForce) ? '' : perCellForce}
                onChange={(e) => setPerCellForce(parseFloat(e.target.value))}
                onKeyDown={(e) => { if (e.key === 'Enter') applyPerCellForce(); }}
                disabled={!isConnected || selectedSensors.length === 0}
                className="flex-1 min-w-0 px-3 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-emerald-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono"
              />
              <span className="px-3 py-2 bg-slate-800 rounded-lg text-slate-400">N</span>
              <button
                onClick={applyPerCellForce}
                disabled={!isConnected || selectedSensors.length === 0 || Number.isNaN(perCellForce)}
                className="px-4 py-2 bg-emerald-600 hover:bg-emerald-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold text-sm"
              >
                Apply
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
