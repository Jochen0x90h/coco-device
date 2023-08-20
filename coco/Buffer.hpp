#pragma once

#include <coco/Array.hpp>
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>
#include <coco/String.hpp>


namespace coco {

/**
	Buffer used for data transfer to/from hardware devices. Typically each device provides its own buffer
	implementation. A Buffer contains an actual data buffer of size given by size() and after a transfer the
	transferred number of bytes can be obtained by transferred().

	A data transfer can be started by start() which does not block. This internally starts for example a DMA transfer
	and changes the state to BUSY.
	Use the convenience methods e.g. co_await buffer.read() or co_await buffer.write(size) to read or write data.
	Use co_await buffer.untilReady() to wait until a buffer becomes ready after opening the device or until completion
	of a transfer.
	Use co_await buffer.untilNotBusy() to wait until a buffer becomes ready after a transfer or disabled because the
	device was closed.
*/
class Buffer {
public:
	enum class State {
		/**
			Buffer is disabled because the owning device is e.g. not connected
		*/
		DISABLED = 0,

		/**
			Buffer is ready and a transfer can be started
		*/
		READY = 1,

		/**
			Transfer is in progress
		*/
		BUSY = 2,
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
		Wait for a state change, for example when a buffer is busy to wait until it becomes ready again (transfer
		completed)
		@param waitFlags flags that indicate in which states to wait for a state change
		@return use co_await on return value to await a state change
	*/
	[[nodiscard]] virtual Awaitable<> stateChange(int waitFlags = -1) = 0;

	/**
		Wait until the buffer becomes disabled. Does not wait when the buffer is in DISABLED state.
	*/
	[[nodiscard]] Awaitable<> untilDisabled() {
		return stateChange(~(1 << int(State::DISABLED)));
	}

	/**
		Wait until the buffer becomes ready. Does not wait when the buffer is in READY state.

		Usage example:
		co_await buffer.untilReady();
		while (buffer.ready()) {
			// use buffer
		}

		@return use co_await on return value to wait until the buffer becomes ready
	*/
	[[nodiscard]] Awaitable<> untilReady() {
		return stateChange(~(1 << int(State::READY)));
	}

	/**
		Wait until the buffer becomes not busy (ready or disabled). Does not wait when the buffer is in READY or DISABLED state.

		Usage example:
		co_await buffer.untilNotBusy();
		if (!buffer.ready()) {
			// abort as buffer is disabled
		}
		// use buffer

		@return use co_await on return value to wait until the buffer becomes ready or disabled
	*/
	[[nodiscard]] Awaitable<> untilNotBusy() {
		return stateChange(1 << int(State::BUSY));
	}

	/**
		Set the header. The data stays unchanged unless explicitly stated in the derived implementation.
		@return true if successful, false on error
	*/
	virtual bool setHeader(const uint8_t *data, int size) = 0;

	/**
		Convenience function for setting the header to some value, e.g. setHeader<uint32_t>(50); or setHeader(header);
		@return true if successful, false on error
	*/
	template <typename T>
	bool setHeader(const T &header) {
		return setHeader(reinterpret_cast<const uint8_t *>(&header), sizeof(T));
	}

	/**
		Convenience function for clearing the header
		@return true if successful, false on error
	*/
	bool clearHeader() {
		bool result = setHeader(nullptr, 0);
		return result;
	}

	/**
		Get the header
		@return true if successful, false on error
	*/
	virtual bool getHeader(uint8_t *data, int size) = 0;

	/**
		Convenience function for getting the header, e.g. header = getHeader<uint32_t>();
		@return header value if successful, default initialized value on error which is considered a bug
	*/
	template <typename T>
	T getHeader() {
		T header = {};
		bool result = getHeader(reinterpret_cast<uint8_t *>(&header), sizeof(T));
		assert(result);
		return header;
	}

	/**
		Convenience function for getting the header, e.g. getHeader(header);
		@return true if successful, false on error
	*/
	template <typename T>
	bool getHeader(T &header) {
		bool result = getHeader(reinterpret_cast<uint8_t *>(&header), sizeof(T));
		return result;
	}


	/**
		Start transfer of the buffer if it is in READY state and set it to BUSY state if the operation does not
		complete immediately. If the buffer completes immediately, it stays in READY state.
		@param op operation flags such as READ or WRITE
		@param size size of data to transfer
		@return true if successful, false on error or when size is out of range which is considered a bug
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
	[[nodiscard]] Awaitable<> acquire() {
		cancel();
		return untilNotBusy();
	}


	/**
		Result of the last transfer operation
	*/
	virtual Result result() = 0;
	int transferred() {return result().transferred;}


