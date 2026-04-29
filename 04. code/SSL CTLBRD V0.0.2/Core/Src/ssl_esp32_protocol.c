#include "ssl_esp32_protocol.h"

#include <string.h>

static void SSL_Esp32Protocol_WriteFloat(float value, uint8_t *buffer);
static float SSL_Esp32Protocol_ReadFloat(const uint8_t *buffer);
static void SSL_Esp32Protocol_WriteInt16(int16_t value, uint8_t *buffer);
static int16_t SSL_Esp32Protocol_ReadInt16(const uint8_t *buffer);

uint8_t SSL_Esp32Protocol_Crc8(const uint8_t *data, size_t length)
{
  uint8_t crc = 0x00U;
  size_t index = 0U;
  uint8_t bit = 0U;

  for (index = 0U; index < length; ++index)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; ++bit)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1U) ^ 0x07U);
      }
      else
      {
        crc <<= 1U;
      }
    }
  }

  return crc;
}

bool SSL_Esp32Protocol_EncodeFrame(
    uint8_t type,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *out_frame,
    size_t *out_length)
{
  uint8_t crc = 0U;

  if ((payload_length > SSL_ESP32_MAX_PAYLOAD) || (out_frame == NULL) || (out_length == NULL))
  {
    return false;
  }

  out_frame[0] = SSL_ESP32_FRAME_HEADER0;
  out_frame[1] = SSL_ESP32_FRAME_HEADER1;
  out_frame[2] = SSL_ESP32_FRAME_VERSION;
  out_frame[3] = type;
  out_frame[4] = payload_length;

  if ((payload != NULL) && (payload_length > 0U))
  {
    memcpy(&out_frame[5], payload, payload_length);
  }

  crc = SSL_Esp32Protocol_Crc8(&out_frame[2], (size_t)3U + payload_length);
  out_frame[5U + payload_length] = crc;
  *out_length = (size_t)6U + payload_length;
  return true;
}

bool SSL_Esp32Protocol_TryDecodeFrame(
    const uint8_t *buffer,
    size_t length,
    SslEsp32Frame *frame,
    size_t *consumed_length)
{
  size_t offset = 0U;
  uint8_t payload_length = 0U;
  uint8_t expected_crc = 0U;
  uint8_t actual_crc = 0U;

  if ((buffer == NULL) || (frame == NULL) || (consumed_length == NULL))
  {
    return false;
  }

  while ((offset + 1U) < length)
  {
    if ((buffer[offset] == SSL_ESP32_FRAME_HEADER0) &&
        (buffer[offset + 1U] == SSL_ESP32_FRAME_HEADER1))
    {
      break;
    }
    ++offset;
  }

  if ((offset + 6U) > length)
  {
    *consumed_length = offset;
    return false;
  }

  if (buffer[offset + 2U] != SSL_ESP32_FRAME_VERSION)
  {
    *consumed_length = offset + 2U;
    return false;
  }

  payload_length = buffer[offset + 4U];
  if (payload_length > SSL_ESP32_MAX_PAYLOAD)
  {
    *consumed_length = offset + 5U;
    return false;
  }

  if ((offset + (size_t)6U + payload_length) > length)
  {
    *consumed_length = offset;
    return false;
  }

  expected_crc = buffer[offset + 5U + payload_length];
  actual_crc = SSL_Esp32Protocol_Crc8(&buffer[offset + 2U], (size_t)3U + payload_length);
  if (expected_crc != actual_crc)
  {
    *consumed_length = offset + 6U + payload_length;
    return false;
  }

  frame->type = buffer[offset + 3U];
  frame->length = payload_length;
  if (payload_length > 0U)
  {
    memcpy(frame->payload, &buffer[offset + 5U], payload_length);
  }
  *consumed_length = offset + 6U + payload_length;
  return true;
}

bool SSL_Esp32Protocol_EncodeVelocityCommand(
    const SslVelocityCommand *velocity,
    uint8_t *out_frame,
    size_t *out_length)
{
  uint8_t payload[12];

  if (velocity == NULL)
  {
    return false;
  }

  SSL_Esp32Protocol_WriteFloat(velocity->vx_mps, &payload[0]);
  SSL_Esp32Protocol_WriteFloat(velocity->vy_mps, &payload[4]);
  SSL_Esp32Protocol_WriteFloat(velocity->wz_radps, &payload[8]);
  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_VEL, payload, 12U, out_frame, out_length);
}

