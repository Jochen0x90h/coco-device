#include "../BufferDevice.hpp"
#include <coco/IntrusiveQueue.hpp>
#include <coco/platform/Loop_native.hpp>
#include <string>


namespace coco {

/// @brief Dummy implementation of a BufferDevice that prints the transfer operations to std::cout
///
class BufferDevice_cout : public BufferDevice {
public:
    /// @brief Constructor
    /// @param loop event loop
    /// @param name name of device, gets printed to std::cout
    /// @param delay simulated delay of transfer
    BufferDevice_cout(Loop_native &loop, std::string_view name, Milliseconds<> delay = 0ms);
    ~BufferDevice_cout() override;


    /// @brief Buffer for transferring data to/from emulated buffer device
    ///
    class Buffer : public coco::Buffer, public IntrusiveListNode, public IntrusiveQueueNode {
        friend class BufferDevice_cout;
    public:
        /// @brief Constructor
        /// @param headerCapacity capacity of the header
        /// @param capacity capacity of the buffer
        /// @param channel channel to attach to
        Buffer(int capacity, BufferDevice_cout &device);
        Buffer(int headerCapacity, int capacity, BufferDevice_cout &device);
        ~Buffer() override;

        bool start() override;
        bool cancel() override;

    protected:

        BufferDevice_cout &device_;
        Op op_;
    };


    // BufferDevice methods
    int getBufferCount() override;
    Buffer &getBuffer(int index) override;

protected:
    void handle();

    Loop_native &loop_;
    std::string name_;
    Milliseconds<> delay_;
    TimedTask<Callback> callback_;

    // list of buffers
    IntrusiveList<Buffer> buffers_;

    // list of active transfers
    IntrusiveQueue<Buffer> transfers_;
};

} // namespace coco
