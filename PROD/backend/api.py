"""
===============================================================================
FILE: api.py
ROLE:
    Serveur API REST (FastAPI) entre le frontend React et la machine.

ARCHITECTURE:
    React UI
      -> POST/GET /api/*        (ce fichier, port 8000)
        -> MachineController     (core/controller.py)
          -> WiFiLink            (HTTP -> ESP8266 192.168.4.1:8080 -> OpenRB-150)
          OU SerialLink          (USB direct, mode DEV)

RESPONSIBILITIES:
    - Exposer connexion/déconnexion, commandes (home/start/stop/freq/force/goto/
      torque/presets) et lecture d'état (/api/status) + logs (/api/logs).
    - En WiFi : /api/status lit le CACHE de l'ESP (pas d'A/R série) -> faible latence.
    - Tenir un buffer de logs (serial_logs) pour le moniteur série du frontend.

DEPENDENCIES:
    - fastapi, uvicorn, pydantic
    - comm.* (liens), core.* (état/contrôleur), tools/config/wifi_manager.py

CONFIG / SECRETS:
    - IP/port/jeton de l'ESP : .machine_config.ini -> WiFiManager (source unique).

LOGS:
    - Lancement : `python api.py` (INFO) | `python api.py -v` (DEBUG) | `-vv` (TRACE).
    - Un middleware logge chaque requête HTTP (méthode, chemin, statut, durée).

MAINTAINER NOTES:
    - Instance `controller` GLOBALE : l'API est mono-machine (un seul banc).
    - Ne PAS envoyer GET_STATUS sur le lien série en WiFi (le cache ESP suffit).
===============================================================================
"""

from fastapi import FastAPI, HTTPException, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional
import asyncio
import json
import logging
import threading
import time

from comm.serial_link import SerialLink
from comm.wifi_link import WiFiLink
from comm import force_cal
from config import DEFAULT_BAUDRATE, DEFAULT_PORT, DEFAULT_TIMEOUT
from core.controller import MachineController
from core.state import MachineState
from core import preset_store
from debug.logger import DebugLogger
from debug.logging_setup import get_logger
from comm.ports import choose_serial_port
import machine_config
import sys
import os
from pathlib import Path

log = get_logger(__name__)

# Add tools/config to path for WiFiManager (layout PROD : backend/ et tools/ sont
# frères sous PROD/, donc on remonte d'UN seul niveau).
tools_config_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../tools/config'))
if tools_config_path not in sys.path:
    sys.path.insert(0, tools_config_path)

try:
    from wifi_manager import WiFiManager
except ImportError as e:
    log.warning(f"Could not import WiFiManager: {e}")
    WiFiManager = None

# Initialise le logging DÈS L'IMPORT : sous uvicorn (dev.sh : `uvicorn api:app`)
# le bloc __main__ ne s'exécute pas. La variable d'env MACHINE_VERBOSE permet à
# dev.sh de demander DEBUG/TRACE. `python api.py` (run.sh) re-règle via argparse.
import machine_config as _machine_config
from debug.logging_setup import setup_logging as _setup_logging
_env_verbose = os.environ.get("MACHINE_VERBOSE", "").lower()
_setup_logging(
    verbose=(2 if _env_verbose in ("2", "vv") else 1 if _env_verbose in ("1", "v", "true", "yes") else 0),
    level_name=_machine_config.log_level(),
)

# --- Models ----------------------------------------------------------

class ConnectRequest(BaseModel):
    port: str

class FrequencyRequest(BaseModel):
    frequency: float

class SpeedRequest(BaseModel):
    speed: int

class ForceRequest(BaseModel):
    force: float
    sensor: Optional[int] = None  # None = global (4 capteurs), 1-4 = par cellule

class GotoRequest(BaseModel):
    table: int      # table 1-4
    position: float  # mm

class TorqueRequest(BaseModel):
    on: bool  # True = lock (TORQUE_ON), False = unlock (TORQUE_OFF)

class PresetRequest(BaseModel):
    preset: str

class CustomPreset(BaseModel):
    name: str
    frequency: float
    force: float
    forces: Optional[list[float]] = None  # forces par cellule (4 capteurs)

