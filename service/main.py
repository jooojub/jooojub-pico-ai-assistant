"""
Entry point — connects to the Pico over USB serial, then starts the
FastAPI HTTP server so the web framework can send commands.

Usage:
  python main.py [--port /dev/ttyACM0] [--host 0.0.0.0] [--api-port 8000]
"""

import argparse
import os
import uvicorn
import api
from serial_bridge import SerialBridge


def on_pico_message(line: str) -> None:
    print(f"[pico] {line}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Pico AI Assistant service")
    parser.add_argument("--port",     default=os.getenv("PICO_SERIAL_PORT"),  help="Serial port")
    parser.add_argument("--host",     default=os.getenv("API_HOST", "0.0.0.0"))
    parser.add_argument("--api-port", default=int(os.getenv("API_PORT", "8000")), type=int)
    args = parser.parse_args()

    bridge = SerialBridge(port=args.port)
    bridge.set_message_handler(on_pico_message)

    if not bridge.connect():
        print("Warning: starting without Pico connection. Connect and restart.")

    # Share the bridge instance with the API module
    api.bridge = bridge

    try:
        uvicorn.run(api.app, host=args.host, port=args.api_port)
    finally:
        bridge.disconnect()


if __name__ == "__main__":
    main()
