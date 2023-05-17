#pragma once

#include <coco/Array.hpp>
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>
#include <coco/String.hpp>


namespace coco {

// helpers
constexpr int align4(int x) {return ((x + 3) & ~3);}
constexpr int align(int x, int n) {return ((x + n - 1) & ~(n - 1));}

/**
	Buffer used for data transfer to/from hardware devices. Typically each device provides its own buffer implementation.
	A Buffer contains an actual data buffer of size given by capacity() and a current size given by size().

	A data transfer can be started by start() which does not block. This internally starts for example a DMA transfer.
	Use the untilXXX-Methods to wait until the buffer transitions into a given state, e.g. co_await untilReady() to wait for completion of the transfer.
*/
class Buffer {
public:
	enum class State {
		/**
			Buffer is disabled because the owning device is e.g. not connected
		*/
		DISABLED,

		/**
			Buffer is ready and a transfer can be started
		*/
		READY,

		/**
			Transfer is in progress
		*/
		BUSY,
	};

	enum class Op {
		NONE = 0,

		/**
			Read operation
		*/
		READ = 1,

		/**
			Write operation
		*/
		WRITE = 2,

		/**
			Read and write simultaneously which is only supported by some devices such as SPI
		*/
		READ_WRITE = 3,

		/**
			Erase e.g. a flash memory
		*/
		ERASE = 4,

		/**
			Partial transfer, i.e. at least one more transfer will follow. If the underlying transport
			protocol is packetized, only full packets may be written (e.g. 64 byte for USB bulk).
			In this case the remaining data (size - transferred) has to be moved to the front of the buffer
		*/
		PARTIAL = 8,

		/**
			Whole transfer is a commmand, e.g. for SPI with command/data line
		*/
		COMMAND = 16
	};

	struct Result {
		/**
			Number of bytes transferred in the last operation
		*/
		int transferred;
	};


	Buffer(uint8_t *data, int size) : dat(data), siz(size) {}

	virtual ~Buffer() {}

	/*
		Get current state
		@return state
	*/
	virtual const State &state() = 0;
	bool disabled() {return state() == State::DISABLED;}
	bool ready() {return state() == State::READY;}
	bool busy() {return state() == State::BUSY;}

	/**
		Wait until the buffer is in the given state (e.g. co_await buffer.targetState(State::READY))
		@param state state to wait for
		@return use co_await on return value to await the given state
	*/
	[[nodiscard]] virtual Awaitable<State> untilState(State state) = 0;
	[[nodiscard]] Awaitable<State> untilDisabled() {return untilState(State::DISABLED);}
	[[nodiscard]] Awaitable<State> untilReady() {return untilState(State::READY);}
	[[nodiscard]] Awaitable<State> untilBusy() {return untilState(State::BUSY);}


	/**
		Set the header. The data stays unchanged unless explicitly stated in the derived implementation.
	*/
	virtual bool setHeader(const uint8_t *data, int size) = 0;

	/**
		Convenience function for setting the header to some value, e.g. setHeader<uint32_t>(50); or setHeader(header);
	*/
	template <typename T>
	void setHeader(const T &header) {
		bool result = setHeader(reinterpret_cast<const uint8_t *>(&header), sizeof(T));
	}

	/**
		Convenience function for clearing the header
	*/
	void clearHeader() {setHeader(nullptr, 0);}

	/**
		Get the header
	*/
	virtual bool getHeader(uint8_t *data, int size) = 0;

	/**
		Convenience function for getting the header, e.g. header = getHeader<uint32_t>();
	*/
	template <typename T>
	T getHeader() {
		T header;
		bool result = getHeader(reinterpret_cast<uint8_t *>(&header), sizeof(T));
		return header;
	}

	/**
		Convenience function for getting the header, e.g. getHeader(header);
	*/
	template <typename T>
	void getHeader(T &header) {
		bool result = getHeader(reinterpret_cast<uint8_t *>(&header), sizeof(T));
		assert(result);
	}


	/**
		Start transfer of the buffer if it is in READY state and set it to BUSY state
		@param op operation flags such as READ or WRITE
		@param size size of data to transfer
		@return true if transfer was started (changed from READY to BUSY state), false on error
	*/
	bool start(int size, Op op) {
		if (size >= 0 && size <= this->siz)
			return startInternal(size, op);
		assert(false);
		return false;
	}

	/**
		Cancel the current transfer operation which means the buffer returns from BUSY to READY after a short amount of
		time. When the transfer got cancelled, size() returns zero. If the transfer complete successfully, size() is as
		if cancel() was not called.
	*/
	virtual void cancel() = 0;

	/**
		Convenience function for acquiring a buffer, i.e. cancel if necessary and wait until ready
	*/
	[[nodiscard]] Awaitable<State> acquire() {
		cancel();
		return untilState(State::READY);
	}


	/**
		Result of the last transfer operation
	*/
	virtual Result result() = 0;
	int transferred() {return result().transferred;}


