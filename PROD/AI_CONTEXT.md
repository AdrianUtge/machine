# AI_CONTEXT — reprendre ce projet sans aucun historique

> Ce fichier permet à un développeur (humain ou IA) **n'ayant jamais vu ce
> projet** de devenir productif immédiatement. Lis-le en entier avant de toucher
> au code. Détails dans [`docs/`](docs/).

---

## WHAT THIS PROJECT IS

Banc de **mesure de force** piloté par ordinateur. Une UI web règle une
fréquence d'oscillation (moteur pas-à-pas), positionne 4 tables (servos
Dynamixel) et lit 4 cellules de force. 4 couches :

```
React UI ──HTTP:8000──▶ FastAPI ──HTTP:8080──▶ ESP8266 (WiFi AP) ──série 19200──▶ OpenRB-150
 frontend/              backend/               firmware/ESP8266                  firmware/OPENRB150
```

Deux modes de liaison : **WiFi** (production) et **USB série** (dev). Le code
backend est agnostique du transport (`WiFiLink` / `SerialLink`, même interface).

---

## HOW IT WORKS

### Descendant (commande)
`React → POST /api/command/* → api.py → MachineController → protocol.cmd_*() →
WiFiLink (traduit en JSON REST) → ESP /api/command → série → OpenRB dispatch()`.

### Montant (statut) — l'idée clé
`OpenRB streame STATE/FREQ/POSITION/FORCE/SLAVE toutes les 100 ms → ESP met en
CACHE → GET /api/status lit le cache (PAS d'aller-retour série) → MachineState →
React (poll 200 ms)`. Latence faible et stable.

### Configuration / secrets — UNE seule source
`.machine_config.ini` (racine PROD/, **gitignoré**) est lu par :
- backend : `backend/machine_config.py` (+ `config.py`, `WiFiManager`) ;
- firmware ESP : `firmware/ESP8266/gen_secrets.py` génère `include/secrets.h` au build ;
- scripts : `run.sh` / `dev.sh`.
Modèle versionné : `.machine_config.example.ini`.

### Lancement
`./run.sh` (prod) ou `./dev.sh` (HMR + reload). Logs : `-v` = DEBUG, `-vv` = TRACE.

---

## CRITICAL FILES (par ordre d'importance)

| Fichier                                   | Pourquoi c'est critique                          |
|-------------------------------------------|--------------------------------------------------|
| `backend/api.py`                          | toutes les routes REST, instance controller      |
| `backend/core/controller.py`              | orchestrateur intentions → protocole → lien      |
| `backend/core/state.py`                   | `MachineState` = modèle de données unique        |
| `backend/comm/protocol.py`                | format des commandes/réponses (contrat)          |
| `backend/comm/wifi_link.py`               | pont texte→JSON REST + retries + cache           |
| `backend/machine_config.py`               | chargement config/secrets                        |
| `firmware/ESP8266/src/main.cpp`           | AP WiFi + REST + cache OpenRB                     |
| `firmware/OPENRB150/src/main.cpp`         | exécutant temps réel (stepper/Dynamixel/force)   |
| `frontend/src/app/hooks/useMachineController.ts` | état + TOUS les appels API du frontend    |
| `.machine_config.ini`                     | config + secrets (gitignoré)                     |

---

## COMMUNICATION FLOW (le contrat à 4)

Deux dialectes — voir [docs/06_COMMUNICATION_PROTOCOL.md](docs/06_COMMUNICATION_PROTOCOL.md) :

1. **Backend ↔ ESP** : HTTP/JSON, `Authorization: Bearer <key>`,
   `GET /api/status`, `POST /api/command {"command":..,params}`.
