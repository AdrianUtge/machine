# Virtual Serial Port Logger

Script Python pour créer un port série virtuel et enregistrer toutes les données envoyées.
Les ports créés apparaissent dans `ls /dev/ttys*` (ports virtuels PTY).

## Installation

### Prérequis
- **socat**: Pour créer les ports virtuels
- **pyserial**: Pour le script de test

### Installation rapide
```bash
./setup.sh
```

### Installation manuelle
```bash
# macOS
brew install socat
pip3 install pyserial

# Linux
sudo apt-get install socat
pip3 install pyserial
```

## Utilisation

### 1. Démarrer le logger du port série virtuel

```bash
python3 virtual_serial.py
```

Le script affichera les noms des deux ports virtuels créés:
- **Port 1** (`/dev/ttys000`): À utiliser dans votre application
- **Port 2** (`/dev/ttys001`): L'autre extrémité (optionnel)

Exemple:
```
✓ Ports virtuels créés:
  Port 1: /dev/ttys000
  Port 2: /dev/ttys001

✓ Fichier log: serial_log.txt

Connectez votre application à: /dev/ttys000
```

Options:
```bash
python3 virtual_serial.py -o my_log.txt  # Spécifier un fichier de log personnalisé
```

### 2. Envoyer des données au port (autre terminal)

#### Mode interactif:
```bash
python3 test_serial.py /dev/ttys000
```

Vous pouvez alors taper du texte librement, chaque ligne est envoyée.

#### Envoyer des données spécifiques:
```bash
python3 test_serial.py /dev/ttys000 -d "message1;message2;message3"
```

#### Avec un baudrate personnalisé:
```bash
python3 test_serial.py /dev/ttys000 -b 115200 -d "hello"
```

### 3. Connecter votre application

Utilisez simplement le port affichée par `virtual_serial.py` dans votre application.
Par exemple, si c'est `/dev/ttys000`:

**Python:**
```python
import serial
ser = serial.Serial('/dev/ttys000', 9600)
ser.write(b'Hello World\n')
```

**C/C++:**
```c
// Utiliser open() et write() sur le port
int fd = open("/dev/ttys000", O_RDWR);
write(fd, "Hello World\n", 12);
```

## Exemple complet

**Terminal 1 - Démarrer le logger:**
```bash
$ python3 virtual_serial.py
✓ Port série virtuel créé: /dev/ttys000
✓ Fichier log: serial_log.txt

Connectez votre application au port: /dev/ttys000
Appuyez sur Ctrl+C pour arrêter...
```

**Terminal 2 - Envoyer des données:**
```bash
$ python3 test_serial.py /dev/ttys000 -d "HELLO;WORLD;TEST"
✓ Connecté à /dev/ttys000
→ Envoyé: HELLO
→ Envoyé: WORLD
→ Envoyé: TEST
✓ Port fermé
```

**Fichier log généré (serial_log.txt):**
```
[2026-05-08 14:25:33.421] REÇU: 'HELLO\n'
[2026-05-08 14:25:33.922] REÇU: 'WORLD\n'
[2026-05-08 14:25:34.423] REÇU: 'TEST\n'
```

## Notes

- Le port virtuel existe seulement pendant que `virtual_serial.py` tourne
- Les données sont loggées en temps réel dans le fichier et sur la console
- Le script tente de décoder les données en UTF-8, sinon affiche en hexadécimal
- Appuyez sur `Ctrl+C` pour arrêter le logger

## Vérifier les ports avec `ls`

```bash
# Voir tous les ports virtuels créés
ls /dev/ttys*

# Voir les ports en temps réel
ls -la /dev/ttys* | grep ttys

# Chercher les ports avant/après le lancement
# Terminal 1: lancer le logger
python3 virtual_serial.py

# Terminal 2: voir les nouveaux ports
ls /dev/ttys* | tail -2
```

Exemple:
```bash
$ ls /dev/ttys*
/dev/ttys000
/dev/ttys001
```

## Voir les ports avec `ls /dev/cu.*`

Les ports virtuels créés apparaissent dans `/dev/ttys*`:

```bash
ls /dev/ttys*
```

Résultat:
```
/dev/ttys000  /dev/ttys001
```

Pour voir aussi dans `/dev/cu.*`, il suffit de créer des liens symboliques:

```bash
# (optionnel) Créer des liens /dev/cu.* vers /dev/ttys*
sudo ln -s /dev/ttys000 /dev/cu.virtual0
sudo ln -s /dev/ttys001 /dev/cu.virtual1
```

Ensuite:
```bash
$ ls /dev/cu.virtual*
/dev/cu.virtual0  /dev/cu.virtual1
```

## Troubleshooting

**socat: command not found**
```bash
brew install socat
```

**"Port not found":**
- Assurez-vous que le port affiché par le logger est correct
- Vérifiez que le logger tourne toujours: `ps aux | grep socat`

**"Permission denied":**
- Normalement pas besoin de `sudo` pour les PTYs
- Vérifiez les permissions: `ls -la /dev/ttys000`

**Données manquantes:**
- Vérifiez le baudrate (doit être le même dans le logger et l'application)

**Le port ne se voit pas dans `ls /dev/ttys*`:**
- Lancer le logger, puis dans un autre terminal:
  ```bash
  ls /dev/ttys* | wc -l  # Avant
  # (lancer le logger)
  ls /dev/ttys* | wc -l  # Après (doit augmenter de 2)
  ```
