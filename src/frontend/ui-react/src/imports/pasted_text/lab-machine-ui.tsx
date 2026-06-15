Design a desktop interface for a serial-controlled laboratory machine.

The interface must allow the user to connect to an Arduino/OpenRB-150-based machine, send machine commands, monitor live machine status, view sensor data, and read serial logs in real time.

The design should be clean, technical, and suitable for engineering/lab use.

Main features to implement:

1. Serial Connection Screen

Create an initial connection screen where the user can select the serial port used by the machine.

The interface must include:

- A title: “Machine Connection”

- A scrollable list or dropdown of available serial ports

- A refresh button to rescan ports

- A connect button

- A connection status message

Each serial port item should show:

- Port name, for example: `/dev/cu.usbmodem101`

- Optional device type, for example: USB / UART / Bluetooth

- Availability status: available / busy / unavailable

Possible connection states:

- No port detected

- Port selected

- Connecting

- Connected

- Connection error

2. Main Dashboard

After connection, show the main machine dashboard.

The dashboard must clearly display:

- Current machine state: `IDLE`, `HOMING`, `RUNNING`, `ERROR`, `SHUTDOWN`

- Serial connection status

- Last command sent

- Last command completed

- Current machine position if available

- Current frequency if available

- Current speed if available

Use clear status badges or indicators for machine state.

3. Command Panel

Create a command panel with the main machine actions.

Buttons to include:

- HOME

- START

- STOP

- HARD RESET

- SHUTDOWN

- GOTO

Behavior:

- Buttons must have clear enabled, disabled, loading, success, and error states.

- When a command is running, the corresponding button should show a loading state.

- The command should remain “in progress” until the interface receives `DONE:<COMMAND>`.

- Dangerous commands like `HARD RESET` and `SHUTDOWN` should have a stronger visual style.

The `GOTO` command must include:

- A numeric input field for target position

- Unit: mm

- A send button

- The generated command format should be: `GOTO:<position_mm>`

4. Motion Settings Panel

Create a settings panel for machine parameters.

Include a speed control:

- Numeric input

- Unit label

- Apply button

- Command format: `SET_SPEED:<value>`

Include a frequency control:

- Numeric input

- Unit: Hz

- Apply button

- Current frequency display

- Command format: `SET_FREQ:<value>`

The frequency setting is used to control the slave motor excitation system.

5. Live Sensor Data Panel

Create a sensor data section for four load cells.

Each load cell card should display:

- Sensor name: Load Cell 1, Load Cell 2, Load Cell 3, Load Cell 4

- Force value in N

- Equivalent mass in g

- Optional raw value

- Sensor status: OK / noisy / error / disconnected

Example data format:

- Load Cell 1: 0.43 N / 43.8 g

- Load Cell 2: 0.39 N / 39.7 g

- Load Cell 3: 0.00 N / 0.0 g

- Load Cell 4: 0.02 N / 2.0 g

Optional: add a small real-time chart for each sensor.

6. Slave Status Panel

Create a dedicated section for the slave board.

The slave board controls the excitation motor.

Display:

- Slave connection status: connected / disconnected

- Slave motor state: running / stopped

- Current excitation frequency

- Last slave message received

Possible slave messages:

- `SLAVE:CONNECTED`

- `SLAVE:DISCONNECTED`

- `SLAVE:FREQ:<value>`

- `SLAVE:RUNNING`

- `SLAVE:STOPPED`

- `SLAVE:ERROR:<message>`

7. Serial Monitor

Create an integrated serial monitor at the bottom of the interface.

The serial monitor must show live logs with timestamps.

It should visually distinguish:

- Commands sent by the app

- Responses received from the machine

- State messages

- Error messages

- DONE messages

Example log display:

[14:32:10] >> HOME

[14:32:11] ACK:HOME

[14:32:12] STATE:HOMING

[14:32:18] Limit detected

[14:32:19] Homing complete

[14:32:19] DONE:HOME

The serial monitor should include:

- Auto-scroll toggle

- Clear logs button

- Optional filter by message type

- Monospace font

8. Error Handling

Create a clear error system.

Errors must be highly visible and easy to understand.

Possible errors:

- Serial port not found

- Serial connection lost

- Command rejected

- Motor blocked

- Sensor disconnected

- Slave not responding

- Timeout: no `DONE:<COMMAND>` received

- Invalid command parameter

Each error message should include:

- Error title

- Short explanation

- Suggested action if possible

- Dismiss button

Example:

“Timeout error: no `DONE:HOME` received. Check the serial connection or machine state.”

9. Command Protocol to Support

The interface must be designed around the following outgoing commands:

HOME

START

STOP

HARD_RESET

SHUTDOWN

GOTO:<position_mm>

SET_SPEED:<value>

SET_FREQ:<value>

GET_STATUS

The interface must be able to parse and display the following incoming responses:

ACK:<COMMAND>

ERROR:<MESSAGE>

STATE:<STATE>

DONE:<COMMAND>

FREQ:<VALUE>

POSITION:<VALUE>

CURRENT:<VALUE>

FORCE:<VALUE>

SLAVE:<MESSAGE>

10. Recommended Layout

Use a desktop layout with four main areas:

Top header:

- Connected port

- Global machine state

- Connection status

- Disconnect button

Left sidebar:

- Main command buttons

- HOME

- START

- STOP

- GOTO

- HARD RESET

- SHUTDOWN

Central dashboard:

- Machine status card

- Motion settings

- Sensor data cards

- Slave status card

Bottom panel:

- Live serial monitor

The layout should feel like an engineering control dashboard.

11. Visual Style

The design should be:

- Minimal

- Technical

- Clear

- Robust

- Easy to read during testing

Use:

- Monospace font for serial logs

- Status badges for machine state

- Neutral colors for normal states

- Green for connected/success states

- Orange for warnings or running states

- Red for errors and dangerous actions

- Clear spacing between panels

- Card-based layout for dashboard sections

12. Important UX Rule

Every command must stay in a pending/running state until the interface receives the matching `DONE:<COMMAND>` message.

For example:

- User clicks HOME

- Interface sends `HOME`

- HOME button becomes loading

- Serial monitor opens or highlights activity

- Interface receives `DONE:HOME`

- HOME button returns to normal

- Dashboard updates last completed command to HOME

This rule applies to all long-running commands.