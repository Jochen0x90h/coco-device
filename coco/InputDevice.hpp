#pragma once

#include "Device.hpp"
#include <coco/ArrayConcept.hpp>


namespace coco {

/// @brief Device for input data e.g. from buttons or sensors.
/// The input data has a sequence number which counts up for every data frame which arrives
class InputDevice : public Device {
public:
    InputDevice(State state) : Device(state) {}

    /// @brief Get the current data.
    /// @param data Input data
    /// @param size Size of the data in bytes
    /// @return Sequence number, can be used to determine if new data is available
    virtual int get(void *data, int size) = 0;

    /// @brief Get the current data into an array
    /// @tparam T array element type
    /// @param array The array
    /// @return Sequence number, can be used to determine if new data is available
    template <typename T> requires (ArrayConcept<T>)
    int get(T &array) {return get(std::data(array), std::size(array) * sizeof(array[0]));}

    /// @brief Wait until new input data is available
    /// Pass in the sequence number from the last call to get(). Does not wait if data with a new sequence number is
    /// already available.
    /// @param sequenceNumber Sequence number that has already been processed
    /// @return use co_await on return value to wait until new data is available
    [[nodiscard]] virtual Awaitable<Events> untilInput(int sequenceNumber) = 0;
};

} // namespace coco
