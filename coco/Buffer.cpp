#include "Buffer.hpp"


namespace coco {

void Buffer::setDisabled() {
    // clear size and error
    setSuccess(0);

    state_ = State::DISABLED;
    notify(Events::ENTER_DISABLED);
}

void Buffer::setReady() {
    state_ = State::READY;
    notify(Events::ENTER_READY);
}

void Buffer::setBusy() {
    state_ = State::BUSY;
    notify(Events::ENTER_BUSY);
}

} // namespace coco
