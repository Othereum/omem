#pragma once
#include <functional>

namespace omem
{
	struct PoolInfo
	{
		PoolInfo(size_t size, size_t count)
			:size{size}, count{count}
		{
		}
		
		const size_t size;
		const size_t count;
		size_t cur = 0;
		size_t peak = 0;
		size_t fault = 0;
	};
	
	namespace detail
	{
		template <class T1, class T2>
		[[nodiscard]] constexpr T1 LogCeil(T1 x, T2 base) noexcept
		{
			T1 cnt = 0;
			auto remain = false;

			for (T1 result{}; (result = x/base) > 0; ++cnt)
			{
				remain = remain || x%base;
				x = result;
			}

			return cnt + remain;
		}

		template <class T1, class T2>
		[[nodiscard]] constexpr auto Max(T1 a, T2 b) noexcept
		{
			return a > b ? a : b;
		}
		
		class MemoryPool
		{
		public:
			MemoryPool(size_t size, size_t count);
			~MemoryPool();
			
			[[nodiscard]] void* Alloc();
			void Free(void* ptr) noexcept;

			MemoryPool(const MemoryPool&) = delete;
			MemoryPool(MemoryPool&&) = delete;
			MemoryPool& operator=(const MemoryPool&) = delete;
			MemoryPool& operator=(MemoryPool&&) = delete;

		private:
			struct Block { Block* next; } *next_;
			void* const blocks_;
			PoolInfo info_;
		};

		template <size_t Size>
		struct MemoryPoolSelectorImpl
		{
			static MemoryPool& Get() noexcept
			{
				thread_local MemoryPool pool{Size, Max(OMEM_POOL_SIZE/Size, 1)};
				return pool;
			}
		};

		template <size_t SizeLog2>
		struct MemoryPoolSelector
		{
			static MemoryPool& Get() noexcept
			{
				return MemoryPoolSelectorImpl<Max(1 << SizeLog2, sizeof(void*))>::Get();
			}
		};
	}
	
	template <class T>
	class Allocator
	{
	public:
		using value_type = T;
		
		constexpr Allocator() noexcept = default;

		template <class Y>
		constexpr Allocator(const Allocator<Y>&) noexcept {}

		template <class Y>
		constexpr bool operator==(const Allocator<Y>&) const noexcept { return true; }

		[[nodiscard]] T* allocate(size_t n) const
		{
			return static_cast<T*>(n == 1 ? GetPool().Alloc() : operator new(n * sizeof T));
		}

		void deallocate(T* p, size_t n) const noexcept
		{
			if (n == 1) GetPool().Free(p);
			else operator delete(p, n * sizeof T);
		}

	private:
		[[nodiscard]] static auto& GetPool() noexcept
		{
			return detail::MemoryPoolSelector<detail::LogCeil(sizeof T, 2)>::Get();
		}
	};

	/**
	 * \brief Register function to be called when memory pool is destroyed. Print info to stdout by default.
	 * \param on_pool_dest function to be called
	 * \note BE CAREFUL: CALLING THIS FUNCTION IS NOT THREAD-SAFE
	 * \note All exceptions thrown from on_pool_dest are swallowed silently.
	 */
	void SetOnPoolDest(const std::function<void(const PoolInfo&)>& on_pool_dest);
	
	/**
	 * \brief Register function to be called when memory pool is destroyed. Print info to stdout by default.
	 * \param on_pool_dest function to be called
	 * \note BE CAREFUL: CALLING THIS FUNCTION IS NOT THREAD-SAFE
	 * \note All exceptions thrown from on_pool_dest are swallowed silently.
	 */
	void SetOnPoolDest(std::function<void(const PoolInfo&)>&& on_pool_dest) noexcept;
}
