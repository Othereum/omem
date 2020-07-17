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
	class Allocator
	{
	public:
		using value_type = double;
		
		double* allocate(size_t n)
		{
			return static_cast<double*>(pool_.Alloc(sizeof(double) * n));
		}

		void deallocate(double* p, size_t n)
		{
			pool_.Free(p, sizeof(double) * n);
		}

	private:
		omem::MemoryPoolManager pool_;
	};
	
	Benchmark(Allocator{});
}

TEST(omem, cppstd)
{
	Benchmark(std::allocator<double>{});
}
