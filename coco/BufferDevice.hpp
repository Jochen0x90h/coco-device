#pragma once

#include "Device.hpp"
#include "Buffer.hpp"


namespace coco {

/// @brief A device that has associated transfer buffers.
///
class BufferDevice : public Device {
public:
    BufferDevice(State state) : Device(state) {}

    /// @brief Get number of buffers of the device.
    ///
    virtual int getBufferCount() = 0;

    /// @brief Get the buffer at the given index.
    ///
    virtual Buffer &getBuffer(int index) = 0;
};

} // namespace coco
