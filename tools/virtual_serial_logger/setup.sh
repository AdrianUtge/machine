#!/bin/bash

echo "═════════════════════════════════════════════════"
echo "Virtual Serial Port Logger - Setup"
echo "═════════════════════════════════════════════════"
echo ""

# Détecter l'OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "🍎 macOS détecté"

    # Vérifier si Homebrew est installé
    if ! command -v brew &> /dev/null; then
        echo "❌ Homebrew non trouvé. Installez-le:"
        echo "   https://brew.sh"
        exit 1
    fi

    # Installer socat
    if ! command -v socat &> /dev/null; then
        echo "📦 Installation de socat..."
        brew install socat
    else
        echo "✓ socat déjà installé"
    fi

elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "🐧 Linux détecté"

    # Installer socat
    if ! command -v socat &> /dev/null; then
        echo "📦 Installation de socat..."
        sudo apt-get update
        sudo apt-get install -y socat
    else
        echo "✓ socat déjà installé"
    fi
fi

# Vérifier si pyserial est installé
if ! python3 -c "import serial" 2>/dev/null; then
    echo "📦 Installation de pyserial..."
    pip3 install pyserial
else
    echo "✓ pyserial déjà installé"
fi

# Rendre les scripts exécutables
chmod +x virtual_serial.py test_serial.py create_virtual_port.sh

echo ""
echo "═════════════════════════════════════════════════"
echo "✅ Installation complète!"
echo "═════════════════════════════════════════════════"
echo ""
echo "📖 Utilisation:"
echo ""
echo "  Option 1 - Avec socat directement:"
echo "    ./create_virtual_port.sh"
echo ""
echo "  Option 2 - Avec logging Python:"
echo "    python3 virtual_serial.py"
echo ""
echo "Ensuite, connectez-vous avec:"
echo "    python3 test_serial.py /dev/ttys000 -d 'hello'"
echo ""
