#pragma once

#include "Device.hpp"
#include <coco/Array.hpp>
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>
#include <coco/String.hpp>
#include <coco/StringConcept.hpp>


namespace coco {

/**
 * Buffer used for data transfer to/from hardware devices. Typically each device provides its own buffer
 * implementation. A Buffer has a capacity and a current size. For write operations the current size is used.
 * For read operations the size or capacity is used depending on the device type and implementation. For example
 * receiving data on a UART with timeout can use the capacity while receiving data on SPI uses the size.
 * A buffer can have a header which can contain separate data such as the address of a SPI/I2C flash
 * or the IP address of a UDP transfer. For example, when reading from an I2C flash, the header containing the
 * address gets written to the I2C bus before the actual data gets read into the buffer.
 *
 * ++++++++++++++**********************-----------------
 * ^             ^                     ^                ^
 * headerData()  data()/begin()        size()/end()     capacity()
 *
 * A data transfer can be started by start() which does not block. This internally starts for example a DMA transfer
 * and changes the state of the buffer to BUSY.
 *
 * States and transitions:
 *
 * DISABLED <----> READY --- start() --> BUSY
 *                   ^                     |
 *  	             |----- completion ----|
 *                   |----- cancel() ------|
 *
 * Use the convenience methods e.g. co_await buffer.read() or co_await buffer.write(size) to read or write data.
 * Use co_await buffer.untilReady() to wait until completion of a transfer.
 * Use co_await buffer.untilReadyOrDisabled() to wait until a buffer becomes ready after a transfer or disabled because the
 * device was closed.
 */
class Buffer {
public:
    enum class State {
        /**
         * Buffer is disabled because the owning device is e.g. not connected
         */
        DISABLED = 0,

        /**
         * Buffer is ready and a transfer can be started
         */
        READY = 1,

        /**
         * Transfer is in progress
         */
        BUSY = 2,
    };

    /// Event flags
    enum class Events {
        NONE = 0,

        /// device entered a state
        ENTER_DISABLED = 1,
        ENTER_READY = 1 << 1,
        ENTER_BUSY = 1 << 2,
        ENTER_ANY = ENTER_DISABLED | ENTER_READY | ENTER_BUSY,
    };

    enum class Op {
        NONE = 0,

        /**
         * Read operation
         */
        READ = 1 << 0,

        /**
         * Write operation
         */
        WRITE = 1 << 1,

        /**
         * SPI: Read and write simultaneously
         * UART/USB: Read after write (the reply to a command can be read into the same buffer)
         */
        READ_WRITE = READ | WRITE,

        /**
         * Erase e.g. a flash memory
         */
        ERASE = 1 << 2,

        CANCEL = 1 << 3,

        /**
         * Partial transfer, i.e. at least one more transfer will follow. If the underlying transport
         * protocol is packetized, only full packets may be written (e.g. 64 bytes for USB bulk) and no "end packet" is
         * generated.
         */
        PARTIAL = 1 << 4,

        /**
         * Whole transfer is a commmand, e.g. for SPI with command/data line
         */
        COMMAND = 1 << 5
    };

    /**
     * Constructor
     * @param buffer data
     * @param capacity buffer capacity
     * @param state initial state of the buffer
     */
    //Buffer(uint8_t *data, int capacity, State state) : p{data, uint32_t(capacity), 0, 0, state} {}
    //Buffer(uint8_t *data, int headerSize, int capacity, State state) : p{data, uint32_t(headerSize + capacity), uint32_t(headerSize), uint16_t(headerSize), state} {}
    Buffer(uint8_t *data, int capacity, State state)
        : p{data, uint32_t(capacity), 0, 0}, st(state) {}
    Buffer(uint8_t *data, int headerSize, int capacity, State state)
        : p{data, uint32_t(headerSize + capacity), uint32_t(headerSize), uint16_t(headerSize)}, st(state) {}
    Buffer(uint8_t *data, int capacity, Device::State state)
        : Buffer(data, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}
    Buffer(uint8_t *data, int headerSize, int capacity, Device::State state)
        : Buffer(data, headerSize, capacity, state <= Device::State::CLOSING ? State::DISABLED : State::READY) {}

    /**
     * Destructor. Do not destroy a buffer that is in BUSY state.
     */
    virtual ~Buffer() {}


// state
// -----

    /**
     * Get current state
     * @return state
     */
    State state() {return this->st.state;}

