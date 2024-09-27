#pragma once

#include "StateTasks.hpp"
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>


namespace coco {

/**
 * Base class for a device that has a state and can be closed.
 *
 * States and transitions:
 *
 * DISABLED --- open() ---> OPENING
 *  ^   ^---------------------|
 *  |                         |
 *  |                         v
 * CLOSING <--- close() --- READY
 *
 * open() changes the state from DISABLED to OPENING and is implemented in subclasses. The device transitions from
 * OPENING to READY as soon as the device is ready (e.g. connection is established). The device transitions from
 * OPENING to DISABLED if open fails (e.g. connection refused).
 *
 * close() changes the state from READY to CLOSING. The device transitions form CLOSING to DISABLED as soon as the
 * device is disabled (e.g. connection is closed).

 * Associated buffers of a BufferDevice become READY on open(), i.e. a transfer can be started when the device is in
 * OPEN state, but the transfer internally starts when the device transitions to READY state. When close() gets
 * called, all transfers get cancelled and the buffers transition to DISABLED state when the cancel operation
 * completes.
 */
class Device {
public:
    /// device state
    enum class State {
        /**
         * Device is disabled e.g. a file closed or a socket disconnected
         */
        DISABLED = 0,

        /**
         * Device is closing
         */
        CLOSING = 1,

        /**
         * Device is opening
         */
        OPENING = 2,

        /**
         * Device is ready for operation
         */
        READY = 3
    };

    /// Event flags
    enum class Events {
        NONE = 0,

        /// device entered a state
        ENTER_DISABLED = 1,
        ENTER_CLOSING = 1 << 1,
        ENTER_OPENING = 1 << 2,
        ENTER_READY = 1 << 3,
        ENTER_ANY = ENTER_DISABLED | ENTER_CLOSING | ENTER_OPENING | ENTER_READY,

        /// device gets requested to do something (e.g. control request for USB)
        REQUEST = 1 << 8,

        /// control signals change their state (e.g. DCD for serial or volgate level for USB PD)
        SIGNALS_CHANGED = 1 << 9,

        /// device is readable (for "notify on ready"-model, typically not implemented by BufferDevice subclasses
        /// which implement "notify on completion"-model)
        READABLE = 1 << 10,

        /// device is writable (for "notify on ready"-model, typically not implemented by BufferDevice subclasses
        /// which implement "notify on completion"-model)
        WRITABLE = 1 << 11,
    };

    Device(State state) : st(state) {}

    /**
     * Destructor. Note that it is not always allowed to destroy a device. For example for BufferDevice, no buffer can
     * be in BUSY state.
     */
    virtual ~Device() {}


    //virtual StateTasks<const State, Events> &getStateTasks() = 0;

    /**
     * Get current state
     * @return state
     */
    State state() {return this->st.state;}

    /// Returns true if the device is disabled
    bool disabled() {return this->st.state == State::DISABLED;}

    /// Returns true if the device is opening
    bool opening() {return this->st.state == State::OPENING;}

    /// Returns true if the device is ready
    bool ready() {return this->st.state == State::READY;}

    /// Returns true if the device is closing
    bool closing() {return this->st.state == State::CLOSING;}

    /**
     * Wait until the device state changed, e.g. from OPENING to READY
     * @return use co_await on return value to await a state change
     */
    [[nodiscard]] Awaitable<Events> untilStateChanged() {return {this->st.tasks, Events::ENTER_ANY};}

    /**
     * Wait until the device is disabled. Does not wait when the device is already in DISABLED state.
     * @return use co_await on return value to wait until the device becomes disabled
     */
    [[nodiscard]] Awaitable<Events> untilDisabled() {
        //auto &st = getStateTasks();
        if (this->st.state == State::DISABLED)
            return {};
        return {this->st.tasks, Events::ENTER_DISABLED};
    }

    /**
     * Wait until the device is ready. Does not wait when the device is already in READY state.
     * @return use co_await on return value to wait until the device becomes ready
     */
    [[nodiscard]] Awaitable<Events> untilReady() {
        //auto &st = getStateTasks();
        if (this->st.state == State::READY)
            return {};
        return {this->st.tasks, Events::ENTER_READY};
    }

    /**
     * Wait unless the device is ready or disabled. Does not wait when the device is in READY or DISABLED state.
     * @return use co_await on return value to wait until the device becomes ready or disabled
     */
    [[nodiscard]] Awaitable<Events> untilReadyOrDisabled() {
        //auto &st = getStateTasks();
        if (this->st.state == State::READY || st.state == State::DISABLED)
            return {};
        return {this->st.tasks, Events(int(Events::ENTER_READY) | int(Events::ENTER_DISABLED))};
    }


    /**
     * Close the device. May not take effect immediately, therefore use co_await untilDisabled() to wait until close completes.
     */
    virtual void close();

protected:
    // state and tasks (waiting coroutines)
    StateTasks<State, Events> st;
};
COCO_ENUM(Device::Events);

} // namespace coco
