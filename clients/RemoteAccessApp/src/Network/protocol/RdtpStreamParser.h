#pragma once

#include <QByteArray>
#include <QList>

#include "Protocolheader.h"

namespace Protocol {

class RdtpStreamParser
{
public:
    struct Message {
        ProtocolHeader header;
        QByteArray payload;
    };

    enum class Error {
        None,
        MalformedHeader,
        InvalidMagic,
        UnsupportedVersion,
        PayloadTooLarge
    };

    struct FeedResult {
        QList<Message> messages;
        Error error = Error::None;
    };

    FeedResult feed(const QByteArray &bytes);

private:
    QByteArray m_buffer;
};

} // namespace Protocol
