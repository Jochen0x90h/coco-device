#pragma once

#include "Buffer.hpp"


namespace coco {

/**
	Base class for a device that has a state and transfer buffers.
*/
class Device {
public:
	using State = Buffer::State;

	virtual ~Device() {}

	/**
		Get current state
		@return state
	*/
	virtual State state() = 0;
	bool disabled() {return state() == State::DISABLED;}
	bool ready() {return state() == State::READY;}
	bool busy() {return state() == State::BUSY;}

	/**
		Wait until the device is in the given state (e.g. co_await file.untilState(File::State::OPEN))
		@param state state to wait for
		@return use co_await on return value to await the given state
	*/
	virtual Awaitable<State> untilState(State state) = 0;
	virtual Awaitable<State> untilDisabled() {return untilState(State::DISABLED);}
	virtual Awaitable<State> untilReady() {return untilState(State::READY);}
	virtual Awaitable<State> untilBusy() {return untilState(State::BUSY);}

	/**
		Get number of buffers of the device
	*/
	virtual int getBufferCount() = 0;

	/**
		Get the buffer at the given index
	*/
	virtual Buffer &getBuffer(int index) = 0;
};

} // namespace coco
