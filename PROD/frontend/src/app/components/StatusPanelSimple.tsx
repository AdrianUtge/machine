import { useEffect, useState } from 'react';
import { AlertCircle, Signal, Clock, Gauge, Weight, Unlock, Lock, Wifi } from 'lucide-react';
import type { MachineState, CustomPresets } from '../hooks/useMachineController';

interface StatusPanelSimpleProps {
  isConnected: boolean;
  machineState: MachineState | null;
  customPresets: CustomPresets;
  latencyMs?: number | null;  // latence aller-retour du poll (ms), -1 = lien coupé
  onTorqueOff?: () => void;
  onTorqueOn?: () => void;
}

// Couleur/libellé du badge de latence selon la qualité du lien.
function latencyStyle(ms: number | null | undefined): { text: string; cls: string } {
  if (ms == null) return { text: '— ms', cls: 'text-slate-400 bg-slate-700/40 border-slate-600' };
  if (ms < 0) return { text: 'no link', cls: 'text-red-400 bg-red-900/30 border-red-700' };
  if (ms < 150) return { text: `${ms} ms`, cls: 'text-green-400 bg-green-900/30 border-green-700' };
  if (ms < 400) return { text: `${ms} ms`, cls: 'text-amber-400 bg-amber-900/30 border-amber-700' };
  return { text: `${ms} ms`, cls: 'text-red-400 bg-red-900/30 border-red-700' };
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
  latencyMs,
  onTorqueOff,
  onTorqueOn,
}: StatusPanelSimpleProps) {
  const [now, setNow] = useState(Date.now());
  const [torqueLocked, setTorqueLocked] = useState(true);

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
        {isConnected && (() => {
          const { text, cls } = latencyStyle(latencyMs);
          return (
            <span
              className={`ml-auto flex items-center gap-1 px-2 py-0.5 rounded-md border text-xs font-mono font-semibold ${cls}`}
              title="Latence aller-retour du lien (frontend → backend → ESP → OpenRB)"
            >
              <Wifi className="w-3.5 h-3.5" />
              {text}
            </span>
          );
        })()}
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

      {/* Slave (OpenRB / Dynamixel) connection indicator */}
      {(() => {
        const online = machineState.slave_status === 'ONLINE';
        return (
          <div className="border-t border-slate-700 pt-2">
            <div className={`flex items-center justify-between gap-2 px-3 py-2 rounded-lg ${online ? 'bg-green-900/30 border border-green-700' : 'bg-red-900/30 border border-red-700'}`}>
              <span className="flex items-center gap-2 text-sm">
                <Signal className={`w-4 h-4 ${online ? 'text-green-400' : 'text-red-400'}`} />
                <span className="text-slate-300">Slave</span>
              </span>
              <span className="flex items-center gap-2">
                <span className={`w-2 h-2 rounded-full ${online ? 'bg-green-500' : 'bg-red-500'}`}></span>
                <span className={`font-semibold text-sm ${online ? 'text-green-400' : 'text-red-400'}`}>
                  {online ? 'Connected' : 'Disconnected'}
                </span>
              </span>
            </div>
          </div>
        );
      })()}

      {/* Torque unlock / lock — pour positionner les moteurs à la main */}
      {isConnected && machineState?.slave_status === 'ONLINE' && (
        <div className="border-t border-slate-700 pt-2">
          {torqueLocked ? (
            <button
              onClick={() => { onTorqueOff?.(); setTorqueLocked(false); }}
              className="w-full flex items-center justify-center gap-2 px-3 py-2 bg-amber-600 hover:bg-amber-500 rounded-lg text-sm font-semibold transition-colors"
              title="Désactive le couple sur tous les Dynamixels — rotation libre"
            >
              <Unlock className="w-4 h-4" />
              Unlock motors
            </button>
          ) : (
            <button
              onClick={() => { onTorqueOn?.(); setTorqueLocked(true); }}
              className="w-full flex items-center justify-center gap-2 px-3 py-2 bg-green-700 hover:bg-green-600 rounded-lg text-sm font-semibold transition-colors"
              title="Réactive le couple — les moteurs tiennent leur position"
            >
              <Lock className="w-4 h-4" />
              Lock motors
            </button>
          )}
        </div>
      )}

      {/* Resistance status (per board) */}
      <div className="border-t border-slate-700 pt-2 space-y-2 text-sm">
        <p className="text-xs text-slate-400 uppercase font-semibold">Resistance (Rg)</p>
        <div className="grid grid-cols-2 gap-2">
          <div className="bg-slate-900 rounded p-2">
            <div className="text-[10px] text-slate-500 mb-1">Board 0 (D4)</div>
            <div className="font-mono font-bold text-emerald-400">30 Ω</div>
            <div className="text-[10px] text-slate-500 mt-0.5">Cells 0-1</div>
          </div>
          <div className="bg-slate-900 rounded p-2">
            <div className="text-[10px] text-slate-500 mb-1">Board 1 (D5)</div>
            <div className="font-mono font-bold text-emerald-400">30 Ω</div>
            <div className="text-[10px] text-slate-500 mt-0.5">Cells 2-3</div>
          </div>
        </div>
      </div>

      {/* Cell voltages (raw, mV) — useful for calibrating */}
      {machineState.cell_volts_mv && (
        <div className="border-t border-slate-700 pt-2 space-y-2 text-sm">
          <p className="text-xs text-slate-400 uppercase font-semibold">Cell Voltage (mV)</p>
          <div className="grid grid-cols-4 gap-2">
            {machineState.cell_volts_mv.slice(0, 4).map((v, i) => (
              <div key={i} className="bg-slate-900 rounded p-2 text-center">
                <div className="text-[10px] text-slate-500 mb-0.5">C{i}</div>
                <div className="font-mono font-bold text-cyan-400 text-sm">{(v ?? 0).toFixed(0)}</div>
              </div>
            ))}
          </div>
        </div>
      )}

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
