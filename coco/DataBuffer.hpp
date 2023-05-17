#pragma once

#include <coco/assert.hpp>
#include <coco/Array.hpp>
#include <cstdint>


namespace coco {

/**
	Buffer of fixed size with methods to set or modify data at a given offset
*/
template <int N>
class DataBuffer {
public:

	void setU8(int index, uint8_t value) {
		this->p.data[index] = value;
	}

	void setU16L(int index, uint16_t value) {
		this->p.data[index + 0] = value;
		this->p.data[index + 1] = value >> 8;
	}

	void setU16B(int index, uint16_t value) {
		this->p.data[index + 0] = value >> 8;
		this->p.data[index + 1] = value;
	}

	void xorU16B(int index, uint16_t value) {
		this->p.data[index + 0] ^= value >> 8;
		this->p.data[index + 1] ^= value;
	}

	void setU32L(int index, uint32_t value) {
		this->p.data[index + 0] = value;
		this->p.data[index + 1] = value >> 8;
		this->p.data[index + 2] = value >> 16;
		this->p.data[index + 3] = value >> 24;
	}

	void setU64L(int index, uint64_t value) {
		setU32L(index, uint32_t(value));
		setU32L(index + 4, uint32_t(value >> 32));
	}

	template <int M>
	void setData(int index, const DataBuffer<M> &buffer) {
		int l = std::min(N - index, M);
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data;
		for (; it < end; ++it, ++data)
			*it = *data;
	}

	template <int M>
	void setData(int index, Array<const uint8_t, M> buffer) {
		int l = std::min(N - index, M);
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data();
		for (; it < end; ++it, ++data)
			*it = *data;
	}

	void setData(int index, int length, const uint8_t *data) {
		int l = std::min(N - index, length);
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it = *data;
	}

	template <int M>
	void xorData(int index, const DataBuffer<M> &buffer) {
		int l = std::min(N - index, M);
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	template <int M>
	void xorData(int index, Array<const uint8_t, M> buffer) {
		int l = std::min(N - index, M);
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		uint8_t const *data = buffer.data();
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	void xorData(int index, int length, const uint8_t *data) {
		int l = std::min(N - index, length);
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it, ++data)
			*it ^= *data;
	}

	void fill(uint8_t value) {
		uint8_t *it = this->p.data;
		uint8_t *end = it + N;
		for (; it < end; ++it)
			*it = value;
	}

	/**
	 * Pad with zeros from index to end of buffer
	 * @param index index where to start padding
	 * @param value padding value
	 */
	void pad(int index, uint8_t value = 0) {
		int l = N - index;
		uint8_t *it = this->p.data + index;
		uint8_t *end = it + l;
		for (; it < end; ++it)
			*it = value;
	}

	uint8_t &operator[] (int index) {
		assert(index >= 0 && index < N);
		return this->p.data[index];
	}

	uint8_t operator[] (int index) const {
		assert(index >= 0 && index < N);
		return this->p.data[index];
	}

	operator Array<uint8_t, N>() {return Array<uint8_t, N>(this->p.data);}
	operator Array<const uint8_t, N>() const {return Array<const uint8_t, N>(this->p.data);}

	template <int M>
	Array<uint8_t, M> array(int index) {
		assert(index >= 0 && index + M <= N);
		return Array<uint8_t, M>(this->p.data + index);
	}

	uint8_t *data() {return this->p.data;}
	static int size() {return N;}

	uint8_t *begin() {return this->p.data;}
	uint8_t *end() {return this->p.data + N;}

	struct {
		uint8_t data[N];
	} p;
};

} // namespace coco
