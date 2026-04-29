import queue
import socket
import sys
import threading

from PySide6.QtCore import QEvent, QObject, Qt, QTimer
from PySide6.QtGui import QKeySequence, QShortcut
from PySide6.QtWidgets import (
    QApplication,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

import protocol


class CommitLineEdit(QLineEdit):
    def __init__(self, owner, text="", width=None):
        super().__init__(text)
        self.owner = owner
        self.setReadOnly(True)
        if width is not None:
            self.setFixedWidth(width)

    def mousePressEvent(self, event):
        if self.isReadOnly():
            self.setReadOnly(False)
            self.selectAll()
        super().mousePressEvent(event)

    def focusInEvent(self, event):
        if self.isReadOnly():
            self.setReadOnly(False)
            self.selectAll()
        super().focusInEvent(event)


class SslHostWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SSL Host WiFi Console V0.0.1")

        self.sock = None
        self.reader_thread = None
        self.running = False
        self.rx_queue = queue.Queue()
        self.rx_buffer = bytearray()

        self._build_ui()
        self._bind_shortcuts()

        self.rx_timer = QTimer(self)
        self.rx_timer.timeout.connect(self._poll_rx)
        self.rx_timer.start(100)

    def _build_ui(self):
        central = QWidget(self)
        self.setCentralWidget(central)

        root_layout = QVBoxLayout(central)
        grid = QGridLayout()
        root_layout.addLayout(grid)

        grid.addWidget(QLabel("ESP32 IP"), 0, 0)
        self.ip_edit = CommitLineEdit(self, "192.168.1.100", 140)
        grid.addWidget(self.ip_edit, 0, 1)
        grid.addWidget(QLabel("Port"), 0, 2)
        self.port_edit = CommitLineEdit(self, "9000", 60)
        grid.addWidget(self.port_edit, 0, 3)

        self.connect_button = QPushButton("Connect")
        self.connect_button.clicked.connect(self.connect_to_bridge)
        grid.addWidget(self.connect_button, 0, 4)

        self.disconnect_button = QPushButton("Disconnect")
        self.disconnect_button.clicked.connect(self.disconnect_from_bridge)
        grid.addWidget(self.disconnect_button, 0, 5)

        grid.addWidget(QLabel("Link"), 1, 0)
        self.link_value = QLabel("Disconnected")
        grid.addWidget(self.link_value, 1, 1, 1, 5)

        grid.addWidget(QLabel("vx (m/s)"), 2, 0)
        self.vx_edit = CommitLineEdit(self, "0.000")
        grid.addWidget(self.vx_edit, 2, 1)
        grid.addWidget(QLabel("vy (m/s)"), 2, 2)
        self.vy_edit = CommitLineEdit(self, "0.000")
        grid.addWidget(self.vy_edit, 2, 3)
        grid.addWidget(QLabel("wz (rad/s)"), 2, 4)
        self.wz_edit = CommitLineEdit(self, "0.000")
        grid.addWidget(self.wz_edit, 2, 5)

        grid.addWidget(QLabel("Linear step"), 3, 0)
        self.linear_step_edit = CommitLineEdit(self, "0.100")
        grid.addWidget(self.linear_step_edit, 3, 1)
        grid.addWidget(QLabel("Angular step"), 3, 2)
        self.angular_step_edit = CommitLineEdit(self, "0.200")
        grid.addWidget(self.angular_step_edit, 3, 3)

        button_row = QHBoxLayout()
        root_layout.addLayout(button_row)

        self.send_vel_button = QPushButton("Send VEL")
        self.send_vel_button.clicked.connect(self.send_velocity)
        button_row.addWidget(self.send_vel_button)

        self.stop_button = QPushButton("STOP")
        self.stop_button.clicked.connect(self.send_stop)
        button_row.addWidget(self.stop_button)

        self.status_button = QPushButton("STATUS")
        self.status_button.clicked.connect(self.send_status_request)
        button_row.addWidget(self.status_button)

        self.ping_button = QPushButton("PING")
        self.ping_button.clicked.connect(self.send_ping)
        button_row.addWidget(self.ping_button)

        hint = QLabel("Keyboard: W/S forward, A/D lateral, Left/Right yaw, Space stop")
        root_layout.addWidget(hint)

        raw_grid = QGridLayout()
        root_layout.addLayout(raw_grid)

        raw_grid.addWidget(QLabel("RAW FL"), 0, 0)
        self.raw_fl_edit = CommitLineEdit(self, "0")
        raw_grid.addWidget(self.raw_fl_edit, 0, 1)
        raw_grid.addWidget(QLabel("RAW FR"), 0, 2)
        self.raw_fr_edit = CommitLineEdit(self, "0")
        raw_grid.addWidget(self.raw_fr_edit, 0, 3)
        raw_grid.addWidget(QLabel("RAW RL"), 0, 4)
        self.raw_rl_edit = CommitLineEdit(self, "0")
        raw_grid.addWidget(self.raw_rl_edit, 0, 5)
        raw_grid.addWidget(QLabel("RAW RR"), 1, 0)
        self.raw_rr_edit = CommitLineEdit(self, "0")
        raw_grid.addWidget(self.raw_rr_edit, 1, 1)

        self.send_raw_button = QPushButton("Send RAW")
        self.send_raw_button.clicked.connect(self.send_raw)
        raw_grid.addWidget(self.send_raw_button, 1, 2, 1, 4)

        feedback_row = QHBoxLayout()
        root_layout.addLayout(feedback_row)
        feedback_row.addWidget(QLabel("Feedback"))
        self.feedback_value = QLabel("No status")
        self.feedback_value.setTextInteractionFlags(Qt.TextSelectableByMouse)
        feedback_row.addWidget(self.feedback_value, 1)

        self.commit_edits = [
            self.ip_edit,
            self.port_edit,
            self.vx_edit,
            self.vy_edit,
            self.wz_edit,
            self.linear_step_edit,
            self.angular_step_edit,
            self.raw_fl_edit,
            self.raw_fr_edit,
            self.raw_rl_edit,
            self.raw_rr_edit,
        ]

        for edit in self.commit_edits:
            edit.installEventFilter(self)

        self.ip_edit.returnPressed.connect(self._commit_ip)
        self.port_edit.returnPressed.connect(self._commit_port)
        self.vx_edit.returnPressed.connect(self._commit_velocity)
        self.vy_edit.returnPressed.connect(self._commit_velocity)
        self.wz_edit.returnPressed.connect(self._commit_velocity)
        self.linear_step_edit.returnPressed.connect(self._commit_steps)
        self.angular_step_edit.returnPressed.connect(self._commit_steps)
        self.raw_fl_edit.returnPressed.connect(self._commit_raw)
        self.raw_fr_edit.returnPressed.connect(self._commit_raw)
        self.raw_rl_edit.returnPressed.connect(self._commit_raw)
        self.raw_rr_edit.returnPressed.connect(self._commit_raw)

    def _bind_shortcuts(self):
        shortcuts = [
            ("W", self._handle_forward),
            ("S", self._handle_backward),
            ("A", self._handle_left),
            ("D", self._handle_right),
            ("Left", self._handle_yaw_left),
            ("Right", self._handle_yaw_right),
            ("Space", self._handle_stop),
        ]

        for key, handler in shortcuts:
            shortcut = QShortcut(QKeySequence(key), self)
            shortcut.setContext(Qt.WindowShortcut)
            shortcut.activated.connect(handler)

    def eventFilter(self, watched: QObject, event: QEvent):
        if watched in self.commit_edits and event.type() == QEvent.FocusOut:
            watched.setReadOnly(True)
        return super().eventFilter(watched, event)

    def _commit_finish(self):
        for edit in self.commit_edits:
            edit.setReadOnly(True)
        self.send_vel_button.setFocus()

    def _commit_ip(self):
        self.ip_edit.setText(self.ip_edit.text().strip())
        self.feedback_value.setText(f"IP saved: {self.ip_edit.text()}")
        self._commit_finish()

    def _commit_port(self):
        try:
            port_value = int(self.port_edit.text())
        except ValueError as exc:
            self.feedback_value.setText(f"Port input error: {exc}")
            return

        if not (0 < port_value < 65536):
            self.feedback_value.setText("Port must be 1..65535")
            return

        self.port_edit.setText(str(port_value))
        self.feedback_value.setText(f"Port saved: {port_value}")
        self._commit_finish()

    def _commit_velocity(self):
        try:
            vx_value = float(self.vx_edit.text())
            vy_value = float(self.vy_edit.text())
            wz_value = float(self.wz_edit.text())
        except ValueError as exc:
            self.feedback_value.setText(f"VEL input error: {exc}")
            return

        self.vx_edit.setText(f"{vx_value:.3f}")
        self.vy_edit.setText(f"{vy_value:.3f}")
        self.wz_edit.setText(f"{wz_value:.3f}")
        self.feedback_value.setText("Velocity values saved")
        self._commit_finish()

    def _commit_steps(self):
        linear_step = self._linear_step()
        angular_step = self._angular_step()
        self.linear_step_edit.setText(f"{linear_step:.3f}")
        self.angular_step_edit.setText(f"{angular_step:.3f}")
        self.feedback_value.setText("Step values saved")
        self._commit_finish()

    def _commit_raw(self):
        try:
            raw_values = [
                int(self.raw_fl_edit.text()),
                int(self.raw_fr_edit.text()),
                int(self.raw_rl_edit.text()),
                int(self.raw_rr_edit.text()),
            ]
        except ValueError as exc:
            self.feedback_value.setText(f"RAW input error: {exc}")
            return

        self.raw_fl_edit.setText(str(raw_values[0]))
        self.raw_fr_edit.setText(str(raw_values[1]))
        self.raw_rl_edit.setText(str(raw_values[2]))
        self.raw_rr_edit.setText(str(raw_values[3]))
        self.feedback_value.setText("RAW values saved")
        self._commit_finish()

    def _read_float(self, edit: QLineEdit, label: str) -> float:
        try:
            return float(edit.text())
        except ValueError as exc:
            self.feedback_value.setText(f"{label} input error: {exc}")
            raise

    def _linear_step(self) -> float:
        try:
            step_value = self._read_float(self.linear_step_edit, "Linear step")
        except ValueError:
            return 0.0
        if step_value < 0.0:
            self.feedback_value.setText("Linear step must be >= 0")
            return 0.0
        return step_value

    def _angular_step(self) -> float:
        try:
            step_value = self._read_float(self.angular_step_edit, "Angular step")
        except ValueError:
            return 0.0
        if step_value < 0.0:
            self.feedback_value.setText("Angular step must be >= 0")
            return 0.0
        return step_value

    def connect_to_bridge(self):
        self.disconnect_from_bridge()
        try:
            self.sock = socket.create_connection((self.ip_edit.text(), int(self.port_edit.text())), timeout=3.0)
            self.sock.settimeout(0.2)
            self.running = True
            self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
            self.reader_thread.start()
            self.link_value.setText("Connected")
        except OSError as exc:
            self.link_value.setText(f"Connect failed: {exc}")

    def disconnect_from_bridge(self):
        self.running = False
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None
        self.link_value.setText("Disconnected")

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
                self.disconnect_from_bridge()
                break

            self.rx_buffer.extend(item)
            while True:
                frame = protocol.try_decode_frame(self.rx_buffer)
                if frame is None:
                    break
                self._handle_frame(frame)

    def _handle_frame(self, frame):
        if frame["type"] == protocol.MSG_ACK:
            self.feedback_value.setText("ACK")
        elif frame["type"] == protocol.MSG_ERROR:
            self.feedback_value.setText(f"ERR code={frame['payload'][0] if frame['payload'] else -1}")
        elif frame["type"] == protocol.MSG_STATUS and len(frame["payload"]) == 20:
            status = protocol.decode_status(frame["payload"])
            self.feedback_value.setText(
                f"vx={status['vx']:.3f} m/s  vy={status['vy']:.3f} m/s  "
                f"wz={status['wz']:.3f} rad/s  rpm={status['rpm']}"
            )
        else:
            self.feedback_value.setText(f"RX type=0x{frame['type']:02X} len={len(frame['payload'])}")

    def _send(self, payload: bytes):
        if self.sock is None:
            self.link_value.setText("Not connected")
            return
        try:
            self.sock.sendall(payload)
        except OSError as exc:
            self.link_value.setText(f"Send failed: {exc}")
            self.disconnect_from_bridge()

    def adjust_velocity(self, vx_delta: float = 0.0, vy_delta: float = 0.0, wz_delta: float = 0.0):
        try:
            vx_value = float(self.vx_edit.text()) + vx_delta
            vy_value = float(self.vy_edit.text()) + vy_delta
            wz_value = float(self.wz_edit.text()) + wz_delta
        except ValueError as exc:
            self.feedback_value.setText(f"VEL input error: {exc}")
            return

        self.vx_edit.setText(f"{vx_value:.3f}")
        self.vy_edit.setText(f"{vy_value:.3f}")
        self.wz_edit.setText(f"{wz_value:.3f}")
        self.send_velocity()

    def _handle_forward(self):
        self.adjust_velocity(vx_delta=self._linear_step())

    def _handle_backward(self):
        self.adjust_velocity(vx_delta=-self._linear_step())

    def _handle_left(self):
        self.adjust_velocity(vy_delta=self._linear_step())

    def _handle_right(self):
        self.adjust_velocity(vy_delta=-self._linear_step())

    def _handle_yaw_left(self):
        self.adjust_velocity(wz_delta=self._angular_step())

    def _handle_yaw_right(self):
        self.adjust_velocity(wz_delta=-self._angular_step())

    def _handle_stop(self):
        self.vx_edit.setText("0.000")
        self.vy_edit.setText("0.000")
        self.wz_edit.setText("0.000")
        self.send_stop()

    def send_velocity(self):
        try:
            payload = protocol.encode_velocity(
                float(self.vx_edit.text()),
                float(self.vy_edit.text()),
                float(self.wz_edit.text()),
            )
        except ValueError as exc:
            self.feedback_value.setText(f"VEL input error: {exc}")
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
                int(self.raw_fl_edit.text()),
                int(self.raw_fr_edit.text()),
                int(self.raw_rl_edit.text()),
                int(self.raw_rr_edit.text()),
            )
        except ValueError as exc:
            self.feedback_value.setText(f"RAW input error: {exc}")
            return

        self._send(payload)

    def closeEvent(self, event):
        self.disconnect_from_bridge()
        super().closeEvent(event)


def main():
    app = QApplication(sys.argv)
    window = SslHostWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
