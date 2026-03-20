#include <gtest/gtest.h>
#include <coco/Buffer.hpp>
#include <coco/BufferReader.hpp>
#include <coco/BufferWriter.hpp>
#include <coco/ArrayConcept.hpp>
#include <coco/StreamOperators.hpp>


using namespace coco;


// test enums
enum Enum16 : uint16_t {
    FOO = 50
};
enum Enum32 : uint32_t {
    BAR = 1337
};


// todo: DataBuffer should fulfill the ArrayConcept
template <typename T> requires (ArrayConcept<T>)
void useAsArray(const T &array) {
}


template <int H, int D>
class TestBuffer : public Buffer {
public:
    TestBuffer()
        : Buffer(h, H, d, D, State::READY) {}

    ~TestBuffer() override {
    }

    bool start() override {
        return true;
    }

    bool cancel() override {
        return false;
    }

    uint8_t h[std::max(H, 1)];
    alignas(4) uint8_t d[D];
};

TEST(cocoTest, Buffer_setHeader) {
    TestBuffer<8, 128> buffer;

    // value
    buffer.setHeader<uint32_t>(10);
    EXPECT_EQ(buffer.header<uint32_t>(), 10);

    uint64_t u64 = UINT64_C(50000000000);
    buffer.setHeader(u64);
    EXPECT_EQ(buffer.header<uint64_t>(), u64);

    // array
    int a[] = {10, 50};
    buffer.setHeader(a);
    EXPECT_EQ(buffer.headerPointer<int>()[0], 10);
    EXPECT_EQ(buffer.headerPointer<int>()[1], 50);

    // data
    buffer.setHeader(reinterpret_cast<uint8_t *>(a), sizeof(a));
    EXPECT_EQ(buffer.headerPointer<int>()[0], 10);
    EXPECT_EQ(buffer.headerPointer<int>()[1], 50);

    // check if index operator works in presence of a header
    buffer.data()[0] = 55;
    EXPECT_EQ(buffer[0], 55);
}

TEST(cocoTest, Buffer_assign) {
    TestBuffer<0, 4> buffer;
    EXPECT_EQ(buffer.capacity(), 4);

    // byte
    buffer.assign(uint8_t(50));
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer[0], 50);

    // data, size
    buffer.assign("foo", 3);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer[2], 'o');
    buffer.assign("foo__", 5);
    EXPECT_EQ(buffer.size(), 4);

    // c-string
    buffer.assign("bar");
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer[2], 'r');
    buffer.assign("bar__");
    EXPECT_EQ(buffer.size(), 4);
    char s1[4] = {'f', 'o', 'o', 0};
    buffer.assign(s1);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer[2], 'o');

    // byte span
    const uint8_t a1[3] = {1, 2, 3};
    buffer.assign(a1);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer[2], 3);
    std::vector<uint8_t> v1{11, 12, 13, 14, 15};
    buffer.assign(v1);
    EXPECT_EQ(buffer.size(), 4);
    EXPECT_EQ(buffer[2], 13);
    std::list<uint8_t> l1{10, 20, 30};
    buffer.assign(l1);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer[2], 30);
}

