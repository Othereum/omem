#pragma once
#include <functional>

#if OMEM_THREADSAFE
#include <mutex>
#endif

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

		template <class T>
		[[nodiscard]] constexpr T PadToPowerOf2(T x) noexcept
		{
			return size_t(1) << LogCeil(x, 2);
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
#if OMEM_THREADSAFE
			std::mutex mutex_;
#endif
		};

		template <size_t Size>
		struct MemoryPoolSelectorImpl
		{
			static MemoryPool& Get() noexcept
			{
				static MemoryPool pool{Size, Max(OMEM_POOL_SIZE/Size, 1)};
				return pool;
			}
		};

		template <size_t Size>
		struct MemoryPoolSelector
		{
			static MemoryPool& Get() noexcept
			{
				return MemoryPoolSelectorImpl<Max(PadToPowerOf2(Size), sizeof(void*))>::Get();
			}
		};
	}

	/**
	 * \brief Allocate Size bytes of memory from pool
	 * \tparam Size Bytes of memory to be allocated
	 * \return Allocated memory
	 * \note If no memory left in the pool, allocate new memory
	 * \note MUST BE RETURNED by Free<Size>(p) with SAME SIZE
	 */
	template <size_t Size>
	[[nodiscard]] void* Alloc()
	{
		return detail::MemoryPoolSelector<Size>::Get().Alloc();
	}

	/**
	 * \brief Return memory to pool
	 * \tparam Size MUST BE SAME SIZE as allocated by Alloc()
	 * \param p Memory to be returned
	 */
	template <size_t Size>
	void Free(void* p) noexcept
	{
		detail::MemoryPoolSelector<Size>::Get().Free(p);
	}

	/**
	 * \brief Allocate memory from pool and create object
	 * \tparam T Object type to be created
	 * \param args Arguments to be passed to constructor of T
	 * \return Created object
	 * \note MUST BE DELETED by Delete<T>(p) with SAME TYPE. DO NOT CAST POINTER BEFORE DELETE
	 */
	template <class T, class... Args>
	[[nodiscard]] T* New(Args&&... args)
	{
		return new (Alloc<sizeof T>()) T{std::forward<Args>(args)...};
	}

	/**
	 * \brief Destroy object pointed by p and return memory to pool
	 * \tparam T MUST BE SAME TYPE as allocated by New().
	 * \param p Object to be deleted
	 */
	template <class T>
	void Delete(T* p) noexcept
	{
		p->~T();
		Free<sizeof T>(p);
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
			return detail::MemoryPoolSelector<sizeof T>::Get();
		}
	};

	template <class T>
	class Deleter
	{
	public:
		void operator()(T* p) const noexcept
		{
			Delete(p);
		}
	};

	/**
	 * \brief Register function to be called when memory pool is destroyed. Print info to stdout by default.
	 * \param on_pool_dest function to be called
	 * \note CALLING THIS FUNCTION IS NOT THREAD-SAFE
	 * \note All exceptions thrown from on_pool_dest are swallowed silently.
	 */
	void SetOnPoolDest(const std::function<void(const PoolInfo&)>& on_pool_dest);
	
	/**
	 * \brief Register function to be called when memory pool is destroyed. Print info to stdout by default.
	 * \param on_pool_dest function to be called
	 * \note CALLING THIS FUNCTION IS NOT THREAD-SAFE
	 * \note All exceptions thrown from on_pool_dest are swallowed silently.
	 */
	void SetOnPoolDest(std::function<void(const PoolInfo&)>&& on_pool_dest) noexcept;
}
