#pragma once

#include "Device.hpp"
#include <coco/Array.hpp>
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>
#include <coco/String.hpp>
#include <coco/StringConcept.hpp>
#include <system_error>


namespace coco {


/// @brief Buffer used for data transfer to/from hardware devices.
/// Typically each device provides its own buffer implementation. A Buffer has a capacity and a current size. For write
/// operations the current size is used. For read operations the size or capacity is used depending on the device type
/// and implementation. For example receiving data on a UART with timeout can use the capacity while receiving data on
/// SPI uses the size. A buffer can have a header which can contain separate data such as the address of a SPI/I2C
/// flash or the IP address of a UDP transfer. For example, when reading from an I2C flash, the header containing the
/// address gets written to the I2C bus before the actual data gets read into the buffer.
///
/// ++++++++++++++**********************-----------------
/// ^             ^                     ^                ^
/// headerData()  data()/begin()        size()/end()     capacity()
///
/// A data transfer can be started by start() which does not block. This internally starts for example a DMA transfer
/// and changes the state of the buffer to BUSY.
///
/// States and transitions:
///
/// DISABLED <----> READY --- start() --> BUSY
///                   ^                     |
///                   |----- completion ----|
///                   |----- cancel() ------|
///
/// Use the convenience methods e.g. co_await buffer.read() or co_await buffer.write(size) to read or write data.
/// Use co_await buffer.untilReady() to wait until completion of a transfer.
/// Use co_await buffer.untilReadyOrDisabled() to wait until a buffer becomes ready after a transfer or disabled because the
/// device was closed.
class Buffer {
public:
    /// @brief State of the buffer.
    ///
    enum class State : uint8_t {
        /// Buffer is disabled because the owning device is disabled, e.g. not connected
        ///
        DISABLED = 0,

        /// Buffer is ready and a transfer can be started
        ///
        READY = 1,

        /// Transfer is in progress
        ///
        BUSY = 2,
    };

    /// @brief Event flags.
    /// Event flags are used to wait for specific state transitions
    enum class Events {
        NONE = 0,

        /// device entered a state
        ENTER_DISABLED = 1,
        ENTER_READY = 1 << 1,
        ENTER_BUSY = 1 << 2,
        ENTER_ANY = ENTER_DISABLED | ENTER_READY | ENTER_BUSY,
    };

    /// @brief Operation.
    /// Read, write or special operation that can be passed to start().
    enum class Op : uint8_t {
        NONE = 0,

        /// Read operation
        ///
        READ = 1 << 0,

        /// Write operation
        ///
        WRITE = 1 << 1,

        /// SPI: Read and write simultaneously
        /// UART/USB: Read after write (the reply to a command can be read into the same buffer)
        READ_WRITE = READ | WRITE,

        /// Erase e.g. a flash memory
        ///
        ERASE = 1 << 2,

        //CANCEL = 1 << 3,

        /// Partial transfer, i.e. at least one more transfer will follow. If the underlying transport
        /// protocol is packetized, only full packets may be written (e.g. 64 bytes for USB bulk) and no "end packet" is
        /// generated.
        PARTIAL = 1 << 4,
    };

    /// @brief Result of operation.
    /*enum class Result : uint8_t {
        /// Operation completed successfully
        ///
        SUCCESS = 0,

        /// @brief Operation failed because device is busy, try again later
        BUSY = 1,

        /// @brief An error occurred (e.g. invalid parity or checksum)
        ///
        FAIL = 2,

        /// @brief An expected acknowledge was not received (e.g. unused i2c address or timeout waiting for acknowledge)
        ///
        NO_REPLY = 3,
    };*/

    /// @brief Constructor
    /// @param buffer data
    /// @param capacity buffer capacity
    /// @param state initial state of the buffer
    Buffer(uint8_t *data, int capacity, State state)
        : data_(data), header_(), capacity_(capacity), headerCapacity_(), state_(state), error_{} {}

    Buffer(uint8_t *headerAndData, int headerCapacity, int capacity, State state)
        : data_(headerAndData + headerCapacity), header_(headerAndData), capacity_(capacity)
        , headerCapacity_(headerCapacity), state_(state), error_{} {}

    Buffer(void *header, int headerCapacity, uint8_t *data, int capacity, State state)
        : data_(data), header_((uint8_t *)header), capacity_(capacity)
        , headerCapacity_(headerCapacity), state_(state), error_{} {}