class MachineStateResponse(BaseModel):
    preset_name: str
    frequency_hz: Optional[float]
    t_speed_percent: int
    force_target: Optional[float]
    force_targets: list[float]
    positions: list[float]
    sensors: list[float]  # DEPRECATED: utiliser cell_forces_N
    cell_volts_mv: list[float]  # Tensions brutes de l'OpenRB (mV)
    cell_forces_N: list[float]  # Forces calculées via calibration (Newton)
    motor_current: Optional[str]
    errors: str
    slave_status: str
    machine_status: str
    cycle_start: Optional[int]  # epoch ms du début de cycle (None = pas démarré)

class SerialLogEntry(BaseModel):
    type: str
    message: str

# --- FastAPI App -------------------------------------------------

app = FastAPI(title="Control Panel API", version="1.0.0")

# Enable CORS for React frontend - MUST be first middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
    expose_headers=["*"],
)

# Global exception handler to ensure CORS headers are sent even on errors
from fastapi import Request
from fastapi.responses import JSONResponse


@app.middleware("http")
async def log_requests(request: Request, call_next):
    """Logge chaque requête HTTP : méthode, chemin, statut, durée.

    Visible à partir du niveau DEBUG (--verbose). Le polling /api/status et
    /api/logs (5 Hz / 2 Hz) reste donc silencieux en mode normal (INFO).
    """
    t0 = time.monotonic()
    response = await call_next(request)
    dt_ms = (time.monotonic() - t0) * 1000.0
    log.debug("%s %s -> %s (%.1f ms)",
              request.method, request.url.path, response.status_code, dt_ms)
    return response


@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    log.error("Unhandled exception on %s %s: %s", request.method, request.url.path, exc)
    import traceback
    traceback.print_exc()

    return JSONResponse(
        status_code=500,
        content={"detail": str(exc)},
        headers={
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "*",
            "Access-Control-Allow-Headers": "*",
        }
    )

# Global controller instance
controller: Optional[MachineController] = None
serial_logs: list[SerialLogEntry] = []
background_reader_thread: Optional[threading.Thread] = None
is_reading: bool = False

# --- Helper Functions -----------------------------------------------

# Niveau de log structuré associé à chaque type d'entrée du moniteur série.
_ACTION_LOG_LEVEL = {
    "command": logging.INFO,    # commande envoyée à la machine
    "state": logging.INFO,      # changement d'état (connexion, preset...)
    "response": logging.DEBUG,  # ligne renvoyée par l'OpenRB (bruyant -> DEBUG)
    "error": logging.ERROR,     # erreur
}


def log_action(type_: str, message: str) -> None:
    """Ajoute une entrée au buffer du moniteur série ET la logue (niveau selon type)."""
    serial_logs.append(SerialLogEntry(type=type_, message=message))
    if len(serial_logs) > 200:
        serial_logs.pop(0)
    # Double sortie : buffer UI (déjà fait) + log structuré (visible en --verbose).
    log.log(_ACTION_LOG_LEVEL.get(type_, logging.INFO), "[%s] %s", type_, message)

def background_reader():
    """Thread that continuously reads from serial port and logs everything."""
    global controller, is_reading

    while is_reading:
        try:
            if controller and controller.link.ser:
                line = controller.read_once()
                if line:
                    log_action("response", line)
            time.sleep(0.05)  # Small delay to avoid 100% CPU
        except Exception as e:
            log_action("error", f"Read error: {str(e)}")
            time.sleep(0.5)

def start_background_reader():
    """Start the background reader thread."""
    global background_reader_thread, is_reading

    if is_reading:
        return  # Already running

    is_reading = True
    background_reader_thread = threading.Thread(target=background_reader, daemon=True)
    background_reader_thread.start()
    print("✓ Background serial reader started")

def stop_background_reader():
    """Stop the background reader thread."""
    global is_reading, background_reader_thread

    is_reading = False
    if background_reader_thread:
        background_reader_thread.join(timeout=1.0)
    print("✓ Background serial reader stopped")

# Au-delà de ce délai sans data de l'OpenRB, on considère le slave hors-ligne.
SLAVE_DATA_TIMEOUT_S = 3.0

