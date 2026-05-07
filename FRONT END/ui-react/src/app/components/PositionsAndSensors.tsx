import { TrendingUp, Zap } from 'lucide-react';

interface PositionsAndSensorsProps {
  positions: number[];  // 4 positions
  sensors: number[];    // 4 force sensors
}

export default function PositionsAndSensors({ positions = [0, 0, 0, 0], sensors = [0, 0, 0, 0] }: PositionsAndSensorsProps) {
  // Ensure we have 4 values
  const pos = [...positions].slice(0, 4).concat(Array(4).fill(0)).slice(0, 4);
  const sens = [...sensors].slice(0, 4).concat(Array(4).fill(0)).slice(0, 4);

  return (
    <div className="space-y-4">
      {/* Table Positions */}
      <div className="bg-slate-700 rounded-lg p-4">
        <div className="flex items-center gap-2 mb-3">
          <TrendingUp className="w-5 h-5 text-blue-400" />
          <h3 className="font-semibold text-slate-200">Table Positions</h3>
        </div>
        <div className="grid grid-cols-2 gap-3">
          {pos.map((position, idx) => (
            <div key={idx} className="bg-slate-800 rounded p-3">
              <div className="text-xs text-slate-400 mb-1">Table {idx + 1}</div>
              <div className="font-mono font-bold text-lg text-blue-400">
                {position.toFixed(1)}
                <span className="text-xs text-slate-400 ml-1">mm</span>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Force Sensors */}
      <div className="bg-slate-700 rounded-lg p-4">
        <div className="flex items-center gap-2 mb-3">
          <Zap className="w-5 h-5 text-yellow-400" />
          <h3 className="font-semibold text-slate-200">Force Sensors</h3>
        </div>
        <div className="grid grid-cols-2 gap-3">
          {sens.map((sensor, idx) => (
            <div key={idx} className="bg-slate-800 rounded p-3">
              <div className="text-xs text-slate-400 mb-1">Sensor {idx + 1}</div>
              <div className="font-mono font-bold text-lg text-yellow-400">
                {sensor.toFixed(2)}
                <span className="text-xs text-slate-400 ml-1">N</span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
