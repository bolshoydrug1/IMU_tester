#include "protocolimu.h"

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
    if (payload.size() != kPayloadSize || out == nullptr) return false;
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());

    out->timestampMs = readU64LE(p + 0);
    out->gyroXDps = readFloatLE(p + 8);
    out->gyroYDps = readFloatLE(p + 12);
    out->gyroZDps = readFloatLE(p + 16);
    out->accelXG = readFloatLE(p + 20);
    out->accelYG = readFloatLE(p + 24);
    out->accelZG = readFloatLE(p + 28);
    out->rollDeg = readFloatLE(p + 32);
    out->pitchDeg = readFloatLE(p + 36);
    out->yawDeg = readFloatLE(p + 40);
    return true;
}

bool AckNack::fromPayload(const QByteArray &payload, AckNack *out)
{
    if (payload.size() != kPayloadSize || out == nullptr) return false;
    out->ackType = static_cast<quint8>(payload.at(0));
    out->status = static_cast<quint8>(payload.at(1));
    return true;
}

bool TimeSyncResponse::fromPayload(const QByteArray &payload, TimeSyncResponse *out)
{
    if (payload.size() != kPayloadSize || out == nullptr) return false;
    out->deviceTickMs = readU64LE(reinterpret_cast<const uchar *>(payload.constData()));
    return true;
}

bool CalibResponse::fromPayload(const QByteArray &payload, CalibResponse *out)
{
    if (payload.size() != kPayloadSize || out == nullptr) return false;
    const uchar *p = reinterpret_cast<const uchar *>(payload.constData());
    out->status = p[0];
    out->biasX = readFloatLE(p + 1);
    out->biasY = readFloatLE(p + 5);
    out->biasZ = readFloatLE(p + 9);
    return true;
}

quint16 computeCrc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint16>(static_cast<quint8>(data.at(i))) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<quint16>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<quint16>(crc << 1);
            }
        }
    }
    return crc;
}

QByteArray buildFrame(quint8 type, const QByteArray &payload)
{
    const quint16 len = static_cast<quint16>(payload.size());

    QByteArray frame;
    frame.reserve(kOverheadBytes + len);
    frame.append(char(kHead));
    frame.append(char(type));
    frame.append(char(kVersion));
    frame.append(char(len & 0xFF));
    frame.append(char((len >> 8) & 0xFF));
    frame.append(payload);

    const quint16 crc = computeCrc16(frame.mid(1)); // type+version+length+payload
    frame.append(char(crc & 0xFF));
    frame.append(char((crc >> 8) & 0xFF));
    frame.append(char(kTail));

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
    QByteArray payload;
    payload.append(char(mode));
    return buildFrame(TypeModeConfig, payload);
}

QByteArray buildResetRequest()
{
    QByteArray payload;
    appendU32LE(payload, kResetMagic);
    return buildFrame(TypeReset, payload);
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

} // namespace ImuProtocol