def get_state_dict(state: MachineState) -> MachineStateResponse:
    """Convert MachineState to response dict."""
    # L'état du slave (OpenRB) ne dépend QUE de la réception de data récente,
    # pas de la ligne SLAVE: du firmware (qui ne reflète que le nb de Dynamixels).
    if state.last_data_ts > 0.0 and (time.monotonic() - state.last_data_ts) < SLAVE_DATA_TIMEOUT_S:
        slave_status = "ONLINE"
    else:
        slave_status = "OFFLINE"

    # Convertir les tensions brutes (mV) en forces (N) via calibration
    # state.sensors contient les mV bruts reçus de l'OpenRB
    cell_forces_N = []
    for cell_id, mv in enumerate(state.sensors):
        force_n = force_cal.mV_to_newton(cell_id, mv)
        cell_forces_N.append(force_n)

    return MachineStateResponse(
        preset_name=state.preset_name,
        frequency_hz=state.frequency_hz,
        t_speed_percent=state.t_speed_percent,
        force_target=state.force_target,
        force_targets=state.force_targets,
        positions=state.positions,
        sensors=state.sensors,  # Gardé pour compat (ancien champ)
        cell_volts_mv=state.sensors,  # mV bruts
        cell_forces_N=cell_forces_N,  # Forces converties
        motor_current=state.motor_current,
        errors=state.errors,
        slave_status=slave_status,
        machine_status=state.machine_status,
        cycle_start=state.cycle_start,
    )

# --- Connection Endpoints -------------------------------------------

@app.get("/api/ports")
def get_available_ports():
    """Get list of available serial ports."""
    import serial.tools.list_ports
    ports = [port.device for port in serial.tools.list_ports.comports()]
    return {"ports": ports}

@app.get("/api/wifi-interfaces")
def get_wifi_interfaces():
    """Get list of available WiFi interfaces."""
    if WiFiManager is None:
        return {"interfaces": [], "error": "WiFi manager not available"}

    try:
        manager = WiFiManager()
        interfaces = manager.get_available_wifi_interfaces()
        return {"interfaces": interfaces}
    except Exception as e:
        return {"interfaces": [], "error": str(e)}

def _is_wifi_interface(port: str) -> bool:
    """Check if the port string is a WiFi interface name."""
    # WiFi interfaces on macOS start with 'en', on Linux with 'wlan', on Windows with 'WiFi'
    wifi_prefixes = ('en', 'wlan', 'wifi', 'br', 'vir')
    return any(port.lower().startswith(prefix) for prefix in wifi_prefixes) or port in [
        'wlan0', 'wlan1', 'eth0', 'eth1', 'en0', 'en1', 'en2', 'en3', 'en4', 'en5'
    ]

