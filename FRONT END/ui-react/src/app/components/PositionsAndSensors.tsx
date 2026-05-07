import { useState } from 'react';
import { TrendingUp, Zap, Move } from 'lucide-react';

interface PositionsAndSensorsProps {
  positions: number[];  // 4 positions
  sensors: number[];    // 4 force sensors
  isConnected: boolean;
  onGotoCommand?: (position: number) => void;
  onSensorSelect?: (sensorIdx: number) => void;
}

export default function PositionsAndSensors({
  positions = [0, 0, 0, 0],
  sensors = [0, 0, 0, 0],
  isConnected = false,
  onGotoCommand,
  onSensorSelect
}: PositionsAndSensorsProps) {
  const [selectedTableIdx, setSelectedTableIdx] = useState<number | null>(null);
  const [selectedSensorIdx, setSelectedSensorIdx] = useState<number | null>(null);
  const [gotoPosition, setGotoPosition] = useState(0);

  // Ensure we have 4 values
  const pos = [...positions].slice(0, 4).concat(Array(4).fill(0)).slice(0, 4);
  const sens = [...sensors].slice(0, 4).concat(Array(4).fill(0)).slice(0, 4);

  const handleTableClick = (idx: number) => {
    setSelectedTableIdx(idx);
    setGotoPosition(pos[idx]);
  };

  const handleSensorClick = (idx: number) => {
    setSelectedSensorIdx(idx);
    if (onSensorSelect) {
      onSensorSelect(idx);
    }
  };

  const handleGotoChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value) || 0;
    setGotoPosition(Math.min(newValue, 96)); // Max 96mm
  };

  const handleGotoSend = () => {
    if (onGotoCommand) {
      onGotoCommand(gotoPosition);
    }
  };

  return (
    <div className="space-y-4">
      {/* Table + Sensor Pairs */}
      <div className="space-y-3">
        {[0, 1, 2, 3].map((idx) => (
          <div key={idx} className="bg-slate-700 rounded-lg p-4 space-y-2">
            {/* Table */}
            <button
              onClick={() => handleTableClick(idx)}
              disabled={!isConnected}
              className={`w-full rounded p-3 transition-all text-left ${
                selectedTableIdx === idx
                  ? 'bg-blue-600 ring-2 ring-blue-400'
                  : 'bg-slate-800 hover:bg-slate-700'
              } disabled:opacity-50 disabled:cursor-not-allowed`}
            >
              <div className="flex items-center justify-between">
                <div>
                  <div className="text-xs text-slate-300 mb-1">
                    <TrendingUp className="w-4 h-4 inline mr-1" />
                    Table {idx + 1}
                  </div>
                  <div className="font-mono font-bold text-lg text-blue-400">
                    {pos[idx].toFixed(1)}
                    <span className="text-xs text-slate-400 ml-1">mm</span>
                  </div>
                </div>
              </div>
            </button>

            {/* Sensor - Clickable to show graph */}
            <button
              onClick={() => handleSensorClick(idx)}
              disabled={!isConnected}
              className={`w-full rounded p-3 transition-all text-left ${
                selectedSensorIdx === idx
                  ? 'bg-red-600 ring-2 ring-red-400'
                  : 'bg-slate-800 hover:bg-slate-700'
              } disabled:opacity-50 disabled:cursor-not-allowed`}
            >
              <div className="flex items-center justify-between">
                <div>
                  <div className="text-xs text-slate-300 mb-1">
                    <Zap className="w-4 h-4 inline mr-1" />
                    Sensor {idx + 1}
                  </div>
                  <div className="font-mono font-bold text-lg text-yellow-400">
                    {sens[idx].toFixed(2)}
                    <span className="text-xs text-slate-400 ml-1">N</span>
                  </div>
                </div>
              </div>
            </button>
          </div>
        ))}
      </div>

      {/* GOTO Position */}
      <div className="bg-slate-700 rounded-lg p-4">
        <div className="flex items-center gap-2 mb-3">
          <Move className="w-5 h-5 text-purple-400" />
          <h3 className="font-semibold text-slate-200">
            GOTO Position {selectedTableIdx !== null ? `(Table ${selectedTableIdx + 1})` : ''}
          </h3>
        </div>
        <div className="flex gap-2">
          <input
            type="number"
            min="0"
            max="96"
            step="0.1"
            value={gotoPosition}
            onChange={handleGotoChange}
            disabled={!isConnected}
            className="flex-1 px-4 py-2 bg-slate-600 rounded-lg border border-slate-500 focus:border-purple-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono"
          />
          <span className="px-3 py-2 bg-slate-800 rounded-lg text-slate-400">mm</span>
          <button
            onClick={handleGotoSend}
            disabled={!isConnected}
            className="px-6 py-2 bg-purple-600 hover:bg-purple-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded-lg transition-colors font-semibold"
          >
            Send
          </button>
        </div>
        <div className="mt-2 text-xs text-slate-400">
          Max travel: 96mm {selectedTableIdx !== null && `| Selected: Table ${selectedTableIdx + 1}`}
        </div>
      </div>

      {/* Info */}
      <div className="bg-slate-800 rounded-lg p-3 text-xs text-slate-400">
        💡 Click on a sensor to view its force graph over time
      </div>
    </div>
  );
}
