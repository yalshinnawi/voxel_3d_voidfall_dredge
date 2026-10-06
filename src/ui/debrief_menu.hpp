#pragma once

#include "orbital_hub.hpp"
#include <string>
#include <functional>

namespace Voidfall {

class DebriefMenu {
public:
    using RedeployCallback = std::function<void(int)>;
    using ReturnToHubCallback = std::function<void()>;

    DebriefMenu();
    ~DebriefMenu() = default;

    static void SetRedeployHandler(RedeployCallback cb);
    static void SetReturnToHubHandler(ReturnToHubCallback cb);

    void SetCurrentSectorIndex(int sector) { m_currentSectorIndex = sector; }
    int GetCurrentSectorIndex() const { return m_currentSectorIndex; }

    DebriefAction ProcessClick(const std::string& button_label);
    DebriefAction ProcessClick(DebriefAction action);

private:
    int m_currentSectorIndex{1};
};

} // namespace Voidfall
