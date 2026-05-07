# 📦 Guide d'Installation - Control Panel

## ⚡ Démarrage Rapide

### Méthode 1: Script Automatique (Recommandé ⭐)

```bash
./START.sh
```

Le script va:
- ✅ Créer un environnement virtuel Python
- ✅ Installer les dépendances Python
- ✅ Installer les dépendances Node
- ✅ Lancer les deux serveurs automatiquement

**Puis ouvrir:** http://localhost:5173

---

## 🔧 Méthode 2: Manuel Étape par Étape

### Step 1: Environnement Python

```bash
cd "FRONT END"

# Créer venv
python3 -m venv venv

# Activer venv
source venv/bin/activate

# Installer dépendances
pip install --upgrade pip setuptools wheel
pip install pyserial fastapi uvicorn pydantic
```

### Step 2: Backend FastAPI

```bash
# Depuis le terminal où venv est activé
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000
```

L'API sera disponible à: `http://localhost:8000`  
Documentation: `http://localhost:8000/docs`

### Step 3: Frontend React (Nouveau Terminal)

```bash
cd "FRONT END/ui-react"

# Installer dépendances
npm install

# Lancer dev server
npm run dev
```

Le frontend sera disponible à: `http://localhost:5173`

---

## ❌ Problèmes Courants & Solutions

### Problème 1: `ModuleNotFoundError: No module named 'pip'`

**Cause:** Python 3.14+ a parfois pip mal configuré

**Solution:**
```bash
python3 -m ensurepip --upgrade
```

### Problème 2: `maturin pep517 build-wheel` erreur

**Cause:** Problème de compilation de dépendance native

**Solution:**
```bash
# Créer un venv propre
python3 -m venv venv
source venv/bin/activate

# Upgrader pip first
pip install --upgrade pip setuptools wheel

# Puis installer les dépendances
pip install pyserial fastapi uvicorn pydantic
```

### Problème 3: Port 8000 ou 5173 déjà utilisé

**Solution:**
```bash
# Trouver le process
lsof -i :8000   # ou :5173
kill -9 <PID>

# Ou changer le port dans vite.config.ts:
# export default defineConfig({
#   server: {
#     port: 5174  // Changer ici
#   }
# })
```

### Problème 4: `Arduino not found` / Port série invalide

**Solution:**
```bash
# Vérifier les ports disponibles
ls /dev/cu.*    # macOS
ls /dev/ttyUSB* # Linux

# Mettre à jour dans FRONT END/config.py:
DEFAULT_PORT = "/dev/cu.usbmodem101"  # ← Votre port ici
```

### Problème 5: Node modules corruption

**Solution:**
```bash
cd "FRONT END/ui-react"
rm -rf node_modules package-lock.json
npm install
```

### Problème 6: Permission denied sur START.sh

**Solution:**
```bash
chmod +x START.sh
./START.sh
```

---

## ✅ Vérifier que tout marche

### Test 1: API Health

```bash
curl http://localhost:8000/api/health
```

**Réponse attendue:**
```json
{"status":"ok","connected":false}
```

### Test 2: Lister les ports

```bash
curl http://localhost:8000/api/ports
```

**Réponse attendue:**
```json
{"ports":["/dev/cu.usbmodem101", ...]}
```

### Test 3: Frontend chargé

Ouvrir: http://localhost:5173  
Vous devriez voir l'interface de connexion

### Test 4: API Documentation

Ouvrir: http://localhost:8000/docs  
Vous devriez voir Swagger UI

---

## 📋 Checklist Prérequis

- [ ] Python 3.8+
- [ ] Node.js 16+
- [ ] npm ou pnpm
- [ ] Arduino connecté (optionnel pour tester UI)
- [ ] Git (pour cloner si nécessaire)

Vérifier:
```bash
python3 --version  # 3.8+
node --version     # 16+
npm --version      # 8+
```

---

## 🎯 Workflows Recommandés

### Workflow 1: Développement Normal

```bash
# Terminal 1
./START.sh
# Lance les deux serveurs automatiquement

# Terminal 2 (optionnel, pour logs)
# Voir les logs dans le premier terminal
```

### Workflow 2: Debug Backend

```bash
# Terminal 1
cd "FRONT END"
source venv/bin/activate
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000

# Terminal 2
cd "FRONT END/ui-react"
npm run dev

# Ouvrir http://localhost:8000/docs pour tester API
```

### Workflow 3: Debug Frontend

```bash
# Terminal 1
cd "FRONT END"
source venv/bin/activate
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000

# Terminal 2
cd "FRONT END/ui-react"
npm run dev

# Ouvrir http://localhost:5173
# Appuyer F12 pour DevTools
```

---

## 🔄 Mise à Jour des Dépendances

### Vérifier les mises à jour

```bash
# Python
pip list --outdated

# Node
npm outdated
```

### Mettre à jour

```bash
# Python
pip install --upgrade pyserial fastapi uvicorn pydantic

# Node
npm update
```

---

## 🗑️ Nettoyer

Si vous voulez repartir de zéro:

```bash
# Supprimer venv Python
cd "FRONT END"
rm -rf venv

# Supprimer node_modules
cd "ui-react"
rm -rf node_modules package-lock.json

# Puis relancer START.sh
./START.sh
```

---

## 📚 Ressources

- **FastAPI Docs:** https://fastapi.tiangolo.com
- **React Docs:** https://react.dev
- **Vite Docs:** https://vitejs.dev
- **PySerial Docs:** https://pyserial.readthedocs.io

---

## 🆘 Besoin d'Aide?

1. **Lire** ce guide au complet
2. **Consulter** FRONTEND_INTEGRATION.md
3. **Vérifier** http://localhost:8000/docs
4. **Voir les logs** dans la console

---

## ✨ Notes

- `venv/` est créé automatiquement et peut être supprimé
- `node_modules/` contient ~280 packages, peut être long à installer
- Les deux serveurs se rechargent automatiquement (Hot Reload)
- Port 8000: Backend Python
- Port 5173: Frontend React

---

**🚀 Ready? Lancez: `./START.sh`**
