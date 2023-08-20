#pragma once

#include "Device.hpp"


namespace coco {

/**
	Base class for a device that has a state and transfer buffers.
*/
class BufferDevice : public Device {
public:
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
