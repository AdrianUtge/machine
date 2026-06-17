"""
API FastAPI pour le frontend React.
Expose les commandes de contrôle machine via REST.
"""

from fastapi import FastAPI, HTTPException, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional
import asyncio
import json
import threading
import time

from comm.serial_link import SerialLink
from comm.wifi_link import WiFiLink
from config import DEFAULT_BAUDRATE, DEFAULT_PORT, DEFAULT_TIMEOUT
from core.controller import MachineController
from core.state import MachineState
from core import preset_store
from debug.logger import DebugLogger
from comm.ports import choose_serial_port
import sys
import os
from pathlib import Path

# Add tools/config to path for WiFiManager
tools_config_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../tools/config'))
if tools_config_path not in sys.path:
    sys.path.insert(0, tools_config_path)

try:
    from wifi_manager import WiFiManager
except ImportError as e:
    print(f"Warning: Could not import WiFiManager: {e}")
    WiFiManager = None

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
    sensors: list[float]
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

@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    print(f"[ERROR] Unhandled exception: {exc}")
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

def log_action(type_: str, message: str) -> None:
    """Add an entry to serial logs."""
    serial_logs.append(SerialLogEntry(type=type_, message=message))
    if len(serial_logs) > 200:
        serial_logs.pop(0)

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

    return MachineStateResponse(
        preset_name=state.preset_name,
        frequency_hz=state.frequency_hz,
        t_speed_percent=state.t_speed_percent,
        force_target=state.force_target,
        force_targets=state.force_targets,
        positions=state.positions,
        sensors=state.sensors,
        motor_current=state.motor_current,
        errors=state.errors,
        slave_status=slave_status,
        machine_status=state.machine_status,
        cycle_start=state.cycle_start,
    )

# --- Connection Endpoints -------------------------------------------

@app.get("/api/ports")
async def get_available_ports():
    """Get list of available serial ports."""
    import serial.tools.list_ports
    ports = [port.device for port in serial.tools.list_ports.comports()]
    return {"ports": ports}

@app.get("/api/wifi-interfaces")
async def get_wifi_interfaces():
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
async def connect(request: ConnectRequest):
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

                # Get NodeMCU configuration from setup.json
                if wifi_manager and wifi_manager.config:
                    nodeMcu_config = wifi_manager.config.get('nodeMcu', {})
                    print(f"[WiFi] Config found: {nodeMcu_config}")
                else:
                    print("[WiFi] No config found, using defaults")
                    nodeMcu_config = {}

                # Get IP and port from config or use defaults (AP mode defaults)
                ip = nodeMcu_config.get('ip', '192.168.4.1')  # Default AP mode IP
                port = nodeMcu_config.get('port', 8080)
                key = nodeMcu_config.get('key', 'bearer_token_secret')  # Default token

                print(f"[WiFi] Connecting to ESP8266 at {ip}:{port}")
                print(f"[WiFi] Using auth token: {'*' * len(key)}")

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
async def disconnect():
    """Disconnect from machine."""
    global controller

    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    # Stop background reader
    stop_background_reader()

    controller.disconnect()
    log_action("state", "Disconnected")
    return {"success": True, "message": "Disconnected"}

@app.get("/api/status")
async def get_status():
    """Get current machine state."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    # WiFi: actively request a fresh status from the OpenRB (the ESP relays it).
    if isinstance(controller.link, WiFiLink):
        controller.get_status()

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
async def home():
    """Execute HOME command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.home()
    log_action("command", "HOME")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/start")
async def start():
    """Execute START command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.start()
    log_action("command", "START")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/stop")
async def stop():
    """Execute STOP command (alias for hard_reset for now)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.hard_reset()
    log_action("command", "STOP")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/hard-reset")
async def hard_reset():
    """Execute HARD_RESET command."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.hard_reset()
    log_action("command", "HARD_RESET")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/frequency")
async def set_frequency(request: FrequencyRequest):
    """Set frequency (Hz)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.set_frequency(request.frequency)
    log_action("command", f"SET_FREQ:{request.frequency}")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/speed")
async def set_speed(request: SpeedRequest):
    """Set T_Speed (%)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.set_speed(request.speed)
    log_action("command", f"SET_SPEED:{request.speed}")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/force")
async def set_force(request: ForceRequest):
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
async def goto(request: GotoRequest):
    """Move a table (1-4) to a position (mm)."""
    if not controller:
        raise HTTPException(status_code=400, detail="Not connected")

    controller.goto(request.table, request.position)
    log_action("command", f"GOTO table {request.table} -> {request.position} mm")
    _read_all_responses()

    return {"success": True, "state": get_state_dict(controller.state)}

@app.post("/api/command/preset")
async def apply_preset(request: PresetRequest):
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
async def send_manual_command(request: dict):
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
async def list_presets():
    """List all saved custom presets (frequency + force couples)."""
    return {"presets": preset_store.load_presets()}

@app.post("/api/presets")
async def save_custom_preset(preset: CustomPreset):
    """Create or update a custom preset. Persisted to preset.json."""
    name = preset.name.strip()
    if not name:
        raise HTTPException(status_code=400, detail="Preset name is required")

    presets = preset_store.set_preset(name, preset.frequency, preset.force, preset.forces)
    log_action("state", f"Preset saved: {name} ({preset.frequency} Hz / {preset.force} N)")
    return {"success": True, "presets": presets}

@app.delete("/api/presets/{name}")
async def remove_custom_preset(name: str):
    """Delete a custom preset. Persisted to preset.json."""
    presets = preset_store.delete_preset(name)
    log_action("state", f"Preset deleted: {name}")
    return {"success": True, "presets": presets}

@app.post("/api/presets/{name}/apply")
async def apply_custom_preset(name: str):
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
async def get_logs(limit: int = 50):
    """Get serial logs."""
    return {"logs": serial_logs[-limit:]}

@app.get("/api/logs/commands")
async def get_command_history(limit: int = 30):
    """Get command history."""
    if not controller:
        return {"commands": []}

    return {"commands": controller.logger.command_history[-limit:]}

@app.get("/api/logs/console")
async def get_console_logs(limit: int = 30):
    """Get console logs."""
    if not controller:
        return {"logs": []}

    return {"logs": controller.logger.console_lines[-limit:]}

# --- Health Check ------------------------------------------------

@app.get("/api/health")
async def health_check():
    """Health check endpoint."""
    return {
        "status": "ok",
        "connected": controller is not None and controller.state.machine_status == "CONNECTED"
    }

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
