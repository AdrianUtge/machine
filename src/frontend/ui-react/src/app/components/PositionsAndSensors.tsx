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
    <div className="space-y-4 h-full flex flex-col">
      {/* Table + Sensor Pairs - 2x2 Grid */}
      <div className="grid grid-cols-2 gap-3 flex-1">
        {[0, 1, 2, 3].map((idx) => (
          <div key={idx} className="bg-slate-700 rounded-lg p-3 space-y-2 flex flex-col">
            {/* Table */}
            <button
              onClick={() => handleTableClick(idx)}
              disabled={!isConnected}
              className={`rounded p-2 transition-all text-left flex-1 ${
                selectedTableIdx === idx
                  ? 'bg-blue-600 ring-2 ring-blue-400'
                  : 'bg-slate-800 hover:bg-slate-700'
              } disabled:opacity-50 disabled:cursor-not-allowed`}
            >
              <div>
                <div className="text-xs text-slate-300 mb-1">
                  <TrendingUp className="w-3 h-3 inline mr-1" />
                  Table {idx + 1}
                </div>
                <div className="font-mono font-bold text-base text-blue-400">
                  {pos[idx].toFixed(1)}
                  <span className="text-xs text-slate-400 ml-1">mm</span>
                </div>
              </div>
            </button>

            {/* Sensor - Clickable to show graph */}
            <button
              onClick={() => handleSensorClick(idx)}
              disabled={!isConnected}
              className={`rounded p-2 transition-all text-left flex-1 ${
                selectedSensorIdx === idx
                  ? 'bg-red-600 ring-2 ring-red-400'
                  : 'bg-slate-800 hover:bg-slate-700'
              } disabled:opacity-50 disabled:cursor-not-allowed`}
            >
              <div>
                <div className="text-xs text-slate-300 mb-1">
                  <Zap className="w-3 h-3 inline mr-1" />
                  Sensor {idx + 1}
                </div>
                <div className="font-mono font-bold text-base text-yellow-400">
                  {sens[idx].toFixed(2)}
                  <span className="text-xs text-slate-400 ml-1">N</span>
                </div>
              </div>
            </button>
          </div>
        ))}
      </div>

      {/* GOTO Position - Compact */}
      <div className="bg-slate-700 rounded-lg p-3">
        <div className="flex items-center gap-2 mb-2">
          <Move className="w-4 h-4 text-purple-400" />
          <h3 className="text-sm font-semibold text-slate-200">
            GOTO {selectedTableIdx !== null ? `T${selectedTableIdx + 1}` : ''}
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
            className="flex-1 px-2 py-1 bg-slate-600 rounded border border-slate-500 focus:border-purple-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed font-mono text-xs"
          />
          <span className="px-2 py-1 bg-slate-800 rounded text-slate-400 text-xs">mm</span>
          <button
            onClick={handleGotoSend}
            disabled={!isConnected}
            className="px-3 py-1 bg-purple-600 hover:bg-purple-700 disabled:bg-slate-600 disabled:cursor-not-allowed rounded text-xs font-semibold"
          >
            Send
          </button>
        </div>
      </div>

      {/* Info */}
      <div className="bg-slate-800 rounded-lg p-2 text-xs text-slate-400 text-center">
        Click sensor for graph
      </div>
    </div>
  );
}
