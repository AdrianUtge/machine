/**
 * ===========================================================================
 * FILE: App.tsx
 * ROLE:
 *   Composant racine de l'UI : écran de connexion, puis tableau de bord
 *   (statut, contrôle mouvement/force, positions & capteurs, moniteur série).
 *
 * ARCHITECTURE:
 *   App.tsx -> useMachineController() (tout l'état + appels API REST)
 *           -> composants de présentation (ConnectionScreen, MotionControl,
 *              StatusPanelSimple, PositionsAndSensors, ForceGraph, SerialMonitor).
 *
 * RESPONSIBILITIES:
 *   - Aiguillage écran connexion <-> tableau de bord (showConnection).
 *   - Mode "Advanced" (moniteur série + force par cellule).
 *   - Relayer les actions UI vers les commandes du hook.
 *
 * MAINTAINER NOTES:
 *   - Toute la logique réseau est dans useMachineController : ce fichier reste
 *     purement présentation/état d'affichage.
 * ===========================================================================
 */
import { useState, useEffect } from 'react';
import { AlertTriangle, SlidersHorizontal, Settings as SettingsIcon } from 'lucide-react';
import ConnectionScreen from './components/ConnectionScreen';
import MotionControl from './components/MotionControl';
import StatusPanelSimple from './components/StatusPanelSimple';
import SerialMonitor from './components/SerialMonitor';
import PositionsAndSensors from './components/PositionsAndSensors';
import ForceGraph from './components/ForceGraph';
import Settings from './components/Settings';
import { useMachineController } from './hooks/useMachineController';

