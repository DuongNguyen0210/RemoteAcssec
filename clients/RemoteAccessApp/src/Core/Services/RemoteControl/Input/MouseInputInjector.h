#pragma once

#include "RemoteMouseEvent.h"

class MouseInputInjector
{
public:
    bool inject(const RemoteMouseEvent &event) const;
};
