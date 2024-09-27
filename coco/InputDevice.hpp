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
    /// @param counters array of input counters
    /// @return sequence number, can be used to determine if new values are available
    virtual int get(void *data, int size) = 0;

    /// @brief Get the current data into an array
    /// @tparam T array element type
    /// @param array the array
    /// @return sequence number, can be used to determine if new values are available
    template <typename T> requires (ArrayConcept<T>)
    int get(T &array) {return get(std::data(array), std::size(array) * sizeof(array[0]));}

    /// @brief Wait until new input data is available
    /// @param sequenceNumber sequence number that has already been processed
    [[nodiscard]] virtual Awaitable<Events> untilInput(int sequenceNumber) = 0;
};

} // namespace coco
