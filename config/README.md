# WiFi & NodeMCU Configuration Setup

Ce dossier contient la configuration pour la connexion WiFi et la communication avec le NodeMCU.

## 📋 Structure

```
config/
├── setup.json          # Fichier de configuration (éditable avec un éditeur de texte)
├── wifi_manager.py     # Script Python pour gérer la connexion
├── README.md          # Ce fichier
└── logs/              # Dossier pour les logs
```

## 🔧 Configuration (setup.json)

Le fichier `setup.json` contient toute la configuration nécessaire. **Éditez-le avec votre éditeur de texte préféré** (VS Code, Sublime, Notepad, etc.):

```json
{
  "wifi": {
    "ssid": "YOUR_WIFI_SSID",              // Nom du réseau WiFi
    "password": "YOUR_WIFI_PASSWORD",      // Mot de passe WiFi
    "timeout": 10                           // Timeout en secondes
  },
  "nodeMcu": {
    "ip": "192.168.1.100",                 // Adresse IP du NodeMCU
    "port": 80,                             // Port HTTP/HTTPS
    "key": "YOUR_SECRET_KEY_HERE",         // Clé secrète pour l'authentification
    "protocol": "http"                      // http ou https
  },
  "serial": {
    "port": "auto",                        // Port série (auto détection)
    "baudrate": 115200,                    // Vitesse de communication
    "timeout": 1
  },
  "logging": {
    "level": "INFO",                       // DEBUG, INFO, WARNING, ERROR
    "file": "logs/setup.log"               // Fichier de log
  },
  "debug": false                            // Mode debug (true/false)
}
```

## 📝 Instructions d'Installation

### 1. Éditer setup.json

Ouvrez `config/setup.json` avec votre éditeur de texte et remplissez les informations:

```bash
# Exemple sur macOS/Linux
nano config/setup.json

# Ou avec VS Code
code config/setup.json
```

**Informations à remplir:**
- `wifi.ssid`: Le nom exact de votre réseau WiFi
- `wifi.password`: Le mot de passe du WiFi
- `nodeMcu.ip`: L'adresse IP du NodeMCU sur votre réseau local
- `nodeMcu.key`: Une clé secrète pour l'authentification (peut être n'importe quel string)

### 2. Tester la connexion

```bash
python3 config/wifi_manager.py
```

Le script va:
1. ✓ Vérifier le WiFi actuel
2. ✓ Comparer avec la configuration
3. ✓ Connecter au NodeMCU si le WiFi match
4. ✓ Afficher le statut

## 🔑 Utilisation en Python

```python
from config.wifi_manager import WiFiManager

# Créer un gestionnaire
manager = WiFiManager()

# Vérifier le WiFi correct
if manager.is_correct_wifi():
    # Connecter au NodeMCU
    if manager.connect_to_nodeMcu():
        # Envoyer une commande
        response = manager.send_command("HOME")
        print(f"Réponse: {response}")
        
        # Récupérer le statut
        status = manager.get_status()
        print(f"Statut: {status}")
```

## 🌐 Utilisation avec la React App

Dans l'application React (FastAPI), utilisez ce gestionnaire pour:

```python
from config.wifi_manager import WiFiManager

manager = WiFiManager()

# Dans le endpoint de connexion
@app.post("/api/connect")
async def connect(port: str):
    if not manager.is_correct_wifi():
        raise HTTPException(status_code=400, detail="Wrong WiFi network")
    
    if not manager.connect_to_nodeMcu():
        raise HTTPException(status_code=500, detail="NodeMCU connection failed")
    
    # Continuer avec la connexion série...
```

## 🚀 Intégration FastAPI

Exemple d'intégration dans votre `api.py`:

```python
from config.wifi_manager import WiFiManager

# Instance globale
wifi_manager = WiFiManager()

@app.on_event("startup")
async def startup_event():
    """Vérifier la connexion WiFi au démarrage"""
    if wifi_manager.is_correct_wifi():
        logger.info("✓ Bon WiFi détecté")
        if wifi_manager.connect_to_nodeMcu():
            logger.info("✓ NodeMCU connecté")
    else:
        logger.warning(f"Wrong WiFi. Expected: {wifi_manager.config['wifi']['ssid']}")
```

## ℹ️ Notes Importantes

- **setup.json doit être valide JSON** - Vérifiez la syntaxe si ça ne marche pas
- **Clé secrète**: Utilisez une clé forte et sécurisée (min 20 caractères)
- **WiFi case-sensitive**: Le SSID doit correspondre exactement (majuscules/minuscules)
- **IP du NodeMCU**: Vérifiez l'IP avec votre routeur (routeur → DHCP clients)

## 🔍 Dépannage

### "Not on correct WiFi network"
```
Solution: Vérifiez que:
1. Vous êtes connecté au bon WiFi
2. Le SSID dans setup.json est correct
3. SSID est case-sensitive
```

### "Could not connect to NodeMCU"
```
Solution: Vérifiez que:
1. Le NodeMCU est alimenté
2. L'IP est correcte (ping 192.168.x.x)
3. Le port 80 est accessible
4. La clé secrète est correcte
```

### "Invalid JSON in config file"
```
Solution: Validez le JSON sur:
- https://jsonlint.com/
- Ou utilisez: python3 -m json.tool config/setup.json
```

## 📦 Dépendances

```bash
pip install requests
```

## 📜 Licence

Configuration et scripts pour le contrôle du machine TIMC.
