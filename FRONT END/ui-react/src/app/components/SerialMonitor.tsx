import { useState, useEffect, useRef } from 'react';
import { Terminal, Trash2, ChevronsDown } from 'lucide-react';

interface SerialLog {
  timestamp: string;
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;
}

interface SerialMonitorProps {
  logs: SerialLog[];
  onClear: () => void;
}

export default function SerialMonitor({ logs, onClear }: SerialMonitorProps) {
  const [autoScroll, setAutoScroll] = useState(true);
  const [filter, setFilter] = useState<SerialLog['type'] | 'all'>('all');
  const logsEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (autoScroll) {
      logsEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, autoScroll]);

  const filteredLogs = filter === 'all' ? logs : logs.filter(log => log.type === filter);

  const getLogColor = (type: SerialLog['type']) => {
    switch (type) {
      case 'command':
        return 'text-blue-400';
      case 'response':
        return 'text-slate-300';
      case 'state':
        return 'text-yellow-400';
      case 'error':
        return 'text-red-400';
      case 'done':
        return 'text-green-400';
      default:
        return 'text-slate-400';
    }
  };

  return (
    <div className="bg-slate-800 rounded-lg overflow-hidden">
      <div className="flex items-center justify-between p-4 border-b border-slate-700">
        <div className="flex items-center gap-3">
          <Terminal className="w-5 h-5 text-blue-400" />
          <h2 className="text-lg font-semibold">Serial Monitor</h2>
        </div>

        <div className="flex items-center gap-2">
          <select
            value={filter}
            onChange={(e) => setFilter(e.target.value as SerialLog['type'] | 'all')}
            className="px-3 py-1 bg-slate-700 border border-slate-600 rounded text-sm focus:border-blue-500 focus:outline-none"
          >
            <option value="all">All</option>
            <option value="command">Commands</option>
            <option value="response">Responses</option>
            <option value="state">States</option>
            <option value="done">Done</option>
            <option value="error">Errors</option>
          </select>

          <button
            onClick={() => setAutoScroll(!autoScroll)}
            className={`px-3 py-1 rounded text-sm transition-colors ${
              autoScroll ? 'bg-blue-600 text-white' : 'bg-slate-700 text-slate-300'
            }`}
          >
            <ChevronsDown className="w-4 h-4" />
          </button>

          <button
            onClick={onClear}
            className="px-3 py-1 bg-slate-700 hover:bg-slate-600 rounded text-sm transition-colors"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
      </div>

      <div className="h-48 overflow-y-auto p-4 bg-slate-900 font-mono text-sm">
        {filteredLogs.length === 0 ? (
          <div className="text-slate-500 text-center py-8">No logs yet</div>
        ) : (
          filteredLogs.map((log, index) => (
            <div key={index} className="mb-1">
              <span className="text-slate-500">[{log.timestamp}]</span>{' '}
              <span className={getLogColor(log.type)}>{log.message}</span>
            </div>
          ))
        )}
        <div ref={logsEndRef} />
      </div>
    </div>
  );
}
