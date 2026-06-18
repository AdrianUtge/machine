# 11 — Dépannage (problèmes fréquents)

## La connexion WiFi tombe / l'ESP redémarre (brownout)

**Cause la plus fréquente : alimentation instable**, pas le code. Un hub USB
partagé entre l'OpenRB et les Dynamixel fait chuter la tension de l'ESP lors des
pics de courant (mouvement moteur) → l'ESP brownout et perd le WiFi.

**Solution :** alimenter l'ESP8266 **séparément** (sa propre source USB / 5 V).
Les retries (`MAX_RETRIES`) et le timeout court atténuent les pertes ponctuelles
mais ne remplacent pas une alim saine.

## « Failed to connect to NodeMCU at 192.168.4.1:8080 »

Vérifier dans l'ordre :
1. l'ESP est **alimenté** (LED qui clignote) ;
2. le PC est **connecté au WiFi** `NodeMCU-Control` (menu système, **pas** CLI) ;
3. l'IP/port/jeton du `.machine_config.ini` correspondent au firmware ;
4. test direct : `curl -H "Authorization: Bearer <key>" http://192.168.4.1:8080/api/status`.

## 401 Unauthorized depuis l'ESP

Le jeton ne correspond pas. `[nodemcu].key` du `.ini` doit être **identique** à
`AUTH_TOKEN` du firmware. Après modif du `.ini` : `pio run -t upload` (l'ESP
recompile `secrets.h`).

## Backend injoignable (« Failed to fetch » dans l'UI)

uvicorn ne tourne pas ou pas sur le bon port. Vérifier `./run.sh`/`./dev.sh`,
`curl http://localhost:8000/api/health`, port 8000 libre.

## 404 sur une commande (`POST /api/command/... → 404`)

La route n'existe pas côté backend, ou le serveur n'a pas redémarré après une
modif. Vérifier la route dans `api.py` et relancer (`./dev.sh` recharge tout seul).

## Connecté mais aucune donnée live (positions/forces à 0)

- `rb_online` est-il `true` dans `/api/status` ? Sinon l'OpenRB ne streame pas.
- Logs ESP : voit-on `[OpenRB] > ...` et le cache se remplir ?
- Câblage série ESP↔OpenRB (croisement TX/RX, GND commun, baud 19200 des deux côtés).
- L'OpenRB a-t-il bien booté (`ACK:BOOT` sur son moniteur série) ?

## Le moteur pas-à-pas ne tourne pas

- L'OpenRB a-t-il reçu `START` (`ACK:START`) ?
- Câblage DM542T (anode commune, actif bas) : ENA bas = activé, PUL = pulses.
- Fréquence > 0 et ≤ 10 Hz (`F_ROTATION_MAX`). `SET_FREQ` hors plage → `ERROR:FREQ_OUT_OF_RANGE`.

## Les forces sont nulles ou incohérentes

La **calibration est un placeholder** (`FORCE_GAIN=1`, `FORCE_OFFSET=0`). Il faut
calibrer ADC→Newton avec le capteur réel. Voir [05_OPENRB150.md](05_OPENRB150.md)
et la section conversion de [14_HARDWARE_REFERENCE.md](14_HARDWARE_REFERENCE.md).

## GOTO ne déplace pas correctement (mm faux)

`DXL_PER_MM = 1.0` est un **placeholder** : la conversion mm↔unités Dynamixel
doit être calibrée mécaniquement.

## SLAVE:OFFLINE en permanence

L'OpenRB ne détecte pas les 4 Dynamixel (`dxlScan`). Vérifier l'alim du bus,
les IDs (1..4), le baud `DXL_BAUD` (57600). `SLAVE:ONLINE` exige exactement 4
moteurs trouvés.

## `.machine_config.ini introuvable` au démarrage

Le backend retombe sur les défauts « usine » (192.168.4.1:8080) et logge un
warning. Créer le fichier : `cp .machine_config.example.ini .machine_config.ini`.

## Le port série n'apparaît pas (mode USB)

Carte non branchée, ou pilote USB manquant. `curl localhost:8000/api/ports`.
Sur macOS, le port ressemble à `/dev/cu.usbmodemXXXX`.
