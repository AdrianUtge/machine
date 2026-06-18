# 📖 Exemples d'Utilisation du Hook `useMachineController`

## Importation

```typescript
import { useMachineController } from './hooks/useMachineController';
```

## Exemple 1: Composant Simple - Boutons de Contrôle

```typescript
import { useMachineController } from '../hooks/useMachineController';

export function ControlButtons() {
  const { isConnected, home, start, stop } = useMachineController();

  if (!isConnected) {
    return <p>❌ Non connecté</p>;
  }

  return (
    <div className="flex gap-4">
      <button 
        onClick={home}
        className="px-4 py-2 bg-blue-500 rounded"
      >
        🏠 Home
      </button>
      <button 
        onClick={start}
        className="px-4 py-2 bg-green-500 rounded"
      >
        ▶️ Start
      </button>
      <button 
        onClick={stop}
        className="px-4 py-2 bg-red-500 rounded"
      >
        ⏹️ Stop
      </button>
    </div>
  );
}
```

---

## Exemple 2: Slider de Fréquence

```typescript
export function FrequencyControl() {
  const { machineState, setFrequency } = useMachineController();

  const handleFrequencyChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const freq = parseFloat(e.target.value);
    setFrequency(freq);
  };

  return (
    <div className="space-y-2">
      <label>Fréquence: {machineState?.frequency_hz} Hz</label>
      <input
        type="range"
        min="0.1"
        max="10"
        step="0.1"
        value={machineState?.frequency_hz || 0}
        onChange={handleFrequencyChange}
        className="w-full"
      />
    </div>
  );
}
```

---

## Exemple 3: Presets Dropdown

```typescript
export function PresetSelector() {
  const { applyPreset, machineState } = useMachineController();

  const presets = [
    { key: '1', name: 'Humain (0.8 Hz)' },
    { key: '2', name: 'Boeuf (0.4 Hz)' },
    { key: '3', name: 'Souris (3.7 Hz)' },
  ];

  const handlePresetChange = (presetKey: string) => {
    applyPreset(presetKey);
  };

  return (
    <select 
      onChange={(e) => handlePresetChange(e.target.value)}
      className="px-3 py-2 bg-slate-700 rounded"
    >
      <option value="custom">Personnalisé</option>
      {presets.map(p => (
        <option key={p.key} value={p.key}>{p.name}</option>
      ))}
    </select>
  );
}
```

---

## Exemple 4: Affichage de l'État

```typescript
export function MachineStateDisplay() {
  const { machineState, isConnected } = useMachineController();

  if (!isConnected || !machineState) {
    return <p>Pas de données</p>;
  }

  return (
    <div className="bg-slate-800 p-4 rounded space-y-2">
      <p><strong>État:</strong> {machineState.machine_status}</p>
      <p><strong>Fréquence:</strong> {machineState.frequency_hz} Hz</p>
      <p><strong>Speed:</strong> {machineState.t_speed_percent}%</p>
      <p><strong>Position:</strong> {machineState.position || 'N/A'}</p>
      <p><strong>Erreurs:</strong> {machineState.errors}</p>
      <p><strong>Slave:</strong> {machineState.slave_status}</p>
    </div>
  );
}
```

---

## Exemple 5: Connection Manager

```typescript
export function ConnectionManager() {
  const {
    isConnected,
    isLoading,
    error,
    getAvailablePorts,
    connect,
    disconnect,
  } = useMachineController();

  const [ports, setPorts] = useState<string[]>([]);
  const [selectedPort, setSelectedPort] = useState('');

  useEffect(() => {
    const loadPorts = async () => {
      const available = await getAvailablePorts();
      setPorts(available);
      if (available.length > 0) {
        setSelectedPort(available[0]);
      }
    };
    loadPorts();
  }, [getAvailablePorts]);

  const handleConnect = async () => {
    if (selectedPort) {
      await connect(selectedPort);
    }
  };

  if (isConnected) {
    return (
      <button
        onClick={disconnect}
        className="px-4 py-2 bg-red-600 rounded"
      >
        Déconnecter
      </button>
    );
  }

  return (
    <div className="space-y-3">
      {error && <p className="text-red-500">{error}</p>}
      
      <select
        value={selectedPort}
        onChange={(e) => setSelectedPort(e.target.value)}
        className="w-full px-3 py-2 bg-slate-700 rounded"
      >
        {ports.map(port => (
          <option key={port} value={port}>{port}</option>
        ))}
      </select>

      <button
        onClick={handleConnect}
        disabled={isLoading || !selectedPort}
        className="w-full px-4 py-2 bg-green-600 rounded disabled:opacity-50"
      >
        {isLoading ? 'Connexion...' : 'Connecter'}
      </button>
    </div>
  );
}
```

