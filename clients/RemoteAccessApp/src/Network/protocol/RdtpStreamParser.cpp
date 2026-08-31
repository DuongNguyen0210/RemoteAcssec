#include "RdtpStreamParser.h"

#include "protocolconstants.h"
#include "protocolserializer.h"

namespace Protocol {

RdtpStreamParser::FeedResult RdtpStreamParser::feed(const QByteArray &bytes)
{
    m_buffer.append(bytes);

    FeedResult result;
    const int headerSize = static_cast<int>(HEADER_SIZE);

    while (m_buffer.size() >= headerSize) {
        const QByteArray headerBytes = m_buffer.left(headerSize);
        const auto decodedHeader = ProtocolSerializer::deserializeHeader(headerBytes);

        if (!decodedHeader.has_value()) {
            result.error = Error::MalformedHeader;
            m_buffer.clear();
            return result;
        }

        const ProtocolHeader &header = decodedHeader.value();

        if (header.magic != PROTOCOL_MAGIC) {
            result.error = Error::InvalidMagic;
            m_buffer.clear();
            return result;
        }

        if (header.version != PROTOCOL_VERSION) {
            result.error = Error::UnsupportedVersion;
            m_buffer.clear();
            return result;
        }

        if (header.payloadLength > MAX_PAYLOAD_LENGTH) {
            result.error = Error::PayloadTooLarge;
            m_buffer.clear();
            return result;
        }

        const int payloadLength = static_cast<int>(header.payloadLength);
        const int messageSize = headerSize + payloadLength;

        if (m_buffer.size() < messageSize) {
            break;
        }

        Message message {
            header,
            m_buffer.mid(headerSize, payloadLength)
        };
        result.messages.append(message);
        m_buffer.remove(0, messageSize);
    }

    return result;
}

} // namespace Protocol
