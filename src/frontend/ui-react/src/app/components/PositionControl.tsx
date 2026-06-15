import { useState, useEffect } from 'react';
import { Move } from 'lucide-react';

interface PositionControlProps {
  id: number;
  value: number;
  onChange: (value: number) => void;
  isConnected: boolean;
}

export default function PositionControl({ id, value, onChange, isConnected }: PositionControlProps) {
  const [targetPosition, setTargetPosition] = useState(value);
  const [isMoving, setIsMoving] = useState(false);

  useEffect(() => {
    if (targetPosition !== value) {
      setIsMoving(true);
      const timer = setTimeout(() => {
        onChange(targetPosition);
        setIsMoving(false);
      }, 500);
      return () => clearTimeout(timer);
    }
  }, [targetPosition, value, onChange]);

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value);
    setTargetPosition(newValue);
  };

  return (
    <div className="bg-slate-700 rounded-lg p-4 space-y-3">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Move className="w-4 h-4 text-blue-400" />
          <span className="font-semibold">Axis {id}</span>
        </div>
        <div className={`w-2 h-2 rounded-full ${isConnected ? (isMoving ? 'bg-yellow-500 animate-pulse' : 'bg-green-500') : 'bg-red-500'}`}></div>
      </div>

      <div className="space-y-2">
        <div className="flex justify-between text-sm">
          <span className="text-slate-400">Current Position</span>
          <span className="font-mono font-semibold">{value.toFixed(2)} mm</span>
        </div>

        <input
          type="range"
          min="0"
          max="1000"
          step="0.1"
          value={targetPosition}
          onChange={handleInputChange}
          disabled={!isConnected}
          className="w-full h-2 bg-slate-600 rounded-lg appearance-none cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-blue-500"
        />

        <div className="flex gap-2">
          <input
            type="number"
            min="0"
            max="1000"
            step="0.1"
            value={targetPosition}
            onChange={handleInputChange}
            disabled={!isConnected}
            className="flex-1 px-3 py-2 bg-slate-600 rounded border border-slate-500 focus:border-blue-500 focus:outline-none disabled:opacity-50 disabled:cursor-not-allowed"
          />
          <span className="px-3 py-2 bg-slate-800 rounded text-slate-400">mm</span>
        </div>
      </div>
    </div>
  );
}
