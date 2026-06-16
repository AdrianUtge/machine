import { useEffect, useState } from 'react';
import { AlertCircle, Signal, Clock, Gauge, Weight } from 'lucide-react';
import type { MachineState, CustomPresets } from '../hooks/useMachineController';

interface StatusPanelSimpleProps {
  isConnected: boolean;
  machineState: MachineState | null;
  customPresets: CustomPresets;
}

function formatDuration(ms: number): string {
  const totalSec = Math.max(0, Math.floor(ms / 1000));
  const h = Math.floor(totalSec / 3600);
  const m = Math.floor((totalSec % 3600) / 60);
  const s = totalSec % 60;
  const pad = (n: number) => n.toString().padStart(2, '0');
  return h > 0 ? `${h}:${pad(m)}:${pad(s)}` : `${pad(m)}:${pad(s)}`;
}

// Does the current frequency/forces match a saved preset? -> name, else null
function matchPreset(machineState: MachineState, presets: CustomPresets): string | null {
  const freq = machineState.frequency_hz ?? 0;
  const forces = machineState.force_targets ?? [0, 0, 0, 0];
  const eq = (a: number, b: number) => Math.abs(a - b) < 0.01;

  for (const [name, p] of Object.entries(presets)) {
    const pForces = p.forces && p.forces.length === 4
      ? p.forces
      : [p.force, p.force, p.force, p.force];
    if (eq(p.frequency, freq) && pForces.every((f, i) => eq(f, forces[i] ?? 0))) {
      return name;
    }
  }
  return null;
}

export default function StatusPanelSimple({
  isConnected,
  machineState,
  customPresets,
}: StatusPanelSimpleProps) {
  const [now, setNow] = useState(Date.now());

  // Tick every second to keep the runtime live
  useEffect(() => {
    if (!isConnected) return;
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, [isConnected]);

  if (!machineState) {
    return (
      <div className="bg-slate-800 rounded-lg p-6 flex-1">
        <p className="text-slate-400">Loading machine state...</p>
      </div>
    );
  }

  const hasError = machineState.errors !== 'NONE' && machineState.errors !== 'None';

  // Runtime is computed from the cycle start time stored by the node (absolute,
  // so it survives a reload/reconnect instead of resetting to zero).
  const cycleStart = machineState.cycle_start ?? null;
  const runtime = isConnected && cycleStart ? formatDuration(now - cycleStart) : '--:--';

  // Preset: matching saved preset name, or "Unsaved" if the current settings
  // don't correspond to any saved preset.
  const matchedPreset = matchPreset(machineState, customPresets);

  const Row = ({ label, value }: { label: string; value: React.ReactNode }) => (
    <div className="flex justify-between">
      <span className="text-slate-400">{label}:</span>
      <span className="font-semibold font-mono">{value}</span>
    </div>
  );

  return (
    <div className="bg-slate-800 rounded-lg p-6 space-y-4 flex-1">
      <h3 className="text-lg font-semibold flex items-center gap-2">
        <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`}></div>
        Status
      </h3>

      {/* General */}
      <div className="space-y-2 text-sm">
        <div className="flex justify-between">
          <span className="text-slate-400 flex items-center gap-1"><Clock className="w-3.5 h-3.5" /> Runtime:</span>
          <span className="font-semibold font-mono">{runtime}</span>
        </div>
        <Row label="State" value={machineState.machine_status} />
        <div className="flex justify-between">
          <span className="text-slate-400">Preset:</span>
          {matchedPreset ? (
            <span className="font-semibold font-mono">{matchedPreset}</span>
          ) : (
            <span className="font-semibold font-mono text-amber-400">Unsaved</span>
          )}
        </div>
      </div>

      {/* Motion */}
      <div className="border-t border-slate-700 pt-2 space-y-2 text-sm">
        <div className="flex justify-between">
          <span className="text-slate-400 flex items-center gap-1"><Gauge className="w-3.5 h-3.5" /> Frequency:</span>
          <span className="font-semibold font-mono">{machineState.frequency_hz?.toFixed(2) ?? 'N/A'} Hz</span>
        </div>
        <Row label="Speed" value={`${machineState.t_speed_percent}%`} />
        <div className="flex justify-between">
          <span className="text-slate-400 flex items-center gap-1"><Weight className="w-3.5 h-3.5" /> Force target:</span>
          <span className="font-semibold font-mono">{(machineState.force_target ?? 0).toFixed(1)} N</span>
        </div>
      </div>

      {/* Motor current */}
      {machineState.motor_current && (
        <div className="border-t border-slate-700 pt-2 text-sm">
          <Row label="Motor current" value={machineState.motor_current} />
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
    </div>
  );
}