    /// Returns true if the device is disabled
    bool disabled() {return this->st.state == State::DISABLED;}

    /// Returns true if the device is ready
    bool ready() {return this->st.state == State::READY;}

    /// Returns true if the device is ready
    bool busy() {return this->st.state == State::BUSY;}

    /**
     * Wait until the buffer state changed, e.g. from BUSY to READY
     * @return use co_await on return value to await a state change
     */
    [[nodiscard]] Awaitable<Events> untilStateChanged() {return {this->st.tasks, Events::ENTER_ANY};}

    /**
     * Wait until the buffer is disabled. Does not wait when the device is already in DISABLED state.
     * @return use co_await on return value to wait until the buffer becomes disabled
     */
    [[nodiscard]] Awaitable<Events> untilDisabled() {
        if (this->st.state == State::DISABLED)
            return {};
        return {this->st.tasks, Events::ENTER_DISABLED};
    }

    /**
     * Wait unless the buffer is ready. Does not wait when the buffer is in READY state.
     *
     * Usage example:
     * co_await buffer.untilReady();
     * while (buffer.ready()) {
     *   // use buffer
     * }
     *
     * @return use co_await on return value to wait until the buffer becomes ready
     */
    [[nodiscard]] Awaitable<Events> untilReady() {
        if (this->st.state == State::READY)
            return {};
        return {this->st.tasks, Events::ENTER_READY};
    }

    /**
     * Wait unless the buffer is ready or disabled. Does not wait when the buffer is in READY or DISABLED state.
     *
     * Usage example:
     * co_await buffer.untilReadyOrDisabled();
     * if (!buffer.ready()) {
     *   // abort as buffer is disabled
     * }
     * // use buffer
     *
     * @return use co_await on return value to wait until the buffer becomes ready or disabled
     */
    [[nodiscard]] Awaitable<Events> untilReadyOrDisabled() {
        if (this->st.state == State::READY || this->st.state == State::DISABLED)
            return {};
        return {this->st.tasks, Events(int(Events::ENTER_READY) | int(Events::ENTER_DISABLED))};
    }


// header
// ------

    /**
        Get the size of the header
        @return size
    */
    int headerSize() const {return this->p.headerSize;}

    /**
        Set the size of the header
        @param size size of header
    */
    void headerResize(int size) {
        assert(uint32_t(size) <= this->p.capacity);
        this->p.headerSize = std::min(uint32_t(size), uint32_t(this->p.capacity));
    }

    /**
        Get header of the buffer
    */
    uint8_t *headerData() {return this->p.data;}
    const uint8_t *headerData() const {return this->p.data;}

    /**
        Set the header. Some buffer implementations modify the header during transfer operations (e.g. SPI).
        Note that the buffer gets cleared.
        @return true if successful, false on error
    */
    void setHeader(const uint8_t *data, int size) {
        auto &p = this->p;
        if (unsigned(size) > p.capacity) {
            size = size > 0 ? p.capacity : 0;
            //assert(false);
        }
        p.headerSize = size;
        //p.size = size;
        auto src = data;
        auto end = data + size;
        auto dst = p.data;
        while (src != end) {
            *dst = *src;
            ++dst;
            ++src;
        }
    }

    /**
        Convenience function for setting the header to some value, e.g. setHeader<uint32_t>(50); or setHeader(header);
        Note that the buffer gets cleared.
    */
    template <typename T>
    void setHeader(const T &header) {
        auto &p = this->p;
        unsigned size = sizeof(T);
        if (size > p.capacity) {
            assert(false);
            return;
        }
        p.headerSize = size;
        //p.size = size;
        *reinterpret_cast<T *>(p.data) = header;
    }

    /**
        Convenience function for setting the header to some value, e.g. setHeader<uint32_t>(50); or setHeader(header);
        Note that the buffer gets cleared.
    */
    template <typename T> requires (ArrayConcept<T>)
    void setHeader(const T &array) {
        auto &p = this->p;
        auto src = std::data(array);
        auto count = std::size(array);
        unsigned size = count * sizeof(*src);
        if (size > p.capacity) {
            assert(false);
            return;
        }

        // cast buffer data to array element type
        auto data = p.data;
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(data);

        p.headerSize = size;
        //p.size = size;
        auto end = src + count;
        std::copy(src, end, dst);
    }