    Buffer(uint8_t *data, int capacity, Device::State state)
        : Buffer(data, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}

    Buffer(uint8_t *headerAndData, int headerCapacity, int capacity, Device::State state)
        : Buffer(headerAndData, headerCapacity, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}

    Buffer(void *header, int headerCapacity, uint8_t *data, int capacity, Device::State state)
        : Buffer(header, headerCapacity, data, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}

    /*Buffer(uint8_t *data, int capacity, State state)
        : data_(data), header_(), capacity_(capacity), size_(), headerCapacity_(), headerType_()
        , result_(Result::SUCCESS), st(state) {}

    Buffer(uint8_t *headerAndData, int headerCapacity, int type, int capacity, State state)
        : data_(headerAndData + headerCapacity), header_(headerAndData), capacity_(capacity), size_()
        , headerCapacity_(headerCapacity), headerType_(type), result_(Result::SUCCESS), st(state) {}

    Buffer(void *header, int headerCapacity, int type, uint8_t *data, int capacity, State state)
        : data_(data), header_((uint8_t *)header), capacity_(capacity), size_()
        , headerCapacity_(headerCapacity), headerType_(type), result_(Result::SUCCESS), st(state) {}

    Buffer(uint8_t *data, int capacity, Device::State state)
        : Buffer(data, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}

    Buffer(uint8_t *headerAndData, int headerCapacity, int type, int capacity, Device::State state)
        : Buffer(headerAndData, headerCapacity, type, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}

    Buffer(void *header, int headerCapacity, int type, uint8_t *data, int capacity, Device::State state)
        : Buffer(header, headerCapacity, type, data, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}
*/

    /// @brief Destructor. Do not destroy a buffer that is in BUSY state.
    ///
    virtual ~Buffer() {}


// state
// -----

    /// @brief Get current state of the buffer
    /// @return State
    State state() {return state_;}

    /// @brief Returns true if the device is disabled
    ///
    bool disabled() {return state_ == State::DISABLED;}

    /// @briei Returns true if the device is ready
    ///
    bool ready() {return state_ == State::READY;}

    /// @brief Returns true if the device is ready
    ///
    bool busy() {return state_ == State::BUSY;}

    /// @brief Wait until the buffer state changed, e.g. from BUSY to READY.
    /// @return Use co_await on return value to await a state change
    [[nodiscard]] Awaitable<Events> untilStateChanged() {return {tasks_, Events::ENTER_ANY};}

    /// @brief Wait until the buffer is disabled. Does not wait when the device is already in DISABLED state.
    /// @return Use co_await on return value to wait until the buffer becomes disabled
    [[nodiscard]] Awaitable<Events> untilDisabled() {
        if (state_ == State::DISABLED)
            return {};
        return {tasks_, Events::ENTER_DISABLED};
    }

    /// @brief Wait unless the buffer is ready. Does not wait when the buffer is in READY state.
    ///
    /// Usage example:
    /// co_await buffer.untilReady();
    /// while (buffer.ready()) {
    ///   // use buffer
    /// }
    ///
    /// @return Use co_await on return value to wait until the buffer becomes ready
    [[nodiscard]] Awaitable<Events> untilReady() {
        if (state_ == State::READY)
            return {};
        return {tasks_, Events::ENTER_READY};
    }

    /// @brief Wait unless the buffer is ready or disabled. Does not wait when the buffer is in READY or DISABLED state.
    ///
    /// Usage example:
    /// co_await buffer.untilReadyOrDisabled();
    /// if (!buffer.ready()) {
    ///   // abort as buffer is disabled
    /// }
    /// // use buffer
    ///
    /// @return Use co_await on return value to wait until the buffer becomes ready or disabled
    [[nodiscard]] Awaitable<Events> untilReadyOrDisabled() {
        if (state_ == State::READY || state_ == State::DISABLED)
            return {};
        return {tasks_, Events(int(Events::ENTER_READY) | int(Events::ENTER_DISABLED))};
    }


// header
// ------

    /// @brief Get the capacity of the header.
    /// @return size
    int headerCapacity() const {return headerCapacity_;}

    /// @brief Set the header.
    /// Note that some buffer implementations modify the header during transfer operations (e.g. SPI).
    /// @return Number of bytes written into the header
    int setHeader(const uint8_t *header, int size) {
        size = std::clamp(size, 0, int(headerCapacity_));
        auto src = header;
        auto end = header + size;
        auto dst = header_;
        while (src != end) {
            *dst = *src;
            ++dst;
            ++src;
        }
        return size;
    }

