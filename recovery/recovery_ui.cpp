/*
 * Copyright (C) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <android-base/properties.h>
#include <recovery_ui/device.h>
#include <recovery_ui/screen_ui.h>

namespace {

constexpr int kRebootEdl = 1000;
using MenuAction = std::pair<std::string, int>;

class FujisanRecoveryDevice final : public Device {
  public:
    FujisanRecoveryDevice() : Device(new ScreenRecoveryUI) {
        main_actions_ = {
                { "Reboot system now", REBOOT },
                { "Apply update", APPLY_UPDATE },
                { "Factory reset", MENU_WIPE },
                { "Advanced", MENU_ADVANCED },
        };
        advanced_actions_ = {
                { "Reboot to recovery", REBOOT_RECOVERY },
                { "Reboot to EDL", kRebootEdl },
                { "View recovery logs", VIEW_RECOVERY_LOGS },
                { "Power off", SHUTDOWN },
        };
        wipe_actions_ = {
                { "Format data/factory reset", WIPE_DATA },
                { "Format cache partition", WIPE_CACHE },
                { "Format system partition", WIPE_SYSTEM },
        };
        current_actions_ = &main_actions_;
        PopulateMenuItems();
    }

    const std::vector<std::string>& GetMenuItems() override {
        return menu_items_;
    }

    const std::vector<std::string>& GetMenuHeaders() override {
        if (current_actions_ == &advanced_actions_) return advanced_header_;
        if (current_actions_ == &wipe_actions_) return wipe_header_;
        return main_header_;
    }

    void GoHome() override {
        current_actions_ = &main_actions_;
        PopulateMenuItems();
    }

    BuiltinAction InvokeMenuItem(size_t menu_position) override {
        if (menu_position >= current_actions_->size()) return NO_ACTION;

        const int action = (*current_actions_)[menu_position].second;
        if (action == MENU_WIPE) {
            current_actions_ = &wipe_actions_;
            PopulateMenuItems();
            return NO_ACTION;
        }
        if (action == MENU_ADVANCED) {
            current_actions_ = &advanced_actions_;
            PopulateMenuItems();
            return NO_ACTION;
        }
        if (action == kRebootEdl) {
            GetUI()->Print("Rebooting to EDL...\n");
            // This is the reboot reason accepted by the stock kernel and bootloader.
            android::base::SetProperty("sys.powerctl", "reboot,edl");
            return NO_ACTION;
        }
        return static_cast<BuiltinAction>(action);
    }

    void RemoveMenuItemForAction(BuiltinAction action) override {
        RemoveAction(main_actions_, action);
        RemoveAction(advanced_actions_, action);
        RemoveAction(wipe_actions_, action);
        PopulateMenuItems();
    }

  private:
    static void RemoveAction(std::vector<MenuAction>& actions, BuiltinAction action) {
        actions.erase(std::remove_if(actions.begin(), actions.end(),
                                     [action](const MenuAction& entry) {
                                         return entry.second == action;
                                     }),
                      actions.end());
    }

    void PopulateMenuItems() {
        menu_items_.clear();
        for (const auto& entry : *current_actions_) {
            menu_items_.push_back(entry.first);
        }
    }

    const std::vector<std::string> main_header_{};
    const std::vector<std::string> advanced_header_{ "Advanced options" };
    const std::vector<std::string> wipe_header_{ "Factory reset" };
    std::vector<MenuAction> main_actions_;
    std::vector<MenuAction> advanced_actions_;
    std::vector<MenuAction> wipe_actions_;
    std::vector<MenuAction>* current_actions_ = nullptr;
    std::vector<std::string> menu_items_;
};

}  // namespace

Device* make_device() {
    return new FujisanRecoveryDevice;
}
