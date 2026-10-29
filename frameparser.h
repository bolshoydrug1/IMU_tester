#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include <QByteArray>
#include <QObject>

/* Собирает кадры протокола ИИС из потока байт, приходящего с COM-порта.
 * Устойчив к «мусору» и разрыву кадра: при неверной CRC/tail или подозрительно
 * большой длине payload сдвигается на 1 байт и ищет следующий head заново. */
class FrameParser : public QObject
{
    Q_OBJECT
public:
    explicit FrameParser(QObject *parent = nullptr);

    void reset();

public slots:
    void addData(const QByteArray &data);

signals:
    void frameReady(quint8 type, const QByteArray &payload);

private:
    QByteArray m_buffer;
    static constexpr int kMaxPayloadLen = 512;
};

#endif // FRAMEPARSER_H