bool SSL_Esp32Protocol_EncodeRawCommand(
    const int16_t *wheel_rpm,
    uint8_t *out_frame,
    size_t *out_length)
{
  uint8_t payload[8];
  uint32_t index = 0U;

  if (wheel_rpm == NULL)
  {
    return false;
  }

  for (index = 0U; index < 4U; ++index)
  {
    SSL_Esp32Protocol_WriteInt16(wheel_rpm[index], &payload[index * 2U]);
  }

  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_RAW, payload, 8U, out_frame, out_length);
}

bool SSL_Esp32Protocol_EncodePing(uint8_t *out_frame, size_t *out_length)
{
  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_PING, NULL, 0U, out_frame, out_length);
}

bool SSL_Esp32Protocol_EncodeStopCommand(uint8_t *out_frame, size_t *out_length)
{
  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_STOP, NULL, 0U, out_frame, out_length);
}

bool SSL_Esp32Protocol_EncodeStatusRequest(uint8_t *out_frame, size_t *out_length)
{
  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_STATUS_REQ, NULL, 0U, out_frame, out_length);
}

bool SSL_Esp32Protocol_FrameToCommand(const SslEsp32Frame *frame, SslControlCommand *command)
{
  uint32_t index = 0U;

  if ((frame == NULL) || (command == NULL))
  {
    return false;
  }

  memset(command, 0, sizeof(*command));
  command->has_command = true;

  if ((frame->type == SSL_ESP32_MSG_VEL) && (frame->length == 12U))
  {
    command->velocity.vx_mps = SSL_Esp32Protocol_ReadFloat(&frame->payload[0]);
    command->velocity.vy_mps = SSL_Esp32Protocol_ReadFloat(&frame->payload[4]);
    command->velocity.wz_radps = SSL_Esp32Protocol_ReadFloat(&frame->payload[8]);
    return true;
  }

  if ((frame->type == SSL_ESP32_MSG_RAW) && (frame->length == 8U))
  {
    command->raw_mode = true;
    for (index = 0U; index < 4U; ++index)
    {
      command->raw_rpm[index] = SSL_Esp32Protocol_ReadInt16(&frame->payload[index * 2U]);
    }
    return true;
  }

  if ((frame->type == SSL_ESP32_MSG_STOP) && (frame->length == 0U))
  {
    command->stop_requested = true;
    return true;
  }

  if ((frame->type == SSL_ESP32_MSG_STATUS_REQ) && (frame->length == 0U))
  {
    command->status_requested = true;
    return true;
  }

  if ((frame->type == SSL_ESP32_MSG_PING) && (frame->length == 0U))
  {
    command->ping_requested = true;
    return true;
  }

  return false;
}

bool SSL_Esp32Protocol_EncodeAck(uint8_t *out_frame, size_t *out_length)
{
  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_ACK, NULL, 0U, out_frame, out_length);
}

bool SSL_Esp32Protocol_EncodeError(uint8_t error_code, uint8_t *out_frame, size_t *out_length)
{
  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_ERROR, &error_code, 1U, out_frame, out_length);
}

bool SSL_Esp32Protocol_EncodeStatus(
    const SslEsp32StatusPayload *status,
    uint8_t *out_frame,
    size_t *out_length)
{
  uint8_t payload[20];
  uint32_t index = 0U;

  if (status == NULL)
  {
    return false;
  }

  SSL_Esp32Protocol_WriteFloat(status->velocity.vx_mps, &payload[0]);
  SSL_Esp32Protocol_WriteFloat(status->velocity.vy_mps, &payload[4]);
  SSL_Esp32Protocol_WriteFloat(status->velocity.wz_radps, &payload[8]);
  for (index = 0U; index < 4U; ++index)
  {
    SSL_Esp32Protocol_WriteInt16(status->wheel_rpm[index], &payload[12U + (index * 2U)]);
  }

  return SSL_Esp32Protocol_EncodeFrame(SSL_ESP32_MSG_STATUS, payload, 20U, out_frame, out_length);
}

static void SSL_Esp32Protocol_WriteFloat(float value, uint8_t *buffer)
{
  memcpy(buffer, &value, sizeof(float));
}

static float SSL_Esp32Protocol_ReadFloat(const uint8_t *buffer)
{
  float value = 0.0f;

  memcpy(&value, buffer, sizeof(float));
  return value;
}

static void SSL_Esp32Protocol_WriteInt16(int16_t value, uint8_t *buffer)
{
  buffer[0] = (uint8_t)(value & 0xFF);
  buffer[1] = (uint8_t)((uint16_t)value >> 8U);
}

static int16_t SSL_Esp32Protocol_ReadInt16(const uint8_t *buffer)
{
  return (int16_t)((uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8U));
}
