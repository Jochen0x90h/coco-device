#include "BufferDevice_cout.hpp"
#include <coco/align.hpp>
#include <iostream>


namespace coco {

BufferDevice_cout::BufferDevice_cout(Loop_native &loop, std::string_view name, Milliseconds<> delay)
	: BufferDevice(State::READY), loop(loop), name(name), delay(delay), callback(makeCallback<BufferDevice_cout, &BufferDevice_cout::handle>(this))
{
}

BufferDevice_cout::~BufferDevice_cout() {
}

//StateTasks<const Device::State, Device::Events> &BufferDevice_cout::getStateTasks() {
	//return makeConst(this->st);
//}

int BufferDevice_cout::getBufferCount() {
	return this->buffers.count();
}

BufferDevice_cout::Buffer &BufferDevice_cout::getBuffer(int index) {
	return this->buffers.get(index);
}

void BufferDevice_cout::handle() {
	auto buffer = this->transfers.pop();
	if (buffer != nullptr) {
		std::cout << this->name << ": ";

		auto op = buffer->op;
		int headerSize = buffer->p.headerSize;
		int count = buffer->p.size - headerSize;
		if ((op & Buffer::Op::COMMAND) != 0)
			std::cout << "command ";
		if (headerSize > 0)
			std::cout << "header " << headerSize << ' ';
		if ((op & Buffer::Op::READ) != 0)
			std::cout << "read ";
		if ((op & Buffer::Op::WRITE) != 0)
			std::cout << "write ";
		std::cout << count << std::endl;

		// check if there are more buffers in the list
		if (!this->transfers.empty())
			this->loop.invoke(this->callback, this->delay);

		// set buffer to ready state and notify application
		buffer->setReady();
	}
}


// Buffer

BufferDevice_cout::Buffer::Buffer(int capacity, BufferDevice_cout &device)
	: coco::Buffer(new uint8_t[capacity], capacity, State::READY)
	, device(device)
{
	device.buffers.add(*this);
}

BufferDevice_cout::Buffer::~Buffer() {
	delete [] this->p.data;
}

bool BufferDevice_cout::Buffer::start(Op op) {
	if (this->st.state != State::READY) {
		// staring a buffer that is busy is considered a bug
		assert(this->st.state != State::BUSY);
		return false;
	}

	// check if READ or WRITE flag is set
	assert((op & Op::READ_WRITE) != 0);

	this->op = op;

	// add buffer to list of transfers and let event loop call I2cMaster_cout::handle() when the first was added
	if (this->device.transfers.push(*this))
		this->device.loop.invoke(this->device.callback, this->device.delay);

	// set state
	setBusy();

	return true;
}

bool BufferDevice_cout::Buffer::cancel() {
	if (this->st.state != State::BUSY)
		return false;

	// small transfers can be cancelled immeditely, otherwise cancel has no effect (this is arbitrary and only for testing)
	if (this->p.size < 4) {
		this->device.transfers.remove(*this);
		setReady(0);
	}
	return true;
}

} // namespace coco