    /// @brief Function for setting the header to some value, e.g. setHeader<uint32_t>(50) or setHeader(header).
    /// Only succeeds if the header capacity is large enough for the given data type
    /// @tparam T Data type of value
    /// @param value Value to set as header
    /// @return Number of bytes written into the header
    template <typename T>
    int setHeader(const T &value) {
        if (sizeof(T) > headerCapacity_)
            return 0;

        *reinterpret_cast<T *>(header_) = value;
        return sizeof(T);
    }

    /// @param Function for setting the header an array.
    /// Only succeeds if the header capacity is large enough for the given array
    /// @tparam T Type of array element
    /// @param array Array to set as header
    /// @return Number of bytes written into the header
    template <typename T> requires (ArrayConcept<T>)
    int setHeader(const T &array) {
        auto src = std::data(array);
        auto count = std::size(array);
        unsigned size = count * sizeof(*src);
        if (size > headerCapacity_)
            return 0;
        auto end = src + count;

        // cast header data to array element type
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(header_);
        std::copy(src, end, dst);
        return size;
    }

    /// @brief Get the header
    /// @return Number of bytes read from the header
    int getHeader(uint8_t *header, int size) {
        size = std::clamp(size, 0, int(headerCapacity_));
        uint8_t *src = header_;
        uint8_t *end = src + size;
        std::copy(src, end, header);
        return size;
    }

    /// @brief Function for getting the header as some value, e.g. getHeader<uint32_t>().
    /// Only succeeds if the header capacity is large enough for the given data type
    /// @tparam T Data type of value
    /// @param value Value to set as header
    /// @return Number of bytes read from the header
    template <typename T>
    int getHeader(T &value) {
        if (sizeof(T) > headerCapacity_)
            return 0;
        value = *reinterpret_cast<T *>(header_);
        return sizeof(T);
    }

    /// @brief Get the header as a reference to the given value, e.g. header = header<uint32_t>().
    /// @tparam T Data type of value
    /// @return Reference to the value
    template <typename T>
    T &header() {
        assert(sizeof(T) <= headerCapacity_);
        return *reinterpret_cast<T *>(header_);
    }

    /// @brief Get the header data of the buffer.
    ///
    uint8_t *headerData() {return header_;}
    const uint8_t *headerData() const {return header_;}

    /// @brief Get the header of the buffer as a pointer to given type
    /// @tparam T type
    template <typename T>
    T *headerPointer() {
        auto data = header_;
        return reinterpret_cast<T *>(data);
    }
/*
    /// @brief Set the header type.
    /// This is device specific
    /// @tparam T Header type enum
    /// @param type Header type
    /// @return *this for chaining
    template <typename T>
    auto &setHeaderType(T type) {
        static_assert(sizeof(T) == 1, "Size of header type enum must be 1 byte");
        headerType_ = uint8_t(type);
        return *this;
    }

    /// @brief Get the header type.
    /// @tparam T Header type enum
    /// @return Protocol (transfer mode)
    template <typename T>
    T headerType() {return T(headerType_);}

    /// @brief Set header size.
    /// This is device specific, e.g. header size for SPI and I2C.
    /// @param headerSize Header Size
    /// @return *this for chaining
    auto &setHeaderSize(int headerSize) {
        headerType_ = headerSize;
        return *this;
    }

    /// @brief Get the header size.
    /// @return header size
    int headerSize() {return headerType_;}
*/

// data
// ----

    /// @brief Check if the buffer is empty
    /// @return true when empty
    bool empty() const {return size_ == 0;}

    /// @brief Get the current size of the buffer
    /// @return size
    int size() const {return size_;}

    /// @brief Set the current size of the buffer
    /// @param size size of buffer, gets clamped to the capacity minus the header size
    void resize(int size) {
        assert(unsigned(size) <= capacity_);
        size_ = std::clamp(size, 0, int(capacity_));
    }

    /// @brief Clear the buffer (equivalent to resize(0))
    ///
    void clear() {
        size_ = 0;
    }

    /// @brief Get the capacity of the buffer
    /// @return size
    int capacity() const {return capacity_;}