@app.post("/api/connect")
def connect(request: ConnectRequest):
    """Connect to machine on specified port or WiFi interface."""
    global controller

    try:
        # Check if this is a WiFi interface or a serial port
        if _is_wifi_interface(request.port):
            # WiFi connection (AP Mode - ESP8266 broadcasts its own network)
            try:
                log_action("state", f"Attempting WiFi connection to {request.port}")
                print(f"\n[WiFi] Connecting to: {request.port}")

                # Load configuration
                try:
                    wifi_manager = WiFiManager()
                except Exception as e:
                    print(f"[WiFi] Warning: Could not load WiFiManager: {e}")
                    wifi_manager = None

                # SOURCE UNIQUE des paramètres ESP : .machine_config.ini, lu via
                # machine_config (qui localise le .ini de façon fiable depuis
                # backend/). On NE dépend PLUS d'un fallback hardcodé pour le jeton.
                nodeMcu_config = _machine_config.nodemcu()
                # Compat : si WiFiManager a chargé une config (ex: legacy setup.json),
                # elle ne sert qu'à compléter d'éventuels champs absents du .ini.
                if wifi_manager and wifi_manager.config:
                    legacy = wifi_manager.config.get('nodeMcu', {})
                    for k, v in legacy.items():
                        nodeMcu_config.setdefault(k, v)
                log.debug("[WiFi] nodeMcu config résolue: ip=%s port=%s key=%s",
                          nodeMcu_config.get('ip'), nodeMcu_config.get('port'),
                          '(défini)' if nodeMcu_config.get('key') else '(VIDE)')

                # IP/port depuis le .ini (défauts AP usine si vraiment absents).
                ip = nodeMcu_config.get('ip') or '192.168.4.1'
                port = nodeMcu_config.get('port') or 8080

                # ⚠️ HARDCODE TEMPORAIRE DU JETON — voir PROD/TODO.md (#1).
                # La résolution du jeton depuis .machine_config.ini ne remonte pas
                # correctement jusqu'à l'ESP dans l'environnement actuel (l'ESP
                # recevait "bearer_token_secret"). On force la valeur attendue par
                # le firmware pour débloquer le dev. À RETIRER une fois le bug
                # de chargement de config résolu (réactiver la ligne ci-dessous).
                # key = nodeMcu_config.get('key', '')
                key = "1276371237612hj1h12387dsads8912"  # TODO(config): dé-hardcoder

                log.info("[WiFi] Connexion ESP8266 %s:%s (jeton: %s) [HARDCODE temporaire]",
                         ip, port, '*' * len(key))

                if not ip:
                    raise HTTPException(
                        status_code=500,
                        detail="NodeMCU IP address not configured in setup.json"
                    )

                # Create WiFi link
                wifi_link = WiFiLink(ip=ip, port=port, auth_token=key)

                # Test connection to ESP8266
                print(f"[WiFi] Testing connectivity to {ip}:{port}...")
                if not wifi_link.connect():
                    log_action("error", f"Failed to connect to NodeMCU at {ip}:{port}")
                    print(f"[WiFi] ❌ Connection failed!")
                    raise HTTPException(
                        status_code=500,
                        detail=f"Failed to connect to NodeMCU at {ip}:{port}. Make sure:\n1. NodeMCU is powered on\n2. You're connected to '{nodeMcu_config.get('wifi', {}).get('ssid', 'NodeMCU-Control')}' WiFi\n3. IP address in setup.json is correct"
                    )

                print(f"[WiFi] ✅ Connected to ESP8266!")

                # Create controller with WiFi link
                state = MachineState()
                logger = DebugLogger()
                controller = MachineController(wifi_link, state, logger)
                # WiFi link is already verified above; mark the state connected
                # (the serial path does this via controller.connect()).
                controller.state.machine_status = "CONNECTED"

                # Restore what we were doing from the node, so a frontend reload /
                # reconnect resumes the current state (the node remembers it).
                try:
                    esp_status = wifi_link.get_status()
                    if esp_status:
                        # Runtime (start of cycle)
                        cs = int(float(esp_status.get("cycle_start", 0) or 0))
                        controller.state.cycle_start = cs if cs > 0 else None

                        # Frequency setpoint
                        freq = esp_status.get("frequency")
                        if freq is not None:
                            controller.state.frequency_hz = float(freq)

                        # Per-cell force setpoints (+ global force_target for display)
                        forces = esp_status.get("forces")
                        if isinstance(forces, list) and len(forces) >= 4:
                            forces_f = [float(x) for x in forces[:4]]
                            controller.state.force_targets = forces_f
                            controller.state.force_target = (
                                forces_f[0] if len(set(forces_f)) == 1 else max(forces_f)
                            )

                        print(f"[WiFi] Restored from node: "
                              f"freq={controller.state.frequency_hz}, "
                              f"forces={controller.state.force_targets}, "
                              f"cycle_start={controller.state.cycle_start}")
                except Exception as e:
                    print(f"[WiFi] Could not restore state from node: {e}")

                log_action("state", f"Connected via WiFi to {ip}:{port}")
                print(f"[WiFi] MachineController initialized\n")

                return {"success": True, "message": f"Connected via WiFi to {ip}:{port}"}

            except HTTPException:
                raise
            except FileNotFoundError:
                raise HTTPException(
                    status_code=500,
                    detail="setup.json not found. Please configure WiFi settings in tools/config/setup.json"
                )
            except Exception as e:
                log_action("error", f"WiFi connection error: {str(e)}")
                raise HTTPException(
                    status_code=500,
                    detail=f"WiFi connection error: {str(e)}"
                )
        else:
            # Serial connection
            link = SerialLink(
                port=request.port,
                baudrate=DEFAULT_BAUDRATE,
                timeout=DEFAULT_TIMEOUT,
            )
            state = MachineState()
            logger = DebugLogger()

            controller = MachineController(link, state, logger)
            ok = controller.connect()

            if ok:
                log_action("state", f"Connected to {request.port}")
                # Start background reader for continuous monitoring
                start_background_reader()
                return {"success": True, "message": f"Connected to {request.port}"}
            else:
                log_action("error", f"Failed to connect to {request.port}")
                raise HTTPException(status_code=500, detail="Failed to connect")
    except HTTPException:
        raise
    except Exception as e:
        log_action("error", str(e))
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/disconnect")
def disconnect():
    """Disconnect from machine."""
    global controller

    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    # Stop background reader
    stop_background_reader()

    controller.disconnect()
    log_action("state", "Disconnected")
    return {"success": True, "message": "Disconnected"}

