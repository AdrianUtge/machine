#!/usr/bin/env bash
# =============================================================================
#  run.sh — Lancement PRODUCTION de la machine de test
# -----------------------------------------------------------------------------
#  - Vérifie les dépendances (python3, node/npm)
#  - Crée/active le venv backend et installe requirements.txt si besoin
#  - Vérifie .machine_config.ini (le crée depuis l'exemple si absent)
#  - Construit le frontend (npm run build) s'il n'est pas déjà buildé
#  - Lance le backend (FastAPI :8000) + sert le frontend buildé (:5173)
#  - Affiche l'IP locale, le mode (WiFi AP / USB) et les URLs
#
#  USAGE :
#    ./run.sh             # logs INFO
#    ./run.sh --verbose   # logs DEBUG (requêtes API, commandes, réponses)
#    ./run.sh -vv         # logs TRACE (tout)
# =============================================================================
set -euo pipefail

# --- Repérage des chemins (le script marche depuis n'importe quel CWD) -------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$SCRIPT_DIR/backend"
FRONTEND_DIR="$SCRIPT_DIR/frontend"
CONFIG_FILE="$SCRIPT_DIR/.machine_config.ini"
CONFIG_EXAMPLE="$SCRIPT_DIR/.machine_config.example.ini"
FRONTEND_PORT=5173
BACKEND_PORT=8000

# --- Couleurs ----------------------------------------------------------------
if [ -t 1 ]; then C_OK="\033[32m"; C_WARN="\033[33m"; C_ERR="\033[31m"; C_INFO="\033[36m"; C_RST="\033[0m"
else C_OK=""; C_WARN=""; C_ERR=""; C_INFO=""; C_RST=""; fi
ok()   { echo -e "${C_OK}✓${C_RST} $*"; }
warn() { echo -e "${C_WARN}⚠${C_RST} $*"; }
err()  { echo -e "${C_ERR}✗${C_RST} $*" >&2; }
info() { echo -e "${C_INFO}ℹ${C_RST} $*"; }

# --- Arguments ---------------------------------------------------------------
VERBOSE_ARGS=()
for arg in "$@"; do
  case "$arg" in
    --verbose|-v) VERBOSE_ARGS+=("-v") ;;
    -vv)          VERBOSE_ARGS+=("-vv") ;;
    *) warn "Argument ignoré : $arg" ;;
  esac
done

echo "============================================================"
echo "  Machine de test — LANCEMENT PRODUCTION"
echo "============================================================"

# --- 1. Dépendances système --------------------------------------------------
command -v python3 >/dev/null 2>&1 || { err "python3 introuvable."; exit 1; }
ok "python3 : $(python3 --version 2>&1)"
command -v npm >/dev/null 2>&1 || { err "npm/node introuvable (frontend)."; exit 1; }
ok "node    : $(node --version 2>&1)"

# --- 2. Configuration centrale ----------------------------------------------
if [ ! -f "$CONFIG_FILE" ]; then
  warn ".machine_config.ini absent."
  if [ -f "$CONFIG_EXAMPLE" ]; then
    cp "$CONFIG_EXAMPLE" "$CONFIG_FILE"
    warn "Copié depuis l'exemple -> ÉDITEZ $CONFIG_FILE (SSID/jeton) puis relancez."
  else
    err "Ni .machine_config.ini ni .machine_config.example.ini."; exit 1
  fi
fi
ok "config  : $CONFIG_FILE"

# --- 3. venv backend + dépendances Python -----------------------------------
cd "$BACKEND_DIR"
if [ ! -d "venv" ]; then
  info "Création du venv backend…"
  python3 -m venv venv
fi
# shellcheck disable=SC1091
source venv/bin/activate
info "Vérification des dépendances Python…"
pip install --quiet --disable-pip-version-check -r requirements.txt
ok "dépendances backend installées"

# --- 4. Build frontend si nécessaire ----------------------------------------
cd "$FRONTEND_DIR"
if [ ! -d "node_modules" ]; then
  info "Installation des paquets npm (peut prendre une minute)…"
  npm install --silent
fi
if [ ! -d "dist" ]; then
  info "Build du frontend…"
  npm run build
fi
ok "frontend buildé : $FRONTEND_DIR/dist"

# --- 5. Infos réseau / mode --------------------------------------------------
LOCAL_IP="$(ipconfig getifaddr en0 2>/dev/null || hostname -I 2>/dev/null | awk '{print $1}' || echo '127.0.0.1')"
ESP_IP="$(awk -F= '/^\[nodemcu\]/{s=1} s&&/^ip/{gsub(/[ \t]/,"",$2);print $2;exit}' "$CONFIG_FILE" 2>/dev/null || echo '192.168.4.1')"
echo "------------------------------------------------------------"
info "Mode liaison machine : WiFi AP (ESP8266 @ ${ESP_IP}) — USB = mode DEV (dev.sh)"
info "IP locale du PC      : ${LOCAL_IP}"
info "Frontend (UI)        : http://localhost:${FRONTEND_PORT}"
info "Backend  (API)       : http://localhost:${BACKEND_PORT}"
[ ${#VERBOSE_ARGS[@]} -gt 0 ] && info "Niveau de log        : VERBOSE (${VERBOSE_ARGS[*]})"
echo "------------------------------------------------------------"
warn "Rappel : le PC doit être connecté au WiFi de l'ESP avant de cliquer 'Connect'."

# --- 6. Lancement backend + frontend, arrêt propre au Ctrl-C ----------------
PIDS=()
cleanup() {
  echo; info "Arrêt…"
  for pid in "${PIDS[@]:-}"; do kill "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

cd "$BACKEND_DIR"
python api.py "${VERBOSE_ARGS[@]}" --port "$BACKEND_PORT" &
PIDS+=($!)

cd "$FRONTEND_DIR"
npx vite preview --port "$FRONTEND_PORT" --host >/dev/null 2>&1 &
PIDS+=($!)

ok "Démarré. Ctrl-C pour arrêter."
wait