    /**
        Clear the header.
        Note that the buffer gets cleared, too.
        @return true if successful, false on error
    */
    void clearHeader() {
        auto &p = this->p;
        p.headerSize = 0;
        //p.size = 0;
    }

    /**
        Get the header
        @return actual size of header that was copied
    */
    int getHeader(uint8_t *data, int size) {
        auto &p = this->p;
        if (unsigned(size) > p.headerSize) {
            size = size > 0 ? p.headerSize : 0;
            //assert(false);
        }
        uint8_t *src = p.data - size;
        while (src != p.data) {
            *data = *src;
            ++src;
            ++data;
        }
        return size;
    }

    /**
        Get the header of a given type
        @tparam T type of header
    */
    template <typename T>
    void getHeader(T &header) {
        auto &p = this->p;
        auto size = sizeof(T);
        if (unsigned(size) > p.headerSize) {
            assert(false);
            return;
        }
        uint8_t *src = p.data - size;
        auto dst = reinterpret_cast<uint8_t *>(&header);
        while (src != p.data) {
            *dst = *src;
            ++src;
            ++dst;
        }
    }

    /**
        Get the header as a reference to the given type, e.g. header = getHeader<uint32_t>();
        @tparam T type
        @return reference to the type
    */
    template <typename T>
    T &header() {
        assert(sizeof(T) <= this->p.headerSize);
        return *reinterpret_cast<T *>(this->p.data);
    }


// data
// ----

    /**
     * Get the current size of the buffer
     * @return size
     */
    int size() const {return this->p.size - this->p.headerSize;}

    /**
     * Set the current size of the buffer
     * @param size size of buffer, gets clamped to the capacity minus the header size
     */
    void resize(int size) {
        size += this->p.headerSize;
        assert(unsigned(size) <= this->p.capacity);
        this->p.size = std::min(uint32_t(size), this->p.capacity);
    }

    /**
     * Clear the buffer (equivalent to resize(0))
     */
    void clear() {
        this->p.size = this->p.headerSize;
    }

    /**
     * Get the capacity of the buffer
     * @return size
     */
    int capacity() const {return this->p.capacity - this->p.headerSize;}

    /**
     * Get data of the buffer
     */
    uint8_t *data() {return this->p.data + this->p.headerSize;}
    const uint8_t *data() const {return this->p.data + this->p.headerSize;}

    /**
     * Get begin iterator
     */
    uint8_t *begin() {return this->p.data + this->p.headerSize;}
    const uint8_t *begin() const {return this->p.data + this->p.headerSize;}

    /**
     * Get end iterator
     */
    uint8_t *end() {return this->p.data + this->p.size;}
    const uint8_t *end() const {return this->p.data + this->p.size;}

    /**
     * Get whole buffer as array
     */
    Array<uint8_t> all() {return {this->p.data + this->p.headerSize, int(this->p.capacity - this->p.headerSize)};}

    /**
     * Index operator
     * @param index index between (-header capacity) and (buffer capacity - 1)
     */
    uint8_t &operator [](int index) {
        index += this->p.headerSize;
        assert(unsigned(index) < this->p.capacity);
        auto data = this->p.data;
        return data[index];
    }
    uint8_t operator [](int index) const {
        index += this->p.headerSize;
        assert(unsigned(index) < this->p.capacity);
        auto data = this->p.data;
        return data[index];
    }

    /**
     * Get the data of the buffer as a value of given type
     * @tparam T value type
     */
    template <typename T>
    T &value() {
        assert(sizeof(T) <= this->p.capacity - this->p.headerSize);
        auto data = this->p.data + this->p.headerSize;
        return *reinterpret_cast<T *>(data);
    }

    /**
     * Get the data of the buffer as a pointer to given type
     * @tparam T type
     */
    template <typename T>
    T *pointer() {
        auto data = this->p.data + this->p.headerSize;
        return reinterpret_cast<T *>(data);
    }

    /**
     * Get the current data of the buffer as an array of given type
     * @tparam T array element type
     */
    template <typename T>
    Array<T> array() {
        auto data = this->p.data + this->p.headerSize;
        auto size = this->p.size - this->p.headerSize;
        return {reinterpret_cast<T *>(data), int(size / int(sizeof(T)))};
    }

    /**
     * Get the current data of the buffer as a string
     */
    String string() {
        auto data = this->p.data + this->p.headerSize;
        auto size = this->p.size - this->p.headerSize;
        return String(data, size);
    }


// transfer
// --------

