#pragma once

#include "StateTasks.hpp"
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>


namespace coco {


/// @brief Base class for a device that has a state and can be closed.
///
/// States and transitions:
///
/// DISABLED --- open() ---> OPENING
///  ^   ^-----------------------|
///  |   |                       |
///  |   |--------------------|  |
///  |                        |  v
/// CLOSING <--- close() ---- READY
///
/// Calling open() changes the state from DISABLED to OPENING and is implemented in subclasses. The device transitions
/// from OPENING to READY as soon as the device is ready (e.g. connection is established). The device transitions from
/// OPENING to DISABLED if open fails (e.g. connection refused).
///
/// Calling close() changes the state from READY to CLOSING. The device transitions form CLOSING to DISABLED as soon as
/// the device is disabled (e.g. connection is closed).
///
/// A device can change its state from READY to DISABLED (e.g. other end of connection closed the connection).
///
/// Associated buffers of a BufferDevice become READY on open(), i.e. a transfer can be started when the device is in
/// OPEN state, but the transfer internally starts when the device transitions to READY state. When close() gets
/// called, all transfers get cancelled and the buffers transition to DISABLED state when the cancel operation
/// completes.
class Device {
public:
    /// device state
    enum class State {
        /// @brief Device is disabled e.g. a file closed or a socket disconnected
        ///
        DISABLED = 0,

        /// @brief Device is closing
        ///
        CLOSING = 1,

        /// @brief Device is opening
        ///
        OPENING = 2,

        /// @brief Device is ready for operation
        ///
        READY = 3
    };

    /// @brief Event flags
    ///
    enum class Events {
        NONE = 0,


        /// @brief Device entered disabled state
        ///
        ENTER_DISABLED = 1,

        /// @brief Device entered closing state
        ///
        ENTER_CLOSING = 1 << 1,

        /// @brief Device entered opening state
        ///
        ENTER_OPENING = 1 << 2,

        /// @brief Device entered ready state
        ///
        ENTER_READY = 1 << 3,

        /// @brief Device entered any state
        ///
        ENTER_ANY = ENTER_DISABLED | ENTER_CLOSING | ENTER_OPENING | ENTER_READY,


        /// @brief Device gets requested to do something (e.g. control request for USB)
        ///
        REQUEST = 1 << 8,

        /// @brief Control signals change their state (e.g. DCD for serial or volgate level for USB PD)
        ///
        SIGNALS_CHANGED = 1 << 9,

        /// @brief Device is readable (for "notify on ready"-model, typically not implemented by BufferDevice
        /// subclasses which implement "notify on completion"-model)
        READABLE = 1 << 10,

        /// @brief Device is writable (for "notify on ready"-model, typically not implemented by BufferDevice
        /// subclasses which implement "notify on completion"-model)
        WRITABLE = 1 << 11,
    };


    /// @brief Constructor
    /// @param state initial state of the device
    Device(State state) : st(state) {}

    /// @brief Destructor. Note that it is not always allowed to destroy a device. For example for BufferDevice, no buffer may
    /// be in BUSY state.
    virtual ~Device() {}


    /// @brief Get current state
    /// @return state
    State state() {return this->st.state;}

    /// @brief Returns true if the device is disabled
    ///
    bool disabled() {return this->st.state == State::DISABLED;}

    /// @brief Returns true if the device is opening
    ///
    bool opening() {return this->st.state == State::OPENING;}

    /// @brief Returns true if the device is ready
    ///
    bool ready() {return this->st.state == State::READY;}

    /// @brief Returns true if the device is closing
    ///
    bool closing() {return this->st.state == State::CLOSING;}

    /// @brief Wait until the device state changed, e.g. from OPENING to READY
    /// @return use co_await on return value to await a state change
    [[nodiscard]] Awaitable<Events> untilStateChanged() {return {this->st.tasks, Events::ENTER_ANY};}

    /// @brief Wait until the device is disabled. Does not wait when the device is already in DISABLED state.
    /// @return use co_await on return value to wait until the device becomes disabled
    [[nodiscard]] Awaitable<Events> untilDisabled() {
        //auto &st = getStateTasks();
        if (this->st.state == State::DISABLED)
            return {};
        return {this->st.tasks, Events::ENTER_DISABLED};
    }

    /// @brief Wait until the device is ready. Does not wait when the device is already in READY state.
    /// @return use co_await on return value to wait until the device becomes ready
    [[nodiscard]] Awaitable<Events> untilReady() {
        //auto &st = getStateTasks();
        if (this->st.state == State::READY)
            return {};
        return {this->st.tasks, Events::ENTER_READY};
    }

    /// @brief Wait unless the device is ready or disabled. Does not wait when the device is in READY or DISABLED state.
    /// @return use co_await on return value to wait until the device becomes ready or disabled
    [[nodiscard]] Awaitable<Events> untilReadyOrDisabled() {
        //auto &st = getStateTasks();
        if (this->st.state == State::READY || st.state == State::DISABLED)
            return {};
        return {this->st.tasks, Events(int(Events::ENTER_READY) | int(Events::ENTER_DISABLED))};
    }


    /// @brief Close the device. May not take effect immediately, therefore use co_await untilDisabled() to wait until close completes.
    ///
    virtual void close();

protected:
    // state and tasks (waiting coroutines)
    StateTasks<State, Events> st;
};
COCO_ENUM(Device::Events);

} // namespace coco
