import { useEffect, useRef, useState } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { TrendingDown } from 'lucide-react';

interface ForceGraphProps {
  sensorIdx: number;  // 0-3
  currentForce: number;
  onClose?: () => void;
}

interface DataPoint {
  time: number;   // secondes depuis le début du graphe
  force: number;
}

const WINDOW_POINTS = 150;  // 150 × 200 ms = 30 s de données

export default function ForceGraph({ sensorIdx, currentForce, onClose }: ForceGraphProps) {
  const [data, setData] = useState<DataPoint[]>([]);
  const [maxForce, setMaxForce] = useState(0);
  const [avgForce, setAvgForce] = useState(0);
  const startTimeRef = useRef<number | null>(null);

  // Update graph data when sensor force changes
  useEffect(() => {
    const now = Date.now();
    if (startTimeRef.current === null) startTimeRef.current = now;
    const elapsed = (now - startTimeRef.current) / 1000;  // secondes

    setData((prevData) => {
      const newData = [...prevData, { time: elapsed, force: currentForce }];
      const trimmed = newData.slice(-WINDOW_POINTS);
      const forces = trimmed.map((d) => d.force);
      setMaxForce(Math.max(...forces));
      setAvgForce(forces.reduce((a, b) => a + b, 0) / forces.length);
      return trimmed;
    });
  }, [currentForce]);

  return (
    <div className="bg-slate-700 rounded-lg p-6 space-y-4">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <TrendingDown className="w-6 h-6 text-red-400" />
          <h2 className="text-2xl font-bold text-slate-200">Sensor {sensorIdx + 1} Force</h2>
        </div>
        {onClose && (
          <button
            onClick={onClose}
            className="px-4 py-2 bg-slate-600 hover:bg-slate-500 rounded-lg text-sm transition-colors"
          >
            Close
          </button>
        )}
      </div>

      {/* Stats */}
      <div className="grid grid-cols-3 gap-4">
        <div className="bg-slate-800 rounded p-4">
          <div className="text-xs text-slate-400 mb-1">Current Force</div>
          <div className="font-mono font-bold text-2xl text-red-400">
            {currentForce.toFixed(2)}
            <span className="text-xs text-slate-400 ml-1">N</span>
          </div>
        </div>
        <div className="bg-slate-800 rounded p-4">
          <div className="text-xs text-slate-400 mb-1">Max Force</div>
          <div className="font-mono font-bold text-2xl text-orange-400">
            {maxForce.toFixed(2)}
            <span className="text-xs text-slate-400 ml-1">N</span>
          </div>
        </div>
        <div className="bg-slate-800 rounded p-4">
          <div className="text-xs text-slate-400 mb-1">Average Force</div>
          <div className="font-mono font-bold text-2xl text-yellow-400">
            {avgForce.toFixed(2)}
            <span className="text-xs text-slate-400 ml-1">N</span>
          </div>
        </div>
      </div>

      {/* Chart */}
      <div className="bg-slate-800 rounded-lg p-4">
        <ResponsiveContainer width="100%" height={300}>
          <LineChart data={data}>
            <CartesianGrid strokeDasharray="3 3" stroke="#475569" />
            <XAxis
              dataKey="time"
              stroke="#94a3b8"
              style={{ fontSize: '12px' }}
              tickFormatter={(v: number) => `${v.toFixed(1)}s`}
              label={{ value: 'Time (s)', position: 'insideBottom', offset: -5 }}
            />
            <YAxis
              stroke="#94a3b8"
              style={{ fontSize: '12px' }}
              label={{ value: 'Force (N)', angle: -90, position: 'insideLeft' }}
            />
            <Tooltip
              contentStyle={{
                backgroundColor: '#1e293b',
                border: '1px solid #475569',
                borderRadius: '8px',
              }}
              formatter={(value: number) => [value.toFixed(2) + ' N', 'Force']}
              labelFormatter={(t: number) => `t = ${Number(t).toFixed(2)} s`}
              labelStyle={{ color: '#94a3b8' }}
            />
            <Legend
              wrapperStyle={{ paddingTop: '20px' }}
              style={{ color: '#94a3b8' }}
            />
            <Line
              type="monotone"
              dataKey="force"
              stroke="#ef4444"
              dot={false}
              isAnimationActive={true}
              animationDuration={300}
              name="Force"
              strokeWidth={2}
            />
          </LineChart>
        </ResponsiveContainer>
      </div>

      <div className="text-xs text-slate-400 text-center">
        Displaying last 30 s • Updates at 5 Hz
      </div>
    </div>
  );
}
