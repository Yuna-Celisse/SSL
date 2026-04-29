import struct

FRAME_HEADER0 = 0x53
FRAME_HEADER1 = 0x4C
FRAME_VERSION = 0x01

MSG_VEL = 0x01
MSG_RAW = 0x02
MSG_STOP = 0x03
MSG_STATUS_REQ = 0x04
MSG_PING = 0x05
MSG_ACK = 0x81
MSG_STATUS = 0x82
MSG_ERROR = 0x83


def crc8(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def encode_frame(msg_type: int, payload: bytes = b"") -> bytes:
    head = bytes([FRAME_HEADER0, FRAME_HEADER1, FRAME_VERSION, msg_type, len(payload)])
    checksum = crc8(head[2:] + payload)
    return head + payload + bytes([checksum])


def encode_velocity(vx: float, vy: float, wz: float) -> bytes:
    return encode_frame(MSG_VEL, struct.pack("<fff", vx, vy, wz))


def encode_raw(fl: int, fr: int, rl: int, rr: int) -> bytes:
    return encode_frame(MSG_RAW, struct.pack("<hhhh", fl, fr, rl, rr))


def encode_stop() -> bytes:
    return encode_frame(MSG_STOP)


def encode_status_request() -> bytes:
    return encode_frame(MSG_STATUS_REQ)


def encode_ping() -> bytes:
    return encode_frame(MSG_PING)


def try_decode_frame(buffer: bytearray):
    while len(buffer) >= 2:
        if buffer[0] == FRAME_HEADER0 and buffer[1] == FRAME_HEADER1:
            break
        buffer.pop(0)

    if len(buffer) < 6:
        return None

    if buffer[2] != FRAME_VERSION:
        buffer.pop(0)
        return None

    payload_length = buffer[4]
    frame_length = 6 + payload_length
    if len(buffer) < frame_length:
        return None

    frame = bytes(buffer[:frame_length])
    if frame[-1] != crc8(frame[2:-1]):
        del buffer[:frame_length]
        return None

    del buffer[:frame_length]
    return {
        "type": frame[3],
        "payload": frame[5:-1],
    }


def decode_status(payload: bytes):
    if len(payload) != 20:
        raise ValueError(f"invalid status payload length: {len(payload)}")
    values = struct.unpack("<fffhhhh", payload)
    return {
        "vx": values[0],
        "vy": values[1],
        "wz": values[2],
        "rpm": values[3:7],
    }