---

## Exemple 6: Historique des Logs

```typescript
export function LogViewer() {
  const { logs } = useMachineController();

  return (
    <div className="bg-slate-900 p-4 rounded h-64 overflow-y-auto">
      <div className="space-y-1 font-mono text-sm">
        {logs.map((log, idx) => (
          <div 
            key={idx}
            className={`
              ${log.type === 'error' ? 'text-red-400' : ''}
              ${log.type === 'command' ? 'text-blue-400' : ''}
              ${log.type === 'response' ? 'text-green-400' : ''}
              ${log.type === 'state' ? 'text-yellow-400' : ''}
            `}
          >
            [{log.type}] {log.message}
          </div>
        ))}
      </div>
    </div>
  );
}
```

---

## Exemple 7: Slider de Vitesse (T_Speed)

```typescript
export function SpeedControl() {
  const { machineState, setSpeed } = useMachineController();

  const handleSpeedChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const speed = parseInt(e.target.value);
    setSpeed(speed);
  };

  return (
    <div className="space-y-2">
      <div className="flex justify-between items-center">
        <label>T_Speed:</label>
        <span className="text-lg font-bold">
          {machineState?.t_speed_percent}%
        </span>
      </div>
      
      <input
        type="range"
        min="0"
        max="100"
        step="5"
        value={machineState?.t_speed_percent || 100}
        onChange={handleSpeedChange}
        className="w-full"
      />

      <div className="flex gap-2 text-xs text-slate-400">
        <span>0%</span>
        <span>50%</span>
        <span>100%</span>
      </div>
    </div>
  );
}
```

---

## Exemple 8: Composant Combiné Complet

```typescript
export function ControlPanel() {
  const {
    isConnected,
    machineState,
    logs,
    home,
    start,
    stop,
    setFrequency,
    setSpeed,
    applyPreset,
  } = useMachineController();

  if (!isConnected) {
    return <ConnectionManager />;
  }

  return (
    <div className="grid grid-cols-3 gap-4">
      {/* Gauche: État */}
      <div className="space-y-4">
        <h3 className="font-bold">État Machine</h3>
        <MachineStateDisplay />
      </div>

      {/* Centre: Contrôles */}
      <div className="space-y-4">
        <h3 className="font-bold">Contrôles</h3>
        <ControlButtons />
        <PresetSelector />
        <FrequencyControl />
        <SpeedControl />
      </div>

      {/* Droite: Logs */}
      <div className="space-y-4">
        <h3 className="font-bold">Historique</h3>
        <LogViewer />
      </div>
    </div>
  );
}
```

---

## Patterns Utiles

### Pattern 1: Validation Avant Envoi

```typescript
const handleCommand = async (command: string) => {
  if (!isConnected) {
    alert('Non connecté!');
    return;
  }

  try {
    await sendManualCommand(command);
  } catch (err) {
    console.error('Erreur:', err);
  }
};
```

### Pattern 2: Debounce pour Slider

```typescript
import { useCallback, useState } from 'react';

export function FrequencySlider() {
  const { setFrequency } = useMachineController();
  const [freq, setFreq] = useState(0);
  const [timeout, setTimeout] = useState<NodeJS.Timeout | null>(null);

  const handleChange = useCallback((value: number) => {
    setFreq(value);
    
    if (timeout) clearTimeout(timeout);
    const newTimeout = setTimeout(() => {
      setFrequency(value);
    }, 500); // Envoyer après 500ms d'inactivité
    
    setTimeout(newTimeout);
  }, [setFrequency, timeout]);

  return (
    <input
      type="range"
      value={freq}
      onChange={(e) => handleChange(parseFloat(e.target.value))}
    />
  );
}
```

### Pattern 3: Polling d'État

```typescript
import { useEffect } from 'react';

export function AutoRefresh() {
  const { getStatus } = useMachineController();

  useEffect(() => {
    const interval = setInterval(() => {
      getStatus();
    }, 1000); // Rafraîchir chaque seconde

    return () => clearInterval(interval);
  }, [getStatus]);

  return <p>Auto-rafraîchissement actif</p>;
}
```

---

## 🎯 Checklist pour Utilisation

- ✅ Importer le hook
- ✅ Utiliser `isConnected` pour afficher/masquer UI
- ✅ Vérifier `error` pour afficher les erreurs
- ✅ Utiliser `isLoading` pour désactiver boutons
- ✅ Afficher `machineState` pour infos en temps réel
- ✅ Utiliser `logs` pour afficher l'historique
- ✅ Appeler les commandes sur click/change

---

**Besoin d'aide? Consulter la documentation API: http://localhost:8000/docs**
