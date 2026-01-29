#include "Buffer.hpp"


namespace coco {

void Buffer::setDisabled() {
    size_ = 0;
    st.set(State::DISABLED).notify(Events::ENTER_DISABLED);
}
void Buffer::setReady() {
    st.set(State::READY).notify(Events::ENTER_READY);
}
void Buffer::setReady(int transferred) {
    size_ = transferred;
    st.set(State::READY).notify(Events::ENTER_READY);
}
/*void Buffer::setReady(Device::State state, int transferred) {
    p.size = p.headerSize + transferred;
    setState(state <= Device::State::CLOSING ? State::DISABLED : State::READY);
}*/
void Buffer::setBusy() {
    st.set(State::BUSY).notify(Events::ENTER_BUSY);
}

} // namespace coco
