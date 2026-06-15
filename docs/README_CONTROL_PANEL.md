# 🎮 Control Panel - Machine Frontend

Une **application web moderne** pour contrôler votre machine Arduino via une interface élégante conçue avec Figma.

## 🎯 Vision

Transformer le **CLI console Python** en une **application web React moderne** tout en conservant la puissance de la logique Python existante.

```
┌─────────────────────────────┐
│   React Web UI (Figma)      │ ← Vous êtes ici!
│   Modern & Beautiful        │
└──────────────┬──────────────┘
               │ REST API
               ↓
┌─────────────────────────────┐
│   FastAPI Backend           │
│   Robust & Documented       │
└──────────────┬──────────────┘
               │
               ↓
┌─────────────────────────────┐
│   Python Logic (Existant)   │
│   SerialLink + Protocol     │
└──────────────┬──────────────┘
               │
               ↓
        🤖 Your Machine
```

---

## ⚡ Quick Start

### Option 1: Automatique (Recommandé)
```bash
./START.sh
```
Puis ouvrir: **http://localhost:5173**

### Option 2: Manuel
**Terminal 1 - Backend:**
```bash
cd "FRONT END"
pip install -r requirements.txt
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000
```

**Terminal 2 - Frontend:**
```bash
cd "FRONT END/ui-react"
npm install
npm run dev
```

### Option 3: Docker
```bash
docker-compose up
```

**Dans tous les cas:**
- Frontend: http://localhost:5173
- API: http://localhost:8000
- Docs API: http://localhost:8000/docs

---

## 📚 Documentation

| Document | Contenu |
|----------|---------|
| **[FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md)** | Guide complet - Installation, endpoints, config |
| **[IMPLEMENTATION_SUMMARY.md](./IMPLEMENTATION_SUMMARY.md)** | Résumé technique - Architecture, flux, structure |
| **[CHANGELOG.md](./CHANGELOG.md)** | Changements - Fichiers créés/modifiés, stats |
| **[USAGE_EXAMPLES.md](./FRONT\ END/ui-react/src/app/hooks/USAGE_EXAMPLES.md)** | Code examples - 8+ exemples pratiques |

---

## 🎨 Features

### Pour l'Utilisateur
- ✅ Interface graphique moderne (design Figma)
- ✅ Connexion USB/Bluetooth
- ✅ Contrôle de fréquence (Hz)
- ✅ Presets prédéfinis (Humain, Boeuf, Souris)
- ✅ Contrôle de vitesse (T_Speed %)
- ✅ Affichage temps réel de l'état
- ✅ Historique des commandes/logs

### Pour le Développeur
- ✅ API REST complète et documentée (Swagger)
- ✅ Hook React réutilisable
- ✅ Code modulaire et maintenable
- ✅ CORS enabled pour développement
- ✅ Error handling robuste
- ✅ Configuration centralisée

---

## 📡 API Endpoints

### Connection
```
GET  /api/ports              # Lister ports série
POST /api/connect            # Connecter
POST /api/disconnect         # Déconnecter
GET  /api/status             # État courant
```

### Commands
```
POST /api/command/home           # HOME
POST /api/command/start          # START
POST /api/command/stop           # STOP
POST /api/command/hard-reset     # HARD_RESET
POST /api/command/frequency      # Set fréquence (Hz)
POST /api/command/speed          # Set T_Speed (%)
POST /api/command/preset         # Apply preset
POST /api/command/manual         # Free command
```

### Monitoring
```
GET /api/logs                # Serial logs
GET /api/logs/commands       # Command history
GET /api/logs/console        # Console logs
GET /api/health              # Status check
```

**Documentation interactive:** http://localhost:8000/docs

---

## 🔧 Configuration

### Port Série
**`FRONT END/config.py`:**
```python
DEFAULT_PORT = "/dev/cu.usbmodem101"  # ← Modifier ici
DEFAULT_BAUDRATE = 115200
DEFAULT_TIMEOUT = 1.0
```

### Presets
**`FRONT END/core/presets.py`:**
```python
PRESETS = {
    "1": ("Humain", 0.8),
    "2": ("Boeuf", 0.4),
    "3": ("Souris", 3.7),
    "4": ("Custom", 2.5),  # Ajouter facilement
}
```

---

## 📂 Structure

```
📦 Project Root
├── 📄 FRONTEND_INTEGRATION.md          ← Lire en premier!
├── 📄 IMPLEMENTATION_SUMMARY.md
├── 📄 CHANGELOG.md
├── 🚀 START.sh                         ← Lancer ceci
│
└── FRONT END/
    ├── 🆕 api.py                       ← FastAPI Backend
    ├── main.py                         ← CLI Console (ancien)
    ├── config.py
    ├── requirements.txt
    │
    ├── core/
    │   ├── controller.py
    │   ├── state.py
    │   └── presets.py
    │
    ├── comm/
    │   ├── serial_link.py
    │   └── protocol.py
    │
    ├── debug/
    │   └── logger.py
    │
    └── 🆕 ui-react/                    ← React Frontend
        ├── package.json
        ├── vite.config.ts
        ├── src/
        │   └── app/
        │       ├── 🆕 App.tsx           ← Modifié
        │       ├── 🆕 hooks/
        │       │   ├── useMachineController.ts  ← Nouveau hook
        │       │   └── USAGE_EXAMPLES.md
        │       └── components/
        │           ├── ConnectionScreen.tsx
        │           ├── MotionControl.tsx
        │           ├── StatusPanel.tsx
        │           └── SerialMonitor.tsx
        └── index.html
```

