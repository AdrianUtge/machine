# 📋 Contexte Projet - Pour Prochaine Itération

## 🎯 Vue d'ensemble

Nous transformons une **console CLI Python** en une **application web moderne React + FastAPI** pour contrôler une machine Arduino.

### État du Projet
- ✅ Architecture complète implantée
- ✅ API REST fonctionnelle
- ✅ Frontend React connecté
- ✅ Communication série activée
- 🔄 En développement/amélioration continue

---

## 🏗️ Architecture Actuelle

```
┌─────────────────────────────────┐
│   React Frontend (Port 5173)    │
│   - ConnectionScreen.tsx        │
│   - StatusPanelSimple.tsx       │
│   - MotionControl.tsx           │
│   - SerialMonitor.tsx           │
│   - Hook: useMachineController  │
└──────────────┬──────────────────┘
               │ HTTP REST API
               ↓
┌─────────────────────────────────┐
│   FastAPI Backend (Port 8000)   │
│   - api.py (320 lignes)         │
│   - 20+ endpoints REST          │
│   - Lecture série complète      │
└──────────────┬──────────────────┘
               │ Utilise
               ↓
┌─────────────────────────────────┐
│   Logique Python (Préservée)    │
│   - SerialLink (com série)      │
│   - MachineController           │
│   - Protocol (parsing)          │
│   - MachineState (données)      │
└──────────────┬──────────────────┘
               │
               ↓
        Arduino (Série 115200)
```

---

## 📁 Structure Clé des Fichiers

### Backend
```
FRONT END/
├── api.py                          # API FastAPI (CŒUR)
├── config.py                       # Port/baud (défaut: /dev/cu.usbmodem101)
├── requirements.txt                # Deps: fastapi, uvicorn, pyserial
├── main.py                         # Ancien CLI (encore fonctionnel)
│
├── core/
│   ├── controller.py              # MachineController (IMPORTANT)
│   ├── state.py                   # MachineState dataclass
│   └── presets.py                 # Presets fréquence
│
└── comm/
    ├── serial_link.py             # Bas niveau série
    └── protocol.py                # Codage/décodage commandes
```

### Frontend
```
FRONT END/ui-react/
├── src/app/
│   ├── App.tsx                    # Point d'entrée (IMPORTANT)
│   │
│   ├── hooks/
│   │   ├── useMachineController.ts # Hook API (IMPORTANT)
│   │   └── USAGE_EXAMPLES.md      # 8+ exemples d'utilisation
│   │
│   └── components/
│       ├── ConnectionScreen.tsx    # Page connexion ports
│       ├── StatusPanelSimple.tsx   # Affiche état machine
│       ├── MotionControl.tsx       # Fréquence/vitesse/presets
│       ├── SerialMonitor.tsx       # Historique logs
│       └── figma/                  # Composants réutilisables
```

---

## 🚀 Démarrer le Projet

### Option 1: Automatique
```bash
./START.sh
# Lance les 2 serveurs en parallèle
```

### Option 2: Manuel
```bash
# Terminal 1 - Backend
cd "FRONT END"
source venv/bin/activate
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000

# Terminal 2 - Frontend
cd "FRONT END/ui-react"
npm run dev
```

### URLs
- Frontend: http://localhost:5173
- API: http://localhost:8000
- API Docs: http://localhost:8000/docs (Swagger)

---

## 📊 État Actuel - Ce Qui Fonctionne

### ✅ Implémenté et Testé

1. **Connexion au Port Série**
   - Page affiche les vrais ports détectés
   - Auto-sélectionne le premier port
   - Gère les erreurs de connexion