TEST(cocoTest, Buffer_cast) {
    TestBuffer<0, 4> buffer;
    buffer.resize(4);
    *reinterpret_cast<uint32_t *>(buffer.d) = 1337;

    // cast to base type
    EXPECT_EQ(buffer.cast<uint32_t>(), 1337);

    // cast to reference of base type
    buffer.cast<uint32_t &>() = 0xbaadcafe;

    // cast to array
    auto ar = buffer.cast<Array<uint32_t>>();
    EXPECT_EQ(ar.size(), 1);
    EXPECT_EQ(ar[0], 0xbaadcafe);
}

    /*
TEST(cocoTest, writeValue) {
    uint8_t buffer[128];
    TestBuffer b(buffer, 128);
    EXPECT_EQ(b.capacity(), 128);

    int value = 1337;

    // use writeValue()
    auto awaitable = b.writeValue(value);
    EXPECT_EQ(b.size(), sizeof(value));
    EXPECT_EQ(b.value<int>(), 1337);

    // set header
    b.setHeader<int>(50);
    EXPECT_EQ(b.capacity(), 128);

    // check again
    auto awaitable2 = b.writeValue(value);
    EXPECT_EQ(b.size(), sizeof(value));
    EXPECT_EQ(b.value<int>(), 1337);
}

TEST(cocoTest, writeArray) {
    uint8_t buffer[128];
    TestBuffer b(buffer, 128);

    const int array[2] = {10, 50};

    // use writeArray()
    auto awaitable = b.writeArray(array);
    EXPECT_EQ(b.size(), sizeof(array));
    EXPECT_EQ(b.array<int>()[0], 10);
    EXPECT_EQ(b.array<int>()[1], 50);

    // use generic write
    auto awaitable2 = b.write(array);
    EXPECT_EQ(b.size(), sizeof(array));
    EXPECT_EQ(b.array<int>()[0], 10);
    EXPECT_EQ(b.array<int>()[1], 50);

    // set header
    b.setHeader<int>(50);

    // check again
    auto awaitable3 = b.writeArray(array);
    EXPECT_EQ(b.size(), sizeof(array));
    EXPECT_EQ(b.array<int>()[0], 10);
    EXPECT_EQ(b.array<int>()[1], 50);
    auto awaitable4 = b.write(array);
    EXPECT_EQ(b.size(), sizeof(array));
    EXPECT_EQ(b.array<int>()[0], 10);
    EXPECT_EQ(b.array<int>()[1], 50);
}

TEST(cocoTest, writeString) {
    uint8_t buffer[128];
    TestBuffer b(buffer, 128);

    String str("foo");

    // use writeString()
    auto awaitable = b.writeString(str);
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b.string(), str);
    EXPECT_EQ(b[0], 'f');
    EXPECT_EQ(b.array<uint8_t>().size(), 3);
    EXPECT_EQ(b.array<char>()[0], 'f');

    // use generic write
    auto awaitable2 = b.write(str);
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b.string(), str);

    // use writeString()
    auto awaitable3 = b.writeString("bar");
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b.string(), "bar");

    // use generic write
    auto awaitable4 = b.write("bar");
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b.string(), "bar");

    // set header
    b.setHeader<int>(50);

    // check again
    auto awaitable5 = b.writeString(str);
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b.string(), str);
    auto awaitable6 = b.write(str);
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b.string(), str);
}

TEST(cocoTest, readWriteData) {
    uint8_t header[1];
    uint8_t buffer[3];
    TestBuffer b(header, 1, buffer, 2);
    uint8_t data[] = {1, 2, 3};
    uint8_t data2[] = {20, 21, 22};
    uint8_t data3[] = {30, 31, 32};

    // set value behind end of buffer
    buffer[2] = 50;

    // write data which exceeds buffer size
    auto a1 = b.writeData(data, 3);

    // check if value behind buffer is intact
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 50);

    // read data which exceeds buffer size
    auto a2 = b.readData(data2, 3);
    EXPECT_EQ(data2[0], 1);
    EXPECT_EQ(data2[1], 2);
    EXPECT_EQ(data2[2], 22);

    // write data which exceeds buffer size
    auto a3 = b.writeData(data, 3);

    // set header
    //b.headerResize(1);
    b.header<uint8_t>() = 10;

    // check if value behind buffer is intact
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 50);

    // read data which exceeds buffer size
    auto a4 = b.readData(data3, 3);
    EXPECT_EQ(data3[0], 1);
    EXPECT_EQ(data3[1], 2);
    EXPECT_EQ(data3[2], 32);
}
*/

TEST(cocoTest, error) {
    TestBuffer<0, 2> buffer;

    auto error = buffer.error();
    EXPECT_TRUE(!error);
    EXPECT_FALSE(error == std::errc::operation_canceled);
}

