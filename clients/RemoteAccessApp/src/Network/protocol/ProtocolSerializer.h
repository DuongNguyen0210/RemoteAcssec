#pragma once

#include <optional>
#include <QByteArray>
#include "Protocolheader.h"

namespace Protocol {

class ProtocolSerializer
{
public:
    static QByteArray serializeHeader(const ProtocolHeader &header);
    static std::optional<ProtocolHeader> deserializeHeader(const QByteArray &data);
};

}
