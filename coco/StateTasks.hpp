#include <coco/Coroutine.hpp>


namespace coco {

template <typename S, typename E>
struct StateTasks {
    S state;
    CoroutineTaskList<E> tasks;

    StateTasks(S state) : state(state) {}

    void set(S state, E events) {
        this->state = state;
        this->tasks.doAll([events](E e) {
            return (events & e) != 0;
        });
    }

    void doAll(E events) {
        // resume all coroutines waiting for the given event
        this->tasks.doAll([events](E e) {
            return (events & e) != 0;
        });
    }
};

} // namespace coco
