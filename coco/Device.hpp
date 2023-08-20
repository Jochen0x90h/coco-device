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
		Wait for a state change
		@param waitFlags flags that indicate in which states to wait for a state change
		@return use co_await on return value to await a state change
	*/
	[[nodiscard]] virtual Awaitable<> stateChange(int waitFlags = -1) = 0;

	/**
		Wait until the device becomes disabled. Does not wait when the device is in DISABLED state.
		@return use co_await on return value to wait until the device becomes disabled
	*/
	[[nodiscard]] Awaitable<> untilDisabled() {
		return stateChange(~(1 << int(State::DISABLED)));
	}

	/**
		Wait until the device becomes ready. Does not wait when the device is in READY state.
		@return use co_await on return value to wait until the device becomes ready
	*/
	[[nodiscard]] Awaitable<> untilReady() {
		return stateChange(~(1 << int(State::READY)));
	}

	/**
		Wait until the device becomes not busy (ready or disabled). Does not wait when the device is in READY or DISABLED state.
		@return use co_await on return value to wait until the device becomes ready or disabled
	*/
	[[nodiscard]] Awaitable<> untilNotBusy() {
		return stateChange(1 << int(State::BUSY));
	}

	/**
		Close the device. May not take effect immediately, therefore use co_await untilDisabled() to wait until close completes.
	*/
	virtual void close();
};

} // namespace coco
