/**
 * ===========================================================================
 * FILE: Settings.tsx
 * ROLE:
 *   Machine calibration & configuration settings: motor identification,
 *   resistance/gain selection for INA125 sensors (per-board control).
 *
 * RESPONSIBILITIES:
 *   - Motor LED blink identification (user clicks button, motor blinks, confirms ID)
 *   - Switch between 30 Ω (high sensitivity) and 90 Ω (wide range) calibrations
 *   - Separate control for Board 0 (cells 0-1, relay D4) and Board 1 (cells 2-3, relay D5)
 *
 * MAINTAINER NOTES:
 *   - All commands are sent via the controller hook (blinkMotor, setResistance)
 * ===========================================================================
 */

import { Zap, ChevronLeft, Activity } from 'lucide-react';
import { useState, useEffect } from 'react';

interface SettingsProps {
  isConnected: boolean;
  onClose: () => void;
  onBlinkMotor: (motorId: number, durationMs?: number) => void;
  onSetResistance: (resistanceOhm: number, boardId?: number) => void;
  currentResistances?: { board0: number; board1: number };
  onSetForceSampleCount?: (sampleCount: number) => Promise<void>;
  currentForceSampleCount?: number;
}

export default function Settings({
  isConnected,
  onClose,
  onBlinkMotor,
  onSetResistance,
  currentResistances = { board0: 30, board1: 30 },
  onSetForceSampleCount,
  currentForceSampleCount = 8192,
}: SettingsProps) {
  const [forceSampleCount, setForceSampleCount] = useState(currentForceSampleCount);
  const [forceConfigSyncing, setForceConfigSyncing] = useState(false);
  const [forceConfigStatus, setForceConfigStatus] = useState<'synced' | 'syncing' | 'error'>('synced');
  const motors = [
    { id: 0, label: 'Motor 0 (Table 0)', color: 'from-blue-500 to-blue-600' },
    { id: 1, label: 'Motor 1 (Table 1)', color: 'from-purple-500 to-purple-600' },
    { id: 2, label: 'Motor 2 (Table 2)', color: 'from-pink-500 to-pink-600' },
    { id: 3, label: 'Motor 3 (Table 3)', color: 'from-orange-500 to-orange-600' },
  ];

  const boards = [
    { id: 0, label: 'Board 0 (Cells 0-1)', relay: 'D4', cells: '0, 1' },
    { id: 1, label: 'Board 1 (Cells 2-3)', relay: 'D5', cells: '2, 3' },
  ];

  const resistanceOptions = [
    { value: 30, label: '30 Ω (High Sensitivity)', description: 'For light loads, precise measurement' },
    { value: 90, label: '90 Ω (Wide Range)', description: 'For heavy loads, wider dynamic range' },
  ];

  const handleBlinkMotor = (motorId: number) => {
    if (!isConnected) {
      alert('Machine not connected');
      return;
    }
    onBlinkMotor(motorId, 1000); // Blink for 1 second
  };

  const handleSetResistance = (boardId: number, resistance: number) => {
    if (!isConnected) {
      alert('Machine not connected');
      return;
    }
    onSetResistance(resistance, boardId);
  };

  const handleApplyForceSampleCount = async () => {
    if (!isConnected || !onSetForceSampleCount) {
      alert('Machine not connected');
      return;
    }

    // Validate range
    if (forceSampleCount < 256 || forceSampleCount > 12000) {
      setForceConfigStatus('error');
      alert('Sample count must be between 256 and 12000');
      return;
    }

    setForceConfigSyncing(true);
    setForceConfigStatus('syncing');

    try {
      await onSetForceSampleCount(forceSampleCount);
      setForceConfigStatus('synced');
    } catch (err) {
      setForceConfigStatus('error');
      alert(`Failed to apply force sample count: ${err}`);
    } finally {
      setForceConfigSyncing(false);
    }
  };

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
      <div className="bg-slate-800 rounded-lg max-w-3xl w-full max-h-[90vh] overflow-y-auto">
        {/* Header */}
        <div className="sticky top-0 bg-slate-900 border-b border-slate-700 p-6 flex items-center justify-between">
          <h1 className="text-2xl font-bold text-white flex items-center gap-3">
            <Zap size={24} className="text-yellow-500" />
            Machine Settings
          </h1>
          <button
            onClick={onClose}
            className="p-2 hover:bg-slate-700 rounded-lg transition-colors"
            title="Close settings"
          >
            <ChevronLeft size={24} />
          </button>
        </div>

        <div className="p-6 space-y-8">
          {/* Motor Identification Section */}
          <div className="border border-slate-700 rounded-lg p-6">
            <h2 className="text-xl font-semibold text-white mb-4 flex items-center gap-2">
              <div className="w-3 h-3 bg-blue-500 rounded-full"></div>
              Motor Identification
            </h2>
            <p className="text-slate-400 text-sm mb-6">
              Click a button to blink the motor's LED. Observe which physical motor blinks to confirm the ID mapping.
              Normally motors don't move between positions, but this helps if Dynamixel IDs get shuffled.
            </p>

            <div className="grid grid-cols-2 gap-4">
              {motors.map((motor) => (
                <button
                  key={motor.id}
                  onClick={() => handleBlinkMotor(motor.id)}
                  disabled={!isConnected}
                  className={`p-4 rounded-lg font-semibold transition-all transform hover:scale-105 disabled:opacity-50 disabled:cursor-not-allowed
                    ${!isConnected
                      ? 'bg-slate-600 text-slate-400'
                      : `bg-gradient-to-r ${motor.color} text-white hover:shadow-lg`
                    }`}
                >
                  <div className="text-sm font-bold">{motor.label}</div>
                  <div className="text-xs opacity-90 mt-1">Click to blink LED</div>
                </button>
              ))}
            </div>
          </div>

          {/* Resistance Selection Section - Per Board */}
          <div className="border border-slate-700 rounded-lg p-6">
            <h2 className="text-xl font-semibold text-white mb-4 flex items-center gap-2">
              <div className="w-3 h-3 bg-emerald-500 rounded-full"></div>
              INA125 Gain Selection (Per Board)
            </h2>
            <p className="text-slate-400 text-sm mb-6">
              Switch the feedback resistor via relay control to change the INA125 gain independently for each board.
              Board 0 controls cells 0-1, Board 1 controls cells 2-3.
            </p>

            <div className="space-y-8">
              {boards.map((board) => {
                const currentResistance = board.id === 0 ? currentResistances.board0 : currentResistances.board1;
                return (
                  <div key={board.id} className="bg-slate-900 rounded-lg p-4 border border-slate-700">
                    <div className="mb-4">
                      <h3 className="text-lg font-semibold text-white">{board.label}</h3>
                      <p className="text-xs text-slate-400 mt-1">
                        Relay: <span className="font-mono bg-slate-800 px-2 py-1 rounded">{board.relay}</span> |
                        Cells: {board.cells}
                      </p>
                    </div>

                    <div className="space-y-2">
                      {resistanceOptions.map((option) => (
                        <button
                          key={option.value}
                          onClick={() => handleSetResistance(board.id, option.value)}
                          disabled={!isConnected}
                          className={`w-full p-3 rounded-lg transition-all text-left text-sm ${
                            currentResistance === option.value
                              ? 'ring-2 ring-emerald-500 bg-emerald-900 bg-opacity-30'
                              : 'hover:bg-slate-800'
                          } ${!isConnected ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}`}
                        >
                          <div className="flex items-center justify-between">
                            <div>
                              <div className="font-semibold text-white">{option.label}</div>
                              <div className="text-xs text-slate-400">{option.description}</div>
                            </div>
                            <div className="flex items-center gap-2">
                              {currentResistance === option.value && (
                                <div className="px-2 py-1 bg-emerald-600 rounded text-xs font-semibold text-white">
                                  Active
                                </div>
                              )}
                              <div className="font-bold text-slate-300">{option.value}Ω</div>
                            </div>
                          </div>
                        </button>
                      ))}
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Force Acquisition Configuration */}
          <div className="border border-slate-700 rounded-lg p-6">
            <h2 className="text-xl font-semibold text-white mb-4 flex items-center gap-2">
              <Activity size={20} className="text-cyan-500" />
              Force Acquisition (DMA Freerun)
            </h2>
            <p className="text-slate-400 text-sm mb-6">
              Configure the number of samples per burst for load cell measurement. Higher values improve signal quality but increase latency.
              Range: 256–12000 samples (default: 8192 @ 87 kSPS = 94 ms per burst).
            </p>

            <div className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-white mb-2">
                  Sample Count per Burst
                </label>
                <div className="flex items-center gap-3">
                  <input
                    type="number"
                    min={256}
                    max={12000}
                    step={256}
                    value={forceSampleCount}
                    onChange={(e) => setForceSampleCount(Math.max(256, Math.min(12000, parseInt(e.target.value) || 256)))}
                    disabled={!isConnected || forceConfigSyncing}
                    className="flex-1 px-3 py-2 bg-slate-700 border border-slate-600 rounded-lg text-white text-sm focus:outline-none focus:ring-2 focus:ring-cyan-500"
                  />
                  <button
                    onClick={handleApplyForceSampleCount}
                    disabled={!isConnected || forceConfigSyncing}
                    className={`px-4 py-2 rounded-lg font-semibold text-sm transition-all ${
                      !isConnected || forceConfigSyncing
                        ? 'bg-slate-600 text-slate-400 cursor-not-allowed'
                        : 'bg-cyan-600 text-white hover:bg-cyan-700'
                    }`}
                  >
                    {forceConfigSyncing ? 'Applying...' : 'Apply'}
                  </button>
                </div>
              </div>

              <div className="grid grid-cols-2 gap-4 pt-4 border-t border-slate-700">
                <div>
                  <div className="text-xs text-slate-500 uppercase tracking-wider">Sample Rate</div>
                  <div className="text-lg font-semibold text-cyan-400">87 kSPS</div>
                </div>
                <div>
                  <div className="text-xs text-slate-500 uppercase tracking-wider">Burst Duration</div>
                  <div className="text-lg font-semibold text-cyan-400">
                    {((forceSampleCount / 87) / 1000).toFixed(1)} ms
                  </div>
                </div>
                <div>
                  <div className="text-xs text-slate-500 uppercase tracking-wider">SNR Target</div>
                  <div className="text-lg font-semibold text-cyan-400">~80 dB</div>
                </div>
                <div>
                  <div className="text-xs text-slate-500 uppercase tracking-wider">Config Status</div>
                  <div className={`text-lg font-semibold ${
                    forceConfigStatus === 'synced' ? 'text-emerald-400' :
                    forceConfigStatus === 'syncing' ? 'text-yellow-400' :
                    'text-red-400'
                  }`}>
                    {forceConfigStatus === 'synced' ? '✓ Synced' :
                     forceConfigStatus === 'syncing' ? '◌ Syncing' :
                     '✗ Error'}
                  </div>
                </div>
              </div>

              <div className="bg-slate-900 rounded-lg p-3 text-xs text-slate-400 border border-slate-700">
                <div className="font-mono">
                  Current: {currentForceSampleCount} samples
                  {currentForceSampleCount !== forceSampleCount && (
                    <span className="text-yellow-500"> → Pending: {forceSampleCount} samples</span>
                  )}
                </div>
              </div>
            </div>
          </div>

          {/* Status */}
          <div className="bg-slate-900 rounded-lg p-4 text-sm text-slate-300 border border-slate-700">
            <strong>Status:</strong> {isConnected ? '✓ Connected and ready' : '✗ Not connected'}
          </div>
        </div>
      </div>
    </div>
  );
}
