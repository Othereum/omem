#pragma once
#include <algorithm>
#include <cassert>
#include <new>
#include <unordered_map>

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
		MemoryPool(size_t size, size_t count)
			:next_{nullptr}, blocks_{nullptr}, info_{size, count}
		{
			assert(size >= sizeof(Block));
			if (count == 0) return;

			blocks_ = operator new(size * count);
			
			auto* it = static_cast<char*>(blocks_);
			auto* next = next_ = static_cast<Block*>(blocks_);
			
			for (size_t i=1; i<count; ++i)
				next = next->next = reinterpret_cast<Block*>(it += size);
			
			next->next = nullptr;
		}
		
		MemoryPool(MemoryPool&& r) noexcept
			:next_{r.next_}, blocks_{r.blocks_}, info_{r.info_}
		{
			r.next_ = nullptr;
			r.blocks_ = nullptr;
			info_ = {};
		}
		
		~MemoryPool()
		{
			if (blocks_) operator delete(blocks_);
		}

		MemoryPool& operator=(MemoryPool&& r) noexcept
		{
			MemoryPool{std::move(r)}.swap(*this);
			return *this;
		}

		MemoryPool(const MemoryPool&) = delete;
		MemoryPool& operator=(const MemoryPool&) = delete;

		[[nodiscard]] void* Alloc()
		{
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

		void swap(MemoryPool& r) noexcept
		{
			using std::swap;
			swap(next_, r.next_);
			swap(blocks_, r.blocks_);
			swap(info_, r.info_);
		}

	private:
		struct Block { Block* next; } *next_;
		void* blocks_;
		PoolInfo info_;
	};

	class MemoryPoolManager
	{
	public:
		template <class T, class... Args>
		[[nodiscard]] T* New(Args&&... args)
		{
			auto* const p = Alloc(sizeof(T));
			try { return new (p) T{std::forward<Args>(args)...}; }
			catch (...) { Free(p, sizeof(T)); throw; }
		}

		template <class T, class... Args>
		[[nodiscard]] T* NewArr(size_t n, Args&&... args)
		{
			const auto p = Alloc(n * sizeof(T));
			try { return new (p) T[n]{std::forward<Args>(args)...}; }
			catch (...) { Free(p, n * sizeof(T)); throw; }
		}

		template <class T>
		void Delete(T* p) noexcept
		{
			p->~T();
			Free(p, sizeof(T));
		}

		template <class T>
		void DeleteArr(T* p, size_t n) noexcept
		{
			for (size_t i=0; i<n; ++i) p[i].~T();
			Free(p, n * sizeof(T));
		}

		[[nodiscard]] void* Alloc(size_t size)
		{
			return Get(size).Alloc();
		}

		void Free(void* p, size_t size) noexcept
		{
			Get(size).Free(p);
		}
		
		MemoryPool& Get(size_t size)
		{
			constexpr auto pool_size = size_t(1) << LogCeil(OMEM_POOL_SIZE, 2);
			constexpr auto min_log = LogCeil(sizeof(void*), 2);
			const auto log = std::max(LogCeil(size, 2), min_log);
			const auto real_size = size_t(1) << log;
			return pools_.try_emplace(log, real_size, pool_size/real_size).first->second;
		}

		[[nodiscard]] auto& Pools() const noexcept { return pools_; }

	private:
		std::unordered_map<size_t, MemoryPool> pools_;
	};
}