    /**
        Start transfer of the buffer if it is in READY state and set it to BUSY state if the operation does not
        complete immediately. If the buffer completes immediately, it stays in READY state. Depending on the underlying device
        and transfer direction, either the whole buffer gets transferred or only the current size.
        @param op operation flags such as READ or WRITE
        @return true if successful, false on error e.g. when the state is DISABLED or BUSY. Calling start() on a busy
        buffer is considered a bug
    */
    virtual bool start(Op op) = 0;

    /**
        Convenience method for start() that sets the current buffer size
        @param size size of data to transfer
        @param op operation flags such as READ or WRITE
        @return true if successful, false on error
    */
    bool start(int size, Op op) {
        size += this->p.headerSize;
        if (unsigned(size) > this->p.capacity) {
            size = size > 0 ? this->p.capacity : 0;
            //assert(false);
        }
        this->p.size = size;
        return start(op);
    }

    /**
        Convenience method for start() that sets the current buffer size
        @param end end iterator pointing behind the end of the data to be transferred in the buffer
        @param op operation flags such as READ or WRITE
        @return true if successful, false on error
    */
    bool start(const uint8_t *end, Op op) {
        unsigned size = end - this->p.data;
        if (unsigned(size) > this->p.capacity) {
            size = size > 0 ? this->p.capacity : 0;
            //assert(false);
        }
        this->p.size = size;
        return start(op);
    }

    /**
        Result of the last transfer operation
    */
    //virtual Result result() = 0;

    /**
        Convenience function for receiving data of size up to size(), e.g. radio, UART or USB bulk
        @param op additional operation flag
        @return use co_await on return value to await completion of receive operation
    */
    [[nodiscard]] Awaitable<Events> read(Op op = Op::NONE) {
        start(Op(int(Op::READ) | int(op)));
        return untilReadyOrDisabled();
    }

    bool startRead(Op op = Op::NONE) {
        return start(Op(int(Op::READ) | int(op)));
    }

    /**
        Convenience function for initiating a read operation of given size e.g. from file, I2C or USB control
        @param size size to read
        @param op additional operation flag
        @return use co_await on return value to await completion of read operation
    */
    [[nodiscard]] Awaitable<Events> read(int size, Op op = Op::NONE) {
        start(size, Op(int(Op::READ) | int(op)));
        return untilReadyOrDisabled();
    }

    bool startRead(int size, Op op = Op::NONE) {
        return start(size, Op(int(Op::READ) | int(op)));
    }

