================================================================================
                    MACHINE CONTROL PANEL - SETUP & RUN
================================================================================

REQUIREMENTS:
- Python 3.8+
- Node.js 18+ (for React frontend)
- npm or pnpm (package manager)
- Serial port connection to Arduino/NodeMCU

================================================================================
QUICK START (3 STEPS)
================================================================================

1. CONFIGURE WiFi & NodeMCU
   - Edit: config/setup.json
   - Fill in your WiFi SSID, password, and NodeMCU IP address
   - See: config/README.md for detailed instructions

2. START BACKEND (FastAPI)
   cd "FRONT END"
   pip install -r requirements.txt
   python3 api.py

   Expected output:
   → Uvicorn running on http://127.0.0.1:8000
   → API documentation at http://127.0.0.1:8000/docs

3. START FRONTEND (React)
   cd "FRONT END/ui-react"
   npm install
   npm run dev

   Expected output:
   → Local: http://localhost:5173/
   → Open this URL in your browser

================================================================================
DETAILED SETUP INSTRUCTIONS
================================================================================

STEP 1: Configure WiFi Connection
──────────────────────────────────
File: config/setup.json

Edit with any text editor (VS Code, Sublime, Notepad, etc.):
  nano config/setup.json
  code config/setup.json

Required fields:
  - wifi.ssid: Your WiFi network name (exact match, case-sensitive)
  - wifi.password: WiFi password
  - nodeMcu.ip: NodeMCU IP address on your network (find in router)
  - nodeMcu.key: Secret key for authentication (any string, 20+ chars)

Example:
{
  "wifi": {
    "ssid": "MyNetwork",
    "password": "MyPassword123"
  },
  "nodeMcu": {
    "ip": "192.168.1.100",
    "key": "my-secret-key-here"
  }
}

Test connection:
  python3 config/wifi_manager.py

================================================================================
STEP 2: Install Dependencies
────────────────────────────
Backend (FastAPI + Serial Communication):
  cd "FRONT END"
  pip install -r requirements.txt

  This installs:
  - fastapi
  - uvicorn
  - pyserial
  - requests

Frontend (React UI):
  cd "FRONT END/ui-react"
  npm install

  This downloads React, TypeScript, Tailwind CSS, and dependencies

================================================================================
STEP 3: Start Backend Server
──────────────────────────
cd "FRONT END"
python3 api.py

Expected output:
  INFO:     Uvicorn running on http://127.0.0.1:8000 [Press ENTER to quit]

API Documentation:
  Visit: http://127.0.0.1:8000/docs

Endpoints available:
  GET    /api/ports           - List available serial ports
  POST   /api/connect         - Connect to machine
  POST   /api/disconnect      - Disconnect from machine
  GET    /api/status          - Get machine status
  POST   /api/command         - Send command (HOME, START, STOP, etc.)
  GET    /api/logs            - Get serial communication logs

================================================================================
STEP 4: Start Frontend Server
──────────────────────────
In a NEW terminal:
  cd "FRONT END/ui-react"
  npm run dev

Expected output:
  ➜  Local:   http://localhost:5173/
  ➜  Press q to quit

Open in browser:
  http://localhost:5173/

================================================================================
COMPLETE WORKFLOW
================================================================================

1. Terminal 1 - Backend:
   cd "FRONT END"
   python3 api.py

2. Terminal 2 - Frontend:
   cd "FRONT END/ui-react"
   npm run dev

3. Open browser:
   http://localhost:5173/

4. Use the Control Panel:
   - Select serial port (auto-detected)
   - Click "Connect"
   - Use motion controls, force graphs, sensor displays

5. To stop:
   - Press Ctrl+C in both terminals

================================================================================
TROUBLESHOOTING
================================================================================

ISSUE: "Port not found"
SOLUTION:
  1. Check serial connection
  2. Install CH340 drivers if using Arduino Uno clone
  3. Verify device is powered on

ISSUE: "WiFi connection failed"
SOLUTION:
  1. Check config/setup.json WiFi SSID matches exactly
  2. Verify WiFi password is correct
  3. Check NodeMCU IP is reachable: ping 192.168.x.x
  4. Run: python3 config/wifi_manager.py