	/**
		Index operator
	*/
	uint8_t &operator [](int index) {
		assert(uint32_t(index) < uint32_t(this->siz));
		return this->dat[index];
	}

	/**
		Get data of the buffer
	*/
	uint8_t *data() {return this->dat;}
	const uint8_t *data() const {return this->dat;}

	/**
		Get size of the buffer
		@return size
	*/
	int size() const {return this->siz;}

	/**
		Get begin iterator
	*/
	uint8_t *begin() {return this->dat;}
	const uint8_t *begin() const {return this->dat;}

	/**
		Get end iterator
	*/
	uint8_t *end() {return this->dat + this->siz;}
	const uint8_t *end() const {return this->dat + this->siz;}


	/**
		Convenience function for receiving data of size up to size(), e.g. radio, UART or USB bulk
		@param op additional operation flag
		@return use co_await on return value to await completion of receive operation
	*/
	[[nodiscard]] Awaitable<State> read(Op op = Op::NONE) {
		startInternal(this->siz, Op(int(Op::READ) | int(op)));
		return untilState(State::READY);
	}

	/**
		Convenience function for initiating a read operation of given size e.g. from file, I2C or USB control
		@param size size to read
		@param op additional operation flag
		@return use co_await on return value to await completion of read operation
	*/
	[[nodiscard]] Awaitable<State> read(int size, Op op = Op::NONE) {
		start(size, Op(int(Op::READ) | int(op)));
		return untilState(State::READY);
	}

	/**
		Convenience function for writing data
		@param size size of data to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<State> write(int size, Op op = Op::NONE) {
		start(size, Op(int(Op::WRITE) | int(op)));
		return untilState(State::READY);
	}

	/**
		Convenience function for writing data when using BufferWriter, e.g. BufferWriter w(buffer); w.u8(10); buffer.write(w);
		@param end end pointer of data to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<State> write(uint8_t *end, Op op = Op::NONE) {
		start(end - this->dat, Op(int(Op::WRITE) | int(op)));
		return untilState(State::READY);
	}

	/**
		Convenience function for writing a value
		@tparam T value type
		@param value value to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	template <typename T>
	[[nodiscard]] Awaitable<State> writeValue(const T &value, Op op = Op::NONE) {
		if (sizeof(value) <= this->siz) {
			*reinterpret_cast<T *>(this->dat) = value;
			startInternal(sizeof(value), Op(int(Op::WRITE) | int(op)));
		} else {
			assert(false);
		}
		return untilState(State::READY);
	}

	/**
		Convenience function for writing data
		@param data data to write
		@param size size of data to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<State> writeData(const uint8_t *data, int size, Op op = Op::NONE) {
		if (size >= 0 && size <= this->siz) {
			std::copy(data, data + size, this->dat);
			startInternal(size, Op(int(Op::WRITE) | int(op)));
		} else {
			assert(false);
		}
		return untilState(State::READY);
	}

	/**
		Convenience function for writing array data. The array must support std::data() and std::size().
		@tparam T array type
		@param array array to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	template <typename T>
	[[nodiscard]] Awaitable<State> writeArray(const T &array, Op op = Op::NONE) {
		auto src = std::data(array);
		auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(this->dat);

		auto count = std::size(array);
		auto size = count * sizeof(*src);
		if (size <= this->siz) {
			auto end = src + count;
			std::copy(src, end, dst);
		} else {
			assert(false);
		}
		startInternal(size, Op(int(Op::WRITE) | int(op)));
		return untilState(State::READY);
	}

	/**
		Convenience function for writing a string
		@param str string to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<State> writeString(String str, Op op = Op::NONE) {
		return writeData(reinterpret_cast<const uint8_t *>(str.data()), str.size(), op);
	}

	/**
		Convenience function for sending an erase command e.g. to an SPI or I2C flash memory
	*/
	[[nodiscard]] Awaitable<State> erase() {
		startInternal(0, Op::ERASE);
		return untilState(State::READY);
	}


	/**
		Get the data of the buffer as a value of given type
		@tparam T value type
	*/
	template <typename T>
	T &value() {
		assert(sizeof(T) <= this->siz);
		return *reinterpret_cast<T *>(this->dat);
	}

	/**
		Get the entire data of the buffer as an array of given type
		@tparam T array element type
	*/
	template <typename T>
	Array<T> array() {
		return {reinterpret_cast<T *>(this->dat), this->siz / int(sizeof(T))};
	}

	/**
		Get the transferred data of the buffer as an array of given type
		@tparam T array element type
	*/
	template <typename T>
	Array<T> transferredArray() {
		return {reinterpret_cast<T *>(this->dat), result().transferred / int(sizeof(T))};
	}

	/**
		Get the transferred data of the buffer as a string
	*/
	String transferredString() {
		return String(this->dat, result().transferred);
	}

protected:
	virtual bool startInternal(int size, Op op) = 0;

	// buffer data
	uint8_t *dat;
	int siz;
};
COCO_ENUM(Buffer::Op);

} // namespace coco