	/**
		Index operator
		@param index index, an index which is out of range is considered a bug
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
	[[nodiscard]] Awaitable<> read(Op op = Op::NONE) {
		startInternal(this->siz, Op(int(Op::READ) | int(op)));
		return untilNotBusy();
	}

	void startRead(Op op = Op::NONE) {
		startInternal(this->siz, Op(int(Op::READ) | int(op)));
	}

	/**
		Convenience function for initiating a read operation of given size e.g. from file, I2C or USB control
		@param size size to read
		@param op additional operation flag
		@return use co_await on return value to await completion of read operation
	*/
	[[nodiscard]] Awaitable<> read(int size, Op op = Op::NONE) {
		start(size, Op(int(Op::READ) | int(op)));
		return untilNotBusy();
	}

	void startRead(int size, Op op = Op::NONE) {
		start(this->siz, Op(int(Op::READ) | int(op)));
	}

	/**
		Convenience function for writing the whole buffer
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<> write(Op op = Op::NONE) {
		start(this->siz, Op(int(Op::WRITE) | int(op)));
		return untilNotBusy();
	}

	void startWrite(Op op = Op::NONE) {
		start(this->siz, Op(int(Op::WRITE) | int(op)));
	}

	/**
		Convenience function for writing data
		@param size size of data to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<> write(int size, Op op = Op::NONE) {
		start(size, Op(int(Op::WRITE) | int(op)));
		return untilNotBusy();
	}

	void startWrite(int size, Op op = Op::NONE) {
		start(size, Op(int(Op::WRITE) | int(op)));
	}

	/**
		Convenience function for writing data when using BufferWriter, e.g. BufferWriter w(buffer); w.u8(10); buffer.write(w);
		@param end end pointer of data to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<> write(uint8_t *end, Op op = Op::NONE) {
		start(end - this->dat, Op(int(Op::WRITE) | int(op)));
		return untilNotBusy();
	}

	void startWrite(uint8_t *end, Op op = Op::NONE) {
		start(end - this->dat, Op(int(Op::WRITE) | int(op)));
	}

	/**
		Convenience function for writing a value
		@tparam T value type
		@param value value to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	template <typename T>
	[[nodiscard]] Awaitable<> writeValue(const T &value, Op op = Op::NONE) {
		if (sizeof(value) <= this->siz) {
			*reinterpret_cast<T *>(this->dat) = value;
			startInternal(sizeof(value), Op(int(Op::WRITE) | int(op)));
		} else {
			// error: size of value too large
			assert(false);
		}
		return untilNotBusy();
	}

	/**
		Convenience function for writing data
		@param data data to write
		@param size size of data to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<> writeData(const void *data, int size, Op op = Op::NONE) {
		if (size >= 0 && size <= this->siz) {
			auto d = reinterpret_cast<const uint8_t *>(data);
			std::copy(d, d + size, this->dat);
			startInternal(size, Op(int(Op::WRITE) | int(op)));
		} else {
			// error: size of data too large or negative
			assert(false);
		}
		return untilNotBusy();
	}

	[[nodiscard]] Awaitable<> writeData(Buffer &buffer, Op op = Op::NONE) {
		int size = buffer.size();
		if (size >= 0 && size <= this->siz) {
			auto data = buffer.data();
			std::copy(data, data + size, this->dat);
			startInternal(size, Op(int(Op::WRITE) | int(op)));
		} else {
			// error: size of data too large or negative
			assert(false);
		}
		return untilNotBusy();
	}

	/**
		Convenience function for writing array data. The array must support std::data() and std::size().
		@tparam T array type
		@param array array to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	template <typename T>
	[[nodiscard]] Awaitable<> writeArray(const T &array, Op op = Op::NONE) {
		auto src = std::data(array);
		auto dst = reinterpret_cast<std::add_pointer_t<std::remove_const_t<std::remove_reference_t<decltype(*src)>>>>(this->dat);

		auto count = std::size(array);
		auto size = count * sizeof(*src);
		if (size <= this->siz) {
			auto end = src + count;
			std::copy(src, end, dst);
			startInternal(size, Op(int(Op::WRITE) | int(op)));
		} else {
			// error: size of data too large or negative
			assert(false);
		}
		return untilNotBusy();
	}

	/**
		Convenience function for writing a string
		@param str string to write
		@param op additional operation flag
		@return use co_await on return value to await completion of write operation
	*/
	[[nodiscard]] Awaitable<> writeString(String str, Op op = Op::NONE) {
		return writeData(reinterpret_cast<const uint8_t *>(str.data()), str.size(), op);
	}

	/**
		Convenience function for sending an erase command e.g. to an SPI or I2C flash memory
	*/
	[[nodiscard]] Awaitable<> erase() {
		startInternal(0, Op::ERASE);
		return untilNotBusy();
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
