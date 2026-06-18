/**
 * ===========================================================================
 * FILE: Settings.tsx
 * ROLE:
 *   Machine calibration & configuration settings: motor identification,
 *   resistance/gain selection for INA125 sensors.
 *
 * RESPONSIBILITIES:
 *   - Motor LED blink identification (user clicks button, motor blinks, confirms ID)
 *   - Switch between 30 Ω (high sensitivity) and 90 Ω (wide range) calibrations
 *
 * MAINTAINER NOTES:
 *   - All commands are sent via the controller hook (blinkMotor, setResistance)
 * ===========================================================================
 */

import { Zap, ChevronLeft } from 'lucide-react';

interface SettingsProps {
  isConnected: boolean;
  onClose: () => void;
  onBlinkMotor: (motorId: number, durationMs?: number) => void;
  onSetResistance: (resistanceOhm: number) => void;
  currentResistance?: number;
}

export default function Settings({
  isConnected,
  onClose,
  onBlinkMotor,
  onSetResistance,
  currentResistance = 30,
}: SettingsProps) {
  const motors = [
    { id: 0, label: 'Motor 0 (Table 0)', color: 'from-blue-500 to-blue-600' },
    { id: 1, label: 'Motor 1 (Table 1)', color: 'from-purple-500 to-purple-600' },
    { id: 2, label: 'Motor 2 (Table 2)', color: 'from-pink-500 to-pink-600' },
    { id: 3, label: 'Motor 3 (Table 3)', color: 'from-orange-500 to-orange-600' },
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

  const handleSetResistance = (resistance: number) => {
    if (!isConnected) {
      alert('Machine not connected');
      return;
    }
    onSetResistance(resistance);
  };

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
      <div className="bg-slate-800 rounded-lg max-w-2xl w-full max-h-[90vh] overflow-y-auto">
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

          {/* Resistance Selection Section */}
          <div className="border border-slate-700 rounded-lg p-6">
            <h2 className="text-xl font-semibold text-white mb-4 flex items-center gap-2">
              <div className="w-3 h-3 bg-emerald-500 rounded-full"></div>
              INA125 Gain Selection
            </h2>
            <p className="text-slate-400 text-sm mb-6">
              Switch the feedback resistor via relay control to change the INA125 operational amplifier gain.
              This changes the calibration curve and sensitivity of the force sensors.
            </p>

            <div className="space-y-3">
              {resistanceOptions.map((option) => (
                <button
                  key={option.value}
                  onClick={() => handleSetResistance(option.value)}
                  disabled={!isConnected}
                  className={`w-full p-4 rounded-lg transition-all text-left ${
                    currentResistance === option.value
                      ? 'ring-2 ring-emerald-500 bg-slate-700'
                      : 'hover:bg-slate-700'
                  } ${!isConnected ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}`}
                >
                  <div className="flex items-center justify-between">
                    <div>
                      <div className="font-semibold text-white">{option.label}</div>
                      <div className="text-sm text-slate-400">{option.description}</div>
                    </div>
                    <div className="flex items-center gap-3">
                      {currentResistance === option.value && (
                        <div className="px-3 py-1 bg-emerald-600 rounded-full text-sm font-semibold text-white">
                          Active
                        </div>
                      )}
                      <div className="text-lg font-bold text-slate-300">{option.value}Ω</div>
                    </div>
                  </div>
                </button>
              ))}
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