    /// @brief Index operator
    /// @param index index between (-header capacity) and (buffer capacity - 1)
    uint8_t &operator [](int index) {
        assert(unsigned(index) < capacity_);
        auto data = data_;
        return data[index];
    }
    uint8_t operator [](int index) const {
        assert(unsigned(index) < capacity_);
        auto data = data_;
        return data[index];
    }

    /// @brief Get data of the buffer
    ///
    uint8_t *data() {return data_;}
    const uint8_t *data() const {return data_;}

    /// @brief Get begin iterator
    ///
    uint8_t *begin() {return data_;}
    const uint8_t *begin() const {return data_;}

    /// @brief Get end iterator
    ///
    uint8_t *end() {return data_ + size_;}
    const uint8_t *end() const {return data_ + size_;}

    /// @brief Get the data of the buffer as a value of given type
    /// @tparam T value type
    template <typename T>
    T &value() {
        assert(sizeof(T) <= capacity_);
        auto data = data_;
        return *reinterpret_cast<T *>(data);
    }

    /// @brief Get the data of the buffer as a pointer to given type
    /// @tparam T type
    template <typename T>
    T *pointer() {
        auto data = data_;
        return reinterpret_cast<T *>(data);
    }

    /// @brief Get the current data of the buffer as an array of given type
    /// @tparam T array element type
    template <typename T>
    Array<T> array() {
        auto data = data_;
        auto size = size_;
        return {reinterpret_cast<T *>(data), int(size / int(sizeof(T)))};
    }

    /// @brief Get the current data of the buffer as a string
    ///
    String string() {
        auto data = data_;
        auto size = size_;
        return String(data, size);
    }

    /// @brief Get whole buffer as array
    ///
    Array<uint8_t> all() {return {data_, int(capacity_)};}


// transfer
// --------

    /// @brief Get the currently set operation
    /// @return Current operation
    Op op() {return op_;}

    /// @brief Start transfer of the buffer if it is in READY state and set it to BUSY state if the operation does not
    /// complete immediately. If the buffer completes immediately, it stays in READY state. Depending on the underlying
    /// device and transfer direction, either the whole buffer gets transferred or only the current size.
    /// @return true if successful, false on error e.g. when the state is DISABLED or BUSY. Calling start() on a busy
    /// buffer is considered a bug and triggers an assertion if enabled.
    virtual bool start() = 0;

    /// @brief Convenience method for start() that sets the operation.
    /// @param op operation flags such as READ or WRITE
    /// @return true if successful, false on error
    bool start(Op op) {
        op_ = op;
        return start();
    }

    /// @brief Convenience method for start() that sets the operation and buffer size.
    /// @param op operation flags such as READ or WRITE
    /// @param size size of data to transfer
    /// @return true if successful, false on error
    bool start(Op op, int size) {
        op_ = op;
        size_ = std::clamp(size, 0, int(capacity_));
        return start();
    }

    /// @brief Convenience method for start() using an iterator.
    /// The buffer size is calculated from the given end iterator which must point into or just behind the buffer.
    /// @param op operation flags such as READ or WRITE
    /// @param end end iterator pointing behind the end of the data to be transferred in the buffer
    /// @return true if successful, false on error
    bool start(Op op, const uint8_t *end) {
        op_ = op;
        int size = end - data_;
        size_ = std::clamp(size, 0, int(capacity_));
        return start();
    }

    /// @brief Convenience function for receiving data of size up to capacity().
    /// Useful e.g. for UART, USB bulk, radio.
    /// @param op Additional operation flag
    /// @return Use co_await on return value to await completion of receive operation or device being disabled
    [[nodiscard]] Awaitable<Events> read() {
        size_ = capacity_;
        op_ = Op::READ;
        start();
        return untilReadyOrDisabled();
    }

    bool startRead() {
        size_ = capacity_;
        op_ = Op::READ;
        return start();
    }

    /// @brief Convenience function for initiating a read operation of given size.
    /// Useful e.g. for SPI, I2C, USB control, file.
    /// @param size size to read
    /// @return use co_await on return value to await completion of read operation
    [[nodiscard]] Awaitable<Events> read(int size) {
        start(Op::READ, size);
        return untilReadyOrDisabled();
    }

    bool startRead(int size) {
        return start(Op::READ, size);
    }


    /// @brief Convenience function for writing data of current size.
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> write() {
        start(Op::WRITE);
        return untilReadyOrDisabled();
    }

