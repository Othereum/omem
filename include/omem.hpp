#pragma once
#include <functional>
#include <unordered_map>

#if OMEM_THREADSAFE
#include <mutex>
#endif

#if OMEM_BUILD_STATIC
	#define OMAPI
#else
	#ifdef _WIN32
		#if OMEM_BUILD
			#define OMAPI __declspec(dllexport)
		#else
			#define OMAPI __declspec(dllimport)
		#endif
	#else
		#if defined(__GNUC__) && __GNUC__>=4
			#define OMAPI __attribute__ ((visibility("default")))
		#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
			#define OMAPI __global
		#else
			#define OMAPI
		#endif
	#endif
#endif

namespace omem
{
	struct PoolInfo
	{
		constexpr PoolInfo() noexcept = default;
		
		PoolInfo(size_t size, size_t count)
			:size{size}, count{count}
		{
		}
		
		size_t size = 0;
		size_t count = 0;
		size_t cur = 0;
		size_t peak = 0;
		size_t fault = 0;
	};
	
	class OMAPI MemoryPool
	{
	public:
		static MemoryPool& Get(size_t size);
		
		MemoryPool(size_t size, size_t count);
		MemoryPool(MemoryPool&& r) noexcept;
		~MemoryPool();
		
		[[nodiscard]] void* Alloc();
		void Free(void* ptr) noexcept;

		[[nodiscard]] const PoolInfo& GetInfo() const noexcept { return info_; }

		MemoryPool(const MemoryPool&) = delete;
		MemoryPool& operator=(const MemoryPool&) = delete;
		MemoryPool& operator=(MemoryPool&&) = delete;

	private:
		struct Block { Block* next; } *next_;
		void* blocks_;
		PoolInfo info_;
		
#if OMEM_THREADSAFE
		std::mutex mutex_;
#endif
	};

	/**
	 * \brief Allocate memory from pool
	 * \note If no memory left in the pool, allocates new memory
	 * \note Must be returned to pool by Free(p) with SAME SIZE
	 */
	[[nodiscard]] inline void* Alloc(size_t size)
	{
		return MemoryPool::Get(size).Alloc();
	}

	/**
	 * \brief Return memory to pool
	 * \param p Memory to be returned to pool
	 * \param size MUST BE SAME SIZE as allocated by Alloc(size)
	 */
	inline void Free(void* p, size_t size) noexcept
	{
		MemoryPool::Get(size).Free(p);
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
		return new (Alloc(sizeof T)) T{std::forward<Args>(args)...};
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
		Free(p, sizeof T);
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
		[[nodiscard]] static MemoryPool& GetPool() noexcept
		{
			return MemoryPool::Get(sizeof T);
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

	template <class T>
	class Deleter<T[]>
	{
	public:
		void operator()(T* p) const noexcept
		{
			delete[] p;
		}
	};

	/**
	 * \brief Register function to be called when memory pool is destroyed. Print info to stdout by default.
	 * \param on_pool_dest function to be called
	 * \note CALLING THIS FUNCTION IS NOT THREAD-SAFE
	 * \note All exceptions thrown from on_pool_dest are swallowed silently.
	 */
	OMAPI void SetOnPoolDest(const std::function<void(const PoolInfo&)>& on_pool_dest);
	
	/**
	 * \brief Register function to be called when memory pool is destroyed. Print info to stdout by default.
	 * \param on_pool_dest function to be called
	 * \note CALLING THIS FUNCTION IS NOT THREAD-SAFE
	 * \note All exceptions thrown from on_pool_dest are swallowed silently.
	 */
	OMAPI void SetOnPoolDest(std::function<void(const PoolInfo&)>&& on_pool_dest) noexcept;

	[[nodiscard]] OMAPI const std::unordered_map<size_t, MemoryPool>& GetPools() noexcept;
}