def _apply_esp_live_status(state: MachineState, esp: dict) -> None:
    """Map the OpenRB live cache (relayed by the ESP) into the machine state.

    Plus aucun aller-retour série : l'OpenRB streame son statut en burst, l'ESP
    le cache, et GET /api/status renvoie ce cache instantanément.
    """
    rb_state = esp.get("rb_state")
    if rb_state and rb_state != "UNKNOWN":
        state.machine_status = rb_state

    rb_freq = esp.get("rb_frequency")
    if rb_freq is not None:
        try:
            state.frequency_hz = float(rb_freq)
        except (TypeError, ValueError):
            pass

    positions = esp.get("positions")
    if isinstance(positions, list) and positions:
        state.positions = [float(p) for p in positions[:4]]
        while len(state.positions) < 4:
            state.positions.append(0.0)

    sensors = esp.get("sensors")
    if isinstance(sensors, list) and sensors:
        state.sensors = [float(s) for s in sensors[:4]]
        while len(state.sensors) < 4:
            state.sensors.append(0.0)

    # OpenRB/slave en ligne = l'ESP rapporte une donnée fraîche (rb_online).
    # On rafraîchit last_data_ts pour que get_state_dict() calcule ONLINE.
    if esp.get("rb_online"):
        state.last_data_ts = time.monotonic()


@app.get("/api/status")
def get_status():
    """Get current machine state."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    if isinstance(controller.link, WiFiLink):
        # Liaison permanente : on lit le cache de l'ESP (GET léger), sans envoyer
        # de GET_STATUS sur le lien série -> latence ≈ WiFi seul.
        esp = controller.link.get_status()
        if esp:
            _apply_esp_live_status(controller.state, esp)
    else:
        _read_all_responses()

    return get_state_dict(controller.state)

# --- Command Endpoints -----------------------------------------------

def _read_all_responses():
    """Read & parse all available response lines, and log them for the monitor."""
    if not controller:
        return
    for _ in range(40):  # Read up to 40 lines (a status burst = ~6 lines)
        line = controller.read_once()
        if not line:
            break
        log_action("response", line)

@app.post("/api/command/home")
def home():
    """Execute HOME command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.home()
    log_action("command", "HOME")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/start")
def start():
    """Execute START command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.start()
    log_action("command", "START")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/stop")
def stop():
    """Execute STOP command (alias for hard_reset for now)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.hard_reset()
    log_action("command", "STOP")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/hard-reset")
def hard_reset():
    """Execute HARD_RESET command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.hard_reset()
    log_action("command", "HARD_RESET")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/frequency")
def set_frequency(request: FrequencyRequest):
    """Set frequency (Hz)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.set_frequency(request.frequency)
    log_action("command", f"SET_FREQ:{request.frequency}")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/speed")
def set_speed(request: SpeedRequest):
    """Set T_Speed (%)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.set_speed(request.speed)
    log_action("command", f"SET_SPEED:{request.speed}")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/force")
def set_force(request: ForceRequest):
    """Set force target (N)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.set_force(request.force, request.sensor)
    if request.sensor is None:
        log_action("command", f"SET_FORCE:{request.force}")
    else:
        log_action("command", f"SET_FORCE[cell {request.sensor}]:{request.force}")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/goto")
def goto(request: GotoRequest):
    """Move a table (1-4) to a position (mm)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.goto(request.table, request.position)
    log_action("command", f"GOTO table {request.table} -> {request.position} mm")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/torque")
def set_torque(request: TorqueRequest):
    """Lock/unlock the Dynamixel motors. on=False -> unlock (manual positioning)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.torque(request.on)
    log_action("command", "TORQUE_ON" if request.on else "TORQUE_OFF")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/preset")
