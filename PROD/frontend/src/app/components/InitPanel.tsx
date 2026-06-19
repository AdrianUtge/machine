import React, { useState } from "react";
import { useInitState } from "../hooks/useInitState";
import InitConfigDialog from "./InitConfigDialog";

const InitPanel: React.FC = () => {
  const {
    initStatus,
    initConfig,
    isInitRunning,
    loading,
    error,
    startInit,
    stopInit,
    updateConfig,
  } = useInitState();

  const [showConfigDialog, setShowConfigDialog] = useState(false);

  const handleStart = async () => {
    await startInit();
  };

  const handleStop = async () => {
    await stopInit();
  };

  const getPhaseColor = (phase: string) => {
    switch (phase) {
      case "PHASE1":
        return "bg-blue-500";
      case "PHASE2":
        return "bg-green-500";
      case "PHASE3":
        return "bg-yellow-500";
      case "COMPLETE":
        return "bg-green-600";
      case "ERROR":
        return "bg-red-600";
      default:
        return "bg-gray-500";
    }
  };

  const getPhaseLabel = (phase: string) => {
    switch (phase) {
      case "PHASE1":
        return "Fast Descent";
      case "PHASE2":
        return "Slow Descent";
      case "PHASE3":
        return "Force Hold";
      case "COMPLETE":
        return "Complete";
      case "ERROR":
        return "Error";
      default:
        return "Idle";
    }
  };

  const formatTime = (ms: number) => {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${minutes}:${secs.toString().padStart(2, "0")}`;
  };

  if (!initStatus) {
    return <div className="text-gray-500">Loading init status...</div>;
  }

  return (
    <div className="border rounded-lg p-4 bg-gray-50">
      <div className="flex justify-between items-center mb-4">
        <h3 className="text-lg font-semibold">Motor Initialization</h3>
        <button
          onClick={() => setShowConfigDialog(true)}
          className="px-3 py-1 text-sm bg-gray-200 hover:bg-gray-300 rounded"
          disabled={isInitRunning}
        >
          Settings
        </button>
      </div>

      {error && (
        <div className="mb-4 p-2 bg-red-100 border border-red-400 text-red-700 rounded">
          {error}
        </div>
      )}

      {/* Status Display */}
      <div className="mb-4 p-3 bg-white rounded border">
        <div className="flex items-center gap-3 mb-3">
          <div
            className={`w-3 h-3 rounded-full ${getPhaseColor(
              initStatus.phase
            )}`}
          />
          <span className="font-medium">{getPhaseLabel(initStatus.phase)}</span>
          <span className="text-sm text-gray-600">
            {initStatus.elapsed_ms > 0 &&
              `${formatTime(initStatus.elapsed_ms)} / ${formatTime(120000)}`}
          </span>
        </div>

        {/* Progress Bar */}
        <div className="w-full h-6 bg-gray-200 rounded overflow-hidden mb-3">
          <div
            className={`h-full transition-all duration-300 ${getPhaseColor(
              initStatus.phase
            )}`}
            style={{ width: `${initStatus.progress_percent}%` }}
          />
        </div>

        {/* Force Peaks */}
        <div className="grid grid-cols-4 gap-2 mb-3">
          {initStatus.force_peaks.map((force, i) => (
            <div key={i} className="text-center p-2 bg-gray-100 rounded">
              <div className="text-xs text-gray-600">Motor {i + 1}</div>
              <div className="font-medium">{force.toFixed(1)} mV</div>
              {initStatus.complete_motors[i] && (
                <div className="text-xs text-green-600">✓ Complete</div>
              )}
            </div>
          ))}
        </div>

        {/* Motor Status */}
        <div className="text-sm text-gray-600 mb-3">
          Motors completed: {initStatus.complete_motors.filter(Boolean).length}/4
        </div>
      </div>

      {/* Control Buttons */}
      <div className="flex gap-2">
        <button
          onClick={handleStart}
          disabled={isInitRunning || loading}
          className={`flex-1 px-4 py-2 rounded font-medium transition-colors ${
            isInitRunning || loading
              ? "bg-gray-300 cursor-not-allowed text-gray-500"
              : "bg-blue-500 hover:bg-blue-600 text-white"
          }`}
        >
          {loading ? "..." : "Start Init"}
        </button>
        <button
          onClick={handleStop}
          disabled={!isInitRunning || loading}
          className={`flex-1 px-4 py-2 rounded font-medium transition-colors ${
            !isInitRunning || loading
              ? "bg-gray-300 cursor-not-allowed text-gray-500"
              : "bg-red-500 hover:bg-red-600 text-white"
          }`}
        >
          {loading ? "..." : "Stop"}
        </button>
      </div>

      {/* Config Dialog */}
      {showConfigDialog && initConfig && (
        <InitConfigDialog
          config={initConfig}
          onSave={async (newConfig) => {
            await updateConfig(newConfig);
            setShowConfigDialog(false);
          }}
          onCancel={() => setShowConfigDialog(false)}
        />
      )}
    </div>
  );
};

export default InitPanel;
