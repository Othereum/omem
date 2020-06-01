#include "omem.hpp"
#include <iostream>

namespace omem::detail
{
	static void PrintPoolInfo(const PoolInfo& info)
	{
		std::cout << "[omem] Memory pool with " << info.count << ' ' << info.size << "-byte blocks\n";
		std::cout << "[omem]  Leaked: " << info.cur << " blocks\n";
		std::cout << "[omem]  Peak usage: " << info.peak << " blocks\n";
		std::cout << "[omem]  Block fault: " << info.fault << " times\n";
	}
	
	static std::function<void(const PoolInfo&)> on_pool_dest = &PrintPoolInfo;

	MemoryPool::MemoryPool(const size_t size, const size_t count)
		:blocks_{operator new(size * count)}, info_{size, count}
	{
		auto* it = static_cast<char*>(blocks_);
		auto* next = next_ = static_cast<Block*>(blocks_);
		for (size_t i=1; i<count; ++i) next = next->next = reinterpret_cast<Block*>(it += size);
		next->next = nullptr;
	}

	MemoryPool::~MemoryPool()
	{
		operator delete(blocks_, info_.size * info_.count);
		try { on_pool_dest(info_); } catch (...) {}
	}

	void* MemoryPool::Alloc()
	{
#if OMEM_THREADSAFE
		std::lock_guard<std::mutex> lock{mutex_};
#endif
		info_.peak = Max(info_.peak, ++info_.cur);
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
}

namespace omem
{
	void SetOnPoolDest(const std::function<void(const PoolInfo&)>& on_pool_dest)
	{
		detail::on_pool_dest = on_pool_dest;
	}

	void SetOnPoolDest(std::function<void(const PoolInfo&)>&& on_pool_dest) noexcept
	{
		detail::on_pool_dest = std::move(on_pool_dest);
	}
}
