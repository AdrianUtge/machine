# tools/config — WiFiManager

Outil de **détection des interfaces WiFi** du PC et de **test du lien** vers
l'ESP8266. Utilisé par le backend (`api.py` l'importe via `sys.path`) et
exécutable seul pour diagnostiquer.

## Configuration

⚠️ La configuration n'est **plus** dans `setup.json`. Tout est désormais centralisé
dans **`.machine_config.ini`** à la racine `PROD/` (source unique, gitignorée).
Voir [`../../docs/09_DEPLOYMENT.md`](../../docs/09_DEPLOYMENT.md).

`WiFiManager` lit automatiquement `.machine_config.ini` (en remontant
l'arborescence) et expose `self.config` au format historique
`{wifi, nodeMcu, serial}`. Un fallback legacy sur `setup.json` existe encore
pour compatibilité, mais le `.ini` prime.

## Utilisation

```bash
# Diagnostic complet du lien WiFi -> ESP8266
python3 tools/config/wifi_manager.py
```

```python
from wifi_manager import WiFiManager

m = WiFiManager()
print(m.get_available_wifi_interfaces())   # interfaces WiFi du PC
print(m.config["nodeMcu"]["ip"])           # 192.168.4.1 (depuis le .ini)
m.connect_to_nodeMcu()                       # exige d'être sur le bon WiFi
m.get_status()                               # GET /api/status (Bearer)
```

## Dépendances

```bash
pip install requests          # configparser est dans la stdlib
```

## Dépannage

- « Not on correct WiFi network » → connectez le PC au SSID de l'ESP
  (`[wifi].ssid` du `.ini`), via le **menu système** (pas en CLI).
- « Could not connect to NodeMCU » → ESP alimenté ? IP/port/jeton corrects ?
  `curl -H "Authorization: Bearer <key>" http://192.168.4.1:8080/api/status`.

Plus de détails : [`../../docs/07_WIFI_MODE.md`](../../docs/07_WIFI_MODE.md) et
[`../../docs/11_TROUBLESHOOTING.md`](../../docs/11_TROUBLESHOOTING.md).
