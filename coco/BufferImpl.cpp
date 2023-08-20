#include "BufferImpl.hpp"


namespace coco {

const Buffer::State &BufferImpl::state() {
	return this->stat;
}

Awaitable<> BufferImpl::stateChange(int waitFlags) {
	if ((waitFlags & (1 << int(this->stat))) == 0)
		return {};
	return {this->stateTasks};
}

bool BufferImpl::setHeader(const uint8_t *data, int size) {
	assert(false);
	return false;
}

bool BufferImpl::getHeader(uint8_t *data, int size) {
	assert(false);
	return false;
}

Buffer::Result BufferImpl::result() {
	return {this->xferred};
}

void BufferImpl::setState(State state) {
	this->stat = state;
	this->stateTasks.doAll();
}

} // namespace coco
