# 00 — Vue d'ensemble

> Objectif : comprendre le projet en moins de 15 minutes.

## Qu'est-ce que c'est ?

Un **banc de mesure de force** piloté par ordinateur. Le système :

1. fait **osciller un moteur pas-à-pas** (DM542T) à une fréquence réglable
   (presets biologiques : Humain 0.8 Hz, Bœuf 0.4 Hz, Souris 3.7 Hz) ;
2. **positionne 4 tables** verticales via des servomoteurs **Dynamixel** ;
3. **mesure 4 cellules de force** (capteurs 50 N via ampli INA125) ;
4. expose tout ça dans une **interface web** temps réel (graphes, consignes).

## Les 4 couches

| Couche       | Techno                                   | Dossier             | Rôle                                   |
|--------------|------------------------------------------|---------------------|----------------------------------------|
| Frontend     | React + TypeScript + Vite + Tailwind     | `frontend/`         | Interface utilisateur (UI web)         |
| Backend      | Python 3 + FastAPI + uvicorn + pyserial  | `backend/`          | API REST, état machine, traduction     |
| Passerelle   | ESP8266 (PlatformIO/Arduino)             | `firmware/ESP8266`  | Point d'accès WiFi + cache temps réel  |
| Contrôleur   | OpenRB-150 / SAMD21 (PlatformIO/Arduino) | `firmware/OPENRB150`| Stepper, Dynamixel, cellules de charge |

## Flux de données (résumé)

```
DESCENDANT (commandes)                    MONTANT (télémétrie)
─────────────────────                     ────────────────────
React (clic)                              OpenRB streame ~10 Hz
  → POST /api/command/*                     → burst série vers l'ESP
  → FastAPI                                 → ESP met en cache "live"
  → protocol.py (SET_FREQ:50…)              → GET /api/status (lit le cache)
  → wifi_link.py (HTTP)                      → FastAPI met à jour MachineState
  → ESP8266 (REST)                          → React rafraîchit (5 Hz)
  → série 19200 → OpenRB dispatch()
```

La grande idée : **l'OpenRB streame son état en continu**, l'**ESP le met en
cache**, et `GET /api/status` lit ce cache → pas d'aller-retour série par poll,
donc une latence faible et stable. Voir [07_WIFI_MODE.md](07_WIFI_MODE.md).

## Deux modes de liaison

- **WiFi (production)** : le backend parle à l'ESP8266 en HTTP. L'ESP est un
  point d'accès (Access Point) : le PC se connecte à SON réseau. Voir
  [07_WIFI_MODE.md](07_WIFI_MODE.md).
- **USB (développement)** : le backend parle directement en série à la carte.
  Voir [08_USB_MODE.md](08_USB_MODE.md).

Le `MachineController` est **agnostique du transport** : `WiFiLink` et
`SerialLink` offrent la même interface.

## Lancer

```bash
cd PROD && ./run.sh          # production
cd PROD && ./dev.sh          # développement (HMR + reload)
```

Détails : [09_DEPLOYMENT.md](09_DEPLOYMENT.md).

## Où aller ensuite

- Architecture détaillée + diagrammes : [01_ARCHITECTURE.md](01_ARCHITECTURE.md)
- Le protocole exact (commandes/réponses) : [06_COMMUNICATION_PROTOCOL.md](06_COMMUNICATION_PROTOCOL.md)
- Reprendre le développement sans contexte : [../AI_CONTEXT.md](../AI_CONTEXT.md)

## État du firmware (important)

Le firmware OpenRB est en **ÉTAPE 1** : il exécute les commandes (oscillation,
GOTO, mémorisation des consignes de force) mais **ne fait PAS encore la boucle
fermée de force** (descente automatique jusqu'à la consigne). La calibration
force (ADC→Newton) et la conversion mm↔Dynamixel sont des **placeholders à
régler**. Détails et plan ÉTAPE 2 : [05_OPENRB150.md](05_OPENRB150.md) et
[14_HARDWARE_REFERENCE.md](14_HARDWARE_REFERENCE.md).
