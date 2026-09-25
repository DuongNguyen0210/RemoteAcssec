#pragma once
#include <QObject>
#include <functional>
#include "RelayEndpoint.h"

class RelayEndpointProvider : public QObject {
public:
    using Callback = std::function<void(const RelayEndpoint &, const QString &)>;
    using QObject::QObject;
    virtual void allocate(Callback callback) = 0;
    virtual void lookup(const QString &agentSessionId, Callback callback) = 0;
};
