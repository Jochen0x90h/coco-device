#include <gtest/gtest.h>
#include <coco/BufferImpl.hpp>
#include <coco/BufferReader.hpp>
#include <coco/BufferWriter.hpp>
#include <coco/ArrayConcept.hpp>


using namespace coco;

// todo: DataBuffer should fulfill the ArrayConcept
template <typename T> requires (ArrayConcept<T>)
void useAsArray(const T &array) {
}


class TestBuffer : public BufferImpl {
public:
	TestBuffer(uint8_t *data, int size) : BufferImpl(data + 8, size - 8, Buffer::State::READY) {}

	~TestBuffer() override {
	}

	bool setHeader(const uint8_t *data, int size) override {
		auto src = data;
		auto end = src + size;
		auto dst = this->dat - size;
		while (src < end) {
			*dst = *src;
			++src;
			++dst;
		}
		return true;
	}
	using Buffer::setHeader;

	bool startInternal(int size, Op op) override {
		this->xferred = size;
		return true;
	}

	void cancel() override {
	}
};

TEST(cocoTest, writeValue) {
	uint8_t buffer[128];
	TestBuffer b(buffer, 128);

	int value = 1337;

	auto awaitable = b.writeValue(value);
	EXPECT_EQ(b.transferred(), sizeof(value));

	EXPECT_EQ(b.value<int>(), 1337);
}

TEST(cocoTest, writeArray) {
	uint8_t buffer[128];
	TestBuffer b(buffer, 128);

	const int array[2] = {10, 50};

	auto awaitable = b.writeArray(array);
	EXPECT_EQ(b.transferred(), sizeof(array));

	EXPECT_EQ(b.array<int>()[0], 10);
	EXPECT_EQ(b.array<int>()[1], 50);
}

TEST(cocoTest, writeString) {
	uint8_t buffer[128];
	TestBuffer b(buffer, 128);

	String str("foo");

	auto awaitable = b.writeString(str);
	EXPECT_EQ(b.transferred(), 3);

	EXPECT_EQ(b[0], 'f');
	EXPECT_EQ(b.transferredArray<uint8_t>().size(), 3);
	EXPECT_EQ(b.array<char>()[0], 'f');
	EXPECT_EQ(b.transferredString(), "foo");
}

TEST(cocoTest, setHeader) {
	uint8_t buffer[128];
	TestBuffer b(buffer, 128);
	EXPECT_EQ(b.size(), 120);

	b.setHeader<uint32_t>(10);
	EXPECT_EQ(b.array<int32_t>().data()[-1], 10); // we know that the header precedes the data

	uint64_t u64 = UINT64_C(50000000000);
	b.setHeader(u64);
	EXPECT_EQ(b.array<int64_t>().data()[-1], u64); // we know that the header precedes the data
}

TEST(cocoTest, BufferReader) {
	uint8_t buffer[128] = {50, 0x37, 0x13, 0x13, 0x37};
	BufferReader r(buffer, 128);

	EXPECT_EQ(r.peekU8(), 50);
	EXPECT_EQ(r.u8(), 50);
	EXPECT_EQ(r.u16L(), 0x1337);
	EXPECT_EQ(r.u16B(), 0x1337);
}

TEST(cocoTest, BufferWriter) {
	uint8_t buffer[128];
	TestBuffer b(buffer, 128);
	BufferWriter w(b);
	StringBuffer<10> stringBuffer;
	stringBuffer << "bar";

	// write some data into the buffer
	w.u8(50);
	w.u16L(1337);
	w.u16B(1337);
	w << "foo";
	w << stringBuffer;

	// write the buffer and check transferred()
	auto awaitable = b.write(w);
	EXPECT_EQ(b.transferred(), 11);

	// read and check data
	BufferReader r(b);
	EXPECT_EQ(r.peekU8(), 50);
	EXPECT_EQ(r.u8(), 50);
	EXPECT_EQ(r.u16L(), 1337);
	EXPECT_EQ(r.u16B(), 1337);
	EXPECT_EQ(r.string(3), "foo");
	EXPECT_EQ(r.string(3), "bar");
}

TEST(cocoTest, BufferWriterValueArray) {
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
