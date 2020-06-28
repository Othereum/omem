#pragma once
#include <new>
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
	
	class MemoryPool
	{
	public:
		OMAPI static MemoryPool& Get(size_t size);
		
		MemoryPool(size_t size, size_t count);
		MemoryPool(MemoryPool&& r) noexcept;
		~MemoryPool();
		
		[[nodiscard]] void* Alloc()
		{
#if OMEM_THREADSAFE
			std::lock_guard<std::mutex> lock{mutex_};
#endif
			if (++info_.cur > info_.peak) info_.peak = info_.cur;
			if (next_)
			{
				auto* ret = next_;
				next_ = next_->next;
				return ret;
			}
			++info_.fault;
			return operator new(info_.size);
		}

		void Free(void* ptr) noexcept
		{
#if OMEM_THREADSAFE
			std::lock_guard<std::mutex> lock{mutex_};
#endif
			auto* const block = static_cast<Block*>(ptr);
			const auto idx = static_cast<size_t>(static_cast<char*>(ptr) - static_cast<char*>(blocks_)) / info_.size;
			if (idx < info_.count)
			{
				auto* next = next_;
				next_ = block;
				block->next = next;
			}
			else
			{
				operator delete(ptr);
			}
			--info_.cur;
		}
		
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

	[[nodiscard]] inline void* Alloc(size_t size)
	{
		if (size <= OMEM_POOL_SIZE) return MemoryPool::Get(size).Alloc();
		return operator new(size);
	}

	inline void Free(void* p, size_t size) noexcept
	{
		if (size <= OMEM_POOL_SIZE) MemoryPool::Get(size).Free(p);
		else operator delete(p);
	}

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