def apply_preset(request: PresetRequest):
    """Apply frequency preset."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    ok = controller.apply_preset(request.preset)
    if not ok:
        log_action("error", f"Invalid preset: {request.preset}")
        raise HTTPException(status_code=400, detail="Invalid preset")

    log_action("command", f"PRESET:{request.preset}")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/manual")
def send_manual_command(request: dict):
    """Send manual command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    command = request.get("command", "").strip()
    if not command:
        raise HTTPException(status_code=400, detail="Empty command")

    controller._send(command)
    log_action("command", command)
    _read_all_responses()  # Read all responses just like other commands

    return {"success": True, "state": get_state_dict(controller.state)}

# --- Custom Presets (persisted in preset.json) ----------------------

@app.get("/api/presets")
def list_presets():
    """List all saved custom presets (frequency + force couples)."""
    return {"presets": preset_store.load_presets()}

@app.post("/api/presets")
def save_custom_preset(preset: CustomPreset):
    """Create or update a custom preset. Persisted to preset.json."""
    name = preset.name.strip()
    if not name:
        raise HTTPException(status_code=400, detail="Preset name is required")

    presets = preset_store.set_preset(name, preset.frequency, preset.force, preset.forces)
    log_action("state", f"Preset saved: {name} ({preset.frequency} Hz / {preset.force} N)")
    return {"success": True, "presets": presets}

@app.delete("/api/presets/{name}")
def remove_custom_preset(name: str):
    """Delete a custom preset. Persisted to preset.json."""
    presets = preset_store.delete_preset(name)
    log_action("state", f"Preset deleted: {name}")
    return {"success": True, "presets": presets}

@app.post("/api/presets/{name}/apply")
def apply_custom_preset(name: str):
    """Apply a custom preset: send its frequency and force to the node."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    presets = preset_store.load_presets()
    if name not in presets:
        raise HTTPException(status_code=404, detail=f"Preset '{name}' not found")

    p = presets[name]
    controller.set_frequency(p["frequency"])
    controller.set_force(p["force"])
    controller.state.preset_name = name
    log_action("command", f"PRESET {name}: {p['frequency']} Hz / {p['force']} N")
    _read_all_responses()

    return {
        "success": True,
        "preset": {"name": name, **p},
        "state": get_state_dict(controller.state),
    }

# --- Monitoring Endpoints -------------------------------------------

@app.get("/api/logs")
def get_logs(limit: int = 50):
    """Get serial logs."""
    return {"logs": serial_logs[-limit:]}

@app.get("/api/logs/commands")
def get_command_history(limit: int = 30):
    """Get command history."""
    if not controller:
        return {"commands": []}

    return {"commands": controller.logger.command_history[-limit:]}

@app.get("/api/logs/console")
def get_console_logs(limit: int = 30):
    """Get console logs."""
    if not controller:
        return {"logs": []}

    return {"logs": controller.logger.console_lines[-limit:]}

# --- Health Check ------------------------------------------------

@app.get("/api/health")
def health_check():
    """Health check endpoint."""
    return {
        "status": "ok",
        "connected": controller is not None and controller.state.machine_status == "CONNECTED"
    }


@app.get("/api/calibration/info")
def calibration_info():
    """Retourne les infos de calibration (résistance, nombre de points, plages)."""
    return force_cal.get_info()


if __name__ == "__main__":
    import argparse
    import uvicorn
    from debug.logging_setup import setup_logging
    import machine_config

    parser = argparse.ArgumentParser(description="Backend API de la machine de test")
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="-v = DEBUG (requêtes/commandes), -vv = TRACE (tout)")
    parser.add_argument("--log-level", default=None,
                        help="Niveau explicite : ERROR|WARNING|INFO|DEBUG|TRACE "
                             "(par défaut : [logging].level du .machine_config.ini)")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    # Niveau : -v/-vv prioritaire, sinon --log-level, sinon valeur du .ini.
    level_name = args.log_level or machine_config.log_level()
    level = setup_logging(verbose=args.verbose, level_name=level_name)
    log.info("Démarrage backend sur %s:%s (niveau de log: %s)",
             args.host, args.port, logging.getLevelName(level))

    # Initialiser la calibration des cellules de force
    cal_config = machine_config.calibration()
    resistance = cal_config.get("resistance", 330)
    force_cal.init_calibration(resistance)

    # uvicorn aligné sur notre niveau (DEBUG -> accès loggés par uvicorn aussi).
    uvicorn_level = "debug" if level <= logging.DEBUG else "info"
    uvicorn.run(app, host=args.host, port=args.port, log_level=uvicorn_level)
