"""
Serial bridge — manages the USB serial connection to the Pico.

Protocol (text, newline-terminated):
  Host → Pico:  TALK <text>
                LOVE
                HAPPY <text>
                IDLE
  Pico → Host:  READY   (on startup)
                OK      (after each command)
"""

import threading
import time
import serial
import serial.tools.list_ports
from typing import Callable, Optional


def find_pico_port() -> Optional[str]:
    """Return the first serial port that looks like a Pico CDC device."""
    for port in serial.tools.list_ports.comports():
        desc = (port.description or "").lower()
        if "pico" in desc or "2e8a" in port.hwid.lower():
            return port.device
    return None


class SerialBridge:
    def __init__(self, port: Optional[str] = None, baudrate: int = 115200):
        self._port = port
        self._baudrate = baudrate
        self._serial: Optional[serial.Serial] = None
        self._lock = threading.Lock()
        self._running = False
        self._reader_thread: Optional[threading.Thread] = None
        self._on_message: Optional[Callable[[str], None]] = None

    def set_message_handler(self, handler: Callable[[str], None]) -> None:
        """Register a callback invoked for every line received from the Pico."""
        self._on_message = handler

    def connect(self) -> bool:
        port = self._port or find_pico_port()
        if port is None:
            print("[serial] No Pico port found.")
            return False
        try:
            self._serial = serial.Serial(port, self._baudrate, timeout=1)
            self._running = True
            self._reader_thread = threading.Thread(
                target=self._read_loop, daemon=True
            )
            self._reader_thread.start()
            print(f"[serial] Connected on {port}")
            return True
        except serial.SerialException as e:
            print(f"[serial] Failed to connect: {e}")
            return False

    def disconnect(self) -> None:
        self._running = False
        if self._serial and self._serial.is_open:
            self._serial.close()
        print("[serial] Disconnected.")

    def send(self, command: str) -> bool:
        """Send a newline-terminated command to the Pico."""
        if not self._serial or not self._serial.is_open:
            print("[serial] Not connected.")
            return False
        line = command.strip() + "\n"
        with self._lock:
            try:
                self._serial.write(line.encode())
                return True
            except serial.SerialException as e:
                print(f"[serial] Write error: {e}")
                return False

    # ------------------------------------------------------------------
    # Convenience command helpers
    # ------------------------------------------------------------------

    def cmd_talk(self, text: str) -> bool:
        return self.send(f"TALK {text}")

    def cmd_love(self) -> bool:
        return self.send("LOVE")

    def cmd_happy(self, text: str) -> bool:
        return self.send(f"HAPPY {text}")

    def cmd_idle(self) -> bool:
        return self.send("IDLE")

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _read_loop(self) -> None:
        while self._running:
            try:
                if self._serial and self._serial.in_waiting:
                    line = self._serial.readline().decode(errors="replace").strip()
                    if line and self._on_message:
                        self._on_message(line)
                else:
                    time.sleep(0.01)
            except serial.SerialException:
                print("[serial] Connection lost.")
                self._running = False
