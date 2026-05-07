#!/bin/bash

echo "🚀 Démarrage du Control Panel (React + FastAPI)"
echo "=============================================="

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Vérifier les dépendances Python
echo "📦 Vérification des dépendances Python..."
if [ ! -d "FRONT END/venv" ]; then
    echo "   ⚠️  Créant environnement virtuel Python..."
    cd "FRONT END"
    python3 -m venv venv
    source venv/bin/activate
    pip install --upgrade pip setuptools wheel
    pip install pyserial fastapi uvicorn pydantic
    cd ..
fi

# Vérifier les dépendances Node
echo "📦 Vérification des dépendances Node..."
if [ ! -d "FRONT END/ui-react/node_modules" ]; then
    echo "   ⚠️  Installant dépendances React..."
    cd "FRONT END/ui-react"
    npm install
    cd ../../
fi

echo ""
echo "✅ Dépendances vérifiées"
echo ""

# Démarrer le backend dans un terminal
echo "🔧 Démarrage du backend FastAPI (port 8000)..."
cd "FRONT END"
source venv/bin/activate
python -m uvicorn api:app --reload --host 0.0.0.0 --port 8000 &
BACKEND_PID=$!

sleep 2

# Démarrer le frontend dans un autre terminal
echo "🎨 Démarrage du frontend React (port 5173)..."
cd "ui-react"
npm run dev &
FRONTEND_PID=$!

echo ""
echo "✅ Application démarrée!"
echo ""
echo "📍 Frontend: http://localhost:5173"
echo "📍 API Docs: http://localhost:8000/docs"
echo ""
echo "Appuyez sur Ctrl+C pour arrêter..."
echo ""

# Garder le script actif
wait
