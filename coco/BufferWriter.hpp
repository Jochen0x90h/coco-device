#pragma once

#include "Buffer.hpp"
#include "DataBuffer.hpp"
#include <coco/Array.hpp>
#include <coco/String.hpp>
#include <coco/StringBuffer.hpp>
#include <cstdint>


namespace coco {

/// @brief Helper class for writing data into a buffer
///
/// Note that there is no overflow checking. Make sure that there is enough space left before writing data
class BufferWriter {
public:
    BufferWriter() : current(), end() {}

    /// @brief Constructor
    /// @param begin Begin of data to write to
    /// @param end End of data to write to
    BufferWriter(uint8_t *buffer, uint8_t *end) : current(buffer), end(end) {}
    BufferWriter(char *buffer, char *end) : current((uint8_t *)buffer), end((uint8_t *)end) {}

    /// @brief Constructor
    /// @param data Data to write to
    /// @param length Length of data to write to
    BufferWriter(uint8_t *buffer, int length) : current(buffer), end(buffer + length) {}
    BufferWriter(char *buffer, int length) : current((uint8_t *)buffer), end((uint8_t *)(buffer + length)) {}

    /// @brief Constructor for buffer supporting std::data() and std::size()
    /// @tparam T Buffer type
    /// @param buffer Buffer to write to
    template <typename T>
    BufferWriter(T &buffer) : current(std::data(buffer)), end(std::data(buffer) + std::size(buffer)) {}
    template <typename T>
    BufferWriter(T &&buffer) : current(std::data(buffer)), end(std::data(buffer) + std::size(buffer)) {}

    /// @brief Set the writer to the given write position without changing end position
    /// @param current current write position
    void set(uint8_t *current) {this->current = current;}

    /// @brief Assign the given buffer to the writer
    /// @param data Data to write to
    /// @param length Length of data to write to
    void assign(uint8_t *data, int length) {
        this->current = data;
        this->end = data + length;
    }

    /// @brief Assign the given buffer supporting std::data() and std::size()
    /// @tparam T Buffer type
    /// @param buffer Buffer to write to
    template <typename T>
    void assign(T &buffer) {
        this->current = std::data(buffer);
        this->end = this->current + std::size(buffer);
    }
    template <typename T>
    void assign(T &&buffer) {
        this->current = std::data(buffer);
        this->end = this->current + std::size(buffer);
    }


// fixed size integer and enum

    void i8(int8_t value) {
        this->current[0] = value;
        ++this->current;
    }

    void u8(uint8_t value) {
        this->current[0] = value;
        ++this->current;
    }

    template <typename T>
    void e8(T value) {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
        this->current[0] = uint8_t(value);
        ++this->current;
    }

    void i16L(int16_t value) {
        auto current = this->current;
        current[0] = value;
        current[1] = value >> 8;
        this->current += 2;
    }

    void i16B(int16_t value) {
        auto current = this->current;
        current[0] = value >> 8;
        current[1] = value;
        this->current += 2;
    }

    void u16L(uint16_t value) {
        i16L(value);
    }

    void u16B(uint16_t value) {
        i16B(value);
    }

    template <typename T>
    void e16L(T value) {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
        u16L(uint16_t(value));
    }

    template <typename T>
    void e16B(T value) {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint16_t>::value);
        u16B(uint16_t(value));
    }

    void u24L(uint32_t value) {
        auto current = this->current;
        current[0] = value;
        current[1] = value >> 8;
        current[2] = value >> 16;
        this->current += 3;
    }

    /// @brief Write a 24 bit integer in big endian format. Useful for writing the address of a SPI/I2C flash.
    /// @param value Value to write
    void u24B(uint32_t value) {
        auto current = this->current;
        current[0] = value >> 16;
        current[1] = value >> 8;
        current[2] = value;
        this->current += 3;
    }

    void i32L(int32_t value) {
        auto current = this->current;
        current[0] = value;
        current[1] = value >> 8;
        current[2] = value >> 16;
        current[3] = value >> 24;
        this->current += 4;
    }

    void i32B(int32_t value) {
        auto current = this->current;
        current[0] = value >> 24;
        current[1] = value >> 16;
        current[2] = value >> 8;
        current[3] = value;
        this->current += 4;
    }

    void u32L(uint32_t value) {
        i32L(value);
    }

    void u32B(uint32_t value) {
        i32B(value);
    }

    template <typename T>
    void e32L(T value) {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint32_t>::value);
        u32L(uint32_t(value));
    }

    template <typename T>
    void e32B(T value) {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint32_t>::value);
        u32B(uint32_t(value));
    }

    void i64L(int64_t value) {
        i32L(value);
        i32L(value >> 32);
    }

    void i64B(int64_t value) {
        i32B(value >> 32);
        i32B(value);
    }

    void u64L(uint64_t value) {
        i32L(value);
        i32L(value >> 32);
    }

    void u64B(uint64_t value) {
        i32B(value >> 32);
        i32B(value);
    }


