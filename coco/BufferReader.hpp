#pragma once

#include "Buffer.hpp"
#include <coco/Array.hpp>
#include <coco/String.hpp>
#include <cstdint>


namespace coco {

class BufferReader {
public:
    BufferReader() : current(), end() {}

    /// @brief Constructor.
    /// @param begin Begin of data to read from
    /// @param end End of data to read from
    BufferReader(const uint8_t *begin, uint8_t *end) : current(begin), end(end) {}

    /// @brief Constructor.
    /// @param data Data to read from
    /// @param length Length of data to read from
    BufferReader(const uint8_t *data, int length) : current(data), end(data + length) {}

    /// @brief Constructor for buffer supporting std::data() and std::size().
    /// @tparam T Buffer type
    /// @param buffer Buffer to read from
    template <typename T>
    BufferReader(const T &buffer) : current(std::data(buffer)), end(std::data(buffer) + std::size(buffer)) {}

    /// @brief Set the reader to the given read position without changing end position.
    /// @param current Current read position
    void set(const uint8_t *current) {this->current = current;}

    /// @brief Assign the given data to the reader.
    /// @param data Data to read from
    /// @param length Length of data to read from
    void assign(const uint8_t *data, int length) {
        this->current = data;
        this->end = data + length;
    }

    /// @brief Assign the given buffer supporting std::data() and std::size().
    /// @tparam T Buffer type
    /// @param buffer Buffer to read from
    template <typename T>
    void assign(T &buffer) {
        this->current = std::data(buffer);
        this->end = this->current + std::size(buffer);
    }


// fixed size integer and enum

    int8_t i8() {
        int8_t value = this->current[0];
        ++this->current;
        return value;
    }

    int8_t peekI8() const {
        return this->current[0];
    }

    uint8_t u8() {
        uint8_t value = this->current[0];
        ++this->current;
        return value;
    }

    uint8_t peekU8() const {
        return this->current[0];
    }

    template <typename T>
    T e8() {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
        T value = T(this->current[0]);
        ++this->current;
        return value;
    }

    template <typename T>
    T peekE8() {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
        return T(this->current[0]);
    }

    int16_t i16L() {
        auto current = this->current;
        int16_t value = current[0] | (current[1] << 8);
        this->current += 2;
        return value;
    }

    int16_t i16B() {
        auto current = this->current;
        uint16_t value = (current[0] << 8) | current[1];
        this->current += 2;
        return value;
    }

    uint16_t u16L() {
        return i16L();
    }


    uint16_t u16B() {
        return i16B();
    }

    template <typename T>
    T e16L() {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
        return T(u16L());
    }

    template <typename T>
    T e16B() {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
        return T(u16B());
    }

    int32_t i32L() {
        auto current = this->current;
        int32_t value = current[0] | (current[1] << 8) | (current[2] << 16) | (current[3] << 24);
        this->current += 4;
        return value;
    }

    uint32_t i32B() {
        auto current = this->current;
        uint32_t value = (current[0] << 24) | (current[1] << 16) | (current[2] << 8) | current[3];
        this->current += 4;
        return value;
    }

    uint32_t u32L() {
        return i32L();
    }

    uint32_t u32B() {
        return i32B();
    }

    template <typename T>
    T e32L() {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint32_t>::value);
        return T(u32L());
    }

    template <typename T>
    T e32B() {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint32_t>::value);
        return T(u32B());
    }

    uint64_t u64L() {
        auto lo = uint64_t(u32L());
        return lo | (uint64_t(u32L()) << 32);
    }

    uint64_t u64B() {
        auto hi = uint64_t(u32B()) << 32;
        return hi | uint64_t(u32B());
    }


//  variable length integer

    /// @brief Variable length unsigned integer as used in Protocol Buffers.
    /// @tparam T Type to read
    /// @return Value read
    template <typename T>
    T uVar() {
        T value = 0;
        int shift = 0;
        for (int i = 0; i < 10; ++i) {
            uint8_t v = *this->current;
            ++this->current;
            value |= (v & 0x7f) << shift;
            if ((v & 0x80) == 0)
                break;
            shift += 7;
        }
        return value;
    }

    /// @brief Variable length signed integer as used in Protocol Buffers.
    /// @tparam T Type to read
    /// @return Value read
    template <typename T>
    T iVar() {
        auto value = uVar<std::make_unsigned<T>>();
        return (value >> 1) ^ (value & 1 ? ~T(0) : T(0));
    }


