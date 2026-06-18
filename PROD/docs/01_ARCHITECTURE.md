# 01 — Architecture

## Vue globale

```
┌──────────────────────────────────────────────────────────────────────────┐
│  PC (poste opérateur)                                                       │
│                                                                            │
│   ┌─────────────────┐         HTTP / JSON          ┌────────────────────┐  │
│   │  React UI        │  ───────────────────────▶   │  FastAPI backend    │  │
│   │  (Vite, :5173)   │   GET/POST /api/* (:8000)    │  api.py (:8000)     │  │
│   │  useMachine      │  ◀───────────────────────   │  MachineController   │  │
│   │  Controller.ts   │      MachineState (JSON)     │  state / protocol    │  │
│   └─────────────────┘                              └─────────┬──────────┘  │
└──────────────────────────────────────────────────────────────┼───────────┘
                                                                 │
                              WiFi (mode AP)  HTTP/JSON          │  ou  USB série
                              http://192.168.4.1:8080            │  (mode DEV)
                                                                 ▼
                                            ┌────────────────────────────────┐
                                            │  ESP8266 (NodeMCU)               │
                                            │  - Access Point WiFi             │
                                            │  - Serveur REST /api/{status,    │
                                            │    command}                      │
                                            │  - CACHE "live" de l'OpenRB      │
                                            └──────────────┬──────────────────┘
                                                           │ SoftwareSerial 19200
                                                           │ (RX=GPIO14, TX=GPIO12)
                                                           ▼
                                            ┌────────────────────────────────┐
                                            │  OpenRB-150 (SAMD21)             │
                                            │  - Serial3 <-> ESP               │
                                            │  - Stepper DM542T (Timer TC3)    │
                                            │  - 4 Dynamixel (Serial1)         │
                                            │  - 4 cellules force (A1..A4)     │
                                            │  - STREAM statut ~10 Hz          │
                                            └────────┬───────────────┬─────────┘
                                                     ▼               ▼
                                            ┌──────────────┐  ┌──────────────┐
                                            │ DM542T +     │  │ Dynamixel ×4  │
                                            │ stepper      │  │ (tables)      │
                                            └──────────────┘  └──────────────┘
                                                     │
                                                     ▼
                                            4× cellule de force 50 N → INA125 → ADC
```

## Principe directeur : séparation des responsabilités

| Élément          | Sait faire                                            | Ne sait PAS                          |
|------------------|-------------------------------------------------------|--------------------------------------|
| React            | Afficher, capturer les intentions utilisateur         | Parler à l'ESP/OpenRB directement    |
| FastAPI backend  | Traduire intentions → protocole, tenir l'état         | Piloter le matériel directement      |
| ESP8266          | Relayer commandes, cacher la télémétrie, AP WiFi      | Logique métier / contrôle de force   |
| OpenRB-150       | Exécuter (moteur, tables), mesurer, streamer l'état    | Décider des consignes                |

Le frontend ne connaît **que** le backend (`localhost:8000`). Le backend ne
connaît **que** l'ESP (ou le port série). Chaque couche ignore les détails des
couches au-delà de sa voisine.

## Flux COMMANDE (descendant)

```
[1] Utilisateur règle la fréquence à 50 Hz dans l'UI
[2] useMachineController.setFrequency(50)
        → POST http://localhost:8000/api/command/frequency  {"frequency":50}
[3] api.py set_frequency()
        → MachineController.set_frequency(50)
            → state.frequency_hz = 50 ; protocol.cmd_set_freq(50) = "SET_FREQ:50.0"
            → link.send_line("SET_FREQ:50.0")
[4] WiFiLink traduit "SET_FREQ:50.0" → POST /api/command {"command":"FREQUENCY","frequency":50.0}
[5] ESP handleCommand() → buildOpenRbLine() = "SET_FREQ:50.000" → série → OpenRB
[6] OpenRB dispatch("SET_FREQ:50.000") → g_frequency=50 → ACK:SET_FREQ
```

## Flux STATUT (montant)

```
[1] OpenRB loop() : toutes les 100 ms → sendStatus()
        STATE:RUNNING / FREQ:50.000 / POSITION:.. / FORCE:.. / SLAVE:ONLINE
[2] ESP pumpOpenRB() lit ces lignes en continu → met à jour le cache `live`
[3] React poll : GET /api/status (toutes les 200 ms)
[4] api.py get_status() : si WiFi → WiFiLink.get_status() (lit le cache ESP)
        → _apply_esp_live_status(state, esp)   (pas d'A/R série !)
[5] api.py renvoie MachineStateResponse (JSON) → React met à jour l'affichage
```

## Composants logiciels clés

### Backend (`backend/`)
- `api.py` — routes REST, middleware de log, instance `controller` globale.
- `core/controller.py` — orchestrateur (intentions → protocole → lien).
- `core/state.py` — `MachineState` (dataclass), modèle unique.
- `comm/protocol.py` — sérialisation commandes + parsing réponses.
- `comm/wifi_link.py` / `comm/serial_link.py` — transports interchangeables.
- `machine_config.py` — lit `.machine_config.ini` (config + secrets).
- `debug/logging_setup.py` — logs structurés (ERROR…TRACE, `--verbose`).
- `debug/logger.py` — buffers du moniteur série pour l'UI.

### Frontend (`frontend/src/app/`)
- `App.tsx` — composant racine (aiguillage écrans, mode Advanced).
- `hooks/useMachineController.ts` — état + tous les appels API (unique point réseau).
- `components/` — présentation (voir [02_FRONTEND.md](02_FRONTEND.md)).

### Firmware
- `firmware/ESP8266/src/main.cpp` — AP WiFi + REST + cache OpenRB.
- `firmware/OPENRB150/src/main.cpp` — exécutant temps réel.

## Décisions d'architecture (et pourquoi)

1. **Cache côté ESP + streaming OpenRB** plutôt que requête/réponse série par
   poll → latence faible et stable malgré un lien WiFi imparfait.
2. **Transport abstrait (WiFiLink/SerialLink)** → bascule WiFi/USB sans toucher
   au métier.
3. **Configuration centralisée unique** (`.machine_config.ini`) lue par les 3
   couches logicielles → un seul endroit pour les secrets, jamais committé.
4. **`cycle_start` en epoch ms** mémorisé côté ESP → le runtime survit à un
   reload du frontend (l'ESP « se souvient »).
5. **Retries sur le lien WiFi** (`MAX_RETRIES`) → encaisse les pertes de paquets
   ponctuelles (alim/RF) sans casser l'opération.