export default function App() {
  const {
    isConnected,
    machineState,
    logs,
    isLoading,
    error,
    getAvailablePorts,
    connect,
    disconnect,
    home,
    start,
    stop,
    setFrequency,
    setForce,
    goto,
    sendManualCommand,
    customPresets,
    savePreset,
    deletePreset,
    refreshLogs,
    clearLogs,
    torqueOff,
    torqueOn,
    latencyMs,
    blinkMotor,
    setResistance,
  } = useMachineController();

  const [availablePorts, setAvailablePorts] = useState<string[]>([]);
  const [loadingPorts, setLoadingPorts] = useState(true);
  const [portsError, setPortsError] = useState<string | null>(null);
  const [showConnection, setShowConnection] = useState(true);
  const [connectionError, setConnectionError] = useState<string | null>(null);
  const [selectedSensorIdx, setSelectedSensorIdx] = useState<number | null>(null);
  const [advanced, setAdvanced] = useState(false);
  const [selectedSensors, setSelectedSensors] = useState<number[]>([]);
  const [showSettings, setShowSettings] = useState(false);

  // Load available ports on mount and when needed
  useEffect(() => {
    const loadPorts = async () => {
      setLoadingPorts(true);
      setPortsError(null);
      try {
        const ports = await getAvailablePorts();
        setAvailablePorts(ports);
        if (ports.length === 0) {
          setPortsError('No ports detected. Make sure your device is connected.');
        }
      } catch (err) {
        setPortsError('Failed to load ports. Make sure the API is running.');
        console.error('Error loading ports:', err);
      } finally {
        setLoadingPorts(false);
      }
    };
    loadPorts();
  }, [getAvailablePorts]);

  // Hide connection screen when successfully connected
  useEffect(() => {
    if (isConnected) {
      setShowConnection(false);
      setConnectionError(null);
    }
  }, [isConnected]);

  const handleConnect = async (port: string) => {
    setConnectionError(null);
    try {
      await connect(port);
      setTimeout(() => {
        if (error) {
          setConnectionError(error);
        } else {
          setShowConnection(false);
        }
      }, 100);
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : 'Failed to connect';
      setConnectionError(errorMsg);
      console.error('Connection error:', err);
    }
  };

  const handleDisconnect = async () => {
    await disconnect();
    setShowConnection(true);
  };

  const handleFrequencyChange = (freq: number) => {
    setFrequency(freq);
  };

  // sensor: undefined = global (4 cells), 1-4 = per-cell
  const handleForceChange = (force: number, sensor?: number) => {
    setForce(force, sensor);
  };

  const toggleSensor = (idx: number) => {
    setSelectedSensors((prev) =>
      prev.includes(idx) ? prev.filter((i) => i !== idx) : [...prev, idx].sort()
    );
  };

  const handleGoto = (table: number, position: number) => {
    goto(table, position);
  };

  const handleCommand = (command: string) => {
    if (command === 'HOME') home();
    else if (command === 'START') start();
    else if (command === 'STOP') stop();
  };

  if (showConnection) {
    return (
      <ConnectionScreen
        availablePorts={availablePorts}
        onConnect={handleConnect}
        isLoading={loadingPorts}
        error={connectionError || portsError}
      />
    );
  }

  return (
    <div className="min-h-screen flex flex-col bg-gradient-to-br from-slate-900 via-slate-800 to-slate-900 text-white p-6">
      {/* Header */}
      <div className="mb-6 flex justify-between items-center">
        <div>
          <h1 className="text-3xl font-bold">Control Panel</h1>
          <p className="text-slate-400">Machine Control Interface</p>
        </div>

        {/* Connection Status + controls */}
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2 px-4 py-2 bg-slate-800 rounded-lg">
            <div className={`w-2 h-2 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`}></div>
            <span className={`font-semibold ${isConnected ? 'text-green-500' : 'text-red-500'}`}>
              {isConnected ? 'Connected' : 'Disconnected'}
            </span>
          </div>

          {error && (
            <div className="flex items-center gap-2 px-4 py-2 bg-red-900 rounded-lg">
              <AlertTriangle size={18} />
              <span className="text-sm">{error}</span>
            </div>
          )}

          {/* Settings button */}
          <button
            onClick={() => setShowSettings(true)}
            className="flex items-center gap-2 px-4 py-2 bg-slate-700 hover:bg-slate-600 rounded-lg transition-colors font-semibold"
            title="Machine settings (motor identification, calibration)"
            disabled={!isConnected}
          >
            <SettingsIcon size={18} />
            Settings
          </button>

          {/* Advanced toggle */}
          <button
            onClick={() => setAdvanced((v) => !v)}
            className={`flex items-center gap-2 px-4 py-2 rounded-lg transition-colors font-semibold ${
              advanced ? 'bg-emerald-600 hover:bg-emerald-700' : 'bg-slate-700 hover:bg-slate-600'
            }`}
            title="Toggle advanced mode (serial monitor + per-cell force)"
          >
            <SlidersHorizontal size={18} />
            Advanced {advanced ? 'ON' : 'OFF'}
          </button>

          <button
            onClick={handleDisconnect}
            className="px-4 py-2 bg-red-600 hover:bg-red-700 rounded-lg transition-colors font-semibold"
          >
            Disconnect
          </button>
        </div>
      </div>

      {/* Main Content - fills the page (taller when serial monitor is hidden) */}
      <div className="grid grid-cols-1 xl:grid-cols-4 gap-6 flex-1">
        {/* Status Panel */}
        <div className="xl:col-span-1 flex flex-col">
          <StatusPanelSimple
            isConnected={isConnected}
            machineState={machineState}
            customPresets={customPresets}
            latencyMs={latencyMs}
            onTorqueOff={torqueOff}
            onTorqueOn={torqueOn}
          />
        </div>

        {/* Motion Control or Force Graph */}
        <div className="xl:col-span-2 flex flex-col">
          <div className="bg-slate-800 rounded-lg p-6 flex-1">
            {selectedSensorIdx !== null && machineState ? (
              <ForceGraph
                sensorIdx={selectedSensorIdx}
                currentForce={machineState?.sensors?.[selectedSensorIdx] || 0}
                onClose={() => setSelectedSensorIdx(null)}
              />
            ) : (
              <>
                <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
                  <div className="w-3 h-3 bg-purple-500 rounded-full"></div>
                  Motion Control
                </h2>
                {machineState ? (
                  <MotionControl
                    frequency={machineState?.frequency_hz || 0}
                    onChange={handleFrequencyChange}
                    forceTarget={machineState?.force_target ?? 0}
                    forceTargets={machineState?.force_targets ?? [0, 0, 0, 0]}
                    onForceChange={handleForceChange}
                    isConnected={isConnected}
                    onCommand={handleCommand}
                    machineState={(machineState?.machine_status as any) || 'DISCONNECTED'}
                    pendingCommands={{}}
                    customPresets={customPresets}
                    onSavePreset={savePreset}
                    onDeletePreset={deletePreset}
                    advanced={advanced}
                    selectedSensors={selectedSensors}
                    onToggleSensor={toggleSensor}
                  />
                ) : (
                  <div className="text-slate-400">Loading motion control...</div>
                )}
              </>
            )}
          </div>
        </div>

        {/* Positions & Sensors */}
        <div className="xl:col-span-1">
          {machineState ? (
            <PositionsAndSensors
              positions={machineState?.positions || [0, 0, 0, 0]}
              sensors={machineState?.sensors || [0, 0, 0, 0]}
              isConnected={isConnected}
              machineStatus={machineState?.machine_status}
              forceTargets={machineState?.force_targets}
              onGotoCommand={handleGoto}
              graphSensorIdx={selectedSensorIdx}
              onSensorSelect={setSelectedSensorIdx}
            />
          ) : (
            <div className="text-slate-400">Loading sensors...</div>
          )}
        </div>
      </div>

      {/* Serial Monitor - advanced only */}
      {advanced && (
        <div className="mt-6">
          <SerialMonitor
            logs={logs}
            onClear={clearLogs}
            onSendCommand={sendManualCommand}
            onRefreshLogs={refreshLogs}
            isLoading={isLoading}
          />
        </div>
      )}

      {/* Settings Modal */}
      {showSettings && (
        <Settings
          isConnected={isConnected}
          onClose={() => setShowSettings(false)}
          onBlinkMotor={blinkMotor}
          onSetResistance={setResistance}
          currentResistances={{ board0: 30, board1: 30 }}  // TODO: get from machineState or backend
        />
      )}
    </div>
  );
}
