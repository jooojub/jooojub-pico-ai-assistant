"""
HTTP API skeleton — receives requests from a web framework and forwards
commands to the Pico via SerialBridge.

Run with:  uvicorn api:app --host 0.0.0.0 --port 8000
"""

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import Optional

from serial_bridge import SerialBridge

app = FastAPI(title="Pico AI Assistant Service")

# Shared bridge instance (initialised in main.py)
bridge: Optional[SerialBridge] = None


# ------------------------------------------------------------------
# Request schemas
# ------------------------------------------------------------------

class TalkRequest(BaseModel):
    text: str

class HappyRequest(BaseModel):
    text: str


# ------------------------------------------------------------------
# Routes
# ------------------------------------------------------------------

@app.get("/health")
def health():
    connected = bridge is not None and bridge._serial is not None and bridge._serial.is_open
    return {"status": "ok", "pico_connected": connected}


@app.post("/talk")
def talk(req: TalkRequest):
    """Trigger the talking animation with custom text."""
    if not bridge:
        raise HTTPException(503, "Bridge not initialised")
    ok = bridge.cmd_talk(req.text)
    if not ok:
        raise HTTPException(503, "Failed to send command to Pico")
    return {"sent": f"TALK {req.text}"}


@app.post("/love")
def love():
    """Trigger the shy/love animation."""
    if not bridge:
        raise HTTPException(503, "Bridge not initialised")
    ok = bridge.cmd_love()
    if not ok:
        raise HTTPException(503, "Failed to send command to Pico")
    return {"sent": "LOVE"}


@app.post("/happy")
def happy(req: HappyRequest):
    """Trigger the happy animation with scrolling text."""
    if not bridge:
        raise HTTPException(503, "Bridge not initialised")
    ok = bridge.cmd_happy(req.text)
    if not ok:
        raise HTTPException(503, "Failed to send command to Pico")
    return {"sent": f"HAPPY {req.text}"}


@app.post("/idle")
def idle():
    """Return the face to idle state."""
    if not bridge:
        raise HTTPException(503, "Bridge not initialised")
    ok = bridge.cmd_idle()
    if not ok:
        raise HTTPException(503, "Failed to send command to Pico")
    return {"sent": "IDLE"}
