# 10 — Débogage

## Niveaux de log (backend)

| Niveau   | Montre                                        | Comment l'activer            |
|----------|-----------------------------------------------|------------------------------|
| ERROR    | échecs                                         | (toujours)                   |
| WARNING  | anomalies non bloquantes                       | (toujours)                   |
| INFO     | connexion, commandes, changements d'état       | défaut                       |
| DEBUG    | **requêtes API**, **commandes**, **réponses**  | `-v` / `MACHINE_VERBOSE=v`   |
| TRACE    | tout (payloads bruts)                          | `-vv` / `MACHINE_VERBOSE=vv` |

```bash
python api.py -v               # DEBUG
python api.py -vv              # TRACE
python api.py --log-level WARNING
./run.sh --verbose             # DEBUG en production
./dev.sh                       # DEBUG (uvicorn) ; ./dev.sh --verbose => TRACE
```

Le mode verbose affiche, comme demandé : les **requêtes API** (middleware
`log_requests`), les **commandes envoyées**, les **réponses reçues** et les
**changements d'état** (via `log_action`).

## Le moniteur série dans l'UI

Activer le mode **Advanced** (toggle en haut) → panneau **SerialMonitor** :
toutes les lignes TX (`>`) / RX (`<`) et l'historique des commandes. Endpoints
sous-jacents : `/api/logs`, `/api/logs/commands`, `/api/logs/console`.

## Tester chaque couche isolément

### Backend (sans matériel)
```bash
curl http://localhost:8000/api/health
curl http://localhost:8000/api/ports
```

### Lien backend ↔ ESP (mode WiFi)
```bash
# PC connecté au WiFi de l'ESP :
curl -H "Authorization: Bearer <key>" http://192.168.4.1:8080/api/status
python3 tools/config/wifi_manager.py     # diagnostic complet du lien
```

### ESP (logs USB)
```bash
cd firmware/ESP8266 && pio device monitor -b 115200
# Doit afficher : AP démarré, IP 192.168.4.1, [REST]..., [OpenRB] > ...
```

### OpenRB (logs USB)
```bash
cd firmware/OPENRB150 && pio device monitor -b 115200 --dtr 0 --rts 0
# Au boot : ACK:BOOT, STATE:..., puis le burst de statut toutes les 100 ms
```

## Mesurer la latence du lien

L'UI affiche une latence (EMA du round-trip `GET /api/status`).
- valeur stable basse (~dizaines de ms) = lien sain ;
- `-1` ou pics = lien coupé / brownout (voir [11_TROUBLESHOOTING.md](11_TROUBLESHOOTING.md)).

## Vérifier la config chargée

```bash
cd backend
python3 -c "import machine_config as m; print('ini:', m.config_path()); \
print('nodemcu:', {**m.nodemcu(),'key':'***'}); print('serial:', m.serial())"
```

## Points d'observation par symptôme

| Symptôme                         | Où regarder                                         |
|----------------------------------|-----------------------------------------------------|
| UI ne se connecte pas            | logs backend `-v`, `/api/health`, bon WiFi ?        |
| Connecté mais pas de données     | logs ESP (`[OpenRB] >`), `rb_online` dans /api/status|
| 404 sur une commande             | route présente dans api.py ? backend redémarré ?    |
| Stepper ne tourne pas            | logs OpenRB (`ACK:START`), câblage ENA/PUL, fréquence|
| Forces toujours nulles/aberrantes| calibration placeholder (cf. [05_OPENRB150.md](05_OPENRB150.md)) |
