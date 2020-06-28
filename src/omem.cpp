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
		constexpr auto pool_size = size_t(1) << LogCeil(OMEM_POOL_SIZE, 2);
		constexpr auto min_log = LogCeil(sizeof(void*), 2);
		constexpr auto max_log = LogCeil(pool_size, 2) + 1;
		const auto log = std::clamp(LogCeil(size, 2), min_log, max_log);
		const auto real_size = size_t(1) << log;
#if OMEM_THREADSAFE
		std::lock_guard<std::mutex> lock{pools_mutex};
#endif
		return pools.try_emplace(log, real_size, pool_size/real_size).first->second;
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
			operator delete(blocks_);
			try { on_pool_dest(info_); } catch (...) {}
		}
	}

	void SetOnPoolDest(const std::function<void(const PoolInfo&)>& callback)
	{
		on_pool_dest = callback;
	}

	void SetOnPoolDest(std::function<void(const PoolInfo&)>&& callback) noexcept
	{
		on_pool_dest = std::move(callback);
	}

	const std::unordered_map<size_t, MemoryPool>& GetPools() noexcept
	{
		return pools;
	}
}
