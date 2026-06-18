# 03 — Backend (Python + FastAPI)

Dossier : `backend/`. Point d'entrée : `api.py`.

## Arborescence

```
backend/
├── api.py                 # ★ routes REST + middleware log + instance controller
├── machine_config.py      # lit .machine_config.ini (config + secrets)
├── config.py              # constantes série + tailles buffers (depuis le .ini)
├── requirements.txt       # pyserial, fastapi, uvicorn, requests
├── cal_330.txt            # table de calibration force (volts↔newton)
├── comm/
│   ├── protocol.py        # commandes (cmd_*) + parsing (parse_response)
│   ├── wifi_link.py       # transport HTTP -> ESP8266 (mode WiFi)
│   ├── serial_link.py     # transport série USB (mode DEV)
│   ├── ports.py           # détection/sélection des ports série
│   └── force_cal.py       # interpolation volts -> newton (cal_330.txt)
├── core/
│   ├── controller.py      # MachineController : intentions -> protocole -> lien
│   ├── state.py           # MachineState (dataclass) : modèle unique
│   ├── presets.py         # presets fréquence intégrés (Humain/Bœuf/Souris)
│   └── preset_store.py    # presets utilisateur persistés (preset.json)
└── debug/
    ├── logging_setup.py   # logs structurés (ERROR..TRACE, --verbose)
    └── logger.py          # buffers du moniteur série pour l'UI
```

## Démarrer

```bash
cd backend
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python api.py            # INFO
python api.py -v         # DEBUG (requêtes, commandes, réponses)
python api.py -vv        # TRACE (tout)
python api.py --log-level WARNING --port 8000
```

(En pratique : `./run.sh` / `./dev.sh` font tout ça.)

## Endpoints (résumé)

| Méthode | Route                         | Rôle                                   |
|---------|-------------------------------|----------------------------------------|
| GET     | `/api/health`                 | sonde de vie                           |
| GET     | `/api/ports`                  | ports série disponibles                |
| GET     | `/api/wifi-interfaces`        | interfaces WiFi du PC                   |
| POST    | `/api/connect`                | connexion (WiFi ou série selon `port`) |
| POST    | `/api/disconnect`             | déconnexion                            |
| GET     | `/api/status`                 | état machine (cache ESP en WiFi)       |
| POST    | `/api/command/home`           | HOME                                    |
| POST    | `/api/command/start`          | START (mémorise cycle_start)           |
| POST    | `/api/command/stop`           | STOP (= hard_reset actuellement)       |
| POST    | `/api/command/hard-reset`     | HARD_RESET                             |
| POST    | `/api/command/frequency`      | régler la fréquence                    |
| POST    | `/api/command/speed`          | régler T_Speed                         |
| POST    | `/api/command/force`          | régler la force (globale ou `sensor`)  |
| POST    | `/api/command/goto`           | déplacer une table                     |
| POST    | `/api/command/torque`         | lock/unlock Dynamixel (`on`)           |
| POST    | `/api/command/manual`         | envoyer une commande brute             |
| GET/POST/DELETE | `/api/presets[...]`   | presets utilisateur (CRUD + apply)     |
| GET     | `/api/logs[,/commands,/console]` | logs du moniteur série             |

## Choix d'implémentation importants

- **Instance `controller` globale** : l'API pilote **un seul** banc.
- **WiFi vs série** : `_is_wifi_interface(port)` choisit le transport. En WiFi,
  `/api/status` lit le **cache de l'ESP** (`WiFiLink.get_status()`) au lieu
  d'envoyer un `GET_STATUS` sur la série → faible latence.
- **`background_reader`** (thread) : seulement en mode série, lit la ligne en
  continu pour alimenter le moniteur. En WiFi, le streaming/cache suffit.
- **Slave ONLINE/OFFLINE** : déduit de `last_data_ts` (donnée fraîche < 3 s),
  pas de la ligne `SLAVE:` brute.
- **Reprise après reload** : à la connexion WiFi, le backend relit `cycle_start`,
  `frequency`, `forces` depuis l'ESP → l'état survit à un rechargement du front.

## Configuration & secrets

Tout vient de `.machine_config.ini` (voir [09_DEPLOYMENT.md](09_DEPLOYMENT.md)) :
- `machine_config.py` : accès typés + vue `as_nested_dict()` (compat ancien JSON).
- `config.py` : `DEFAULT_PORT/BAUDRATE/TIMEOUT` depuis `[serial]`.
- `WiFiManager` (`tools/config/wifi_manager.py`) : IP/port/jeton depuis `[nodemcu]`.

## Logs

- `debug/logging_setup.py` : niveaux `ERROR/WARNING/INFO/DEBUG/TRACE`, méthode
  `.trace()` ajoutée. `setup_logging(verbose=…)` configure un handler unique.
- Sous **uvicorn** (`dev.sh`), le niveau vient de l'env `MACHINE_VERBOSE` (lu à
  l'import d'`api.py`). En **`python api.py`**, il vient de `--verbose`/`--log-level`.
- Le **middleware** `log_requests` logge chaque requête (méthode, chemin, statut,
  durée) en DEBUG → le polling reste silencieux en mode normal.