// float types

    float f32L() {
        union Value {
            int32_t i;
            float f;
        };
        Value v = {.i = i32L()};
        return v.f;
    }

    float f64L() {
        union Value {
            uint64_t i;
            double f;
        };
        Value v = {.i = u64L()};
        return v.f;
    }


// array of fixed size integer

    /// @brief Read a byte array of fixed length (e.g. array8<10>()).
    /// @tparam N Fixed length of the array
    /// @return array
    template <int N>
    Array<const uint8_t, N> array8() {
        auto ar = this->current;
        this->current += N;
        return Array<const uint8_t, N>(ar);
    }

    /// @brief Read a byte array of given length.
    /// @param length Length of the array
    /// @return array
    Array<const uint8_t> array8(int length) {
        auto ar = this->current;
        this->current += length;
        return Array<const uint8_t>(ar, length);
    }

    /// @brief Read the contents of an array supporting std::size() as 8 bit integers.
    /// @tparam T Array Type
    /// @param array Array to add
    template <typename T>
    void array8(const T &array) {
        auto current = this->current;
        int size = std::size(array);
        for (int i = 0; i < array.size(); ++i) {
            uint8_t value = current[0];
            array[i] = value;
            ++current;
        }
        this->current = current;
    }

    /*  template <typename T>
    void data8(T *data, int count) {
        for (int i = 0; i < count; ++i) {
            uint8_t value = this->current[i];
            data[i] = T(value);
        }
        this->current += count;
    }

    template <typename T>
    void data16L(T *data, int count) {
        for (int i = 0; i < count; ++i) {
            uint16_t value = this->current[i * 2] | (this->current[i * 2 + 1] << 8);
            data[i] = T(value);
        }
        this->current += count * 2;
    }*/

    /// @brief Read the contents of an array supporting std::size() as little endian 16 bit integers.
    /// @tparam T Array Type
    /// @param array Array to add
    template <typename T>
    void array16L(const T &array) {
        auto current = this->current;
        int size = std::size(array);
        for (int i = 0; i < array.size(); ++i) {
            uint16_t value = current[0] | (current[1] << 8);
            array[i] = value;
            current += 2;
        }
        this->current = current;
    }


// data

    /// @brief Read data.
    /// @param data Data to read
    /// @param size Size of data
    void data(uint8_t *data, int size) {
        auto end = this->current + size;
        std::copy(this->current, end, data);
        this->current = end;
    }

    /// @brief Read the data of a buffer
    /// @tparam T Buffer Type
    /// @param buffer Buffer
    template <typename T>
    void data(T &buffer) {
        data(std::data(buffer), std::size(buffer));
    }


// string

    /// @brief Read string until end of data.
    /// @return string
    String string() {
        auto str = this->current;
        this->current = this->end;
        return String(str, this->end - str);
    }

    /// @brief Read string with 8 bit length.
    /// @return string
    String string8() {
        int length = u8();
        auto str = this->current;
        this->current += length;
        return String(str, length);
    }

    /// @brief Read string with fixed length.
    /// @param length length of string
    /// @return String where trailing zeros are omitted
    String string(int length) {
        auto str = this->current;
        this->current += length;

        // determine acutal length
        int l = 0;
        while (l < length && str[l] != 0)
            ++l;
        return String(str, l);
    }

    /// @brief Read a string that represents a floating point number.
    /// For example "1.3", exponential notation (e.g. "1.3e5") not supported
    /// @return string containing a floating point number
    String floatString() {
        auto str = this->current;
        auto it = str;
        while (it < this->end) {
            if ((*it < '0' || *it > '9') && *it != '.')
                break;
            ++it;
        }
        return String(str, it - str);
    }


// other

    /// @brief Skip some bytes.
    /// @param n number of bytes to skip
    void skip(int n) {
        this->current += n;
    }

    /// @brief Skip white space.
    ///
    void skipSpace() {
        auto it = this->current;
        while (it < this->end && (*it == ' ' || *it == '\t'))
            ++it;
        this->current = it;
    }

    /// @brief Check if the reader is still valid, i.e. did not read past the end.
    /// @return true when before or at the end
    bool isValid() const {
        return this->current <= this->end;
    }

    /// @brief Check if we are at the end of the data.
    /// @return true when at or past the end
    bool atEnd() const {
        return this->current >= this->end;
    }

    /// @brief Get remaining number of bytes in the data.
    /// @return number of remaining bytes
    int remaining() const {
        return int(this->end - this->current);
    }

    /// @brief Cast to pointer.
    ///
    operator const uint8_t *() const {
        return this->current;
    }


    const uint8_t *current;
    const uint8_t *end;
};

} // namespace coco
