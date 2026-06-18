# 13 — Contribuer / faire évoluer le projet

## Règles d'or (à ne jamais casser)

1. **Ne jamais casser la compatibilité** entre Frontend, Backend, ESP8266 et
   OpenRB-150. Le protocole est un contrat à 4 (voir [06](06_COMMUNICATION_PROTOCOL.md)).
2. **Préférer améliorer l'observabilité** (logs) plutôt que réécrire.
3. **Toute décision d'architecture se documente** (dans `docs/` + commentaire de code).
4. **Secrets uniquement dans `.machine_config.ini`** (gitignoré). Jamais en dur,
   jamais committés.

## Conventions de code

### Python (backend)
- Chaque module important commence par un **en-tête** `FILE/ROLE/ARCHITECTURE/
  RESPONSIBILITIES/DEPENDENCIES/MAINTAINER NOTES` (voir les fichiers existants).
- Commentaires qui expliquent le **pourquoi** (décisions, pièges), pas le `+1`.
- Logs via `from debug.logging_setup import get_logger` ; `log = get_logger(__name__)`.
- Le `MachineController` reste **agnostique du transport**.

### TypeScript (frontend)
- **Aucun `fetch` hors de `useMachineController.ts`.**
- Les types (`MachineState`, …) restent alignés sur le backend.

### C++ (firmware)
- En-tête de fichier décrivant rôle/câblage/protocole.
- Un seul lecteur du port série (ESP : `pumpOpenRB` ; OpenRB : boucle `LINK`).

## Ajouter une commande de bout en bout (recette)

Exemple : ajouter `SET_OFFSET`.

1. **OpenRB** (`firmware/OPENRB150/src/main.cpp`) : gérer `SET_OFFSET` dans
   `dispatch()`, répondre `ACK:SET_OFFSET`.
2. **ESP** (`firmware/ESP8266/src/main.cpp`) : ajouter `SET_OFFSET` à
   `validCommands[]` et son mapping dans `buildOpenRbLine()`.
3. **Backend protocole** (`backend/comm/protocol.py`) : `cmd_set_offset()`.
4. **Backend lien** (`backend/comm/wifi_link.py`) : mapper la ligne vers le JSON
   REST (`send_command('SET_OFFSET', offset=…)`).
5. **Backend controller** (`backend/core/controller.py`) : méthode + maj `state`.
6. **Backend API** (`backend/api.py`) : route `POST /api/command/offset`.
7. **Frontend hook** (`useMachineController.ts`) : `setOffset()` → l'UI.
8. **Doc** : mettre à jour [06_COMMUNICATION_PROTOCOL.md](06_COMMUNICATION_PROTOCOL.md).
9. **Re-flasher** l'ESP et l'OpenRB.

## Tester une modification

- Backend importe ? `cd backend && python3 -c "import api"` (dans le venv).
- Syntaxe : `python3 -m py_compile backend/**/*.py`.
- Firmware compile ? `pio run` dans chaque dossier `firmware/*`.
- Frontend build ? `cd frontend && npm run build`.
- Lien complet : `python3 tools/config/wifi_manager.py`.

## Git / commits

- Commit **atomique par feature** sur `main` (projet solo : pas de branches/PR),
  jamais de gros blob non committé.
- Message clair, en français, type `feat(...)`/`fix(...)`/`docs(...)`.

## Dette technique connue → voir [00_OVERVIEW.md](00_OVERVIEW.md) et le rapport d'audit

- Firmware OpenRB en ÉTAPE 1 (pas de boucle fermée de force).
- Calibration force (ADC→N) et conversion mm↔Dynamixel = placeholders.
- `STOP` = `hard_reset` côté API (pas d'arrêt « doux » distinct).
- CORS ouvert (`*`).