    bool startWrite() {
        return start(Op::WRITE);
    }

    /// @brief Convenience function for writing data.
    /// @param size size of data to write
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> write(int size) {
        start(Op::WRITE, size);
        return untilReadyOrDisabled();
    }

    bool startWrite(int size) {
        return start(Op::WRITE, size);
    }

    /// @brief Convenience function for writing data using an iterator.
    /// The buffer size is calculated from the given end iterator which must point into or just behind the buffer.
    /// Also works with BufferWriter, e.g. BufferWriter w(buffer); w.u8(10); buffer.write(w);
    /// @param end end pointer of data to write
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> write(const uint8_t *end) {
        start(Op::WRITE, end);
        return untilReadyOrDisabled();
    }

    bool startWrite(const uint8_t *end) {
        return start(Op::WRITE, end);
    }


    /// @brief Convenience function for writing data of current size and receiving a reply.
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> writeRead() {
        start(Op::READ_WRITE);
        return untilReadyOrDisabled();
    }

    bool startWriteRead() {
        return start(Op::READ_WRITE);
    }

    /// @brief Convenience function for writing data and receiving a reply.
    /// @param size size of data to write
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> writeRead(int size) {
        start(Op::READ_WRITE, size);
        return untilReadyOrDisabled();
    }

    bool startWriteRead(int size) {
        return start(Op::READ_WRITE, size);
    }

    /// @brief Convenience function for writing data using an iterator and receiving a reply.
    /// The buffer size is calculated from the given end iterator which must point into or just behind the buffer.
    /// Also works with BufferWriter, e.g. BufferWriter w(buffer); w.u8(10); buffer.write(w);
    /// @param end end pointer of data to write
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> writeRead(const uint8_t *end) {
        start(Op::READ_WRITE, end);
        return untilReadyOrDisabled();
    }

    bool startWriteRead(const uint8_t *end) {
        return start(Op::WRITE, end);
    }



    /// @brief Convenience function for reading data
    /// @param data data to read
    /// @param size size of data to read
    /// @param op additional operation flag
    /// @return use co_await on return value to await completion of read operation
    [[nodiscard]] AwaitableCoroutine readData(void *data, int size, Op op = Op::NONE) {
        size = std::clamp(size, 0, int(capacity_));

        // set size (needed for SPI but not for UART)
        size_ = size;

        // read
        start(Op(int(Op::READ) | int(op)));
        co_await untilReadyOrDisabled();

        // copy data
        auto src = data_;
        auto end = src + size;
        auto dst = reinterpret_cast<uint8_t *>(data);
        std::copy(src, end, dst);
    }


    /// @brief Convenience function for writing a value
    /// @tparam T value type
    /// @param value value to write
    /// @param op additional operation flag
    /// @return use co_await on return value to await completion of write operation
    template <typename T>
    [[nodiscard]] Awaitable<Events> writeValue(const T &value, Op op = Op::NONE) {
        if (sizeof(value) <= capacity_) {
            size_ = sizeof(value);
            *reinterpret_cast<T *>(data_) = value;
            start(Op(int(Op::WRITE) | int(op)));
        } else {
            // error: size of value too large
            assert(false);
        }
        return untilReadyOrDisabled();
    }

    /// @brief Convenience function for writing data
    /// @param data data to write
    /// @param size size of data to write
    /// @param op additional operation flag
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> writeData(const void *data, int size, Op op = Op::NONE) {
        size = std::clamp(size, 0, int(capacity_));
        size_ = size;

        // copy data
        auto src = reinterpret_cast<const uint8_t *>(data);
        auto end = src + size;
        auto dst = data_;
        std::copy(src, end, dst);

        // write
        start(Op(int(Op::WRITE) | int(op)));
        return untilReadyOrDisabled();
    }

    [[nodiscard]] Awaitable<Events> writeData(const Buffer &buffer, Op op = Op::NONE) {
        return writeData(buffer.data(), buffer.size(), op);
    }

