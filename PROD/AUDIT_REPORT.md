# Rapport d'audit — industrialisation du projet (création de `PROD/`)

Date : 2026-06-18. Objectif : produire une version **maintenable
industriellement**, lançable en < 5 min, compréhensible en < 15 min, reprenable
sans historique. `TEST-PLATFORM/` (source d'origine) **n'a pas été modifié** ;
`PROD/` est une copie propre + une couche d'industrialisation.

---

## 1. Méthode

- Lecture complète du code des 4 couches + des docs existantes
  (`docs/protocol.md`, `compilation_technique.md`, READMEs).
- Copie **sélective** (rsync avec exclusions) de l'essentiel vers `PROD/` :
  aucun artefact, aucun code mort.
- Ajout : config centralisée, logs structurés, en-têtes de fichiers, scripts de
  lancement, documentation `docs/00→14`, `AI_CONTEXT.md`.
- Comportement fonctionnel **préservé** (améliorations additives, pas de réécriture).

## 2. Fichiers / dossiers EXCLUS de PROD (legacy / bloat)

| Exclu                                              | Raison                          |
|----------------------------------------------------|---------------------------------|
| `src/firmware/MACHINE`, `READ_CELL`, `SLAVE`       | firmwares obsolètes/expérimentaux|
| `tests/`                                           | bancs de test hors périmètre prod|
| `tools/virtual_serial_logger`                      | outil de debug non essentiel    |
| `hardware/.../UNO_debug_esp8266`                   | debug ponctuel                  |
| `**/.pio/`, `**/venv`, `**/.venv`, `**/node_modules`, `**/__pycache__` | artefacts |
| `**/*.bak` (`main_banc.cpp.bak`, `main_app.cpp.bak`), `test.txt` | sauvegardes/scratch |
| `firmware/ESP8266/src/link_check.{cpp,h}`          | non référencé par `main.cpp` (mort)|
| React : `LoadCell.tsx`, `PositionControl.tsx`, `StatusPanel.tsx`, `src/imports/` | non importés (morts) |
| `tools/config/setup.json`, `setup.example.json`    | remplacés par `.machine_config.ini`|
| `frontend/dist/`, `backend/preset.json`            | générés au runtime              |
| dépendance PlatformIO `links2004/WebSockets`       | non utilisée par le firmware ESP|

## 3. Fichiers CRÉÉS

**Configuration centralisée**
- `.machine_config.ini` (secrets réels, gitignoré) + `.machine_config.example.ini`
- `backend/machine_config.py` (chargeur)
- `firmware/ESP8266/gen_secrets.py` (génère `include/secrets.h` au build)

**Observabilité**
- `backend/debug/logging_setup.py` (niveaux ERROR→TRACE, `--verbose`, middleware)

**Lancement**
- `run.sh` (production), `dev.sh` (développement)

**Documentation** (`docs/`)
- `00_OVERVIEW` · `01_ARCHITECTURE` · `02_FRONTEND` · `03_BACKEND` · `04_ESP8266`
  · `05_OPENRB150` · `06_COMMUNICATION_PROTOCOL` · `07_WIFI_MODE` · `08_USB_MODE`
  · `09_DEPLOYMENT` · `10_DEBUGGING` · `11_TROUBLESHOOTING` · `12_PROJECT_STRUCTURE`
  · `13_CONTRIBUTING` · `14_HARDWARE_REFERENCE` (préservé)
- `README.md`, `AI_CONTEXT.md`, `AUDIT_REPORT.md` (ce fichier), `.gitignore`

## 4. Fichiers MODIFIÉS (vs copie d'origine)

- `backend/api.py` : en-tête, import `logging`, logger structuré, **fix du chemin**
  `tools/config` (`../tools/config`), middleware de log des requêtes,
  `log_action` → logs, init logging à l'import (env `MACHINE_VERBOSE`),
  `__main__` avec `--verbose/--log-level/--host/--port`.
- `backend/config.py` : valeurs série lues depuis `.machine_config.ini`.
- `backend/comm/{wifi_link,serial_link,protocol,ports}.py`,
  `backend/core/{controller,state,presets}.py`, `backend/debug/logger.py` :
  en-têtes standardisés (FILE/ROLE/…).
- `tools/config/wifi_manager.py` : lit `.machine_config.ini` (fallback JSON), en-tête.
- `firmware/ESP8266/{include/config.h,platformio.ini,.gitignore}` : secrets via
  `secrets.h`, `extra_scripts pre:gen_secrets.py`, retrait WebSockets.
- `frontend/src/app/App.tsx`, `frontend/src/app/hooks/useMachineController.ts` : en-têtes.

## 5. Dépendances utilisées

| Couche   | Dépendances                                                        |
|----------|-------------------------------------------------------------------|
| Backend  | `fastapi`, `uvicorn`, `pyserial`, `requests` (+ stdlib `configparser`, `logging`) ; `questionary` optionnel (sélection port CLI) |
| Frontend | React, Vite, `@vitejs/plugin-react`, `@tailwindcss/vite`, Tailwind, shadcn/ui, `lucide-react`, MUI/emotion, radix-ui (cf. `package.json`) |
| ESP8266  | core Arduino (ESP8266WiFi/WebServer/SoftwareSerial) + `bblanchon/ArduinoJson` |
| OpenRB   | core Arduino SAMD + `Dynamixel2Arduino` (ROBOTIS)                  |

## 6. Vérifications effectuées

- ✅ `py_compile` sur tous les modules backend.
- ✅ `import api` OK (28 routes) via un interpréteur disposant de FastAPI.
- ✅ Chargement `.machine_config.ini` validé (backend + WiFiManager).
- ✅ Logs INFO/DEBUG/TRACE + routage `log_action` (command→INFO, response→DEBUG).
- ✅ `bash -n run.sh dev.sh` (syntaxe).
- ⚠️ **Non exécuté ici** : build frontend (`npm`), compilation firmware (`pio`),
  test sur matériel réel — à faire sur le poste cible.

## 7. Points techniques RISQUÉS

1. **Compatibilité protocole à 4 couches** : toute évolution doit toucher
   protocol.py + wifi_link.py + ESP main.cpp + OpenRB main.cpp, puis re-flasher.
2. **Secrets compilés dans le firmware** : modifier le `.ini` impose un re-flash
   de l'ESP (sinon désync jeton → 401).
3. **Alimentation (brownout)** : hub USB partagé → pertes WiFi. Risque terrain
   majeur, pas logiciel.
4. **CORS `*`** et mot de passe WiFi faible par défaut : OK en AP isolé, à durcir
   si exposition.
5. **`run.sh`/`gen_secrets.py`** non testés sur le poste cible (npm/pio absents ici).

## 8. Dette technique RESTANTE

- Firmware OpenRB en **ÉTAPE 1** : pas de boucle fermée de force (`SET_FORCE` ne
  mémorise que la consigne). Plan ÉTAPE 2 documenté.
- **Calibration force ADC→Newton** (`FORCE_GAIN/OFFSET`) et **conversion
  mm↔Dynamixel** (`DXL_PER_MM`) = placeholders à régler avec le matériel.
- `STOP` ≡ `hard_reset` côté API (pas d'arrêt « doux » distinct).
- `version` ESP figée `"1.0.0-phase1"`.
- Pas de tests automatisés dans PROD (les anciens bancs `tests/` ont été exclus).
- Fallback legacy `setup.json` conservé dans `wifi_manager.py` (peut être retiré
  une fois la migration `.ini` confirmée partout).
