#include "BufferImpl.hpp"


namespace coco {

const Buffer::State &BufferImpl::state() {
	return this->stat;
}

bool BufferImpl::setHeader(const uint8_t *data, int size) {
	assert(false);
	return false;
}

bool BufferImpl::getHeader(uint8_t *data, int size) {
	assert(false);
	return false;
}

Awaitable<Buffer::State> BufferImpl::untilState(State state) {
	// check if already in required state
	if (state == this->stat)
		return {};
	return {this->stateTasks, state};
}

Buffer::Result BufferImpl::result() {
	return {this->xferred};
}

void BufferImpl::setState(State state) {
	this->stat = state;
	this->stateTasks.resumeAll([state](State s) {
		return s == state;
	});
}

} // namespace coco