    /**
        Convenience function for reading data
        @param data data to write
        @param size size of data to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    [[nodiscard]] AwaitableCoroutine readData(void *data, int size, Op op = Op::NONE) {
        auto &p = this->p;
        int headerSize = p.headerSize;
        unsigned capacity = p.capacity - headerSize;

        // clamp size to capacity
        if (unsigned(size) > capacity) {
            size = size > 0 ? capacity : 0;
            //assert(false);
        }
        p.size = headerSize + size;

        // read
        start(Op(int(Op::READ) | int(op)));
        co_await untilReadyOrDisabled();

        // copy data
        auto src = p.data + headerSize;
        auto end = src + size;
        auto dst = reinterpret_cast<uint8_t *>(data);
        std::copy(src, end, dst);
    }


    /**
        Convenience function for writing the whole buffer
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    [[nodiscard]] Awaitable<Events> write(Op op = Op::NONE) {
        start(Op(int(Op::WRITE) | int(op)));
        return untilReadyOrDisabled();
    }

    bool startWrite(Op op = Op::NONE) {
        return start(Op(int(Op::WRITE) | int(op)));
    }

    /**
        Convenience function for writing data
        @param size size of data to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    [[nodiscard]] Awaitable<Events> write(int size, Op op = Op::NONE) {
        start(size, Op(int(Op::WRITE) | int(op)));
        return untilReadyOrDisabled();
    }

    bool startWrite(int size, Op op = Op::NONE) {
        return start(size, Op(int(Op::WRITE) | int(op)));
    }

    /**
        Convenience function for writing data when using BufferWriter, e.g. BufferWriter w(buffer); w.u8(10); buffer.write(w);
        @param end end pointer of data to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    [[nodiscard]] Awaitable<Events> write(const uint8_t *end, Op op = Op::NONE) {
        start(end, Op(int(Op::WRITE) | int(op)));
        return untilReadyOrDisabled();
    }

    bool startWrite(const uint8_t *end, Op op = Op::NONE) {
        return start(end, Op(int(Op::WRITE) | int(op)));
    }

    /**
        Convenience function for writing a value
        @tparam T value type
        @param value value to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    template <typename T>
    [[nodiscard]] Awaitable<Events> writeValue(const T &value, Op op = Op::NONE) {
        auto &p = this->p;
        unsigned size = p.headerSize + sizeof(value);
        if (size <= p.capacity) {
            p.size = size;
            *reinterpret_cast<T *>(p.data + p.headerSize) = value;
            start(Op(int(Op::WRITE) | int(op)));
        } else {
            // error: size of value too large
            assert(false);
        }
        return untilReadyOrDisabled();
    }

    /**
        Convenience function for writing data
        @param data data to write
        @param size size of data to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    [[nodiscard]] Awaitable<Events> writeData(const void *data, int size, Op op = Op::NONE) {
        auto &p = this->p;
        int headerSize = p.headerSize;
        unsigned capacity = p.capacity - headerSize;

        // clamp size to capacity
        if (unsigned(size) > capacity) {
            size = size > 0 ? capacity : 0;
            //assert(false);
        }
        p.size = headerSize + size;

        // copy data
        auto src = reinterpret_cast<const uint8_t *>(data);
        auto end = src + size;
        auto dst = p.data + headerSize;
        std::copy(src, end, dst);

        // write
        start(Op(int(Op::WRITE) | int(op)));
        return untilReadyOrDisabled();
    }

    [[nodiscard]] Awaitable<Events> writeData(const Buffer &buffer, Op op = Op::NONE) {
        return writeData(buffer.data(), buffer.size(), op);
    }

    /**
        Convenience function for writing array data. The array must support std::data() and std::size().
        @tparam T array type
        @param array array to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
    template <typename T> requires (ArrayConcept<T>)
    [[nodiscard]] Awaitable<Events> writeArray(const T &array, Op op = Op::NONE) {
        startWriteArray(array, op);
        /*auto src = std::data(array);
        auto count = std::size(array);
        unsigned size = this->p.headerSize + count * sizeof(*src);

        // cast buffer data to array element type
        auto data = this->p.data + this->p.headerSize;
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(data);

        if (size <= this->p.capacity) {
            this->p.size = size;
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
        unsigned size = this->p.headerSize + count * sizeof(*src);

        // cast buffer data to array element type
        auto data = this->p.data + this->p.headerSize;
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(data);

        if (size > this->p.capacity) {
            // error: size of data too large or negative
            assert(false);
            return false;
        }

        this->p.size = size;
        auto end = src + count;
        std::copy(src, end, dst);
        return start(Op(int(Op::WRITE) | int(op)));
    }

    /**
        Convenience function for writing a string
        @param str string to write
        @param op additional operation flag
        @return use co_await on return value to await completion of write operation
    */
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

    /// @brief Generic write function for strins implementing StringConcept
    /// @param str string to write
    /// @param op optional additional operation
    template <typename T> requires (StringConcept<T>)
    [[nodiscard]] Awaitable<Events> write(const T &str, Op op = Op::NONE) {
        return writeString(str, op);
    }

    /**
        Convenience function for sending an erase command e.g. to an SPI or I2C flash memory
    */
    [[nodiscard]] Awaitable<Events> erase() {
        start(Op::ERASE);
        return untilReadyOrDisabled();
    }

    /**
        Cancel the current transfer operation which means the buffer returns from BUSY to READY after a short amount of
        time. When the transfer got cancelled, size() returns zero. If the transfer complete successfully, size() is as
        if cancel() was not called.
        @return true if a transfer was cancelled
    */
    virtual bool cancel() = 0;

    /**
        Convenience function for acquiring a buffer, i.e. cancel if necessary and wait until ready
    */
    [[nodiscard]] Awaitable<Events> acquire() {
        cancel();
        return untilReadyOrDisabled();
    }

protected:
    void setDisabled();
    void setReady();
    void setReady(int transferred);
    //void setReady(Device::State state, int transferred)
    void setBusy();

    // properties
    struct Properties {
        uint8_t *data;
        uint32_t capacity;
        uint32_t size;
        uint16_t headerSize;
    };
    Properties p;

    // state and tasks (waiting coroutines)
    StateTasks<State, Events> st;
};
COCO_ENUM(Buffer::Events);
COCO_ENUM(Buffer::Op);

} // namespace coco
