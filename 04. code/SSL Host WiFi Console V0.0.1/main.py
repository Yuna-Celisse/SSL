import queue
import socket
import threading
import tkinter as tk
from tkinter import ttk

import protocol


class SslHostApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("SSL Host WiFi Console V0.0.1")

        self.sock = None
        self.reader_thread = None
        self.running = False
        self.rx_queue = queue.Queue()
        self.rx_buffer = bytearray()

        self.ip_var = tk.StringVar(value="192.168.1.100")
        self.port_var = tk.StringVar(value="9000")
        self.status_var = tk.StringVar(value="Disconnected")
        self.vx_var = tk.StringVar(value="0.0")
        self.vy_var = tk.StringVar(value="0.0")
        self.wz_var = tk.StringVar(value="0.0")
        self.raw_fl_var = tk.StringVar(value="0")
        self.raw_fr_var = tk.StringVar(value="0")
        self.raw_rl_var = tk.StringVar(value="0")
        self.raw_rr_var = tk.StringVar(value="0")
        self.feedback_var = tk.StringVar(value="No status")

        self._build_ui()
        self.root.after(100, self._poll_rx)

    def _build_ui(self):
        frame = ttk.Frame(self.root, padding=12)
        frame.grid(sticky="nsew")

        ttk.Label(frame, text="ESP32 IP").grid(row=0, column=0, sticky="w")
        ttk.Entry(frame, textvariable=self.ip_var, width=18).grid(row=0, column=1, sticky="ew")
        ttk.Label(frame, text="Port").grid(row=0, column=2, sticky="w")
        ttk.Entry(frame, textvariable=self.port_var, width=8).grid(row=0, column=3, sticky="ew")
        ttk.Button(frame, text="Connect", command=self.connect).grid(row=0, column=4, padx=4)
        ttk.Button(frame, text="Disconnect", command=self.disconnect).grid(row=0, column=5, padx=4)

        ttk.Label(frame, text="Link").grid(row=1, column=0, sticky="w")
        ttk.Label(frame, textvariable=self.status_var).grid(row=1, column=1, columnspan=5, sticky="w")

        ttk.Label(frame, text="vx (m/s)").grid(row=2, column=0, sticky="w")
        ttk.Entry(frame, textvariable=self.vx_var).grid(row=2, column=1, sticky="ew")
        ttk.Label(frame, text="vy (m/s)").grid(row=2, column=2, sticky="w")
        ttk.Entry(frame, textvariable=self.vy_var).grid(row=2, column=3, sticky="ew")
        ttk.Label(frame, text="wz (rad/s)").grid(row=2, column=4, sticky="w")
        ttk.Entry(frame, textvariable=self.wz_var).grid(row=2, column=5, sticky="ew")

        ttk.Button(frame, text="Send VEL", command=self.send_velocity).grid(row=3, column=0, columnspan=2, sticky="ew", pady=6)
        ttk.Button(frame, text="STOP", command=self.send_stop).grid(row=3, column=2, columnspan=2, sticky="ew", pady=6)
        ttk.Button(frame, text="STATUS", command=self.send_status_request).grid(row=3, column=4, sticky="ew", pady=6)
        ttk.Button(frame, text="PING", command=self.send_ping).grid(row=3, column=5, sticky="ew", pady=6)

        ttk.Label(frame, text="RAW FL").grid(row=4, column=0, sticky="w")
        ttk.Entry(frame, textvariable=self.raw_fl_var).grid(row=4, column=1, sticky="ew")
        ttk.Label(frame, text="RAW FR").grid(row=4, column=2, sticky="w")
        ttk.Entry(frame, textvariable=self.raw_fr_var).grid(row=4, column=3, sticky="ew")
        ttk.Label(frame, text="RAW RL").grid(row=4, column=4, sticky="w")
        ttk.Entry(frame, textvariable=self.raw_rl_var).grid(row=4, column=5, sticky="ew")
        ttk.Label(frame, text="RAW RR").grid(row=5, column=0, sticky="w")
        ttk.Entry(frame, textvariable=self.raw_rr_var).grid(row=5, column=1, sticky="ew")
        ttk.Button(frame, text="Send RAW", command=self.send_raw).grid(row=5, column=2, columnspan=4, sticky="ew")

        ttk.Label(frame, text="Feedback").grid(row=6, column=0, sticky="nw", pady=(10, 0))
        ttk.Label(frame, textvariable=self.feedback_var, wraplength=520, justify="left").grid(
            row=6, column=1, columnspan=5, sticky="w", pady=(10, 0)
        )

        for column in range(6):
            frame.columnconfigure(column, weight=1)

    def connect(self):
        self.disconnect()
        try:
            self.sock = socket.create_connection((self.ip_var.get(), int(self.port_var.get())), timeout=3.0)
            self.sock.settimeout(0.2)
            self.running = True
            self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
            self.reader_thread.start()
            self.status_var.set("Connected")
        except OSError as exc:
            self.status_var.set(f"Connect failed: {exc}")

    def disconnect(self):
        self.running = False
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None
        self.status_var.set("Disconnected")

    def _reader_loop(self):
        while self.running and self.sock is not None:
            try:
                chunk = self.sock.recv(256)
                if not chunk:
                    break
                self.rx_queue.put(chunk)
            except socket.timeout:
                continue
            except OSError:
                break
        self.running = False
        self.rx_queue.put(None)

    def _poll_rx(self):
        while not self.rx_queue.empty():
            item = self.rx_queue.get()
            if item is None:
                self.disconnect()
                break
            self.rx_buffer.extend(item)
            while True:
                frame = protocol.try_decode_frame(self.rx_buffer)
                if frame is None:
                    break
                self._handle_frame(frame)
        self.root.after(100, self._poll_rx)

    def _handle_frame(self, frame):
        if frame["type"] == protocol.MSG_ACK:
            self.feedback_var.set("ACK")
        elif frame["type"] == protocol.MSG_ERROR:
            self.feedback_var.set(f"ERR code={frame['payload'][0] if frame['payload'] else -1}")
        elif frame["type"] == protocol.MSG_STATUS and len(frame["payload"]) == 20:
            status = protocol.decode_status(frame["payload"])
            self.feedback_var.set(
                f"vx={status['vx']:.3f} m/s  vy={status['vy']:.3f} m/s  "
                f"wz={status['wz']:.3f} rad/s  rpm={status['rpm']}"
            )
        else:
            self.feedback_var.set(f"RX type=0x{frame['type']:02X} len={len(frame['payload'])}")

    def _send(self, payload: bytes):
        if self.sock is None:
            self.status_var.set("Not connected")
            return
        try:
            self.sock.sendall(payload)
        except OSError as exc:
            self.status_var.set(f"Send failed: {exc}")
            self.disconnect()

    def send_velocity(self):
        try:
            payload = protocol.encode_velocity(
                float(self.vx_var.get()),
                float(self.vy_var.get()),
                float(self.wz_var.get()),
            )
        except ValueError as exc:
            self.feedback_var.set(f"VEL input error: {exc}")
            return

        self._send(payload)

    def send_stop(self):
        self._send(protocol.encode_stop())

    def send_status_request(self):
        self._send(protocol.encode_status_request())

    def send_ping(self):
        self._send(protocol.encode_ping())

    def send_raw(self):
        try:
            payload = protocol.encode_raw(
                int(self.raw_fl_var.get()),
                int(self.raw_fr_var.get()),
                int(self.raw_rl_var.get()),
                int(self.raw_rr_var.get()),
            )
        except ValueError as exc:
            self.feedback_var.set(f"RAW input error: {exc}")
            return

        self._send(payload)


def main():
    root = tk.Tk()
    app = SslHostApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.disconnect(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    main()
