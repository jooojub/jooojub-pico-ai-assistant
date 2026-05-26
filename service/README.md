# Pico AI Assistant — Host Service

Python service that bridges USB serial communication with the Pico board and exposes an HTTP API for the web framework.

## Architecture

```
Web Framework  →  HTTP API (FastAPI)  →  SerialBridge  →  Pico (USB)
```

- **serial_bridge.py** — manages the USB serial connection to the Pico
- **api.py** — FastAPI HTTP routes
- **main.py** — entry point, wires bridge + API server

## Docker

### Build

```bash
docker compose build
```

### Run

```bash
# Linux (default: /dev/ttyACM0)
docker compose up

# Override serial port (e.g. Mac)
PICO_PORT=/dev/tty.usbmodem101 docker compose up

# Run in background
docker compose up -d
```

### Stop

```bash
docker compose down
```

### Logs

```bash
docker compose logs -f
```

## Running without Docker

```bash
pip install -r requirements.txt

# Auto-detect Pico port
python main.py

# Specify port manually
python main.py --port /dev/ttyACM0
```

## Serial Protocol (Pico ↔ Service)

| Direction   | Message          | Meaning                          |
|-------------|------------------|----------------------------------|
| Pico → Host | `READY`          | Pico started up                  |
| Pico → Host | `OK`             | Command acknowledged             |
| Host → Pico | `TALK <text>`    | Trigger talking animation        |
| Host → Pico | `LOVE`           | Trigger shy/love animation       |
| Host → Pico | `HAPPY <text>`   | Trigger happy + scrolling text   |
| Host → Pico | `IDLE`           | Return to idle face              |

## REST API

Base URL: `http://localhost:8000`

Interactive docs: `http://localhost:8000/docs`

---

### `GET /health`

Check service and Pico connection status.

**Response**
```json
{ "status": "ok", "pico_connected": true }
```

---

### `POST /talk`

Trigger the talking animation with a custom speech bubble.

**Request**
```json
{ "text": "Hello! How can I help you?" }
```

**Response**
```json
{ "sent": "TALK Hello! How can I help you?" }
```

---

### `POST /love`

Trigger the shy/love animation (blinking hearts + blush marks).

**Response**
```json
{ "sent": "LOVE" }
```

---

### `POST /happy`

Trigger the happy animation with scrolling text in the speech bubble.

**Request**
```json
{ "text": "I am your AI assistant. Nice to meet you!" }
```

**Response**
```json
{ "sent": "HAPPY I am your AI assistant. Nice to meet you!" }
```

---

### `POST /idle`

Return the face to the idle state.

**Response**
```json
{ "sent": "IDLE" }
```
