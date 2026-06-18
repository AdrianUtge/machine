# 12 — Structure du projet

```
PROD/
├── README.md                     # point d'entrée, démarrage 5 min
├── AI_CONTEXT.md                 # reprise du dev (humain ou IA) sans historique
├── run.sh                        # lancement PRODUCTION
├── dev.sh                        # lancement DÉVELOPPEMENT (HMR + reload)
├── .machine_config.ini           # CONFIG + SECRETS (gitignoré, source unique)
├── .machine_config.example.ini   # modèle versionné (placeholders)
├── .gitignore
│
├── backend/                      # API Python + logique (voir 03_BACKEND.md)
│   ├── api.py                    # routes REST, middleware log, controller global
│   ├── machine_config.py         # lecture .machine_config.ini
│   ├── config.py                 # constantes série + buffers
│   ├── requirements.txt
│   ├── cal_330.txt               # table de calibration force
│   ├── comm/  (protocol, wifi_link, serial_link, ports, force_cal)
│   ├── core/  (controller, state, presets, preset_store)
│   └── debug/ (logging_setup, logger)
│
├── frontend/                     # UI React (voir 02_FRONTEND.md)
│   ├── index.html · vite.config.ts · package.json
│   └── src/
│       ├── main.tsx
│       ├── app/App.tsx
│       ├── app/hooks/useMachineController.ts   # ★ état + appels API
│       ├── app/components/  (ConnectionScreen, MotionControl, StatusPanelSimple,
│       │                     PositionsAndSensors, ForceGraph, SerialMonitor, …)
│       │   └── ui/           (primitives shadcn, ~48 fichiers génériques)
│       └── styles/
│
├── firmware/
│   ├── ESP8266/                  # passerelle WiFi (voir 04_ESP8266.md)
│   │   ├── platformio.ini        # env nodemcuv2, extra_scripts pre:gen_secrets.py
│   │   ├── gen_secrets.py        # génère include/secrets.h depuis le .ini
│   │   ├── include/config.h      # WIFI/AUTH via secrets.h (+ défauts)
│   │   └── src/main.cpp          # AP WiFi + REST + cache OpenRB
│   └── OPENRB150/                # contrôleur temps réel (voir 05_OPENRB150.md)
│       ├── platformio.ini        # env openrb-150
│       └── src/main.cpp          # stepper + Dynamixel + force + streaming
│
├── tools/
│   └── config/
│       ├── wifi_manager.py        # interfaces WiFi + test lien (lit le .ini)
│       └── README.md
│
└── docs/
    ├── 00_OVERVIEW.md            ├── 08_USB_MODE.md
    ├── 01_ARCHITECTURE.md        ├── 09_DEPLOYMENT.md
    ├── 02_FRONTEND.md            ├── 10_DEBUGGING.md
    ├── 03_BACKEND.md            ├── 11_TROUBLESHOOTING.md
    ├── 04_ESP8266.md            ├── 12_PROJECT_STRUCTURE.md  (ce fichier)
    ├── 05_OPENRB150.md          ├── 13_CONTRIBUTING.md
    ├── 06_COMMUNICATION_PROTOCOL.md
    ├── 07_WIFI_MODE.md          └── 14_HARDWARE_REFERENCE.md
```

## Fichiers générés / non versionnés (gitignore)

| Fichier / dossier                 | Nature                                  |
|-----------------------------------|-----------------------------------------|
| `.machine_config.ini`             | secrets (créé depuis l'exemple)         |
| `firmware/ESP8266/include/secrets.h` | généré au build par gen_secrets.py   |
| `backend/preset.json`             | presets utilisateur (runtime)           |
| `backend/venv/`, `frontend/node_modules/`, `frontend/dist/` | dépendances/build |
| `**/.pio/`, `**/__pycache__/`     | artefacts de build                      |

## Où est passé l'ancien code ?

La version PROD est une **copie propre** de l'essentiel. Ont été **exclus**
(legacy/bloat) : `src/firmware/{MACHINE,READ_CELL,SLAVE}`, `tests/`,
`tools/virtual_serial_logger`, `UNO_debug_esp8266`, les composants React morts
(`LoadCell`, `PositionControl`, `StatusPanel`, `imports/`), `link_check.*`, et
tous les artefacts (`.pio`, `venv`, `node_modules`, `__pycache__`, `.bak`).
Détail dans le rapport d'audit (voir le commit qui introduit PROD/).
```
