#!/bin/bash

################################################################################
#                    MACHINE CONTROL PANEL - START SCRIPT
################################################################################
#
# This script automatically starts the complete Machine Control Panel:
# - Backend: FastAPI server on http://127.0.0.1:8000
# - Frontend: React UI on http://localhost:5173
#
# Usage: ./start.sh
#
################################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}           MACHINE CONTROL PANEL - STARTUP SCRIPT${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

################################################################################
# 1. CHECK REQUIREMENTS
################################################################################

echo -e "${YELLOW}[1/5] Checking requirements...${NC}"

# Check Python
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}✗ Python 3 not found${NC}"
    echo "Please install Python 3.8 or higher"
    exit 1
fi
echo -e "${GREEN}✓ Python $(python3 --version | awk '{print $2}')${NC}"

# Check Node.js
if ! command -v node &> /dev/null; then
    echo -e "${RED}✗ Node.js not found${NC}"
    echo "Please install Node.js 18 or higher"
    exit 1
fi
echo -e "${GREEN}✓ Node.js $(node --version)${NC}"

# Check npm
if ! command -v npm &> /dev/null; then
    echo -e "${RED}✗ npm not found${NC}"
    echo "Please install npm"
    exit 1
fi
echo -e "${GREEN}✓ npm $(npm --version)${NC}"

echo ""

################################################################################
# 2. INSTALL BACKEND DEPENDENCIES
################################################################################

echo -e "${YELLOW}[2/5] Setting up backend...${NC}"

cd "$SCRIPT_DIR/backend"

if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
fi

# Activate virtual environment
source venv/bin/activate

# Install Python dependencies
if [ -f "requirements.txt" ]; then
    echo "Installing Python dependencies..."
    pip install -q -r requirements.txt
    echo -e "${GREEN}✓ Backend dependencies installed${NC}"
else
    echo -e "${RED}✗ requirements.txt not found${NC}"
    exit 1
fi

echo ""

################################################################################
# 3. INSTALL FRONTEND DEPENDENCIES
################################################################################

echo -e "${YELLOW}[3/5] Setting up frontend...${NC}"

cd "$SCRIPT_DIR/frontend/ui-react"

if [ ! -d "node_modules" ]; then
    echo "Installing npm dependencies..."
    npm install -q
    echo -e "${GREEN}✓ Frontend dependencies installed${NC}"
else
    echo -e "${GREEN}✓ Frontend dependencies already installed${NC}"
fi

echo ""

################################################################################
# 4. START BACKEND
################################################################################

echo -e "${YELLOW}[4/5] Starting backend server...${NC}"

cd "$SCRIPT_DIR/backend"

# Start backend in background
python3 api.py > /tmp/machine_backend.log 2>&1 &
BACKEND_PID=$!

echo "Backend PID: $BACKEND_PID"
echo -e "${GREEN}✓ Backend server started${NC}"

# Wait for backend to be ready
echo "Waiting for backend to be ready..."
for i in {1..30}; do
    if curl -s http://127.0.0.1:8000/docs > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Backend is ready!${NC}"
        break
    fi
    if [ $i -eq 30 ]; then
        echo -e "${RED}✗ Backend failed to start${NC}"
        echo "Check logs: tail -f /tmp/machine_backend.log"
        kill $BACKEND_PID 2>/dev/null || true
        exit 1
    fi
    sleep 0.5
done

echo ""

################################################################################
# 5. START FRONTEND
################################################################################

echo -e "${YELLOW}[5/5] Starting frontend...${NC}"

cd "$SCRIPT_DIR/frontend/ui-react"

# Start frontend
echo -e "${GREEN}✓ Frontend starting on http://localhost:5173${NC}"
echo ""

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ MACHINE CONTROL PANEL IS RUNNING${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${YELLOW}📍 SERVICES:${NC}"
echo "   Backend API:     http://127.0.0.1:8000"
echo "   API Docs:        http://127.0.0.1:8000/docs"
echo "   Frontend:        http://localhost:5173"
echo ""
echo -e "${YELLOW}📋 NEXT STEPS:${NC}"
echo "   1. Configure WiFi: nano ../tools/config/setup.json"
echo "   2. Open browser:   http://localhost:5173"
echo "   3. Connect device: Select serial port in UI"
echo ""
echo -e "${YELLOW}⚙️  KEYBOARD SHORTCUTS:${NC}"
echo "   Ctrl+C           - Stop all services"
echo "   Backend logs:    tail -f /tmp/machine_backend.log"
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Start npm dev (frontend)
npm run dev

################################################################################
# CLEANUP (when script exits)
################################################################################

cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down...${NC}"
    if [ ! -z "$BACKEND_PID" ] && kill -0 $BACKEND_PID 2>/dev/null; then
        kill $BACKEND_PID 2>/dev/null || true
        echo -e "${GREEN}✓ Backend stopped${NC}"
    fi
    echo -e "${GREEN}✓ All services stopped${NC}"
    exit 0
}

trap cleanup EXIT INT TERM
