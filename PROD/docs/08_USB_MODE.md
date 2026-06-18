# 08 — Mode USB (développement)

Mode **alternatif** au WiFi : le backend parle **directement en série (USB)** à
la carte, sans passer par le réseau de l'ESP. Pratique pour développer/déboguer.

## Topologie

```
backend ──SerialLink (USB, 115200)──▶ carte (ESP ou OpenRB selon le port branché)
```

Le `MachineController` est agnostique : `SerialLink` offre la même interface que
`WiFiLink` (`open/close/send_line/read_line`).

## Procédure

1. Brancher la carte au PC en USB.
2. Lancer le backend (`./dev.sh` ou `python api.py -v`).
3. Dans l'UI, sélectionner un **port série** (ex : `/dev/cu.usbmodemXXXX`) — et
   non une interface WiFi — puis **Connect**.

`_is_wifi_interface(port)` renvoie alors `False` → bascule sur `SerialLink`.

## Spécificités du mode série

- Un **thread lecteur** (`background_reader` dans api.py) lit la ligne en continu
  et alimente le moniteur série (`log_action("response", …)`).
- `/api/status` lit les réponses série disponibles (`_read_all_responses`) au
  lieu du cache ESP.
- `last_data_ts` est mis à jour à chaque ligne reçue → slave ONLINE/OFFLINE.

## Configuration (`.machine_config.ini`)

```ini
[serial]
port = auto        ; "auto" => DEFAULT_PORT = None (sélection via l'UI / CLI)
baudrate = 115200
timeout = 1
```

`config.py` lit cette section. `port = auto` ⇒ `DEFAULT_PORT = None` : le port
réel est choisi dans l'UI (qui le passe à `/api/connect`) ou via
`comm/ports.choose_serial_port()` (menu interactif).

## Lister les ports

```bash
# via l'API
curl http://localhost:8000/api/ports
# ou en Python
python3 -c "import serial.tools.list_ports as p; print([x.device for x in p.comports()])"
```

## Quand l'utiliser

- Développement firmware/backend sans dépendre du WiFi de l'ESP.
- Lecture directe des logs série de l'OpenRB (`pio device monitor`).
- Diagnostic bas niveau du protocole ligne.

Pour la production, préférer le **mode WiFi** ([07_WIFI_MODE.md](07_WIFI_MODE.md)).
