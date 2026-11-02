#include "protocolimu.h"

#include <cstddef>
#include <cstring>

namespace ImuProtocol {

namespace {

quint32 readU32LE(const uchar *p)
{
    return (quint32)p[0] | ((quint32)p[1] << 8) | ((quint32)p[2] << 16) | ((quint32)p[3] << 24);
}

quint64 readU64LE(const uchar *p)
{
    return (quint64)readU32LE(p) | ((quint64)readU32LE(p + 4) << 32);
}

float readFloatLE(const uchar *p)
{
    quint32 bits = readU32LE(p);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

void appendU32LE(QByteArray &buf, quint32 v)
{
    for (int i = 0; i < 4; ++i) buf.append(char((v >> (8 * i)) & 0xFF));
}

void appendU64LE(QByteArray &buf, quint64 v)
{
    for (int i = 0; i < 8; ++i) buf.append(char((v >> (8 * i)) & 0xFF));
}

} // namespace

bool TelemetryData::fromPayload(const QByteArray &payload, TelemetryData *out)
{
    if (out == nullptr || payload.size() != kPayloadSize) return false;
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());

    out->timestampMs = readU64LE(p + offsetof(Protocol_TelemetryData_t, timestamp_ms));
    out->gyroXDps = readFloatLE(p + offsetof(Protocol_TelemetryData_t, gyro_x_dps));
    out->gyroYDps = readFloatLE(p + offsetof(Protocol_TelemetryData_t, gyro_y_dps));
    out->gyroZDps = readFloatLE(p + offsetof(Protocol_TelemetryData_t, gyro_z_dps));
    out->accelXG = readFloatLE(p + offsetof(Protocol_TelemetryData_t, accel_x_g));
    out->accelYG = readFloatLE(p + offsetof(Protocol_TelemetryData_t, accel_y_g));
    out->accelZG = readFloatLE(p + offsetof(Protocol_TelemetryData_t, accel_z_g));
    out->rollDeg = readFloatLE(p + offsetof(Protocol_TelemetryData_t, roll_deg));
    out->pitchDeg = readFloatLE(p + offsetof(Protocol_TelemetryData_t, pitch_deg));
    out->yawDeg = readFloatLE(p + offsetof(Protocol_TelemetryData_t, yaw_deg));
    out->temperatureC = readFloatLE(p + offsetof(Protocol_TelemetryData_t, temperature_c));
    out->status = p[offsetof(Protocol_TelemetryData_t, status)];
    return true;
}

bool AckNack::fromPayload(const QByteArray &payload, AckNack *out)
{
    if (out == nullptr || payload.size() != kPayloadSize) return false;
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());
    out->ackType = p[offsetof(Protocol_AckNack_t, ack_type)];
    out->status = p[offsetof(Protocol_AckNack_t, status)];
    return true;
}

bool TimeSyncResponse::fromPayload(const QByteArray &payload, TimeSyncResponse *out)
{
    if (out == nullptr || payload.size() != kPayloadSize) return false;
    out->deviceTickMs = readU64LE(reinterpret_cast<const uchar *>(payload.constData())
                                  + offsetof(Protocol_TimeSyncResponse_t, device_tick_ms));
    return true;
}

bool CalibResponse::fromPayload(const QByteArray &payload, CalibResponse *out)
{
    if (out == nullptr || payload.size() != kPayloadSize) return false;
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());
    out->status = p[offsetof(Protocol_CalibResponse_t, status)];
    out->biasX = readFloatLE(p + offsetof(Protocol_CalibResponse_t, bias_x));
    out->biasY = readFloatLE(p + offsetof(Protocol_CalibResponse_t, bias_y));
    out->biasZ = readFloatLE(p + offsetof(Protocol_CalibResponse_t, bias_z));
    return true;
}

quint16 computeCrc16(const QByteArray &data)
{
    return Protocol_ComputeCRC16(reinterpret_cast<const uint8_t *>(data.constData()),
                                 static_cast<uint16_t>(data.size()));
}

QByteArray buildFrame(quint8 type, const QByteArray &payload)
{
    QByteArray frame(static_cast<int>(kOverheadBytes) + payload.size(), Qt::Uninitialized);

    uint16_t frameLen = 0;
    const bool ok = Protocol_BuildFrame(reinterpret_cast<uint8_t *>(frame.data()),
                                        static_cast<uint16_t>(frame.size()),
                                        type,
                                        reinterpret_cast<const uint8_t *>(payload.constData()),
                                        static_cast<uint16_t>(payload.size()),
                                        &frameLen);
    if (!ok) return QByteArray();

    frame.resize(frameLen);
    return frame;
}

QByteArray buildTimeSyncRequest(quint64 hostTimeMs)
{
    QByteArray payload;
    appendU64LE(payload, hostTimeMs);
    return buildFrame(TypeTimeSync, payload);
}

QByteArray buildModeConfig(quint8 mode)
{
    Protocol_ModeConfig_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.mode = mode;
    return buildFrame(TypeModeConfig,
                      QByteArray(reinterpret_cast<const char *>(&cfg), sizeof(cfg)));
}

QByteArray buildResetRequest()
{
    Protocol_ResetRequest_t req;
    req.confirm_magic = PROTOCOL_RESET_MAGIC;
    return buildFrame(TypeReset,
                      QByteArray(reinterpret_cast<const char *>(&req), sizeof(req)));
}

QString typeName(quint8 type)
{
    switch (type) {
    case TypeTelemetry:  return QStringLiteral("Телеметрия");
    case TypeAckNack:    return QStringLiteral("Подтверждение");
    case TypeTimeSync:   return QStringLiteral("Синхронизация времени");
    case TypeCalibAccel: return QStringLiteral("Калибровка акселерометра");
    case TypeCalibGyro:  return QStringLiteral("Калибровка гироскопа");
    case TypeAsciiMsg:   return QStringLiteral("Текстовое сообщение");
    case TypeModeConfig: return QStringLiteral("Настройка режима");
    case TypeReset:      return QStringLiteral("Перезагрузка");
    default:             return QStringLiteral("Неизвестный тип (0x%1)").arg(type, 2, 16, QChar('0'));
    }
}

QString telemetryStatusName(quint8 status)
{
    switch (status) {
    case TelemetryStatusOk:    return QStringLiteral("ОК");
    case TelemetryStatusError: return QStringLiteral("ОШИБКА");
    default:                   return QStringLiteral("неизвестно (0x%1)").arg(status, 2, 16, QChar('0'));
    }
}

} // namespace ImuProtocol