//  variable length integer

    /// @brief Variable length unsigned integer as used in Protocol Buffers
    /// @tparam T Type to write
    /// @param value Value to write
    template <typename T>
    void uVar(T value) {
        auto v = std::make_unsigned_t<T>(value);
        while (v >= 0x80) {
            *this->current = static_cast<uint8_t>(v | 0x80);
            v >>= 7;
            ++this->current;
        }
        *this->current = static_cast<uint8_t>(v);
        ++this->current;
    }

    /// @brief Variable length unsigned integer as used in Protocol Buffers
    /// @tparam T Type to write
    /// @param value Value to write
    template <typename T>
    void iVar(T value) {
        uVar((value << 1) ^ (value >> (sizeof(T) * 8 - 1)));
    }

// float types

    void f32L(float value) {
        union Value {
            uint32_t i;
            float f;
        };
        Value v = {.f = value};
        u32L(v.i);
    }

    void f64L(double value) {
        union Value {
            int64_t i;
            double f;
        };
        Value v = {.f = value};
        u64L(v.i);
    }


// array of fixed size integer

    /// @brief Write the contents of an array supporting std::size() as 8 bit integers
    /// @tparam T Array Type
    /// @param array Array to write
    template <typename T>
    void array8(const T &array) {
        auto current = this->current;
        int size = std::size(array);
        for (int i = 0; i < array.size(); ++i) {
            uint8_t value = current[0];
            array[i] = value;;
            ++current;
        }
        this->current = current;
    }

    /// @brief Write the contents of an array supporting std::size() as little endian 16 bit integers
    /// @tparam T Array Type
    /// @param array Array to write
    template <typename T>
    void array16L(const T &array) {
        auto current = this->current;
        int size = std::size(array);
        for (int i = 0; i < array.size(); ++i) {
            uint16_t value = uint16_t(array[i]);
            current[0] = value;
            current[1] = value >> 8;
            current += 2;
        }
        this->current = current;
    }


// native value and array

    /// @brief Write a value in native byte order and assuming correct alignment
    /// @tparam T Value type
    /// @param value value to write
    template <typename T>
    void value(const T &value) {
        *reinterpret_cast<T *>(this->current) = value;
        this->current += sizeof(value);
    }

    /// @brief Write the contents of an array in native byte order and assuming correct alignment
    /// @tparam T Array type
    /// @param array array to write
    template <typename T>
    void array(const T &array) {
        auto src = std::data(array);

        // create dst pointer with same type as src, but not const
        auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(this->current);

        auto count = std::size(array);
        auto size = count * sizeof(*src);

        auto end = src + count;
        std::copy(src, end, dst);
        this->current += size;
    }


// data

    /// @brief Write data
    /// @param data Data to write
    /// @param size Size of data
    void data(const uint8_t *data, int size) {
        auto current = this->current;
        std::copy(data, data + size, current);
        this->current = current + size;
    }

    /// @brief Write the header of a coco::Buffer
    /// @param buffer Buffer
    void header(Buffer &buffer, int size) {
        data(buffer.headerData(), size);
    }

    /// @brief Write the data of a buffer
    /// @tparam T Buffer Type
    /// @param buffer Buffer
    template <typename T>
    void data(T &buffer) {
        data(std::data(buffer), std::size(buffer));
    }


// string

    /// @brief Write a string without length.
    /// @param str String to add
    void string(const String &str) {
        data(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }

    /// @brief Write a padded string.
    /// @param str String to add
    /// @param size Size of field to be filled with the string, gets padded with zeros if the string is shorter
    void string(const String &str, int size) {
        int l = std::min(str.size(), size);
        data(reinterpret_cast<const uint8_t *>(str.data()), l);
        fill(size - l);
    }

    /// @brief Write string with preceding 8 bit length
    /// @param str String to add
    void string8(const String &str) {
        u8(str.size());
        data(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }


// stream operators

    /// @brief Write a single character.
    /// @param ch Character to add
    BufferWriter &operator <<(char ch) {
        u8(ch);
        return *this;
    }

    /// @brief Write a string without length.
    /// @param str String to add
    BufferWriter &operator <<(const String &str) {
        string(str);
        return *this;
    }

    /// @brief Stream a string concept into the writer (C-string, coco::StringBuffer, std::string).
    /// @tparam T String type
    /// @param str String to add
    /*template <typename T> requires (StringConcept<T>)
    BufferWriter &operator <<(const T &str) {
        string(String(str));
        return *this;
    }*/


// other

    /// @brief Skip bytes (does not modify the skipped bytes)
    /// @param n Number of bytes
    void skip(int n) {
        this->current += n;
    }

    /// @brief Fill bytes
    /// @param n Number of bytes
    /// @param value Fill value
    void fill(int n, int value = 0) {
        auto end = this->current + n;
        for (auto it = this->current; it != end; ++it)
            *it = value;
        this->current = end;
    }

    /// @brief Check if the writer is still valid, i.e. did not write past the end
    /// @return True when before or at the end
    bool isValid() const {
        return this->current <= this->end;
    }

    /// @brief Check if we are at the end of the data
    /// @return True when at or past the end
    bool atEnd() const {
        return this->current >= this->end;
    }

    /// @brief Get remaining number of bytes in the data
    /// @return Number of remaining bytes
    int remaining() const {
        return int(this->end - this->current);
    }

    /// @brief Cast to pointer, e.g. for buffer.write()/send()
    ///
    operator uint8_t *() const {
        return this->current;
    }


    uint8_t *current;
    uint8_t *end;
};


} // namespace coco
