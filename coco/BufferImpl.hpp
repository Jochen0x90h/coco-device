#pragma once

#include <coco/Array.hpp>
#include <coco/Coroutine.hpp>
#include <coco/enum.hpp>
#include "Buffer.hpp"


namespace coco {

/**
	Buffer with some implementation
*/
class BufferImpl : public Buffer {
public:
	BufferImpl(uint8_t *data, int size, State state) : Buffer(data, size), stat(state), xferred(0) {}

	const State &state() override;
	bool setHeader(const uint8_t *data, int size) override;
	using Buffer::setHeader;
	bool getHeader(uint8_t *data, int size) override;
	using Buffer::getHeader;
	[[nodiscard]] Awaitable<State> untilState(State state) override;
	Result result() override;

protected:
	virtual void setState(State state);
	void setDisabled() {setState(State::DISABLED);}
	void setReady() {setState(State::READY);}
	void setReady(int transferred) {
		this->xferred = transferred;
		setState(State::READY);
	}
	void setBusy() {setState(State::BUSY);}

	// properties
	State stat;
	int xferred;

	// coroutines waiting for a state
	TaskList<State> stateTasks;
};

} // namespace coco
