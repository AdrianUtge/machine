import { useState, useEffect, useRef } from 'react';
import { Terminal, Trash2, ChevronsDown, Send, RotateCcw } from 'lucide-react';

interface SerialLog {
  timestamp: string;
  type: 'command' | 'response' | 'state' | 'error' | 'done';
  message: string;
}

interface SerialMonitorProps {
  logs: SerialLog[];
  onClear: () => void;
  onSendCommand?: (command: string) => Promise<any>;
  onRefreshLogs?: () => Promise<void>;
  isLoading?: boolean;
}

export default function SerialMonitor({
  logs,
  onClear,
  onSendCommand,
  onRefreshLogs,
  isLoading = false
}: SerialMonitorProps) {
  const [autoScroll, setAutoScroll] = useState(true);
  const [filter, setFilter] = useState<SerialLog['type'] | 'all'>('all');
  const [manualCommand, setManualCommand] = useState('');
  const [isSubmitting, setIsSubmitting] = useState(false);
  const logsEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (autoScroll) {
      logsEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, autoScroll]);

  const filteredLogs = filter === 'all' ? logs : logs.filter(log => log.type === filter);

  const handleSendCommand = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!manualCommand.trim() || !onSendCommand || isSubmitting) return;

    setIsSubmitting(true);
    try {
      await onSendCommand(manualCommand.trim());
      setManualCommand('');
    } catch (err) {
      console.error('Failed to send command:', err);
    } finally {
      setIsSubmitting(false);
    }
  };

  const handleRefreshLogs = async () => {
    if (!onRefreshLogs) return;
    try {
      await onRefreshLogs();
    } catch (err) {
      console.error('Failed to refresh logs:', err);
    }
  };

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
            title="Auto scroll"
          >
            <ChevronsDown className="w-4 h-4" />
          </button>

          <button
            onClick={handleRefreshLogs}
            disabled={isLoading}
            className="px-3 py-1 bg-slate-700 hover:bg-slate-600 disabled:opacity-50 rounded text-sm transition-colors"
            title="Refresh logs"
          >
            <RotateCcw className="w-4 h-4" />
          </button>

          <button
            onClick={onClear}
            className="px-3 py-1 bg-slate-700 hover:bg-slate-600 rounded text-sm transition-colors"
            title="Clear logs"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
      </div>

      <div className="h-96 overflow-y-auto p-4 bg-slate-900 font-mono text-sm border-b border-slate-700">
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

      <form onSubmit={handleSendCommand} className="p-4 bg-slate-800 border-t border-slate-700">
        <div className="flex gap-2">
          <input
            type="text"
            value={manualCommand}
            onChange={(e) => setManualCommand(e.target.value)}
            placeholder="Enter manual command (e.g., HOME, START, SET_FREQ:1.5)..."
            className="flex-1 px-3 py-2 bg-slate-700 border border-slate-600 rounded text-sm focus:border-blue-500 focus:outline-none text-white placeholder-slate-400"
            disabled={isSubmitting || isLoading}
          />
          <button
            type="submit"
            disabled={!manualCommand.trim() || isSubmitting || isLoading}
            className="px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed rounded text-sm font-medium transition-colors flex items-center gap-2"
          >
            <Send className="w-4 h-4" />
            Send
          </button>
        </div>
      </form>
    </div>
  );
}
