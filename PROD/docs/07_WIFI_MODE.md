# 07 — Mode WiFi (production)

C'est le mode **nominal**. L'ESP8266 crée son propre réseau WiFi (Access Point) ;
le PC s'y connecte ; le backend parle à l'ESP en HTTP.

## Topologie

```
PC ──(se connecte au WiFi "NodeMCU-Control")──▶ ESP8266 (AP, 192.168.4.1:8080)
backend ──HTTP/JSON (Bearer)──▶ ESP ──série 19200──▶ OpenRB-150
```

L'IP de l'ESP en mode AP est **toujours** `192.168.4.1`.

## Procédure de connexion

1. Mettre la machine sous tension (ESP + OpenRB).
2. Sur le PC, **rejoindre le réseau WiFi** diffusé par l'ESP
   (par défaut SSID `NodeMCU-Control`). ⚠️ Le faire via le **menu WiFi du
   système**, pas en CLI (le CLI WiFi s'est révélé peu fiable).
3. Lancer `./run.sh`.
4. Dans l'UI, choisir l'interface WiFi (ex : `en0`) et cliquer **Connect**.

`_is_wifi_interface(port)` (api.py) détecte que `port` est une interface WiFi
(préfixes `en`, `wlan`, `wifi`…) → bascule sur `WiFiLink`.

## Pourquoi c'est rapide : cache + streaming

```
OpenRB ──burst 10 Hz──▶ ESP (cache `live`) ──GET /api/status──▶ backend ──▶ UI
```

Le backend ne demande **jamais** `GET_STATUS` sur la série en WiFi : il lit le
cache déjà rempli par le streaming. La latence affichée dans l'UI ≈ celle du
WiFi seul.

## Robustesse

- **Retries** (`WiFiLink.MAX_RETRIES = 3`) sur `connect`/`send_command` :
  encaisse une perte de paquet ponctuelle.
- **Timeout court** (2 s) : un brownout de l'ESP ne bloque pas un thread FastAPI.
- **Connexion HTTP unique côté ESP** : le backend sérialise ses accès
  (`_http_lock`) → pas de « connection refused » quand un poll et une commande
  tombent en même temps.
- **Reprise après reload** : à la connexion, le backend relit `cycle_start`,
  `frequency`, `forces` depuis l'ESP.

## Sécurité

Jeton **Bearer** obligatoire (`Authorization: Bearer <key>`). La valeur est
`[nodemcu].key` du `.machine_config.ini`, identique au firmware (`secrets.h`).
Le mot de passe WiFi par défaut est faible (`12345678`) — à changer pour un
déploiement réel (éditer le `.ini`, re-flasher l'ESP).

## Configuration concernée (`.machine_config.ini`)

```ini
[wifi]
ssid = NodeMCU-Control
password = 12345678
[nodemcu]
ip = 192.168.4.1
port = 8080
key = <jeton bearer>
```

## Diagnostic rapide

```bash
# Depuis le PC connecté au WiFi de l'ESP :
curl -H "Authorization: Bearer <key>" http://192.168.4.1:8080/api/status
# Tester le lien complet :
python3 tools/config/wifi_manager.py
```

Problèmes fréquents : [11_TROUBLESHOOTING.md](11_TROUBLESHOOTING.md).