---

## 🧪 Test Rapide

```bash
# 1. Vérifier que tout marche
curl http://localhost:8000/api/health

# Résultat attendu:
# {"status":"ok","connected":false}

# 2. Lister les ports
curl http://localhost:8000/api/ports

# 3. Voir la doc
# Ouvrir http://localhost:8000/docs dans le navigateur
```

---

## 🪝 Hook React

Utiliser dans n'importe quel composant:

```typescript
import { useMachineController } from './hooks/useMachineController';

export function MyComponent() {
  const { 
    isConnected,           // boolean
    machineState,          // MachineState | null
    logs,                  // SerialLog[]
    isLoading,             // boolean
    error,                 // string | null
    
    // Commands
    home,                  // () => Promise<void>
    start,                 // () => Promise<void>
    stop,                  // () => Promise<void>
    setFrequency,          // (freq: number) => Promise<void>
    setSpeed,              // (speed: number) => Promise<void>
    applyPreset,           // (preset: string) => Promise<void>
    
    // Utils
    connect,               // (port: string) => Promise<void>
    disconnect,            // () => Promise<void>
    getStatus,             // () => Promise<void>
  } = useMachineController();

  return (
    // Votre UI ici
  );
}
```

**Plus d'exemples:** Voir `USAGE_EXAMPLES.md`

---

## 🚨 Troubleshooting

| Problème | Cause | Solution |
|----------|-------|----------|
| Port 8000 occupé | Autre service | `lsof -i :8000 && kill -9 PID` |
| Port 5173 occupé | React ancien | `kill -9 $(lsof -t -i:5173)` |
| Arduino pas détecté | Câble / Permission | Vérifier `/dev/cu.*` et permissions |
| CORS error | Frontend pas sur localhost | Vérifier URL: http://localhost:5173 |
| Module not found | Dépendances manquantes | `pip install -r requirements.txt` ou `npm install` |

---

## 📖 Voir Aussi

- **Ancien CLI Console**: Exécuter `python FRONT END/main.py`
- **Documentation API Complète**: http://localhost:8000/docs
- **Guide d'Intégration**: [FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md)
- **Exemples de Code**: [USAGE_EXAMPLES.md](./FRONT%20END/ui-react/src/app/hooks/USAGE_EXAMPLES.md)

---

## 💡 Tips

1. **Développement**: Garder les deux serveurs en arrière-plan
   ```bash
   ./START.sh  # Lance les deux en parallèle
   ```

2. **Debug API**: Ouvrir http://localhost:8000/docs pour tester les endpoints

3. **Debug Frontend**: Ouvrir DevTools (F12) pour voir les logs réseau et console

4. **Hot Reload**: Les deux serveurs se reloadent automatiquement

5. **Logs**: Voir la console du terminal pour backend, F12 pour frontend

---

## 🎯 Roadmap

- [x] API FastAPI
- [x] Hook React
- [x] Documentation
- [ ] WebSocket pour real-time
- [ ] Authentication
- [ ] Database pour persistence
- [ ] Electron App
- [ ] Mobile App (React Native)

---

## 📋 Requirements

- **Python** 3.8+
- **Node.js** 16+
- **npm** ou **pnpm**
- **Arduino** avec code approprié

---

## 👨‍💻 Pour les Développeurs

### Stack Technique
- **Backend**: Python 3, FastAPI, Uvicorn
- **Frontend**: React 18, TypeScript, Vite, Tailwind
- **Communication**: REST API, HTTP
- **Hardware**: Arduino via PySerial

### Contributing
1. Modifier les fichiers
2. Tester localement
3. Vérifier la doc

### Architecture Décisions
- REST au lieu de GraphQL (plus simple)
- Hook au lieu de Context (réutilisabilité)
- Pydantic pour validation
- Async/await pattern

---

## 🆘 Support

**Problème?**
1. Lire [FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md) - section "Troubleshooting"
2. Consulter [USAGE_EXAMPLES.md](./FRONT%20END/ui-react/src/app/hooks/USAGE_EXAMPLES.md)
3. Vérifier l'API docs: http://localhost:8000/docs
4. Lire les logs (console du serveur + DevTools)

---

## 📄 License

Votre license ici

---

## 👤 Author

Créé pour moderniser le contrôle machine

---

**🚀 Ready to go! Lancez avec: `./START.sh`**

---

<details>
<summary><b>Commandes Rapides</b></summary>

```bash
# Démarrer tout
./START.sh

# Ou manuellement...

# Backend
cd "FRONT END"
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000

# Frontend (autre terminal)
cd "FRONT END/ui-react"
npm run dev

# Tester API
curl http://localhost:8000/api/health

# Voir docs
open http://localhost:8000/docs
open http://localhost:5173
```

</details>

---

**Last Updated:** 2026-05-07