TEST(cocoTest, BufferReader) {
    uint8_t buffer[128] = {50, 0x37, 0x13, 0x13, 0x37};
    BufferReader r(buffer, 128);

    EXPECT_EQ(r.peekU8(), 50);
    EXPECT_EQ(r.u8(), 50);
    EXPECT_EQ(r.u16L(), 0x1337);
    EXPECT_EQ(r.u16B(), 0x1337);

    // test if it compiles for std::vector
    {
        std::vector<uint8_t> v;
        BufferReader r(v);
    }
}
/*
TEST(cocoTest, BufferWriter) {
    {
        BufferWriter w;
    }

    // test methods with explicit size and endianness
    {
        uint8_t buffer[128];
        TestBuffer b(buffer, 128);
        BufferWriter w(b.all());

        // write some data into the buffer
        w.u8(10);
        w.i16L(-50);
        w.u16B(1337);
        w.e16L(Enum16::FOO);
        w.u32L(0xdeadbeef);
        w.e32L(Enum32::BAR);
        w.u64B(0xbaadcafe);

        // write the buffer and check size()
        auto awaitable = b.writeEnd(w);
        EXPECT_EQ(b.size(), 23);

        // read and check data
        BufferReader r(b);
        EXPECT_EQ(r.peekU8(), 10);
        EXPECT_EQ(r.u8(), 10);
        EXPECT_EQ(r.i16L(), -50);
        EXPECT_EQ(r.u16B(), 1337);
        EXPECT_EQ(r.e16L<Enum16>(), Enum16::FOO);
        EXPECT_EQ(r.u32L(), 0xdeadbeef);
        EXPECT_EQ(r.e32L<Enum32>(), Enum32::BAR);
        EXPECT_EQ(r.u64B(), 0xbaadcafe);

        // check that no data is remaining
        EXPECT_EQ(r.remaining(), 0);
    }

    // test variable integer
    {
        uint8_t buffer[128];
        BufferWriter w(buffer);

        w.uVar(1337);
        w.uVar(0xbaadcafe);

        EXPECT_EQ(buffer[0], (1337 & 0x7f) | 0x80);
        EXPECT_EQ(buffer[1], 1337 >> 7);
        EXPECT_EQ(w.remaining(), 121);

        // read check data
        BufferReader r(buffer);
        EXPECT_EQ(r.uVar<uint32_t>(), 1337);
        EXPECT_EQ(r.uVar<uint32_t>(), 0xbaadcafe);
        EXPECT_EQ(r.remaining(), 121);
    }

    // test native and aligned value() and array() methods
    {
        int buffer[32] = {};
        auto begin = reinterpret_cast<uint8_t *>(buffer);
        BufferWriter w(begin, sizeof(buffer));

        int value = 1337;
        const int array[2] = {10, 50};

        w.value(value);
        w.array(array);

        EXPECT_EQ(buffer[0], 1337);
        EXPECT_EQ(buffer[1], 10);
        EXPECT_EQ(buffer[2], 50);

        EXPECT_EQ(w - begin, 3 * sizeof(int));
    }

    // test string
    {
        uint8_t buffer[128];
        BufferWriter w(buffer);

        // string without length
        w.string("foo");

        // string with preceding 8 bit length
        w.string8("foo");

        // fixed size string
        w.string("bar", 8);

        // check written size
        size_t size = w - buffer;
        EXPECT_EQ(size, 3 + 4 + 8);

        // read to check
        BufferReader r(buffer, w); // w is used as end pointer

        EXPECT_EQ(r.string(3), "foo");
        EXPECT_EQ(r.string8(), "foo");
        EXPECT_EQ(r.string(8), "bar");
    }

    // test stream operators
    {
        uint8_t buffer[128];
        BufferWriter w(buffer);

        String string = "foo";
        StringBuffer<10> stringBuffer;
        stringBuffer << "bar";
        std::string stdString = "std";

        w << "str";
        w << string;
        w << stringBuffer;
        w << stdString;
        w << dec(5.001f);

        // read to check
        BufferReader r(buffer, w); // w is used as end pointer
        EXPECT_EQ(r.string(3), "str");
        EXPECT_EQ(r.string(3), "foo");
        EXPECT_EQ(r.string(3), "bar");
        EXPECT_EQ(r.string(3), "std");
        EXPECT_EQ(r.string(5), "5.001");

        // check that no data is remaining
        EXPECT_EQ(r.remaining(), 0);
    }

    // test if it compiles for std::vector
    {
        std::vector<uint8_t> v;
        BufferWriter w(v);
    }
}
*/
/*
class BufferWrapper {
public:
    template <int N>
    BufferWrapper(uint8_t (&array)[N]) : end_(array), capacity_(array + N) {}

    void append(uint8_t value) {
        if (remaining() > 0) {
            *end_ = value;
            ++end_;
        }
    }

    void append(const uint8_t *data, int size) {
        int n = std::clamp(size, 0, remaining());
        end_ = std::copy_n(data, n, end_);
    }

    int remaining() {
        return capacity_ - end_;
    }

    uint8_t *end() {return end_;}

protected:
    uint8_t *end_;
    uint8_t *capacity_;
};*/

