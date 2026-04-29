#ifndef __SSL_ESP32_PROTOCOL_H
#define __SSL_ESP32_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ssl_control_command.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SSL_ESP32_FRAME_HEADER0 0x53U
#define SSL_ESP32_FRAME_HEADER1 0x4CU
#define SSL_ESP32_FRAME_VERSION 0x01U
#define SSL_ESP32_MAX_PAYLOAD 32U
#define SSL_ESP32_MAX_FRAME_SIZE 38U

typedef enum
{
  SSL_ESP32_MSG_VEL = 0x01U,
  SSL_ESP32_MSG_RAW = 0x02U,
  SSL_ESP32_MSG_STOP = 0x03U,
  SSL_ESP32_MSG_STATUS_REQ = 0x04U,
  SSL_ESP32_MSG_PING = 0x05U,
  SSL_ESP32_MSG_ACK = 0x81U,
  SSL_ESP32_MSG_STATUS = 0x82U,
  SSL_ESP32_MSG_ERROR = 0x83U
} SslEsp32MessageType;

typedef struct
{
  uint8_t type;
  uint8_t payload[SSL_ESP32_MAX_PAYLOAD];
  uint8_t length;
} SslEsp32Frame;

typedef struct
{
  SslVelocityCommand velocity;
  int16_t wheel_rpm[4];
} SslEsp32StatusPayload;

uint8_t SSL_Esp32Protocol_Crc8(const uint8_t *data, size_t length);
bool SSL_Esp32Protocol_EncodeFrame(
    uint8_t type,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *out_frame,
    size_t *out_length);
bool SSL_Esp32Protocol_TryDecodeFrame(
    const uint8_t *buffer,
    size_t length,
    SslEsp32Frame *frame,
    size_t *consumed_length);
bool SSL_Esp32Protocol_EncodeVelocityCommand(
    const SslVelocityCommand *velocity,
    uint8_t *out_frame,
    size_t *out_length);
bool SSL_Esp32Protocol_EncodeRawCommand(
    const int16_t *wheel_rpm,
    uint8_t *out_frame,
    size_t *out_length);
bool SSL_Esp32Protocol_EncodePing(uint8_t *out_frame, size_t *out_length);
bool SSL_Esp32Protocol_EncodeStopCommand(uint8_t *out_frame, size_t *out_length);
bool SSL_Esp32Protocol_EncodeStatusRequest(uint8_t *out_frame, size_t *out_length);
bool SSL_Esp32Protocol_FrameToCommand(const SslEsp32Frame *frame, SslControlCommand *command);
bool SSL_Esp32Protocol_EncodeAck(uint8_t *out_frame, size_t *out_length);
bool SSL_Esp32Protocol_EncodeError(uint8_t error_code, uint8_t *out_frame, size_t *out_length);
bool SSL_Esp32Protocol_EncodeStatus(
    const SslEsp32StatusPayload *status,
    uint8_t *out_frame,
    size_t *out_length);

#ifdef __cplusplus
}
#endif

#endif /* __SSL_ESP32_PROTOCOL_H */
