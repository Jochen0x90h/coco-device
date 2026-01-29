#include <coco/Coroutine.hpp>


namespace coco {

template <typename S, typename E>
struct StateTasks {
    S state;
    CoroutineTaskList<E> tasks;


    StateTasks(S state) : state(state) {}

    /// @brief Set a new state.
    /// @param state New state
    /// @return *this
    auto &set(S state) {
        this->state = state;
        return *this;
    }

    /// @brief Notify waiting coroutines about the given events.
    /// @param events Event flags to notify
    /// @return *this
    auto &notify(E events) {
        // resume all coroutines waiting for the given event
        this->tasks.doAll([events](E e) {
            return (events & e) != 0;
        });
        return *this;
    }
};

} // namespace coco