TEST(cocoTest, BufferWriter) {
    // test methods with explicit size and endianness
    {
        TestBuffer<0, 128> buffer;
        BufferWriter w(buffer);

        // write some data into the buffer
        w.u8(10);
        w.i16L(-50);
        w.u16B(1337);
        w.e16L(Enum16::FOO);
        w.u32L(0xdeadbeef);
        w.e32L(Enum32::BAR);
        w.u64B(0xbaadcafe);

        // write the buffer and check size()
        EXPECT_EQ(buffer.size(), 23);

        // read and check data
        BufferReader r(buffer);
        EXPECT_EQ(r.peekU8(), 10);
        EXPECT_EQ(r.u8(), 10);
        EXPECT_EQ(r.i16L(), -50);
        EXPECT_EQ(r.u16B(), 1337);
        EXPECT_EQ(r.e16L<Enum16>(), Enum16::FOO);
        EXPECT_EQ(r.u32L(), 0xdeadbeef);
        EXPECT_EQ(r.e32L<Enum32>(), Enum32::BAR);
        EXPECT_EQ(r.u64B(), 0xbaadcafe);

        // check that no data is remaining
        EXPECT_EQ(r.remaining(), 0);
    }

    // test variable integer
    {
        TestBuffer<0, 128> buffer;
        BufferWriter w(buffer);

        w.uVar(1337);
        w.uVar(0xbaadcafe);

        EXPECT_EQ(buffer[0], (1337 & 0x7f) | 0x80);
        EXPECT_EQ(buffer[1], 1337 >> 7);
        EXPECT_EQ(w.remaining(), 121);

        // read check data
        BufferReader r(buffer);
        EXPECT_EQ(r.uVar<uint32_t>(), 1337);
        EXPECT_EQ(r.uVar<uint32_t>(), 0xbaadcafe);
        EXPECT_EQ(r.remaining(), 0);
    }

    // test array
    {
        TestBuffer<0, 32> buffer;
        BufferWriter w(buffer);

        const int array[2] = {10, 50};

        w.array8(array);

        EXPECT_EQ(buffer[0], 10);
        EXPECT_EQ(buffer[1], 50);
        EXPECT_EQ(w.remaining(), 30);
    }

    // test data
    {
        TestBuffer<0, 32> buffer;
        BufferWriter w(buffer);

        const uint8_t d1[2] = {uint8_t(1), uint8_t(2)};
        w.data(d1, 2);

        const int8_t d2[2] = {int8_t(10), int8_t(-50)};
        w.data(d2, 2);

        EXPECT_EQ(buffer[0], 1);
        EXPECT_EQ(buffer[1], 2);
        EXPECT_EQ(buffer[2], 10);
        EXPECT_EQ(int8_t(buffer[3]), -50);
        EXPECT_EQ(w.remaining(), 28);
    }

    // test string
    {
        TestBuffer<0, 128> buffer;
        BufferWriter w(buffer);

        // string without length
        w.string("foo");

        // string with preceding 8 bit length
        w.string8("foo");

        // fixed size string
        w.string("bar", 8);

        // check written size
        size_t size = w.current() - buffer.d;
        EXPECT_EQ(size, 3 + 4 + 8);

        // read to check
        BufferReader r(buffer); // w is used as end pointer

        EXPECT_EQ(r.string(3), "foo");
        EXPECT_EQ(r.string8(), "foo");
        EXPECT_EQ(r.string(8), "bar");
    }

    // test stream operators
    {
        TestBuffer<0, 128> buffer;
        BufferWriter w(buffer);

        String string = "foo";
        StringBuffer<10> stringBuffer;
        stringBuffer << "bar";
        std::string stdString = "std";

        w << 'c';
        w << "str";
        w << string;
        w << stringBuffer;
        w << stdString;
        w << dec(5.001f);

        // read to check
        BufferReader r(buffer); // w is used as end pointer
        EXPECT_EQ(r.string(1), "c");
        EXPECT_EQ(r.string(3), "str");
        EXPECT_EQ(r.string(3), "foo");
        EXPECT_EQ(r.string(3), "bar");
        EXPECT_EQ(r.string(3), "std");
        EXPECT_EQ(r.string(5), "5.001");

        // check that no data is remaining
        EXPECT_EQ(r.remaining(), 0);
    }
}

TEST(cocoTest, DataBuffer) {
    DataBuffer<16> b;
    // todo: useAsArray(b);

    b.setU8(0, 50);
    b.setU16L(1, 1337);
    b.setU16B(3, 1337);

    BufferReader r(b.data(), 128);
    EXPECT_EQ(r.peekU8(), 50);
    EXPECT_EQ(r.u8(), 50);
    EXPECT_EQ(r.u16L(), 1337);
    EXPECT_EQ(r.u16B(), 1337);

    b.fill(10);
    for (auto element : b) {
        EXPECT_EQ(element, 10);
    }
}
/*
template <int I = 0>
struct BW {

    ~BW() {
        if constexpr (I == 1)
            std::cout << "~commit" << std::endl;
    }

    BW<I + 1> u8(uint8_t value) {
        if (I > 0 && (I & 3) == 0)
            std::cout << "commit" << std::endl;

        std::cout << "u8" << std::endl;
        ++i_;
        return BW<I + 1>{i_};
    }

    int i_ = 0;
};
*/

int main(int argc, char **argv) {
/*    BW<> w;
    w.u8(1);
        //.u8(2)
        //.u8(3)
        //.u8(4)
        //.u8(5);*/

    testing::InitGoogleTest(&argc, argv);
    int success = RUN_ALL_TESTS();
    return success;
}
