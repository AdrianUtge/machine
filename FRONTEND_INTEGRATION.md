# Frontend Intégration - React + Python Backend

## 📋 Architecture

```
┌─────────────────────────────┐
│   React Frontend (Port 5173)│
│   (FRONT END/ui-react)      │
└──────────────┬──────────────┘
               │ HTTP REST API
               ↓
┌─────────────────────────────┐
│   FastAPI Backend (Port 8000)│
│   (FRONT END/api.py)        │
└──────────────┬──────────────┘
               │
               ↓
┌─────────────────────────────┐
│   Logique Python Existante  │
│   - SerialLink              │
│   - MachineController       │
│   - Protocol                │
└──────────────┬──────────────┘
               │
               ↓
         Arduino (Série)
```

## 🚀 Installation

### 1. Backend (FastAPI)

```bash
cd "FRONT END"

# Installer les dépendances Python
pip install fastapi uvicorn pyserial

# Ajouter à requirements.txt
echo "fastapi==0.104.0" >> requirements.txt
echo "uvicorn==0.24.0" >> requirements.txt
echo "pydantic==2.5.0" >> requirements.txt
```

### 2. Frontend (React)

```bash
cd "FRONT END/ui-react"

# Installer les dépendances
npm install
# ou
pnpm install
```

## 🎯 Lancement

### Terminal 1 - FastAPI Backend

```bash
cd "FRONT END"

# Lancer le serveur API
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000
```

L'API sera disponible à `http://localhost:8000`
Documentation interactive : `http://localhost:8000/docs`

### Terminal 2 - React Frontend

```bash
cd "FRONT END/ui-react"

# Lancer le serveur de développement
npm run dev
# ou
pnpm dev
```

Le frontend sera disponible à `http://localhost:5173`

## 📡 Endpoints API Disponibles

### Connection
- `GET /api/ports` - Lister les ports série disponibles
- `POST /api/connect` - Connecter à une machine
- `POST /api/disconnect` - Déconnecter
- `GET /api/status` - Récupérer l'état courant

### Commands
- `POST /api/command/home` - Commande HOME
- `POST /api/command/start` - Commande START
- `POST /api/command/stop` - Commande STOP
- `POST /api/command/hard-reset` - HARD_RESET
- `POST /api/command/frequency` - Régler fréquence (Hz)
- `POST /api/command/speed` - Régler T_Speed (%)
- `POST /api/command/preset` - Appliquer un preset
- `POST /api/command/manual` - Envoyer commande manuelle

### Logs
- `GET /api/logs` - Récupérer les logs série
- `GET /api/logs/commands` - Historique des commandes
- `GET /api/logs/console` - Logs de console

### Health
- `GET /api/health` - Vérifier l'état du service

## 🎨 Composants React

**Principaux composants :**
- `ConnectionScreen` - Interface de connexion
- `MotionControl` - Contrôle de mouvement (fréquence, presets, speed)
- `StatusPanel` - Affichage de l'état machine
- `SerialMonitor` - Historique série

**Hook personnalisé :**
- `useMachineController` - Gère toute la communication avec l'API

## 📝 Exemple d'Utilisation

```typescript
import { useMachineController } from './hooks/useMachineController';

function MyComponent() {
  const { 
    isConnected, 
    machineState,
    connect, 
    home, 
    setFrequency 
  } = useMachineController();

  const handleConnect = async () => {
    await connect('/dev/cu.usbmodem101');
  };

  const handleHome = async () => {
    await home();
    console.log(machineState); // État mis à jour
  };

  return (
    <>
      <button onClick={handleConnect}>Connecter</button>
      <button onClick={handleHome}>Home</button>
      <p>État: {machineState?.machine_status}</p>
    </>
  );
}
```

## ⚙️ Configuration

### Config Python (FRONT END/config.py)
```python
DEFAULT_PORT = "/dev/cu.usbmodem101"  # Port série
DEFAULT_BAUDRATE = 115200              # Vitesse
DEFAULT_TIMEOUT = 1.0                  # Timeout
```

### Presets (FRONT END/core/presets.py)
```python
PRESETS = {
    "1": ("Humain", 0.8),   # 0.8 Hz
    "2": ("Boeuf", 0.4),    # 0.4 Hz
    "3": ("Souris", 3.7),   # 3.7 Hz
}
```

## 🔍 Debug

### Vérifier les endpoints
```bash
# Récupérer les ports disponibles
curl http://localhost:8000/api/ports

# Vérifier la santé du service
curl http://localhost:8000/api/health

# Voir la documentation interactive
# Ouvrir http://localhost:8000/docs dans le navigateur
```

### Logs
- Logs API : voir la console du serveur FastAPI
- Logs Frontend : voir la console du navigateur (F12)

## 🛠️ Troubleshooting

**Erreur : "Connection refused on port 8000"**
- Vérifier que le backend FastAPI est lancé
- Vérifier le port 8000 est libre

**Erreur : "Cannot find module"**
- Réinstaller les dépendances : `npm install` ou `pip install -r requirements.txt`

**Port série non détecté**
- Vérifier que l'Arduino est connecté
- Vérifier les permissions : `ls -la /dev/ttyUSB*` ou `/dev/cu.*`
- Sur macOS, le port par défaut est `/dev/cu.usbmodem101`

## 📚 Structure des Fichiers

```
FRONT END/
├── main.py              # Point d'entrée console (ancien)
├── api.py              # 🆕 API FastAPI
├── config.py           # Configuration
├── requirements.txt    # Dépendances Python
│
├── core/
│   ├── controller.py   # MachineController
│   ├── state.py        # État machine
│   └── presets.py      # Presets fréquence
│
├── comm/
│   ├── serial_link.py  # Communication série
│   └── protocol.py     # Codage/décodage
│
├── debug/
│   └── logger.py       # Logging
│
└── ui-react/           # 🆕 Frontend React
    ├── package.json
    ├── vite.config.ts
    ├── index.html
    └── src/
        ├── app/
        │   ├── App.tsx
        │   ├── hooks/
        │   │   └── useMachineController.ts  # 🆕 Hook API
        │   └── components/
        │       ├── ConnectionScreen.tsx
        │       ├── MotionControl.tsx
        │       ├── StatusPanel.tsx
        │       └── SerialMonitor.tsx
```

## 📖 Prochaines Étapes

1. ✅ API FastAPI intégrée
2. ✅ Hook React pour communication
3. ⚠️ Adapter les composants React aux données réelles
4. ⚠️ Ajouter WebSocket pour les mises à jour temps réel
5. ⚠️ Packager en application Electron (optionnel)

---

**Créé pour contrôler une machine via interface moderne**
