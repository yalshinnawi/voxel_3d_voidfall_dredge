#include "debrief_menu.hpp"

namespace Voidfall {

static DebriefMenu::RedeployCallback s_redeploy_handler = nullptr;
static DebriefMenu::ReturnToHubCallback s_hub_handler = nullptr;

DebriefMenu::DebriefMenu()
    : m_currentSectorIndex(1) {
}

void DebriefMenu::SetRedeployHandler(RedeployCallback cb) {
    s_redeploy_handler = std::move(cb);
}

void DebriefMenu::SetReturnToHubHandler(ReturnToHubCallback cb) {
    s_hub_handler = std::move(cb);
}

DebriefAction DebriefMenu::ProcessClick(const std::string& button_label) {
    if (button_label.find("RE-DEPLOY") != std::string::npos || button_label.find("LAUNCH") != std::string::npos) {
        if (s_redeploy_handler) {
            s_redeploy_handler(m_currentSectorIndex);
        }
        return DebriefAction::RedeployExpedition;
    }
    if (button_label.find("RETURN") != std::string::npos || button_label.find("HUB") != std::string::npos) {
        if (s_hub_handler) {
            s_hub_handler();
        }
        return DebriefAction::ReturnToHub;
    }
    return DebriefAction::None;
}

DebriefAction DebriefMenu::ProcessClick(DebriefAction action) {
    if (action == DebriefAction::RedeployExpedition || action == DebriefAction::LaunchNextSector) {
        if (s_redeploy_handler) {
            s_redeploy_handler(m_currentSectorIndex);
        }
        return action;
    }
    if (action == DebriefAction::ReturnToHub) {
        if (s_hub_handler) {
            s_hub_handler();
        }
        return action;
    }
    return DebriefAction::None;
}

} // namespace Voidfall
