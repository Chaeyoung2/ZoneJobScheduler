#pragma once

#include <functional>

class Zone;

using Job = std::function<void(Zone&)>;
