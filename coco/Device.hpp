#pragma once

//#include "StateTasks.hpp"
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>
#include <cstdint>
#include <system_error>


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
    enum class State : uint8_t {
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
    Device(State state) : state_(state) {}

    /// @brief Destructor. Note that it is not always allowed to destroy a device. For example for BufferDevice, no buffer may
    /// be in BUSY state.
    virtual ~Device() {}


    /// @brief Get current state
    /// @return state
    State state() {return state_;}

    /// @brief Returns true if the device is disabled
    ///
    bool disabled() {return state_ == State::DISABLED;}

    /// @brief Returns true if the device is opening
    ///
    bool opening() {return state_ == State::OPENING;}

    /// @brief Returns true if the device is ready
    ///
    bool ready() {return state_ == State::READY;}

    /// @brief Returns true if the device is closing
    ///
    bool closing() {return state_ == State::CLOSING;}

    /// @brief Wait until the device state changed, e.g. from OPENING to READY
    /// @return use co_await on return value to await a state change
    [[nodiscard]] Awaitable<Events> untilStateChanged() {return {tasks_, Events::ENTER_ANY};}

    /// @brief Wait until the device is disabled. Does not wait when the device is already in DISABLED state.
    /// @return use co_await on return value to wait until the device becomes disabled
    [[nodiscard]] Awaitable<Events> untilDisabled() {
        //auto &st = getStateTasks();
        if (state_ == State::DISABLED)
            return {};
        return {tasks_, Events::ENTER_DISABLED};
    }

    /// @brief Wait until the device is ready. Does not wait when the device is already in READY state.
    /// @return use co_await on return value to wait until the device becomes ready
    [[nodiscard]] Awaitable<Events> untilReady() {
        //auto &st = getStateTasks();
        if (state_ == State::READY)
            return {};
        return {tasks_, Events::ENTER_READY};
    }

    /// @brief Wait unless the device is ready or disabled. Does not wait when the device is in READY or DISABLED state.
    /// @return use co_await on return value to wait until the device becomes ready or disabled
    [[nodiscard]] Awaitable<Events> untilReadyOrDisabled() {
        //auto &st = getStateTasks();
        if (state_ == State::READY || state_ == State::DISABLED)
            return {};
        return {tasks_, Events(int(Events::ENTER_READY) | int(Events::ENTER_DISABLED))};
    }


    /// @brief Close the device. May not take effect immediately, therefore use co_await untilDisabled() to wait until close completes.
    ///
    virtual void close();


// error
// -----

    /// @brief Get the error code of the last operation.
    /// Check for success: if (!buffer.error()) ...
    /// Check if cancelled: buffer.error() == std::errc::operation_canceled
    /// @return Error code
    std::error_code error() {
#ifdef NATIVE
        // native platforms (Windows, Linux, macOS) use full std::error_code
        return error_;
#else
        // embedded platforms directly use std::errc stored in an uint8_t
        return std::error_code(error_, std::generic_category());
#endif
    }


protected:
    /// @brief Set success and number of transferred bytes.
    ///
    void setSuccess() {
        error_ = {};
    }

    /// @brief Set generic error using std::errc.
    /// @param error Error
    void setError(std::errc error) {
#ifdef NATIVE
        error_ = std::make_error_code(error);
#else
        error_ = uint8_t(error);
#endif
    }

#ifdef NATIVE
    /// @brief Set system error originating from errno or GetLastError()/WSAGetLastError().
    /// @param error System error
    void setSystemError(int error) {
        error_ = {error, std::system_category()};
    }
#endif

    /// @brief Notify waiting coroutines about the given events.
    /// @param events Event flags to notify
    /// @return *this
    auto &notify(Events events) {
        // resume all coroutines waiting for the given event
        tasks_.doAll([events](Events e) {
            return (int(events) & int(e)) != 0;
        });
        return *this;
    }

    // current state of the buffer
    State state_;

    // general purpose flags (fit into the alignment space after state_)
    uint8_t flags;

    // result of last transfer operation
#ifdef NATIVE
    std::error_code error_;
#else
    uint8_t error_ = 0;
#endif

    // tasks (waiting coroutines)
    CoroutineTaskList<Events> tasks_;
};
COCO_ENUM(Device::Events);

} // namespace coco
