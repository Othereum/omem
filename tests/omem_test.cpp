#include <vector>
#include <gtest/gtest.h>
#include <omem.hpp>

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

using namespace omem;

TEST(MemoryPool, Single)
{
	Deallocate(Allocate<int>(1), 1);
	Deallocate(Allocate<char>(1), 1);
}

TEST(MemoryPool, Array)
{
	Deallocate(Allocate<int>(10), 10);
	Deallocate(Allocate<char>(10), 10);
}

TEST(MemoryPool, Repeat)
{
	for (auto i=0; i<10; ++i)
		Deallocate(Allocate<int>(1), 1);
}

TEST(MemoryPool, RepeatArr)
{
	for (auto i=0; i<10; ++i)
		Deallocate(Allocate<int>(10), 10);
}

TEST(MemoryPool, Arr2)
{
	auto arr = Allocate<int*>(10);
	for (auto i=0; i<10; ++i)
		arr[i] = Allocate<int>(10);
	for (auto i=0; i<10; ++i)
		Deallocate(arr[i], 10);
	Deallocate(arr, 10);
}

constexpr size_t kOne = 1;
constexpr size_t kCount = 3000000;
constexpr size_t kLog2 = detail::LogCeil(kCount, 2);

TEST(MemoryPool, Many)
{
	Deallocate(Allocate<int>(kCount), kCount);
}

TEST(MemoryPool, Increase)
{
	auto arr = Allocate<int*>(kLog2);
	for (size_t i = 0; i < kLog2; ++i)
		arr[i] = Allocate<int>(kOne << i);
	for (size_t i = 0; i < kLog2; ++i)
		Deallocate(arr[i], kOne << i);
	Deallocate(arr, kLog2);
}

TEST(MemoryPool, IncreaseDealloc)
{
	for (size_t i = 0; i < kLog2; ++i)
		Deallocate(Allocate<int>(kOne << i), kOne << i);
}

struct TestStruct
{
	TestStruct() {}
	TestStruct(TestStruct&&) {}
	float data[9];
};

TEST(MemoryPool, ManyStruct)
{
	Deallocate(Allocate<TestStruct>(kCount), kCount);
}

TEST(MemoryPool, IncreaseStruct)
{
	auto arr = Allocate<TestStruct*>(kLog2);
	for (size_t i = 0; i < kLog2; ++i)
		arr[i] = Allocate<TestStruct>(kOne << i);
	for (size_t i = 0; i < kLog2; ++i)
		Deallocate(arr[i], kOne << i);
	Deallocate(arr, kLog2);
}

TEST(MemoryPool, IncreaseDeallocStruct)
{
	for (size_t i = 0; i < kLog2; ++i)
		Deallocate(Allocate<TestStruct>(kOne << i), kOne << i);
}

TEST(MemoryPool, VectorAllocator)
{
	std::vector<TestStruct, Allocator<TestStruct>> vec;
	for (size_t i=0; i<kCount; ++i) vec.emplace_back();
}

TEST(MemoryPool, HugeKB)
{
	struct Huge
	{
		Huge() {}
		Huge(Huge&&) {}
		char data[1000];
	};

	std::vector<Huge, Allocator<Huge>> vec;
	for (size_t i=0; i<100000; ++i) vec.emplace_back();
}

TEST(MemoryPool, HugeMB)
{
	struct Huge
	{
		Huge() {}
		Huge(Huge&&) {}
		char data[1000000];
	};

	std::vector<Huge, Allocator<Huge>> vec;
	for (size_t i=0; i<100; ++i) vec.emplace_back();
}
