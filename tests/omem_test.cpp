#include <vector>
#include <gtest/gtest.h>
#include <omem.hpp>

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

template <class T, class Al>
using Rebind = typename std::allocator_traits<Al>::template rebind_alloc<T>;

template <class T, class Al>
static T* allocate(size_t cnt)
{
	return std::allocator_traits<Rebind<T, Al>>::allocate(Rebind<T, Al>{}, cnt);
}

template <class Al>
static void Benchmark()
{
	auto deallocate = [&]<class T>(T* ptr, size_t cnt) {
		std::allocator_traits<Rebind<T, Al>>::deallocate(Rebind<T, Al>{}, ptr, cnt);
	};

	auto arr = new double*[10];
	for (auto i=0; i<10; ++i) arr[i] = allocate<double, Al>(1);

	for (auto i=0; i<10000000; ++i)
		deallocate(allocate<double, Al>(1), 1);
	
	for (auto i=0; i<10; ++i) deallocate(arr[i], 1);
	delete[] arr;
}

TEST(omem, omem)
{
	Benchmark<omem::Allocator<void>>();
}

TEST(omem, cppstd)
{
	Benchmark<std::allocator<void>>();
}
