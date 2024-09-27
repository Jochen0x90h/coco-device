#include "Buffer.hpp"


namespace coco {

void Buffer::setDisabled() {
    this->p.size = 0;
    this->st.state = State::DISABLED;
    this->st.doAll(Events::ENTER_DISABLED);
}
void Buffer::setReady() {
    this->st.state = State::READY;
    this->st.doAll(Events::ENTER_READY);
}
void Buffer::setReady(int transferred) {
    this->p.size = this->p.headerSize + transferred;
    this->st.state = State::READY;
    this->st.doAll(Events::ENTER_READY);
}
/*void Buffer::setReady(Device::State state, int transferred) {
    this->p.size = this->p.headerSize + transferred;
    setState(state <= Device::State::CLOSING ? State::DISABLED : State::READY);
}*/
void Buffer::setBusy() {
    this->st.state = State::BUSY;
    this->st.doAll(Events::ENTER_BUSY);
}

} // namespace coco
