import { useState, useEffect, useRef } from 'react';
import { Terminal, Trash2, ChevronsDown, ChevronsUp, Send, RotateCcw, ArrowDownUp, Copy, Check } from 'lucide-react';
import { SerialLog } from '../hooks/useMachineController';

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
  const [newestFirst, setNewestFirst] = useState(false);
  const [selectedIdx, setSelectedIdx] = useState<number | null>(null);
  const [copied, setCopied] = useState(false);
  const [manualCommand, setManualCommand] = useState('');
  const [isSubmitting, setIsSubmitting] = useState(false);
  const logsContainerRef = useRef<HTMLDivElement>(null);

  // Auto-scroll: only when newest-last (chronological order)
  useEffect(() => {
    if (!autoScroll || newestFirst || !logsContainerRef.current) return;
    const container = logsContainerRef.current;
    setTimeout(() => { container.scrollTop = container.scrollHeight; }, 0);
  }, [logs, autoScroll, newestFirst]);

  const filtered = filter === 'all' ? logs : logs.filter(l => l.type === filter);
  const displayed = newestFirst ? [...filtered].reverse() : filtered;

  const selectedLog = selectedIdx !== null ? displayed[selectedIdx] ?? null : null;

  const copySelected = async () => {
    if (!selectedLog) return;
    await navigator.clipboard.writeText(`[${selectedLog.timestamp}] ${selectedLog.message}`);
    setCopied(true);
    setTimeout(() => setCopied(false), 1500);
  };

  const handleSendCommand = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!manualCommand.trim() || !onSendCommand || isSubmitting) return;
    setIsSubmitting(true);
    try {
      await onSendCommand(manualCommand.trim());
      setManualCommand('');
    } finally {
      setIsSubmitting(false);
    }
  };

  const getLogColor = (type: SerialLog['type']) => {
    switch (type) {
      case 'command':  return 'text-blue-400';
      case 'response': return 'text-slate-300';
      case 'state':    return 'text-yellow-400';
      case 'error':    return 'text-red-400';
      case 'done':     return 'text-green-400';
      default:         return 'text-slate-400';
    }
  };

  const getLogPrefix = (type: SerialLog['type']) => {
    switch (type) {
      case 'command':  return '> ';
      case 'response': return '< ';
      case 'state':    return '= ';
      case 'error':    return '! ';
      case 'done':     return '✓ ';
      default:         return '  ';
    }
  };

  return (
    <div className="bg-slate-800 rounded-lg overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b border-slate-700">
        <div className="flex items-center gap-3">
          <Terminal className="w-5 h-5 text-blue-400" />
          <h2 className="text-lg font-semibold">Serial Monitor</h2>
          <span className="text-xs text-slate-500">{filtered.length} entries</span>
        </div>

        <div className="flex items-center gap-2">
          {/* Type filter */}
          <select
            value={filter}
            onChange={(e) => { setFilter(e.target.value as SerialLog['type'] | 'all'); setSelectedIdx(null); }}
            className="px-3 py-1 bg-slate-700 border border-slate-600 rounded text-sm focus:border-blue-500 focus:outline-none"
          >
            <option value="all">All</option>
            <option value="command">Commands</option>
            <option value="response">Responses</option>
            <option value="state">States</option>
            <option value="done">Done</option>
            <option value="error">Errors</option>
          </select>

          {/* Sort order */}
          <button
            onClick={() => { setNewestFirst(v => !v); setSelectedIdx(null); }}
            className={`flex items-center gap-1 px-3 py-1 rounded text-sm transition-colors ${
              newestFirst ? 'bg-indigo-600 text-white' : 'bg-slate-700 text-slate-300'
            }`}
            title={newestFirst ? 'Newest first' : 'Oldest first'}
          >
            <ArrowDownUp className="w-3.5 h-3.5" />
            {newestFirst ? 'Newest ↑' : 'Oldest ↑'}
          </button>

          {/* Copy selected */}
          {selectedLog && (
            <button
              onClick={copySelected}
              className="flex items-center gap-1 px-3 py-1 bg-slate-700 hover:bg-slate-600 rounded text-sm transition-colors"
              title="Copy selected entry"
            >
              {copied ? <Check className="w-4 h-4 text-green-400" /> : <Copy className="w-4 h-4" />}
            </button>
          )}

          {/* Auto-scroll (only relevant in oldest-first view) */}
          {!newestFirst && (
            <button
              onClick={() => setAutoScroll(v => !v)}
              className={`px-3 py-1 rounded text-sm transition-colors ${
                autoScroll ? 'bg-blue-600 text-white' : 'bg-slate-700 text-slate-300'
              }`}
              title="Auto scroll to bottom"
            >
              <ChevronsDown className="w-4 h-4" />
            </button>
          )}

          {/* Refresh */}
          <button
            onClick={onRefreshLogs}
            disabled={isLoading}
            className="px-3 py-1 bg-slate-700 hover:bg-slate-600 disabled:opacity-50 rounded text-sm transition-colors"
            title="Refresh logs"
          >
            <RotateCcw className="w-4 h-4" />
          </button>

          {/* Clear */}
          <button
            onClick={() => { onClear(); setSelectedIdx(null); }}
            className="px-3 py-1 bg-slate-700 hover:bg-slate-600 rounded text-sm transition-colors"
            title="Clear logs"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Selected entry preview */}
      {selectedLog && (
        <div className="px-4 py-2 bg-slate-700 border-b border-slate-600 flex items-start gap-2 text-xs font-mono">
          <span className="text-slate-400 shrink-0">{selectedLog.timestamp}</span>
          <span className={`shrink-0 ${getLogColor(selectedLog.type)}`}>{getLogPrefix(selectedLog.type)}</span>
          <span className={`break-all ${getLogColor(selectedLog.type)}`}>{selectedLog.message}</span>
        </div>
      )}

      {/* Log list */}
      <div
        ref={logsContainerRef}
        className="h-64 overflow-y-auto p-2 bg-slate-900 font-mono text-xs border-b border-slate-700"
      >
        {displayed.length === 0 ? (
          <div className="text-slate-500 text-center py-8">No logs yet</div>
        ) : (
          displayed.map((log, i) => (
            <div
              key={i}
              onClick={() => setSelectedIdx(i === selectedIdx ? null : i)}
              className={`mb-0.5 flex gap-1 overflow-x-auto cursor-pointer rounded px-1 transition-colors ${
                i === selectedIdx
                  ? 'bg-slate-600'
                  : 'hover:bg-slate-800'
              }`}
            >
              <span className="text-slate-600 whitespace-nowrap shrink-0">{log.timestamp}</span>
              <span className={`${getLogColor(log.type)} whitespace-nowrap shrink-0`}>{getLogPrefix(log.type)}</span>
              <span className={`${getLogColor(log.type)} break-words min-w-0`}>{log.message}</span>
            </div>
          ))
        )}
      </div>

      {/* Command input */}
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
