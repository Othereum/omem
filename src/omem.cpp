#include "omem.hpp"
#include <algorithm>
#include <iostream>

namespace omem
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
	
	static void PrintPoolInfo(const PoolInfo& info)
	{
		std::cout << "[omem] Memory pool with " << info.count << ' ' << info.size << "-byte blocks\n";
		std::cout << "[omem]  Leaked: " << info.cur << " blocks\n";
		std::cout << "[omem]  Peak usage: " << info.peak << " blocks\n";
		std::cout << "[omem]  Block fault: " << info.fault << " times\n";
	}
	
	static std::function<void(const PoolInfo&)> on_pool_dest = &PrintPoolInfo;
	static std::unordered_map<size_t, MemoryPool> pools;

#if OMEM_THREADSAFE
	static std::mutex pools_mutex;
#endif

	MemoryPool& MemoryPool::Get(size_t size)
	{
		const auto real_size = size_t(1) << std::max(LogCeil(size, 2), LogCeil(sizeof(void*), 2));
#if OMEM_THREADSAFE
		std::lock_guard<std::mutex> lock{pools_mutex};
#endif
		return pools.try_emplace(real_size, real_size, OMEM_POOL_SIZE/real_size).first->second;
	}

	MemoryPool::MemoryPool(const size_t size, const size_t count)
		:blocks_{operator new(size * count)}, info_{size, count}
	{
		auto* it = static_cast<char*>(blocks_);
		auto* next = next_ = static_cast<Block*>(blocks_);
		for (size_t i=1; i<count; ++i) next = next->next = reinterpret_cast<Block*>(it += size);
		next->next = nullptr;
	}

	MemoryPool::MemoryPool(MemoryPool&& r) noexcept
		:next_{r.next_}, blocks_{r.blocks_}, info_{r.info_}
	{
		r.next_ = nullptr;
		r.blocks_ = nullptr;
		info_ = {};
	}

	MemoryPool::~MemoryPool()
	{
		if (blocks_)
		{
			operator delete(blocks_, info_.size * info_.count);
			try { on_pool_dest(info_); } catch (...) {}
		}
	}

	void* MemoryPool::Alloc()
	{
#if OMEM_THREADSAFE
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

	void MemoryPool::Free(void* const ptr) noexcept
	{
#if OMEM_THREADSAFE
		std::lock_guard<std::mutex> lock{mutex_};
#endif
		auto* const block = static_cast<Block*>(ptr);
		const auto idx = static_cast<size_t>(reinterpret_cast<char*>(block) - blocks_) / info_.size;
		if (0 <= idx && idx < info_.count)
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

	void SetOnPoolDest(const std::function<void(const PoolInfo&)>& on_pool_dest)
	{
		omem::on_pool_dest = on_pool_dest;
	}

	void SetOnPoolDest(std::function<void(const PoolInfo&)>&& on_pool_dest) noexcept
	{
		omem::on_pool_dest = std::move(on_pool_dest);
	}

	const std::unordered_map<size_t, MemoryPool>& GetPools() noexcept
	{
		return pools;
	}
}
