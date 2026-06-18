#!/usr/bin/env bash
# =============================================================================
#  dev.sh — Lancement DÉVELOPPEMENT de la machine de test
# -----------------------------------------------------------------------------
#  - Backend FastAPI avec rechargement à chaud (uvicorn --reload) et logs DEBUG
#  - Frontend Vite en mode dev (HMR) sur :5173
#  - Logs des deux process affichés dans le terminal (préfixés [API]/[UI])
#
#  USAGE :
#    ./dev.sh             # backend en DEBUG (-v) + frontend HMR
#    ./dev.sh --verbose   # backend en TRACE (-vv)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$SCRIPT_DIR/backend"
FRONTEND_DIR="$SCRIPT_DIR/frontend"
CONFIG_FILE="$SCRIPT_DIR/.machine_config.ini"
CONFIG_EXAMPLE="$SCRIPT_DIR/.machine_config.example.ini"

if [ -t 1 ]; then C_OK="\033[32m"; C_WARN="\033[33m"; C_INFO="\033[36m"; C_RST="\033[0m"
else C_OK=""; C_WARN=""; C_INFO=""; C_RST=""; fi
ok()   { echo -e "${C_OK}✓${C_RST} $*"; }
warn() { echo -e "${C_WARN}⚠${C_RST} $*"; }
info() { echo -e "${C_INFO}ℹ${C_RST} $*"; }

# En dev on est déjà bavard : -v par défaut, -vv si --verbose.
BACKEND_VERBOSE="-v"
for arg in "$@"; do
  case "$arg" in --verbose|-vv) BACKEND_VERBOSE="-vv" ;; esac
done

echo "============================================================"
echo "  Machine de test — MODE DÉVELOPPEMENT"
echo "============================================================"

command -v python3 >/dev/null 2>&1 || { echo "python3 introuvable"; exit 1; }
command -v npm >/dev/null 2>&1 || { echo "npm introuvable"; exit 1; }

# Config (créée depuis l'exemple si absente)
if [ ! -f "$CONFIG_FILE" ] && [ -f "$CONFIG_EXAMPLE" ]; then
  cp "$CONFIG_EXAMPLE" "$CONFIG_FILE"
  warn "Config créée depuis l'exemple -> éditez $CONFIG_FILE."
fi

# venv backend
cd "$BACKEND_DIR"
[ -d "venv" ] || { info "Création venv…"; python3 -m venv venv; }
# shellcheck disable=SC1091
source venv/bin/activate
pip install --quiet --disable-pip-version-check -r requirements.txt
ok "backend prêt (venv + deps)"

# paquets frontend
cd "$FRONTEND_DIR"
[ -d "node_modules" ] || { info "npm install…"; npm install --silent; }
ok "frontend prêt (node_modules)"

info "Backend  : http://localhost:8000  (uvicorn --reload, log ${BACKEND_VERBOSE})"
info "Frontend : http://localhost:5173  (Vite HMR)"
echo "------------------------------------------------------------"

PIDS=()
BACKEND_PID=""
FRONTEND_PID=""

cleanup() {
  echo; info "Arrêt…"
  for pid in "${PIDS[@]:-}"; do
    # Tuer le process group complet (tous les enfants)
    kill -TERM -"$pid" 2>/dev/null || true
    # Attendre un peu pour un arrêt gracieux
    sleep 0.5
    # Fallback : force kill si le processus persiste
    kill -9 -"$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT INT TERM

# Backend avec reload. uvicorn lance `api:app` (le bloc __main__ ne tourne pas),
# donc on passe le niveau verbeux via MACHINE_VERBOSE (lu à l'import par api.py).
cd "$BACKEND_DIR"
export MACHINE_VERBOSE="$( [ "$BACKEND_VERBOSE" = "-vv" ] && echo vv || echo v )"
( uvicorn api:app --host 0.0.0.0 --port 8000 --reload --log-level debug 2>&1 | sed -u 's/^/[API] /' ) &
BACKEND_PID=$!
PIDS+=("$BACKEND_PID")
ok "[API] Backend Python PID: $BACKEND_PID"

cd "$FRONTEND_DIR"
( npm run dev 2>&1 | sed -u 's/^/[UI]  /' ) &
FRONTEND_PID=$!
PIDS+=("$FRONTEND_PID")
ok "[UI]  Frontend Vite PID: $FRONTEND_PID"

ok "Démarré. Ctrl-C pour arrêter les deux."
wait
