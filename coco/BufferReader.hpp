#pragma once

#include "Buffer.hpp"
#include <coco/Array.hpp>
#include <coco/String.hpp>
#include <cstdint>


namespace coco {

class BufferReader {
public:
	BufferReader() : current(), end() {}
	template <int N>
	explicit BufferReader(uint8_t (&array)[N]) : current(array), end(array + N) {}
	BufferReader(uint8_t *buffer, uint8_t *end) : current(buffer), end(end) {}
	BufferReader(uint8_t *buffer, int length) : current(buffer), end(buffer + length) {}
	BufferReader(Buffer &buffer) : current(buffer.begin()), end(buffer.begin() + buffer.transferred()) {}

	/**
		Reset the reader to the given current position
	*/
	void reset(uint8_t *current) {this->current = current;}

	/**
		Reset the reader to the given buffer
	*/
	void reset(Buffer &buffer) {
		this->current = buffer.begin();
		this->end = buffer.begin() + buffer.transferred();
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

	uint16_t u16L() {
		auto current = this->current;
		uint16_t value = current[0] | (current[1] << 8);
		this->current += 2;
		return value;
	}

	uint16_t u16B() {
		auto current = this->current;
		uint16_t value = (current[0] << 8) | current[1];
		this->current += 2;
		return value;
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

	uint32_t u32L() {
		auto current = this->current;
		uint32_t value = current[0] | (current[1] << 8) | (current[2] << 16) | (current[3] << 24);
		this->current += 4;
		return value;
	}

	uint32_t u32B() {
		auto current = this->current;
		uint32_t value = (current[0] << 24) | (current[1] << 16) | (current[2] << 8) | current[3];
		this->current += 4;
		return value;
	}

	float f32L() {
		union Value {
			uint32_t i;
			float f;
		};
		Value v = {.i = u32L()};
		return v.f;
	}

	uint64_t u64L() {
		auto lo = uint64_t(u32L());
		return lo | (uint64_t(u32L()) << 32);
	}

	uint64_t u64B() {
		auto hi = uint64_t(u32B()) << 32;
		return hi | uint64_t(u32B());
	}

	float f64L() {
		union Value {
			uint64_t i;
			double f;
		};
		Value v = {.i = u64L()};
		return v.f;
	}

	/**
		Read a byte array of fixed length (e.g. data8<10>())
		@tparam N length of buffer
		@return array
	*/
	template <int N>
	Array<const uint8_t, N> data8() {
		auto ar = this->current;
		this->current += N;
		return Array<const uint8_t, N>(ar);
	}

	template <typename T>
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
	}

	/**
		Read string until end of data
		@return string
	*/
	String string() {
		auto str = this->current;
		this->current = this->end;
		return String(str, this->end - str);
	}

	/**
		Read string with 8 bit length
		@return string
	*/
	String string8() {
		int length = u8();
		auto str = this->current;
		this->current += length;
		return String(str, length);
	}

	/**
		Read string with given length
		@param length length of string
		@return string
	*/
	String string(int length) {
		auto str = this->current;
		this->current += length;
		return String(str, length);
	}

	/**
		Read a string that represents a floating point number (e.g. "1.3", exponential notation not supported)
		@return string containing a floating point number
	*/
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

	/**
		Skip some bytes
		@param n number of bytes to skip
	*/
	void skip(int n) {
		this->current += n;
	}

	/**
		Skip white space
	*/
	void skipSpace() {
		auto it = this->current;
		while (it < this->end && (*it == ' ' || *it == '\t'))
			++it;
		this->current = it;
	}

	/**
		Check if the reader is still valid, i.e. did not read past the end
		@return true when before or at the end
	*/
	bool isValid() const {
		return this->current <= this->end;
	}

	/**
		Check if we are at the end of the data
		@return true when at or past the end
	*/
	bool atEnd() const {
		return this->current >= this->end;
	}

	/**
		Get remaining number of bytes in the data
		@return number of remaining bytes
	*/
	int remaining() const {
		return int(this->end - this->current);
	}


	uint8_t *current;
	uint8_t *end;
};

} // namespace coco
