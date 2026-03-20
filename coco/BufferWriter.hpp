#pragma once

#include "Buffer.hpp"
#include "DataBuffer.hpp"
#include <coco/Array.hpp>
#include <coco/bits.hpp>
#include <coco/String.hpp>
#include <coco/StringBuffer.hpp>
#include <cstdint>


namespace coco {

/// @brief Helper class for writing data into a buffer
/// @tparam B Buffer reference type, e.g. Buffer &
template <typename B>
class DataWriter {
public:
    DataWriter(B bufferReference) : b_(bufferReference) {}

// fixed size integer and enum

    void u8(uint8_t value) {
        b_.append(value);
    }

    void i8(int8_t value) {
        b_.append(value);
    }

    template <typename T>
    void e8(T value) {
        static_assert(std::is_same<typename std::underlying_type<T>::type, uint8_t>::value);
        b_.append(uint8_t(value));
    }

    void u16L(uint16_t value) {
        if constexpr (std::endian::native == std::endian::little) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 2);
        } else {
            uint16_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 2);
        }
    }

    void u16B(uint16_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 2);
        } else {
            uint16_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 2);
        }
    }

    void i16L(int16_t value) {
        u16L(value);
    }

    void i16B(int16_t value) {
        u16B(value);
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
        if constexpr (std::endian::native == std::endian::little) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 3);
        } else {
            uint32_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 3);
        }
    }

    /// @brief Write a 24 bit integer in big endian format. Useful for writing the address of a SPI/I2C flash.
    /// @param value Value to write
    void u24B(uint32_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            b_.append(reinterpret_cast<uint8_t *>(&value) + 1, 3);
        } else {
            uint32_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v) + 1, 3);
        }
    }

    void u32L(uint32_t value) {
        if constexpr (std::endian::native == std::endian::little) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 4);
        } else {
            uint32_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 4);
        }
    }

    void u32B(uint32_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 4);
        } else {
            uint32_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 4);
        }
    }

    void i32L(int32_t value) {
        u32L(value);
    }

    void i32B(int32_t value) {
        u32B(value);
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

    void u64L(uint64_t value) {
        if constexpr (std::endian::native == std::endian::little) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 8);
        } else {
            uint64_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 8);
        }
    }

    void u64B(uint64_t value) {
        if constexpr (std::endian::native == std::endian::big) {
            b_.append(reinterpret_cast<uint8_t *>(&value), 8);
        } else {
            uint64_t v = byteswap(value);
            b_.append(reinterpret_cast<uint8_t *>(&v), 8);
        }
    }

    void i64L(int64_t value) {
        u64L(value);
    }

    void i64B(int64_t value) {
        u64B(value);
    }

//  variable length integer

    /// @brief Variable length unsigned integer as used in Protocol Buffers
    /// @tparam T Type to write
    /// @param value Value to write
    template <typename T>
    void uVar(T value) {
        auto v = std::make_unsigned_t<T>(value);
        while (v >= 0x80) {
            b_.append(uint8_t(v | 0x80));
            v >>= 7;
        }
        b_.append(uint8_t(v));
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
        for (auto &e : array) {
            u8(uint8_t(e));
        }
    }

    /// @brief Write the contents of an array supporting std::size() as little endian 16 bit integers
    /// @tparam T Array Type
    /// @param array Array to write
    template <typename T>
    void array16L(const T &array) {
        for (auto &e : array) {
            u16L(uint16_t(e));
        }
    }

    /// @brief Write the contents of an array supporting std::size() as big endian 16 bit integers
    /// @tparam T Array Type
    /// @param array Array to write
    template <typename T>
    void array16B(const T &array) {
        for (auto &e : array) {
            u16B(uint16_t(e));
        }
    }

// data

    /// @brief Write data
    /// @param data Data to write
    /// @param size Size of data
    template <typename I> requires (std::input_iterator<I> && ByteConcept<std::iter_value_t<I>>)
    void data(I data, int size) {
        b_.append(data, size);
    }

// string

    /// @brief Write a string without length.
    /// @param str String to add
    void string(const String &str) {
        b_.append(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }

    /// @brief Write a padded string.
    /// @param str String to add
    /// @param size Size of field to be filled with the string, gets padded with zeros if the string is shorter and cut off if the string is longer
    void string(const String &str, int size) {
        int l = std::min(str.size(), size);
        b_.append(reinterpret_cast<const uint8_t *>(str.data()), l);
        fill(size - l);
    }

    /// @brief Write string with preceding 8 bit length.
    /// @param str String to add
    void string8(const String &str) {
        u8(str.size());
        b_.append(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }


// stream operators

    /// @brief Write a single character.
    /// @param ch Character to add
    DataWriter &operator <<(char ch) {
        u8(ch);
        return *this;
    }

    /// @brief Write a string without length.
    /// @param str String to add
    DataWriter &operator <<(const String &str) {
        string(str);
        return *this;
    }

// other

    /// @brief Fill bytes
    /// @param n Number of bytes
    /// @param value Fill value
    void fill(int n, uint8_t value = 0) {
        for (int i = 0; i < n; ++i)
            b_.append(value);
    }

    /// @brief Get remaining number of bytes in the data
    /// @return Number of remaining bytes
    int remaining() const {
        return b_.remaining();
    }

    /// @brief Cast to pointer, e.g. for buffer.write()/send()
    ///
    auto current() {
        return b_.end();
    }

    B b_;
};

using BufferWriter = DataWriter<Buffer &>;

} // namespace coco
