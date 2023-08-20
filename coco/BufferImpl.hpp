#pragma once

#include "Buffer.hpp"


namespace coco {

/**
	Buffer with some implementation
*/
class BufferImpl : public Buffer {
public:
	BufferImpl(uint8_t *data, int size, State state) : Buffer(data, size), stat(state) {}

	const State &state() override;
	[[nodiscard]] Awaitable<> stateChange(int waitFlags = 7) override;

	bool setHeader(const uint8_t *data, int size) override;
	using Buffer::setHeader;
	bool getHeader(uint8_t *data, int size) override;
	using Buffer::getHeader;

	Result result() override;

protected:
	virtual void setState(State state);
	void setDisabled() {
		this->xferred = 0;
		setState(State::DISABLED);
	}
	void setReady() {
		setState(State::READY);
	}
	void setReady(int transferred) {
		this->xferred = transferred;
		setState(State::READY);
	}
	void setBusy() {
		setState(State::BUSY);
	}

	// properties
	State stat;
	int xferred = 0;

	// coroutines waiting for a state change
	CoroutineTaskList<> stateTasks;
};

} // namespace coco
