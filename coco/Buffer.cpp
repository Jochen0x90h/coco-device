#include "Buffer.hpp"


namespace coco {

void Buffer::setDisabled() {
    size_ = 0;
    state_ = State::DISABLED;
    notify(Events::ENTER_DISABLED);
}

void Buffer::setReady() {
    state_ = State::READY;
    notify(Events::ENTER_READY);
}

/*void Buffer::setReady(int transferred) {
    size_ = transferred;
    state_ = State::READY;
    notify(Events::ENTER_READY);
}*/

void Buffer::setBusy() {
    state_ = State::BUSY;
    notify(Events::ENTER_BUSY);
}

} // namespace coco
