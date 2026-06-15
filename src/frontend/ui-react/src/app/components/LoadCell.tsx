import { Scale } from 'lucide-react';

interface LoadCellData {
  force: number;
  mass: number;
  status: 'OK' | 'noisy' | 'error' | 'disconnected';
}

interface LoadCellProps {
  id: number;
  data: LoadCellData;
  isConnected: boolean;
}

export default function LoadCell({ id, data, isConnected }: LoadCellProps) {
  const maxForce = 10; // 10 N max
  const percentage = (data.force / maxForce) * 100;

  const getStatusColor = () => {
    if (!isConnected || data.status === 'disconnected') return 'bg-slate-600';
    if (data.status === 'error') return 'bg-red-500';
    if (data.status === 'noisy') return 'bg-yellow-500';
    if (percentage > 80) return 'bg-orange-500';
    return 'bg-green-500';
  };

  const getStatusBadgeColor = () => {
    if (!isConnected || data.status === 'disconnected') return 'text-red-500';
    if (data.status === 'error') return 'text-red-500';
    if (data.status === 'noisy') return 'text-yellow-500';
    return 'text-green-500';
  };

  return (
    <div className="bg-slate-700 rounded-lg p-4">
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center gap-2">
          <Scale className="w-4 h-4 text-green-400" />
          <span className="text-sm font-semibold">Load Cell {id}</span>
        </div>
        <span className={`text-xs font-semibold ${getStatusBadgeColor()}`}>
          {isConnected ? data.status : 'disconnected'}
        </span>
      </div>

      <div className="space-y-2">
        <div className="flex flex-col gap-1">
          <div className="flex justify-between items-baseline">
            <span className="text-lg font-mono font-bold">
              {isConnected ? data.force.toFixed(2) : '---'}
            </span>
            <span className="text-xs text-slate-400">N</span>
          </div>
          <div className="flex justify-between items-baseline">
            <span className="text-sm font-mono text-slate-300">
              {isConnected ? data.mass.toFixed(1) : '---'}
            </span>
            <span className="text-xs text-slate-400">g</span>
          </div>
        </div>

        {/* Load Bar */}
        <div className="h-1.5 bg-slate-600 rounded-full overflow-hidden">
          <div
            className={`h-full transition-all duration-300 ${getStatusColor()}`}
            style={{ width: isConnected ? `${Math.min(percentage, 100)}%` : '0%' }}
          ></div>
        </div>
      </div>
    </div>
  );
}
