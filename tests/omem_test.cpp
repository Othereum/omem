#include <vector>
#include <gtest/gtest.h>
#include <omem.hpp>

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

template <class Al>
static void Benchmark(Al al)
{
	using T = std::allocator_traits<Al>;
	for (auto i=0; i<10000000; ++i)
		T::deallocate(al, T::allocate(al, 1), 1);
}

TEST(omem, omem)
{
	Benchmark(omem::Allocator<double>{});
}

TEST(omem, cppstd)
{
	Benchmark(std::allocator<double>{});
}
