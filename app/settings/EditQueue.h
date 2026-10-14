#pragma once
#include "SettingsTypes.h"
#include <algorithm>
#include <deque>

namespace Settings {
struct Edit {
    Feature feature;
    std::uint16_t value;
};
// User intent is separate from confirmed device values. At most one operation
// runs, and at most one latest-value edit waits per feature. Never retries I/O.
class EditQueue {
  public:
    void submit(Edit edit) {
        const auto found = std::ranges::find(m_waiting, edit.feature, &Edit::feature);
        if (m_active && m_active->feature == edit.feature && m_active->value == edit.value) {
            if (found != m_waiting.end())
                m_waiting.erase(found);
        } else if (found != m_waiting.end()) {
            found->value = edit.value;
        } else {
            m_waiting.push_back(edit);
        }
    }
    std::optional<Edit> take() {
        if (m_active || m_waiting.empty())
            return {};
        m_active = m_waiting.front();
        m_waiting.pop_front();
        return m_active;
    }
    std::optional<Edit> active() const { return m_active; }
    bool empty() const { return !m_active && m_waiting.empty(); }
    void complete() { m_active.reset(); }
    void clear() {
        m_active.reset();
        m_waiting.clear();
    }
    bool contains(Feature feature) const {
        return (m_active && m_active->feature == feature) ||
               std::ranges::find(m_waiting, feature, &Edit::feature) != m_waiting.end();
    }
    Values projected(Values confirmed) const {
        if (m_active)
            confirmed.set(m_active->feature, m_active->value);
        for (const auto &edit : m_waiting)
            confirmed.set(edit.feature, edit.value);
        return confirmed;
    }

  private:
    std::optional<Edit> m_active;
    std::deque<Edit> m_waiting;
};
} // namespace Settings
