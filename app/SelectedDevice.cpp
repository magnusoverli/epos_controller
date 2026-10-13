#include "SelectedDevice.h"

void SelectedDevice::update(std::optional<DeviceSnapshot> device, bool stale) {
    if (m_device == device && m_stale == stale)
        return;
    m_device = std::move(device);
    m_stale = stale;
    emit changed();
}
