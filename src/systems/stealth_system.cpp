#include "stealth_system.hpp"

namespace Voidfall {

StealthSystem& StealthSystem::instance() {
    static StealthSystem s_instance;
    return s_instance;
}

} // namespace Voidfall
