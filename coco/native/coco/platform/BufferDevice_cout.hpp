#include "../BufferDevice.hpp"
#include <coco/IntrusiveQueue.hpp>
#include <coco/platform/Loop_native.hpp>
#include <string>


namespace coco {

/**
 * Dummy implementation of a BufferDevice that prints the transfer operations to std::cout
 */
class BufferDevice_cout : public BufferDevice {
public:
    /**
     * Constructor
     * @param loop event loop
     * @param name name of device, gets printed to std::cout
     * @param delay simulated delay of transfer
     */
    BufferDevice_cout(Loop_native &loop, std::string_view name, Milliseconds<> delay = 0ms);
    ~BufferDevice_cout() override;


    /**
     * Buffer for transferring data to/from emulated I2C device
     */
    class Buffer : public coco::Buffer, public IntrusiveListNode, public IntrusiveQueueNode {
        friend class BufferDevice_cout;
    public:
        /**
         * Constructor
         * @param headerCapacity capacity of the header
         * @param capacity capacity of the buffer
         * @param channel channel to attach to
         */
        Buffer(int capacity, BufferDevice_cout &device);
        ~Buffer() override;

        bool start(Op op) override;
        bool cancel() override;

    protected:

        BufferDevice_cout &device;
        Op op;
    };


    // Device methods
    //StateTasks<const State, Events> &getStateTasks() override;

    // BufferDevice methods
    int getBufferCount() override;
    Buffer &getBuffer(int index) override;

protected:
    void handle();

    Loop_native &loop;
    std::string name;
    Milliseconds<> delay;
    TimedTask<Callback> callback;

    // device state
    StateTasks<Device::State, Device::Events> st = Device::State::READY;

    // list of buffers
    IntrusiveList<Buffer> buffers;

    // list of active transfers
    IntrusiveQueue<Buffer> transfers;
};

} // namespace coco
