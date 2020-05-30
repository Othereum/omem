#pragma once
#include <cassert>
#include <vector>
#include <thread>
#include <iostream>

namespace omem
{
	namespace detail
	{
		template <class T1, class T2>
		[[nodiscard]] constexpr auto Min(T1 a, T2 b) noexcept
		{
			using T = std::common_type_t<T1, T2>;
			return static_cast<T>(a) < static_cast<T>(b) ? a : b;
		}

		template <class T1, class T2, class T3, class... Ts>
		[[nodiscard]] constexpr auto Min(T1 x1, T2 x2, T3 x3, Ts... xs) noexcept
		{
			return Min(Min(x1, x2), x3, xs...);
		}

		template <class T1, class T2>
		[[nodiscard]] constexpr auto Max(T1 a, T2 b) noexcept
		{
			using T = std::common_type_t<T1, T2>;
			return static_cast<T>(a) > static_cast<T>(b) ? a : b;
		}

		template <class T1, class T2, class T3, class... Ts>
		[[nodiscard]] constexpr auto Max(T1 x1, T2 x2, T3 x3, Ts... xs) noexcept
		{
			return Max(Max(x1, x2), x3, xs...);
		}
		
		template <std::integral T1, std::integral T2>
		constexpr T1 Log(T1 x, T2 base) noexcept
		{
			T1 cnt = 0;
			while ((x /= base) > 0) ++cnt;
			return cnt;
		}
		
		template <std::integral T1, std::integral T2>
		constexpr T1 LogCeil(T1 x, T2 base) noexcept
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
		
		template <size_t BlockSizeLog2>
		class MemoryPool
		{
		public:
			static constexpr auto block_size_ = 1 << BlockSizeLog2;
			
			static MemoryPool& Get() noexcept
			{
				thread_local MemoryPool pool;
				return pool;
			}

			template <class T>
			T* Allocate(const size_t count)
			{
				const auto need_bytes = count * sizeof T;
				const auto need_blocks = CountBlocks(need_bytes);
				
				for (auto& c : containers_)
				{
					if (c.remaining * block_size_ < need_bytes) continue;
					
					size_t continuous = 0;
					for (size_t cur_block = c.first_available; cur_block < c.blocks.size(); ++cur_block)
					{
						if (c.occupied[cur_block]) continuous = 0;
						else if (++continuous * block_size_ >= need_bytes)
						{
							const auto begin = cur_block - need_blocks + 1;
							return Allocate<T>(c, begin, need_blocks);
						}
					}
				}

				const auto new_blocks = Max(total_blocks_ / 2, need_blocks);
				total_blocks_ += new_blocks;
				peak_blocks_ = Max(peak_blocks_, total_blocks_);
				
				auto& new_container = containers_.emplace_back(new_blocks);
				peak_containers_ = detail::Max(peak_containers_, containers_.size());
				
				return Allocate<T>(new_container, 0, need_blocks);
			}
			
			template <class T>
			void Deallocate(T* ptr, size_t count) noexcept
			{
				const auto p = reinterpret_cast<Block*>(ptr);
				
				auto it = containers_.begin();
				assert(("Attempted to deallocate while no memory allocated", it != containers_.end()));
				
				while (it->blocks.data() > p || p >= it->blocks.data() + it->blocks.size())
				{
					++it;
					assert(("Attempted to deallocate invalid pointer", it != containers_.end()));
				}
				
				const auto blocks = CountBlocks(sizeof T * count);
				total_blocks_ -= blocks;
				
				it->remaining += blocks;
				assert(it->remaining <= it->blocks.size());
				if (it->remaining >= it->blocks.size())
				{
					containers_.erase(it);
					return;
				}
				
				const auto begin = static_cast<size_t>(p - it->blocks.data());
				for (auto i = begin; i < begin+blocks; ++i)
				{
					assert(("Deallocating empty block", it->occupied[i]));
					it->occupied[i] = false;
				}
				
				it->first_available = detail::Min(it->first_available, begin);
			}
			
			MemoryPool(const MemoryPool&) = delete;
			MemoryPool(MemoryPool&&) = delete;
			MemoryPool& operator=(const MemoryPool&) = delete;
			MemoryPool& operator=(MemoryPool&&) = delete;
			
		private:
			struct Block
			{
				// ReSharper disable once CppPossiblyUninitializedMember
				Block() noexcept {}
				char block[block_size_];
			};

			struct BlockContainer
			{
				explicit BlockContainer(size_t count)
					:blocks(count), occupied(count), remaining{count}
				{
				}
				
				std::vector<Block> blocks;
				std::vector<bool> occupied;
				size_t first_available = 0;
				size_t remaining;
			};

			static constexpr size_t CountBlocks(size_t bytes) noexcept
			{
				return bytes/block_size_ + Min(1, bytes%block_size_);
			}

			template <class T>
			static T* Allocate(BlockContainer& c, size_t begin, size_t blocks)
			{
				const auto end = begin + blocks;
				auto i = begin;
				for (; i < end; ++i) c.occupied[i] = true;
				for (; i < c.blocks.size() && c.occupied[i]; ++i) {}
				c.first_available = i;
				c.remaining -= blocks;
				return reinterpret_cast<T*>(c.blocks.data() + begin);
			}

			MemoryPool() = default;
			~MemoryPool()
			{
				if (!containers_.empty()) std::cout << "[omem] WARNING: Memory leak detected\n";

				static constexpr const char* units[]{"B", "KB", "MB", "GB", "TB", "PB", "EB"};
				const auto bytes = block_size_ * peak_blocks_;
				const auto log = Log(bytes, 1024);

				std::cout << "[omem] Peak usage of " << block_size_ << " byte memory pool on thread " << std::this_thread::get_id()
					<< ": " << peak_containers_ << " containers with " << peak_blocks_ << " blocks (" << (bytes >> 10 * log)
					<< ' ' << units[log] << ")\n";
			}
			
			std::vector<BlockContainer> containers_;
			size_t total_blocks_ = 0;
			size_t peak_blocks_ = 0;
			size_t peak_containers_ = 0;
		};
	}
	
	template <class T>
	T* Allocate(size_t count)
	{
		return detail::MemoryPool<detail::LogCeil(sizeof T, 2)>::Get().template Allocate<T>(count);
	}
	
	template <class T>
	void Deallocate(T* ptr, size_t count) noexcept
	{
		return detail::MemoryPool<detail::LogCeil(sizeof T, 2)>::Get().Deallocate(ptr, count);
	}
	
	template <class T>
	struct Allocator
	{
		using value_type = T;

		constexpr Allocator() noexcept = default;

		template <class Y>
		constexpr Allocator(const Allocator<Y>&) noexcept {}
		
		T* allocate(size_t count) const
		{
			return Allocate<T>(count);
		}

		void deallocate(T* ptr, size_t count) const noexcept
		{
			Deallocate(ptr, count);
		}

		constexpr bool operator==(const Allocator&) const noexcept { return true; }
	};
}
