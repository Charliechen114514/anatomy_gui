#pragma once

#include <functional>
namespace anatomy_gui::base {
class Guard {
  public:
    Guard(std::function<void(void)> action) : action_(action) {};
    void act() {
        if (action_) {
            action_(); // If ok, call it
        }
    }
    ~Guard() { act(); }

  private:
    std::function<void(void)> action_;
};
} // namespace anatomy_gui::base
