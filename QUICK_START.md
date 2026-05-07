# 🚀 Quick Start - Control Panel

## 5 Secondes

```bash
./START.sh
```

Puis ouvrir: **http://localhost:5173**

---

## 1 Minute (Si Script Échoue)

### Terminal 1 - Backend
```bash
cd "FRONT END"
source venv/bin/activate
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000
```

### Terminal 2 - Frontend
```bash
cd "FRONT END/ui-react"
npm run dev
```

---

## ✅ Vérifier l'Installation

```bash
# Test 1: Health Check
curl http://localhost:8000/api/health

# Test 2: Liste des ports
curl http://localhost:8000/api/ports

# Test 3: UI
# Ouvrir http://localhost:5173

# Test 4: API Docs
# Ouvrir http://localhost:8000/docs
```

---

## 🎯 Architectures

```
React UI (5173) → FastAPI (8000) → Python Logic → Arduino
```

---

## 📍 URLs

| Service | URL |
|---------|-----|
| **Frontend** | http://localhost:5173 |
| **API** | http://localhost:8000 |
| **API Docs** | http://localhost:8000/docs |

---

## 🔧 Configuration

- **Port Série**: Voir `FRONT END/config.py`
- **Presets**: Voir `FRONT END/core/presets.py`
- **API**: Voir `FRONT END/api.py`

---

## 📚 Docs Complètes

| Doc | Contenu |
|-----|---------|
| **[README_CONTROL_PANEL.md](./README_CONTROL_PANEL.md)** | Vue d'ensemble |
| **[FRONTEND_INTEGRATION.md](./FRONTEND_INTEGRATION.md)** | Guide complet |
| **[INSTALLATION_GUIDE.md](./INSTALLATION_GUIDE.md)** | Installation détaillée |
| **[USAGE_EXAMPLES.md](./FRONT%20END/ui-react/src/app/hooks/USAGE_EXAMPLES.md)** | Exemples code |

---

## ⚡ Commandes Utiles

```bash
# Installer from scratch
rm -rf "FRONT END/venv" "FRONT END/ui-react/node_modules"
./START.sh

# Vérifier les ports en écoute
lsof -i :8000
lsof -i :5173

# Tuer un process
kill -9 <PID>

# Logs API
# Voir la console du backend

# Logs Frontend
# Ouvrir DevTools (F12) dans le navigateur
```

---

## ✨ Features

- ✅ Interface moderne (design Figma)
- ✅ API REST complète
- ✅ Hook React réutilisable
- ✅ Auto-reload (Hot Module Replacement)
- ✅ Swagger documentation
- ✅ Error handling robuste

---

## 🎯 Prochaines Étapes

1. **Adapter les composants React** si nécessaire
2. **Connecter un Arduino** et tester
3. **Ajouter WebSocket** pour temps réel (optionnel)
4. **Déployer** avec Docker (optionnel)

---

## 🆘 Erreur?

1. Lire [INSTALLATION_GUIDE.md](./INSTALLATION_GUIDE.md)
2. Vérifier les ports: `lsof -i :8000`
3. Consulter http://localhost:8000/docs
4. Voir la console pour les logs

---

**Lancez maintenant: `./START.sh` 🎉**