2. **Communication Série Complète**
   - Envoie les commandes: HOME, START, STOP, HARD_RESET
   - Réglage fréquence (Hz)
   - Réglage vitesse (%)
   - Presets (Humain 0.8Hz, Boeuf 0.4Hz, Souris 3.7Hz)
   - **Lit TOUS les retours** (jusqu'à 20 lignes par commande)

3. **Affichage État Machine**
   - État courant
   - Fréquence actuelle
   - Vitesse
   - Position
   - Courant moteur
   - Force capteur
   - Erreurs
   - Statut slave

4. **API REST Complète**
   - 20+ endpoints
   - Gestion d'erreur robuste
   - Fallback sur erreur
   - Documentation Swagger auto

5. **Interface React**
   - Connexion dynamique
   - Commandes en temps réel
   - Logs série
   - Responsive design

---

## 🔧 Problèmes Résolus

### ✅ Sessions précédentes ont corrigé:

1. **Installation Python** → Créé venv + installé dépendances
2. **ConnectionScreen** → Affiche vrais ports API
3. **Page blanche après connexion** → Fixé timing isConnected
4. **MachineState null** → Chargé immédiatement après connexion
5. **Crash StatusPanel** → Remplacé par StatusPanelSimple
6. **MotionControl undefined** → Passé pendingCommands
7. **Lecture série incomplète** → Boucle de 20 lectures max

---

## 📝 API Endpoints (Résumé)

### Connection
```
GET  /api/ports              # Lister ports
POST /api/connect            # Connexion
POST /api/disconnect         # Déconnexion
GET  /api/status             # État courant
```

### Commands
```
POST /api/command/home           # HOME
POST /api/command/start          # START
POST /api/command/stop           # STOP
POST /api/command/hard-reset     # HARD_RESET
POST /api/command/frequency      # SET_FREQ:X Hz
POST /api/command/speed          # SET_SPEED:Y %
POST /api/command/preset         # Appliquer preset (1/2/3)
POST /api/command/manual         # Commande libre
```

### Monitoring
```
GET /api/logs                # Logs série (100 derniers)
GET /api/logs/commands       # Historique commandes
GET /api/logs/console        # Logs console
GET /api/health              # Status
```

---

## 🧪 Testing Manual

### Test 1: Vérifier API
```bash
curl http://localhost:8000/api/health
# {"status":"ok","connected":true}

curl http://localhost:8000/api/ports
# {"ports":["/dev/cu.usbmodem101", ...]}
```

### Test 2: Vérifier Frontend
- Ouvrir http://localhost:5173
- Sélectionner un port
- Cliquer "Connect"
- Vérifier que l'état machine s'affiche

### Test 3: Vérifier Communication Série
- Dans App.tsx, console affiche les logs
- Ouvrir F12 > Console
- Voir les communications série en temps réel

---

## 🎯 Prochaines Étapes Possibles

### Court terme (Facile)
1. [ ] Ajouter rafraîchissement automatique de l'état (polling)
2. [ ] Améliorer MotionControl UI (mieux visualiser presets)
3. [ ] Ajouter animations lors des commandes
4. [ ] Paramétrer le timeout du port série

### Moyen terme (Modéré)
1. [ ] WebSocket pour updates temps réel (au lieu de polling)
2. [ ] GraphQL alternative à REST
3. [ ] Authentification (JWT)
4. [ ] Persistence des logs (database)
5. [ ] Statistiques/graphiques des commandes

### Long terme (Complexe)
1. [ ] Electron app (desktop standalone)
2. [ ] Mobile app (React Native)
3. [ ] Docker deployment complet
4. [ ] Tests unitaires/E2E
5. [ ] CI/CD pipeline

---

## 📚 Documentation Principale

- **QUICK_START.md** - Lancer en 5 secondes
- **README_CONTROL_PANEL.md** - Vue d'ensemble complète
- **FRONTEND_INTEGRATION.md** - Guide architecture détaillé
- **INSTALLATION_GUIDE.md** - Troubleshooting
- **USAGE_EXAMPLES.md** - 8+ exemples React
- **CHANGELOG.md** - Historique complet

---

## 🔑 Points Importants à Retenir

### Code Python
- ✅ **Zéro changement** à la logique existante (core/, comm/)
- ✅ Seulement **ajout** de `api.py`
- ✅ Ancien CLI (`main.py`) toujours fonctionnel
- ✅ Bonne séparation concerns

### Code React
- ✅ Hook custom `useMachineController` = tout ce qu'il faut
- ✅ Composants simples et composables
- ✅ TypeScript pour typage
- ✅ Tailwind CSS pour styles

### Communication Série
- ✅ Baudrate: 115200 (config.py)
- ✅ Format: `COMMANDE` ou `COMMANDE:VALEUR`
- ✅ Réponses: `CLÉ:VALEUR` (parsées par protocol.py)
- ✅ Lit maintenant TOUT ce qui arrive (max 20 lignes)

### Tests
- Pas de tests automatisés encore (opportunité!)
- Testing manuel via API Docs (Swagger)
- Console logs utiles pour déboguer

---

## 💡 Tips pour Continuer

1. **Avant de modifier** - Lire le fichier existant en entier
2. **Respecter l'architecture** - Backend/Frontend séparé
3. **Tester via API Docs** - Plus rapide que l'UI
4. **Vérifier les logs** - F12 Console et console du serveur
5. **Comprendre le hook** - useMachineController est la clé

---

## ❓ Questions Fréquentes

**Q: Où ajouter une nouvelle commande?**
A: 1) `core/presets.py` ou `comm/protocol.py`, 2) `api.py` endpoint, 3) UI React

**Q: Comment debug la communication série?**
A: Regarder les logs console (F12) et les logs serveur

**Q: Pourquoi certains props sont undefined?**
A: Vérifier qu'on les passe depuis App.tsx (cf StatusPanelSimple exemple)

**Q: Comment changer le port par défaut?**
A: Modifier `FRONT END/config.py` - `DEFAULT_PORT`

**Q: Pourquoi read_once() ne retourne rien?**
A: Port peut être fermé ou pas de données disponibles. Vérifier avec F12 > Network

---

## 📦 Commit Récents (pour contexte)

```
ade69d8 - fix: read all serial data (up to 20 lines)
477c0e3 - fix: pass pendingCommands to MotionControl
1cb21cc - fix: replace StatusPanel with simplified version
cf931ea - fix: improve JSON parsing for status API
1ceb3e5 - fix: add better error handling for status API
302ac66 - fix: load machine state immediately after connection
1c87fca - fix: ConnectionScreen displays real serial ports
68e2c95 - feat: Integrate React frontend with FastAPI backend
```

---

**Créé**: 2026-05-07  
**Status**: Fonctionnel - Prêt pour amélioration  
**Dernier Test**: ✅ Connexion → État Machine → Commandes
