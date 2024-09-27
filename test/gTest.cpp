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


class TestBuffer : public Buffer {
public:
	TestBuffer(uint8_t *data, int size) : Buffer(data, size, State::READY) {}

	~TestBuffer() override {
	}

	bool start(Op op) override {
		return true;
	}

	bool cancel() override {
		return false;
	}
};

TEST(cocoTest, setHeader) {
	uint8_t buffer[128];
	TestBuffer b(buffer, 128);

	// value
	b.setHeader<uint32_t>(10);
	EXPECT_EQ(b.header<uint32_t>(), 10);

	uint64_t u64 = UINT64_C(50000000000);
	b.setHeader(u64);
	EXPECT_EQ(b.header<uint64_t>(), u64);

	// array
	int a[] = {10, 50};
	b.setHeader(a);
	EXPECT_EQ(b.pointer<int>()[-2], 10);
	EXPECT_EQ(b.pointer<int>()[-1], 50);

	// data
	b.setHeader(reinterpret_cast<uint8_t *>(a), sizeof(a));
	EXPECT_EQ(b.pointer<int>()[-2], 10);
	EXPECT_EQ(b.pointer<int>()[-1], 50);

	// check if index operator works in presence of a header
	b.data()[0] = 55;
	EXPECT_EQ(b[0], 55);
}

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
	EXPECT_EQ(b.capacity(), 128 - sizeof(int));

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
	uint8_t buffer[3];
	TestBuffer b(buffer, 2);
	uint8_t data[] = {1, 2, 3};
	uint8_t data2[] = {20, 21, 22};
	uint8_t data3[] = {30, 31, 32};

	// set value behind buffer
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


	// set header
	b.headerResize(1);
	b.header<uint8_t>() = 10;

	// write data which exceeds buffer size
	auto a3 = b.writeData(data, 3);

	// check if header and value behind buffer is intact
	EXPECT_EQ(buffer[0], 10);
	EXPECT_EQ(buffer[1], 1);
	EXPECT_EQ(buffer[2], 50);

	// read data which exceeds buffer size
	auto a4 = b.readData(data3, 3);
	EXPECT_EQ(data3[0], 1);
	EXPECT_EQ(data3[1], 31);
	EXPECT_EQ(data3[2], 32);
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

TEST(cocoTest, BufferWriter) {
	// test methods with explicit size and endianness
	{
		uint8_t buffer[128];
		TestBuffer b(buffer, 128);
		//BufferWriter w(b.data(), b.capacity());
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
		auto awaitable = b.write(w);
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
		w << flt(5.001f);

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

	// test if it compiles for std::vector
	{
		std::vector<uint8_t> v;
		BufferWriter w(v);
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

int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	int success = RUN_ALL_TESTS();
	return success;
}
