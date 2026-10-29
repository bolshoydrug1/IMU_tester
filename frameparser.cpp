#include "frameparser.h"

#include "protocolimu.h"

FrameParser::FrameParser(QObject *parent)
    : QObject(parent)
{
}

void FrameParser::reset()
{
    m_buffer.clear();
}

void FrameParser::addData(const QByteArray &data)
{
    m_buffer.append(data);

    for (;;) {
        const int headIdx = m_buffer.indexOf(char(ImuProtocol::kHead));
        if (headIdx < 0) {
            m_buffer.clear();
            return;
        }
        if (headIdx > 0) {
            m_buffer.remove(0, headIdx);
        }

        // head(1) + type(1) + version(1) + length(2)
        if (m_buffer.size() < 5) {
            return;
        }

        const quint8 type = static_cast<quint8>(m_buffer.at(1));
        const quint16 length = static_cast<quint8>(m_buffer.at(3))
                                | (static_cast<quint16>(static_cast<quint8>(m_buffer.at(4))) << 8);

        if (length > kMaxPayloadLen) {
            // Похоже на мусор, а не на настоящий заголовок — сдвигаемся и ищем head заново.
            m_buffer.remove(0, 1);
            continue;
        }

        const int frameLen = 5 + length + 2 + 1; // header + payload + crc16 + tail
        if (m_buffer.size() < frameLen) {
            return; // кадр ещё не пришёл целиком
        }

        const QByteArray payload = length > 0 ? m_buffer.mid(5, length) : QByteArray();
        const quint16 crcReceived = static_cast<quint8>(m_buffer.at(5 + length))
                                    | (static_cast<quint16>(static_cast<quint8>(m_buffer.at(5 + length + 1))) << 8);
        const quint16 crcComputed = ImuProtocol::computeCrc16(m_buffer.mid(1, 4 + length));
        const quint8 tail = static_cast<quint8>(m_buffer.at(5 + length + 2));

        if (crcReceived == crcComputed && tail == ImuProtocol::kTail) {
            emit frameReady(type, payload);
            m_buffer.remove(0, frameLen);
        } else {
            m_buffer.remove(0, 1); // не сошлось — ресинхронизация побайтово
        }
    }
}
