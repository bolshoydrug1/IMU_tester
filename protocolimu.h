#ifndef PROTOCOLIMU_H
#define PROTOCOLIMU_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/* Единственный источник истины по формату обмена — Protocol_IMU.h/.c из
 * основного проекта прошивки (папка Protocol). Здесь только C++-обёртки:
 * константы и размеры структур берутся напрямую из прошивочного заголовка,
 * чтобы GUI не расходился с устройством при изменении протокола.
 * Кадр: head(1)=0xAA, type(1), version(1), length(2, LE) payload,
 * crc16(2, LE, CCITT-FALSE по type+version+length+payload), tail(1)=0x55. */
extern "C" {
#include "Protocol_IMU.h"
}

namespace ImuProtocol {

constexpr quint8 kVersion = PROTOCOL_VERSION;
constexpr quint8 kHead = PROTOCOL_HEAD;
constexpr quint8 kTail = PROTOCOL_TAIL;
constexpr int kOverheadBytes = PROTOCOL_OVERHEAD_BYTES;
constexpr quint32 kResetMagic = PROTOCOL_RESET_MAGIC;

enum FrameType : quint8 {
    TypeTelemetry  = PROTOCOL_TYPE_TELEMETRY,
    TypeAckNack    = PROTOCOL_TYPE_ACK_NACK,
    TypeTimeSync   = PROTOCOL_TYPE_TIME_SYNC,
    TypeCalibAccel = PROTOCOL_TYPE_CALIB_ACCEL,
    TypeCalibGyro  = PROTOCOL_TYPE_CALIB_GYRO,
    TypeAsciiMsg   = PROTOCOL_TYPE_ASCII_MSG,
    TypeModeConfig = PROTOCOL_TYPE_MODE_CONFIG,
    TypeReset      = PROTOCOL_TYPE_RESET,
};

enum Status : quint8 {
    StatusOk    = PROTOCOL_STATUS_OK,
    StatusError = PROTOCOL_STATUS_ERROR,
};

enum TelemetryStatus : quint8 {
    TelemetryStatusOk    = PROTOCOL_TELEMETRY_STATUS_OK,
    TelemetryStatusError = PROTOCOL_TELEMETRY_STATUS_ERROR,
};

enum Mode : quint8 {
    Mode100Hz     = PROTOCOL_MODE_100HZ,
    Mode250Hz     = PROTOCOL_MODE_250HZ,
    Mode500Hz     = PROTOCOL_MODE_500HZ,
    Mode1000Hz    = PROTOCOL_MODE_1000HZ,
    ModeOnRequest = PROTOCOL_MODE_ON_REQUEST,
};

struct TelemetryData {
    quint64 timestampMs = 0;
    float gyroXDps = 0, gyroYDps = 0, gyroZDps = 0;
    float accelXG = 0, accelYG = 0, accelZG = 0;
    float rollDeg = 0, pitchDeg = 0, yawDeg = 0;
    float temperatureC = 0;
    quint8 status = TelemetryStatusOk;

    static constexpr int kPayloadSize = sizeof(Protocol_TelemetryData_t);
    static bool fromPayload(const QByteArray &payload, TelemetryData *out);
};

struct AckNack {
    quint8 ackType = 0;
    quint8 status = 0;

    static constexpr int kPayloadSize = sizeof(Protocol_AckNack_t);
    static bool fromPayload(const QByteArray &payload, AckNack *out);
};

struct TimeSyncResponse {
    quint64 deviceTickMs = 0;

    static constexpr int kPayloadSize = sizeof(Protocol_TimeSyncResponse_t);
    static bool fromPayload(const QByteArray &payload, TimeSyncResponse *out);
};

struct CalibResponse {
    quint8 status = 0;
    float biasX = 0, biasY = 0, biasZ = 0;

    static constexpr int kPayloadSize = sizeof(Protocol_CalibResponse_t);
    static bool fromPayload(const QByteArray &payload, CalibResponse *out);
};

quint16 computeCrc16(const QByteArray &data);
QByteArray buildFrame(quint8 type, const QByteArray &payload = QByteArray());

QByteArray buildTimeSyncRequest(quint64 hostTimeMs);
QByteArray buildModeConfig(quint8 mode);
QByteArray buildResetRequest();

QString typeName(quint8 type);
QString telemetryStatusName(quint8 status);

} // namespace ImuProtocol

#endif // PROTOCOLIMU_H
