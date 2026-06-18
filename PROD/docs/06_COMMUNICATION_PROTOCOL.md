# 06 — Protocole de communication

> **Source de vérité** côté code : `backend/comm/protocol.py`,
> `backend/comm/wifi_link.py`, `firmware/ESP8266/src/main.cpp`,
> `firmware/OPENRB150/src/main.cpp`. Toute évolution doit rester **synchrone**
> sur ces quatre fichiers.

Il existe **deux dialectes** sur le chemin :

1. **Backend ↔ ESP8266** : HTTP/JSON REST.
2. **ESP8266 ↔ OpenRB-150** : protocole ligne texte (`CLÉ:valeur\n`) sur série 19200.

`WiFiLink` fait le pont : il reçoit du backend des lignes texte
(`SET_FREQ:50.0`) et les traduit en JSON REST pour l'ESP.

---

## 1. Backend ↔ ESP8266 (HTTP REST, port 8080)

Authentification : en-tête `Authorization: Bearer <AUTH_TOKEN>` (= `[nodemcu].key`
du `.machine_config.ini`, identique au firmware). CORS ouvert (`*`).

### `GET /api/status` → JSON

Réponse (champs principaux) :

```json
{
  "status": "ok",
  "uptime_ms": 123456,
  "version": "1.0.0-phase1",
  "ip": "192.168.4.1",
  "cycle_start": "1718700000000",   // echo du START (epoch ms) ou "0"
  "frequency": 50.0,                 // consigne mémorisée
  "forces": [2.0, 2.0, 2.0, 2.0],    // consignes force par cellule
  "rb_state": "RUNNING",             // état live OpenRB
  "rb_frequency": 50.0,              // Hz réels rapportés
  "rb_online": true,                 // OpenRB frais ET slave online
  "rb_fresh_ms": 42,                 // âge de la dernière ligne OpenRB
  "positions": [0,0,0,0],            // mm (4 tables)
  "sensors":   [0,0,0,0]             // N (4 cellules)
}
```

### `POST /api/command` → JSON

Corps : `{"command":"<CMD>", ...params}`. Commandes acceptées (firmware ESP) :

```
START, STOP, HOME, HARD_RESET, FREQUENCY, SPEED, FORCE, GOTO,
PRESET, MANUAL, STATUS, TORQUE_ON, TORQUE_OFF
```

Paramètres selon la commande : `frequency` (float), `speed` (int),
`force` (float) [+ `sensor` 1-4], `table` (int) + `position` (float),
`start_time` (string, epoch ms).

Réponse :

```json
{ "result":"success", "command":"FREQUENCY", "command_number": 12,
  "lines": ["STATE:RUNNING","FREQ:50.000","POSITION:..","FORCE:..","SLAVE:ONLINE"] }
```

`lines[]` = **snapshot immédiat** du cache live (réponse instantanée, sans
attendre la série). L'état réel suit < 100 ms via le streaming + `GET /api/status`.

---

## 2. ESP8266 ↔ OpenRB-150 (série ligne, 19200 bauds)

### Descendant (ESP → OpenRB) — `buildOpenRbLine()`

| Ligne envoyée                    | Sens                                              |
|----------------------------------|---------------------------------------------------|
| `HOME`                           | référencement (ÉTAPE 1 : passe en READY)          |
| `START`                          | démarre l'oscillation stepper                     |
| `STOP`                           | arrête l'oscillation, désactive le driver         |
| `HARD_RESET`                     | reset complet (mode IDLE, consignes à 0)          |
| `SET_FREQ:<hz>`                  | règle la fréquence d'oscillation (0..10 Hz)       |
| `SET_SPEED:<%>`                  | mémorise T_Speed (%)                              |
| `SET_FORCE:<n>`                  | consigne force globale (4 cellules)               |
| `SET_FORCE:<cell>:<n>`           | consigne force d'une cellule (cell = 1..4)        |
| `GOTO:<table>:<mm>`              | positionne une table (1..4) en mm                 |
| `TORQUE_ON` / `TORQUE_OFF`       | verrouille / déverrouille les Dynamixel           |
| `GET_STATUS`                     | demande un envoi de statut immédiat               |