2. **ESP ↔ OpenRB** : lignes texte `CLÉ:valeur\n` @ 19200 :
   - descendant : `HOME|START[:ms]|STOP|HARD_RESET|SET_FREQ:<hz>|SET_SPEED:<%>|
     SET_FORCE[:<cell>]:<n>|GOTO:<t>:<mm>|TORQUE_ON|TORQUE_OFF|GET_STATUS`
   - montant : `STATE:|FREQ:|POSITION:a,b,c,d|FORCE:a,b,c,d|SLAVE:|ACK:|DONE:|ERROR:`

⚠️ Toute évolution du protocole = modifier **les 4 fichiers** (protocol.py,
wifi_link.py, ESP main.cpp, OpenRB main.cpp) **+ re-flasher**.

---

## DO NOT MODIFY / DO NOT BREAK LIST

- ❌ **Ne pas committer** `.machine_config.ini` ni `firmware/ESP8266/include/secrets.h`
  (secrets). Les secrets vont **uniquement** dans le `.ini`.
- ❌ **Ne pas casser l'interface commune** `WiFiLink`/`SerialLink`
  (`open/close/send_line/read_line`) : le controller en dépend.
- ❌ **Ne pas désynchroniser** le protocole entre les 4 couches.
- ❌ **Ne pas envoyer `GET_STATUS` sur la série en mode WiFi** : utiliser le cache ESP.
- ❌ **Ne pas inverser** l'ordre de parsing dans `wifi_link.send_line` : les
  commandes « deux-points » (`SET_FREQ:`) AVANT le fallback mono-lettre (sinon
  `SET_FREQ` est pris pour `START`).
- ❌ **Ne pas changer le baud 19200** d'un seul côté (ESP `OPENRB_BAUD` =
  OpenRB `LINK_BAUD`).
- ❌ **Ne pas faire de `fetch` hors de `useMachineController.ts`** (frontend).
- ❌ **Ne pas toucher** `frontend/src/app/components/ui/` sans raison (primitives
  shadcn génériques).

---

## KNOWN PITFALLS

- **Brownout ESP = alim, pas code.** Hub USB partagé (OpenRB + Dynamixel) →
  l'ESP perd le WiFi sur les pics moteur. Alimenter l'ESP séparément.
- **Connexion WiFi au réseau de l'ESP via le menu système**, pas en CLI (peu fiable).
- **L'ESP ne gère qu'UNE connexion HTTP** → le backend sérialise (`_http_lock`).
- **Après modif des secrets/SSID** dans le `.ini` → re-flasher l'ESP (`pio run -t upload`).
- **Firmware OpenRB = ÉTAPE 1** : pas de boucle fermée de force ; `SET_FORCE` ne
  fait que mémoriser. Calibration force (ADC→N) et `DXL_PER_MM` (mm↔Dynamixel)
  sont des **placeholders à régler**. Plan ÉTAPE 2 : TODO dans `loop()` + section
  13 de [docs/14_HARDWARE_REFERENCE.md](docs/14_HARDWARE_REFERENCE.md).
- **`STOP` = `hard_reset`** côté API aujourd'hui (pas d'arrêt « doux »).
- **CORS ouvert (`*`)** : acceptable sur AP isolé, à restreindre si exposé.
- **`DEFAULT_PORT = None`** quand `[serial].port = auto` : normal (sélection UI/CLI).

---

## QUICK START (commandes)

```bash
cd PROD
cp .machine_config.example.ini .machine_config.ini    # 1ère fois
./dev.sh                                               # dev (HMR + reload + DEBUG)
# Backend seul :   cd backend && source venv/bin/activate && python api.py -v
# Vérifier config: cd backend && python3 -c "import machine_config as m; print(m.config_path())"
# Tester le lien : python3 tools/config/wifi_manager.py
# Flasher ESP :    cd firmware/ESP8266 && pio run -t upload
```

Pour aller plus loin : [docs/00_OVERVIEW.md](docs/00_OVERVIEW.md) →
[docs/01_ARCHITECTURE.md](docs/01_ARCHITECTURE.md) →
[docs/06_COMMUNICATION_PROTOCOL.md](docs/06_COMMUNICATION_PROTOCOL.md).
