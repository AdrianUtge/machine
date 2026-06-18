# 09 — Déploiement / lancement

## Prérequis

| Outil      | Pour            | Vérifier               |
|------------|-----------------|------------------------|
| Python 3   | backend         | `python3 --version`    |
| Node + npm | frontend        | `node -v` / `npm -v`   |
| PlatformIO | firmware (flash)| `pio --version`        |

## 1. Configuration (une fois)

```bash
cd PROD
cp .machine_config.example.ini .machine_config.ini
$EDITOR .machine_config.ini      # renseigner SSID / mot de passe / jeton
```

`.machine_config.ini` est **l'unique source de configuration et de secrets**.
Il est **gitignoré** : jamais committé. Seul `.machine_config.example.ini`
(placeholders) est versionné.

Sections : `[wifi]`, `[nodemcu]`, `[serial]`, `[esp8266]`, `[logging]`.
Lu par : le backend (`machine_config.py`), le firmware ESP (`gen_secrets.py`),
les scripts shell.

## 2. Lancer (logiciel)

```bash
./run.sh             # PRODUCTION : build frontend + backend + sert dist
./run.sh --verbose   # idem, logs DEBUG
./dev.sh             # DÉVELOPPEMENT : Vite HMR + uvicorn --reload
```

`run.sh` :
1. vérifie python3/npm ;
2. crée le venv backend + installe `requirements.txt` ;
3. crée `.machine_config.ini` depuis l'exemple s'il manque ;
4. `npm install` + `npm run build` si nécessaire ;
5. lance backend (:8000) + `vite preview` (:5173) ;
6. affiche IP locale, IP ESP, mode, URLs ; Ctrl-C arrête tout proprement.

## 3. Flasher les firmwares (au besoin)

```bash
# ESP8266 (régénère secrets.h depuis le .ini, puis flashe)
cd firmware/ESP8266 && pio run -t upload

# OpenRB-150
cd firmware/OPENRB150 && pio run -t upload
```

⚠️ **Après toute modification des secrets/SSID dans le `.ini`, re-flasher l'ESP**
(ses valeurs y sont compilées via `secrets.h`).

## 4. Ordre de mise en route (terrain)

```
1. Alimenter l'OpenRB-150 et l'ESP8266 (idéalement alims séparées — cf. brownout).
2. Sur le PC : rejoindre le WiFi de l'ESP (menu système, pas CLI).
3. ./run.sh
4. UI -> choisir l'interface WiFi -> Connect.
```

## Ports utilisés

| Port | Service                          |
|------|----------------------------------|
| 5173 | Frontend (Vite preview/dev)      |
| 8000 | Backend FastAPI                  |
| 8080 | Serveur HTTP de l'ESP8266 (AP)   |

## Notes production

- Mot de passe WiFi et jeton par défaut sont **faibles** : les changer.
- `CORS` est ouvert (`*`) — acceptable sur réseau AP isolé, à restreindre si exposé.
- `preset.json` (presets utilisateur) est créé au runtime et **gitignoré**.