### Montant (OpenRB → ESP) — `sendStatus()` / `sendAck()`

| Ligne reçue                  | Signification                                    |
|------------------------------|--------------------------------------------------|
| `STATE:<mode>`               | IDLE \| HOMING \| READY \| RUNNING \| ERROR      |
| `FREQ:<hz>`                  | fréquence courante                               |
| `POSITION:a,b,c,d`           | 4 positions de table (mm)                        |
| `FORCE:a,b,c,d`              | 4 forces mesurées (N)                            |
| `SLAVE:ONLINE\|OFFLINE`      | présence des 4 Dynamixel                         |
| `ACK:<cmd>`                  | commande acquittée                               |
| `DONE:<cmd>`                 | action terminée (ex : HOME)                      |
| `ERROR:<code>`               | erreur (`FREQ_OUT_OF_RANGE`, `UNKNOWN_COMMAND`…) |

L'OpenRB **streame** `STATE/FREQ/POSITION/FORCE/SLAVE` toutes les 100 ms
(`STREAM_PERIOD_MS`) sans qu'on le demande. L'ESP lit ce flux en continu
(`pumpOpenRB()`) et alimente son cache.

---

## 3. Dialecte interne backend (protocol.py ↔ wifi_link.py)

`protocol.py` produit des lignes « deux-points » que `WiFiLink.send_line()`
sait re-mapper vers le JSON REST. Il accepte aussi, par compat, des commandes
mono-lettre (`F50`, `V75`, `S`, `H`, `M`, `R`).

```
cmd_set_freq(50)   -> "SET_FREQ:50.0"   -> POST {"command":"FREQUENCY","frequency":50.0}
cmd_set_force(2,3) -> "SET_FORCE:3:2.0" -> POST {"command":"FORCE","force":2.0,"sensor":3}
cmd_start(172...)  -> "START:172..."    -> POST {"command":"START","start_time":"172..."}
cmd_goto(2, 10.5)  -> "GOTO:2:10.5"     -> POST {"command":"GOTO","table":2,"position":10.5}
```

⚠️ **Piège connu** : `SET_FREQ:...` commence par `S` comme `START`. Le parsing
des deux-points est fait **avant** le fallback mono-lettre — ne pas inverser.

---

## 4. Séquence complète (exemple : START)

```
React  ──POST /api/command/start──▶  api.py.start()
api.py ──controller.start()──▶  state.cycle_start = now_ms ; "START:<ms>"
       ──WiFiLink.send_line("START:<ms>")──▶  POST /api/command {"command":"START","start_time":"<ms>"}
ESP    ──mémorise cycleStart ; sendToOpenRB("START")──▶  série
OpenRB ──dispatch("START")──▶  driver ON, stepper ON, mode=RUNNING ──"ACK:START"──▶ ESP (cache)
... puis streaming continu ...
OpenRB ──"STATE:RUNNING / FREQ:.. / POSITION:.. / FORCE:.. / SLAVE:ONLINE"──▶ ESP (cache)
React  ──GET /api/status──▶  api.py lit le cache ESP ──▶ MachineState ──▶ UI (RUNNING)
```

## 5. Constantes de liaison à garder synchronisées

| Constante              | Valeur  | Où                                                |
|------------------------|---------|---------------------------------------------------|
| Baud ESP↔OpenRB        | 19200   | ESP `OPENRB_BAUD`, OpenRB `LINK_BAUD`             |
| Période de streaming   | 100 ms  | OpenRB `STREAM_PERIOD_MS`                          |
| Port HTTP ESP          | 8080    | `[esp8266].http_port` / `[nodemcu].port`         |
| Jeton Bearer           | secret  | `[nodemcu].key` ↔ firmware `AUTH_TOKEN` (secrets.h)|
| Fréquence max stepper  | 10 Hz   | OpenRB `F_ROTATION_MAX`                            |
