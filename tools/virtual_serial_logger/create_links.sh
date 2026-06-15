#!/bin/bash

# Create /dev/cu.* symlinks for virtual serial ports
# Usage: ./create_links.sh

echo "═════════════════════════════════════════════════"
echo "Créer les liens /dev/cu.* pour les ports virtuels"
echo "═════════════════════════════════════════════════"
echo ""

# Check if any /dev/ttys* ports exist
if ! ls /dev/ttys* > /dev/null 2>&1; then
    echo "✗ Aucun port /dev/ttys* trouvé"
    echo "  Assurez-vous que le logger est en cours d'exécution:"
    echo "    python3 virtual_serial.py"
    exit 1
fi

echo "Ports PTY trouvés:"
ls -la /dev/ttys*
echo ""

# Create symlinks
COUNTER=0
for tty in /dev/ttys*; do
    LINK="/dev/cu.virtualserial$COUNTER"

    # Remove existing link if it exists
    if [ -L "$LINK" ]; then
        echo "Suppression du lien existant: $LINK"
        sudo rm "$LINK"
    fi

    # Create new symlink
    echo "Création du lien: $LINK -> $tty"
    sudo ln -s "$tty" "$LINK"

    COUNTER=$((COUNTER + 1))
done

echo ""
echo "═════════════════════════════════════════════════"
echo "✓ Liens créés avec succès!"
echo "═════════════════════════════════════════════════"
echo ""
echo "Vérification:"
ls -la /dev/cu.virtualserial*
echo ""
echo "Utilisation:"
echo "  python3 test_serial.py /dev/cu.virtualserial0 -d 'hello'"
echo ""
