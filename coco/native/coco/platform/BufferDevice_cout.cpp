#include "BufferDevice_cout.hpp"
#include <coco/align.hpp>
#include <iostream>


namespace coco {

BufferDevice_cout::BufferDevice_cout(Loop_native &loop, std::string_view name, Milliseconds<> delay)
    : BufferDevice(State::READY)
    , loop_(loop)
    , name_(name)
    , delay_(delay)
{
}

BufferDevice_cout::~BufferDevice_cout() {
}

int BufferDevice_cout::getBufferCount() {
    return buffers_.count();
}

BufferDevice_cout::Buffer &BufferDevice_cout::getBuffer(int index) {
    return buffers_.get(index);
}

void BufferDevice_cout::onTimeout() {
    auto buffer = transfers_.pop();
    if (buffer != nullptr) {
        std::cout << name_ << ": ";

        auto op = buffer->op_;
        int headerCapacity = buffer->headerCapacity_;
        int count = buffer->size_;
        if (headerCapacity > 0)
            std::cout << "header " << headerCapacity;
        if ((op & Buffer::Op::READ) != 0)
            std::cout << "read ";
        if ((op & Buffer::Op::WRITE) != 0)
            std::cout << "write ";
        std::cout << count << std::endl;

        // check if there are more buffers in the list
        if (!transfers_.empty())
            loop_.invoke(*this, delay_);

        // set buffer to ready state and notify application
        buffer->setReady();
    }
}


// Buffer

BufferDevice_cout::Buffer::Buffer(int capacity, BufferDevice_cout &device)
    : coco::Buffer(new uint8_t[capacity], 0, 0, capacity, State::READY)
    , device_(device)
{
    device.buffers_.add(*this);
}

BufferDevice_cout::Buffer::Buffer(int headerCapacity, int capacity, BufferDevice_cout &device)
    : coco::Buffer(new uint8_t[headerCapacity + capacity], headerCapacity, 0, capacity, State::READY)
    , device_(device)
{
    device.buffers_.add(*this);
}

BufferDevice_cout::Buffer::~Buffer() {
    delete [] header_;
}

bool BufferDevice_cout::Buffer::start() {
    if (state_ != State::READY || (op_ & Op::READ_WRITE) == 0 || size_ == 0) {
        // staring a buffer that is busy is considered a bug
        assert(state_ != State::BUSY);
        return false;
    }

    // add buffer to list of transfers and let event loop call I2cMaster_cout::handle() when the first was added
    if (device_.transfers_.push(*this))
        device_.loop_.invoke(device_, device_.delay_);

    // set state
    setBusy();

    return true;
}

bool BufferDevice_cout::Buffer::cancel() {
    if (state_ != State::BUSY)
        return false;

    // small transfers can be cancelled immediately, otherwise cancel has no effect (this is arbitrary and only for testing)
    if (size_ < 4) {
        device_.transfers_.remove(*this);
        setError(std::errc::operation_canceled);
        setReady();
    }
    return true;
}

} // namespace coco
