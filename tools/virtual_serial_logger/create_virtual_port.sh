#!/bin/bash
# Simple wrapper to create virtual serial ports with socat

echo "════════════════════════════════════════════════════════"
echo "Virtual Serial Port Creator"
echo "════════════════════════════════════════════════════════"
echo ""

# Vérifier que socat est installé
if ! command -v socat &> /dev/null; then
    echo "✗ socat n'est pas installé"
    echo "  Installation: brew install socat"
    exit 1
fi

# Créer deux PTYs connectés
echo "🔌 Création de ports virtuels..."
echo ""
echo "Appuyez sur Ctrl+C pour arrêter"
echo ""

socat -d -d pty,raw,echo=0 pty,raw,echo=0
