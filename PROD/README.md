# Machine de test — banc de mesure de force 

Interface de pilotage d'un banc de test : oscillation d'un moteur pas-à-pas,
positionnement de 4 tables Dynamixel, lecture de 4 cellules de force — le tout
piloté depuis une UI web via une passerelle WiFi.

## Démarrer en 5 minutes

```bash
cd PROD
cp .machine_config.example.ini .machine_config.ini   # puis éditez SSID/jeton si besoin
./run.sh                 # production (build frontend + backend + UI)
# ou
./dev.sh                 # développement (HMR frontend + backend --reload)
```

- Frontend : http://localhost:5173
- Backend (API) : http://localhost:8000
- Avant de cliquer **Connect** en mode WiFi : connectez le PC au réseau WiFi
  diffusé par l'ESP8266 (par défaut `NodeMCU-Control`).

## Architecture en une image

```
  React UI ──HTTP:8000──> FastAPI ──HTTP:8080──> ESP8266 (AP WiFi) ──série 19200──> OpenRB-150
   (web)                  (backend)              (passerelle/cache)                 (moteurs+capteurs)
```

## Documentation

Tout est dans [`docs/`](docs/). Points d'entrée :

| Pour…                                   | Lire                                            |
|-----------------------------------------|-------------------------------------------------|
| Comprendre le projet (15 min)           | [docs/00_OVERVIEW.md](docs/00_OVERVIEW.md)      |
| L'architecture détaillée                | [docs/01_ARCHITECTURE.md](docs/01_ARCHITECTURE.md) |
| Le protocole de communication           | [docs/06_COMMUNICATION_PROTOCOL.md](docs/06_COMMUNICATION_PROTOCOL.md) |
| Déployer / lancer                       | [docs/09_DEPLOYMENT.md](docs/09_DEPLOYMENT.md)  |
| Déboguer un problème                    | [docs/10_DEBUGGING.md](docs/10_DEBUGGING.md) · [docs/11_TROUBLESHOOTING.md](docs/11_TROUBLESHOOTING.md) |
| **Reprendre le dev (humain ou IA)**     | [AI_CONTEXT.md](AI_CONTEXT.md)                  |

## Configuration

Toute la configuration et les secrets sont centralisés dans **un seul fichier
gitignoré** : `.machine_config.ini` (modèle : `.machine_config.example.ini`).
Voir [docs/09_DEPLOYMENT.md](docs/09_DEPLOYMENT.md).