    /// @brief Convenience function for writing array data. The array must support std::data() and std::size().
    /// @tparam T array type
    /// @param array array to write
    /// @param op additional operation flag
    /// @return use co_await on return value to await completion of write operation
    template <typename T> requires (ArrayConcept<T>)
    [[nodiscard]] Awaitable<Events> writeArray(const T &array, Op op = Op::NONE) {
        startWriteArray(array, op);
        /*auto src = std::data(array);
        auto count = std::size(array);
        unsigned size = p.headerSize + count * sizeof(*src);

        // cast buffer data to array element type
        auto data = data_ + p.headerSize;
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(data);

        if (size <= capacity_) {
            size_ = size;
            auto end = src + count;
            std::copy(src, end, dst);
            start(Op(int(Op::WRITE) | int(op)));
        } else {
            // error: size of data too large or negative
            assert(false);
        }*/
        return untilReadyOrDisabled();
    }

    template <typename T> requires (ArrayConcept<T>)
    bool startWriteArray(const T &array, Op op = Op::NONE) {
        auto src = std::data(array);
        auto count = std::size(array);
        unsigned size = count * sizeof(*src);

        // cast buffer data to array element type
        auto data = data_;
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(data);

        if (size > capacity_) {
            // error: size of data too large or negative
            assert(false);
            return false;
        }

        size_ = size;
        auto end = src + count;
        std::copy(src, end, dst);
        return start(Op(int(Op::WRITE) | int(op)));
    }

    /// @brief Convenience function for writing a string
    /// @param str string to write
    /// @param op additional operation flag
    /// @return use co_await on return value to await completion of write operation
    [[nodiscard]] Awaitable<Events> writeString(const String &str, Op op = Op::NONE) {
        return writeData(str.data(), str.size(), op);
    }

    /// @brief Generic write function for arrays implementing ArrayConcept but not StringConcept
    /// @param array array to write
    /// @param op optional additional operation
    template <typename T> requires (ArrayConcept<T> && !StringConcept<T>)
    [[nodiscard]] Awaitable<Events> write(const T &array, Op op = Op::NONE) {
        return writeArray(array, op);
    }

    /// @brief Generic write function for strings implementing StringConcept
    /// @param str string to write
    /// @param op optional additional operation
    template <typename T> requires (StringConcept<T>)
    [[nodiscard]] Awaitable<Events> write(const T &str, Op op = Op::NONE) {
        return writeString(str, op);
    }

    /// @brief Convenience function for sending an erase command e.g. to an SPI or I2C flash memory
    ///
    [[nodiscard]] Awaitable<Events> erase() {
        start(Op::ERASE);
        return untilReadyOrDisabled();
    }

    /// @brief Cancel the current transfer operation which means the buffer returns from BUSY to READY after a short amount of
    /// time. When the transfer got cancelled, size() returns zero. If the transfer complete successfully, size() is as
    /// if cancel() was not called.
    /// @return true if a transfer was cancelled
    virtual bool cancel() = 0;

    /// @brief Convenience function for acquiring a buffer, i.e. cancel if necessary and wait until ready
    ///
    [[nodiscard]] Awaitable<Events> acquire() {
        cancel();
        return untilReadyOrDisabled();
    }


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
    /// @brief Set success and keep buffer size.
    /// @param transferred
    void setSuccess() {
        error_ = {};
    }

    /// @brief Set success and number of transferred bytes.
    /// @param transferred
    void setSuccess(int transferred) {
        size_ = transferred;
        error_ = {};
    }

    /// @brief Set generic error using std::errc.
    /// @param error Error
    void setError(std::errc error) {
        size_ = 0;
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
        size_ = 0;
        error_ = {error, std::system_category()};
    }
#endif

    void setDisabled();
    void setReady();
    //void setReady(int transferred);
    void setBusy();

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


    // buffer data
    uint8_t *data_;

    // header data
    uint8_t *header_;

    // capacity of buffer (without header)
    uint32_t capacity_;

    // size of buffer (without header)
    uint32_t size_ = 0;

    // capacity of header
    uint8_t headerCapacity_;

    // device specific header type or size
    //uint8_t headerType_;

    // current state of the buffer
    State state_;

    // operation
    Op op_ = Op::NONE;

#ifdef NATIVE
    // outstanding steps necessary for the current transfer (fit into the alignment space after op_)
    uint8_t steps_;

    // result of last transfer operation
    std::error_code error_;
#else
    union {
        // result of last transfer operation
        uint8_t error_;

        // outstanding steps necessary for the current transfer (share space with error_)
        uint8_t steps_;
    };
#endif

    // tasks (waiting coroutines)
    CoroutineTaskList<Events> tasks_;
};
COCO_ENUM(Buffer::Events);
COCO_ENUM(Buffer::Op);

} // namespace coco
