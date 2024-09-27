#pragma once

#include "Buffer.hpp"
#include "DataBuffer.hpp"
#include <coco/Array.hpp>
#include <coco/String.hpp>
#include <coco/StringBuffer.hpp>
#include <cstdint>


namespace coco {

class BufferWriter {
public:
	BufferWriter() : current(), end() {}
	BufferWriter(uint8_t *buffer, uint8_t *end) : current(buffer), end(end) {}
	BufferWriter(uint8_t *buffer, int length) : current(buffer), end(buffer + length) {}

	/**
	 * Constructor for buffer supporting std::data() and std::size()
	 */
	template <typename T>
	BufferWriter(T &buffer) : current(std::data(buffer)), end(std::data(buffer) + std::size(buffer)) {}
	template <typename T>
	BufferWriter(T &&buffer) : current(std::data(buffer)), end(std::data(buffer) + std::size(buffer)) {}

	/**
	 * Reset the writer to the given current position without changing end position
	 */
	void reset(uint8_t *current) {this->current = current;}

	/**
	 * Assign the given data to the reader
	 */
	void assign(uint8_t *data, int length) {
		this->current = data;
		this->end = data + length;
	}

	/**
	 * Assign the given buffer supporting std::data() and std::size()
	 */
	template <typename T>
	void assign(T &buffer) {
		this->current = std::data(buffer);
		this->end = this->current + std::size(buffer);
	}

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

	/**
	 * Write a 24 bit integer in big endian format. Useful for writing the address of a SPI/I2C flash.
	 */
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

	void f32L(float value) {
		union Value {
			uint32_t i;
			float f;
		};
		Value v = {.f = value};
		u32L(v.i);
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

	void f64L(double value) {
		union Value {
			int64_t i;
			double f;
		};
		Value v = {.f = value};
		u64L(v.i);
	}

	/**
	 * Add the contents of an array as little endian uint16_t
	 */
	template <typename T, int N>
	void array16L(Array<T, N> array) {
		auto current = this->current;
		for (int i = 0; i < array.size(); ++i) {
			uint16_t value = uint16_t(array[i]);
			current[i * 2] = value;
			current[i * 2 + 1] = value >> 8;
		}
		this->current += array.size() * 2;
	}

	/**
	 * Add a value in native byte order and assuming correct alignment
	 */
	template <typename T>
	void value(const T &value) {
		if (sizeof(value) <= remaining()) {
			*reinterpret_cast<T *>(this->current) = value;
			this->current += sizeof(value);
		} else {
			assert(false);
		}
	}

	/**
	 * Add the contents of an array in native byte order and assuming correct alignment
	 */
	template <typename T>
	void array(const T &array) {
		auto src = std::data(array);
		auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(this->current);

		auto count = std::size(array);
		auto size = count * sizeof(*src);
		if (size <= remaining()) {
			auto end = src + count;
			std::copy(src, end, dst);
			this->current += size;
		} else {
			assert(false);
		}
	}

	/**
	 * Add data
	 */
	void data(const uint8_t *data, int size) {
		if (size >= 0 && size <= remaining()) {
			std::copy(data, data + size, this->current);
			this->current += size;
		} else {
			assert(false);
		}
	}

	void header(Buffer &buffer) {
		data(buffer.headerData(), buffer.headerSize());
	}

	void data(Buffer &buffer) {
		data(buffer.data(), buffer.size());
	}

	/**
	 * Add string without length
	 * @param str string to add
	 */
	void string(const String &str) {
		data(reinterpret_cast<const uint8_t *>(str.data()), str.size());
	}

	/**
	 * Add padded string
	 * @param str string to add
	 * @param size size of field to be filled with the string, gets padded with zeros if the string is shorter
	 */
	void string(const String &str, int size) {
		int l = std::min(str.size(), size);
		data(reinterpret_cast<const uint8_t *>(str.data()), l);
		fill(size - l);
	}

	/**
	 * Add string with preceding 8 bit length
	 * @param str string to add
	 */
	void string8(const String &str) {
		u8(str.size());
		data(reinterpret_cast<const uint8_t *>(str.data()), str.size());
	}

	/**
	 * Stream a single character into the writer
	 */
	BufferWriter &operator <<(char ch) {
		u8(ch);
		return *this;
	}

	/**
	 * Stream a string (e.g. C-string, coco::String, coco::StringBuffer, std::string) into the writer
	 */
	template <typename T> requires (StringConcept<T>)
	BufferWriter &operator <<(const T &str) {
		string(String(str));
		return *this;
	}

	/**
	 * Skip bytes (does not modify the skipped bytes)
	 * @param n number of bytes
	 */
	void skip(int n) {
		this->current += n;
	}

	/**
	 * Fill bytes
	 * @param n number of bytes
	 * @param value fill value
	 */
	void fill(int n, int value = 0) {
		auto end = this->current + n;
		for (auto it = this->current; it != end; ++it)
			*it = value;
		this->current = end;
	}

	/**
	 * Check if the writer is still valid, i.e. did not write past the end
	 * @return true when before or at the end
	 */
	bool isValid() const {
		return this->current <= this->end;
	}

	/**
	 * Check if we are at the end of the data
	 * @return true when at or past the end
	 */
	bool atEnd() const {
		return this->current >= this->end;
	}

	/**
	 * Get remaining number of bytes in the data
	 * @return number of remaining bytes
	 */
	int remaining() const {
		return int(this->end - this->current);
	}

	/**
	 * Cast to pointer, e.g. for buffer.write()/send()
	 */
	operator uint8_t *() const {
		return this->current;
	}


	uint8_t *current;
	uint8_t *end;
};


} // namespace coco
