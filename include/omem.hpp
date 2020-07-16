#pragma once
#include <algorithm>
#include <new>
#include <unordered_map>

#ifdef OMEM_THREADSAFE
#include <mutex>
#endif

#ifdef OMEM_BUILD_STATIC
	#define OMAPI
#else
	#ifdef _WIN32
		#ifdef OMEM_BUILD
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
		
		MemoryPool(MemoryPool&& r) noexcept
			:next_{r.next_}, blocks_{r.blocks_}, info_{r.info_}
		{
			r.next_ = nullptr;
			r.blocks_ = nullptr;
			info_ = {};
		}
		
		~MemoryPool();
		
		[[nodiscard]] void* Alloc()
		{
#ifdef OMEM_THREADSAFE
			std::lock_guard<std::mutex> lock{mutex_};
#endif
			info_.peak = std::max(info_.peak, ++info_.cur);
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
#ifdef OMEM_THREADSAFE
			std::lock_guard<std::mutex> lock{mutex_};
#endif
			auto* const block = static_cast<Block*>(ptr);
			const auto diff = static_cast<char*>(ptr) - static_cast<char*>(blocks_);
			if (static_cast<size_t>(diff) < info_.count * info_.size)
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
		
#ifdef OMEM_THREADSAFE
		std::mutex mutex_;
#endif
	};

	[[nodiscard]] inline void* Alloc(size_t size)
	{
		return MemoryPool::Get(size).Alloc();
	}

	inline void Free(void* p, size_t size) noexcept
	{
		MemoryPool::Get(size).Free(p);
	}

	using PoolMap = std::unordered_map<size_t, MemoryPool>;
	[[nodiscard]] OMAPI const PoolMap& GetPools() noexcept;
}
