#ifndef PROTOCOLIMU_H
#define PROTOCOLIMU_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/* Реализация протокола обмена с ИИС (см. Protocol_IMU.h/.c прошивки).
 * Кадр: head(1)=0xAA, type(1), version(1)=0x01, length(2, LE) payload,
 * crc16(2, LE, CCITT-FALSE по type+version+length+payload), tail(1)=0x55. */
namespace ImuProtocol {

constexpr quint8 kVersion = 0x01;
constexpr quint8 kHead = 0xAA;
constexpr quint8 kTail = 0x55;
constexpr int kOverheadBytes = 8;
constexpr quint32 kResetMagic = 0xDEADBEEFu;

enum FrameType : quint8 {
    TypeTelemetry  = 0x01,
    TypeAckNack    = 0x02,
    TypeTimeSync   = 0x03,
    TypeCalibAccel = 0x04,
    TypeCalibGyro  = 0x05,
    TypeAsciiMsg   = 0x06,
    TypeModeConfig = 0x07,
    TypeReset      = 0x08,
};

enum Status : quint8 {
    StatusOk    = 0x00,
    StatusError = 0x01,
};

enum Mode : quint8 {
    Mode100Hz     = 0x00,
    Mode250Hz     = 0x01,
    Mode500Hz     = 0x02,
    Mode1000Hz    = 0x03,
    ModeOnRequest = 0x04,
};

struct TelemetryData {
    quint64 timestampMs = 0;
    float gyroXDps = 0, gyroYDps = 0, gyroZDps = 0;
    float accelXG = 0, accelYG = 0, accelZG = 0;
    float rollDeg = 0, pitchDeg = 0, yawDeg = 0;

    static constexpr int kPayloadSize = 44;
    static bool fromPayload(const QByteArray &payload, TelemetryData *out);
};

struct AckNack {
    quint8 ackType = 0;
    quint8 status = 0;

    static constexpr int kPayloadSize = 2;
    static bool fromPayload(const QByteArray &payload, AckNack *out);
};

struct TimeSyncResponse {
    quint64 deviceTickMs = 0;

    static constexpr int kPayloadSize = 8;
    static bool fromPayload(const QByteArray &payload, TimeSyncResponse *out);
};

struct CalibResponse {
    quint8 status = 0;
    float biasX = 0, biasY = 0, biasZ = 0;

    static constexpr int kPayloadSize = 13;
    static bool fromPayload(const QByteArray &payload, CalibResponse *out);
};

quint16 computeCrc16(const QByteArray &data);
QByteArray buildFrame(quint8 type, const QByteArray &payload = QByteArray());

QByteArray buildTimeSyncRequest(quint64 hostTimeMs);
QByteArray buildModeConfig(quint8 mode);
QByteArray buildResetRequest();

QString typeName(quint8 type);

} // namespace ImuProtocol

#endif // PROTOCOLIMU_H