ISSUE: Frontend shows blank page
SOLUTION:
  1. Check console for errors (F12)
  2. Verify backend is running (http://127.0.0.1:8000/docs)
  3. Clear browser cache: Ctrl+Shift+Delete
  4. Check network tab in DevTools

ISSUE: "Cannot connect to localhost:5173"
SOLUTION:
  1. Verify npm run dev completed successfully
  2. Check port 5173 is not in use: lsof -i :5173
  3. Kill process if needed: kill -9 <PID>
  4. Restart npm: npm run dev

ISSUE: Serial logs not updating
SOLUTION:
  1. Check serial cable is connected
  2. Verify machine is powered on
  3. Check baudrate in config/setup.json (default 115200)
  4. Restart connection

================================================================================
FEATURES
================================================================================

CONTROL PANEL FEATURES:
✓ Real-time force graph visualization (all 4 sensors)
✓ Adjustable time range (30s to 1h)
✓ Motion control (HOME, START, STOP, HARD RESET)
✓ Frequency adjustment (0-200 Hz)
✓ Position and sensor monitoring (4 tables, 4 sensors)
✓ GOTO position control (0-96mm travel)
✓ Interactive serial monitor with manual command input
✓ Connection status and error indicators
✓ Dark theme optimized for long viewing sessions

GRAPH FEATURES:
✓ Multi-curve display (all 4 sensors simultaneously)
✓ Color-coded sensor lines (Blue, Red, Green, Purple)
✓ Auto-scaling time axis (seconds, minutes, hours)
✓ Real-time data streaming
✓ Max/Avg/Current force statistics
✓ Fullscreen mode for detailed analysis
✓ Resizable height (150px - 500px)

================================================================================
PROJECT STRUCTURE
================================================================================

/
├── FRONT END/           - React UI + FastAPI Backend
│   ├── api.py          - FastAPI server
│   ├── ui-react/       - React application
│   ├── comm/           - Serial communication
│   ├── core/           - Machine control logic
│   └── requirements.txt - Python dependencies
│
├── MACHINE/            - Arduino firmware
├── config/             - WiFi & NodeMCU configuration
├── README.txt          - This file
└── .gitignore         - Git ignore rules

================================================================================
USEFUL COMMANDS
================================================================================

List available ports:
  python3 -c "import serial.tools.list_ports; print(list(serial.tools.list_ports.comports()))"

Check if port is in use:
  lsof -i :5173    (Frontend)
  lsof -i :8000    (Backend)

Kill process on port:
  kill -9 $(lsof -t -i :5173)
  kill -9 $(lsof -t -i :8000)

Test WiFi connection:
  python3 config/wifi_manager.py

View API logs:
  tail -f "FRONT END"/*.log

Clean npm cache:
  npm cache clean --force

================================================================================
SUPPORT & DOCUMENTATION
================================================================================

API Documentation: http://localhost:8000/docs (when backend running)
Config Guide: config/README.md
React Components: "FRONT END"/ui-react/src/app/components/
Backend Code: "FRONT END"/api.py
Serial Protocol: "FRONT END"/protocol.md

================================================================================
KEYBOARD SHORTCUTS (Frontend)
================================================================================

Ctrl+K          - Open command palette (if implemented)
F12             - Developer tools
Ctrl+Shift+I    - Inspector
Ctrl+Shift+C    - Element picker
Ctrl+L          - Clear console

================================================================================
PORTS USED
================================================================================

Frontend:  http://localhost:5173
Backend:   http://127.0.0.1:8000
Serial:    Auto-detected (COM3, /dev/ttyUSB0, etc.)
NodeMCU:   192.168.1.100 (configurable in config/setup.json)

================================================================================
QUICK REFERENCE
================================================================================

Install backend:     pip install -r requirements.txt
Install frontend:    npm install
Start backend:       python3 api.py
Start frontend:      npm run dev
Test WiFi:          python3 config/wifi_manager.py
View API docs:      http://localhost:8000/docs
View frontend:      http://localhost:5173

================================================================================
                            HAPPY TESTING! 🚀
================================================================================
