#include <type_traits>

#include <immintrin.h>
#include <iostream>

#define BTREENG_FAST_APPEND

#define BTREENG_ABORT(message) do { std::cerr << __FILE__ << ":" << __LINE__ << ": " << message << std::endl; std::abort(); } while(0)

#if defined(_DEBUG)
#undef BTREENG_ASSERT
#define BTREENG_ASSERT(...) do { if(!(__VA_ARGS__)) { BTREENG_ABORT(#__VA_ARGS__); } } while(0)
#else
#define BTREENG_ASSERT(...)
#endif

namespace btreeng
{
	template < typename T > static constexpr uint8_t get_tag_mask() { return alignof(T) - static_cast<uint8_t>(1); }

	template < typename T > T* get_ptr(T* ptr)
	{
		union { T* ptr; uintptr_t bits; } p;
		p.ptr = ptr;
		return reinterpret_cast<T*>(p.bits & ~get_tag_mask<T>());
	}

	template < typename T > uint8_t get_tag(T* ptr)
	{
		union { T* ptr; uintptr_t bits; } p;
		p.ptr = ptr;
		return static_cast<uint8_t>(p.bits & static_cast<uintptr_t>(get_tag_mask< T >()));
	}

	template < typename T > void set_tag(T*& ptr, uint8_t value)
	{
		BTREENG_ASSERT((value & get_tag_mask< T >()) == value);
		union { T* ptr; uintptr_t bits; } p;
		p.ptr = ptr;
		p.bits = reinterpret_cast<uintptr_t>(get_ptr(ptr)) | static_cast<uintptr_t>(value & get_tag_mask<T>());
		ptr = p.ptr;
	}


	template < typename T > struct is_trivial
	{
		static constexpr bool value =
			std::is_trivially_copy_constructible<T>::value &&
			std::is_trivially_move_constructible<T>::value &&
			std::is_trivially_destructible<T>::value;
	};

	template < typename T, size_t N, size_t TypeSize = sizeof(T) > struct array_traits;

	template < typename T > struct array_traits< T, 8, 4 >
	{
		static uint8_t count_lt(const T* keys, uint64_t size, T key)
		{
			auto sizemask = (1u << size) - 1;

			__m256i keyv = _mm256_set1_epi32(key);
			__m256i keysv = _mm256_loadu_si256((__m256i*)keys);

			int maskgt = 0;

			if constexpr (std::is_signed_v< T >)
			{
				__m256i maskgtv = _mm256_cmpgt_epi32(keyv, keysv);
				maskgt = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv));
			}
			else
			{
				__m256i maskgtv = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv, keyv), keysv);
				maskgt = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv));
				maskgt = ~maskgt & 0xFF;
			}

			maskgt &= sizemask;
			return _mm_popcnt_u32(maskgt);
		}

		static uint8_t equal(const T* keys, uint64_t size, T key)
		{
			auto sizemask = (1u << size) - 1;

			__m256i keyv = _mm256_set1_epi32(key);
			__m256i keysv = _mm256_loadu_si256((__m256i*)keys);
			__m256i maskeqv = _mm256_cmpeq_epi32(keyv, keysv);
			int maskeq = _mm256_movemask_ps(_mm256_castsi256_ps(maskeqv));
			maskeq &= sizemask;

			return _tzcnt_u32(maskeq);
		}

		static void move(T* keys, uint8_t size, uint8_t target, uint8_t source)
		{
			static const int permindex[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7 };

			__m256i keysv = _mm256_loadu_si256((__m256i*)keys);
			__m256i permindexv = _mm256_loadu_si256((__m256i*)(permindex + 8 - (target - source)));
			__m256i keysv2 = _mm256_permutevar8x32_epi32(keysv, permindexv);

			static const __m256i storemask = _mm256_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0);
			_mm256_maskstore_epi32((int32_t*)keys, storemask, keysv2);
		}

		static void copy(const T* source, T* target)
		{
			_mm256_storeu_si256((__m256i*)target, _mm256_loadu_si256((__m256i*)source));
		}
	};

	template < typename T > struct array_traits< T, 16, 4 >
	{
		static uint8_t count_lt(const T* keys, uint64_t size, uint32_t key)
		{
			auto sizemask = (1u << size) - 1;

			__m256i keyv = _mm256_set1_epi32(key);

			__m256i keysv1 = _mm256_loadu_si256((__m256i*)keys);
			__m256i keysv2 = _mm256_loadu_si256((__m256i*)(keys + 8));

			unsigned maskgt1 = 0, maskgt2 = 0;
			if constexpr (std::is_signed_v< T >)
			{
				__m256i maskgtv1 = _mm256_cmpgt_epi32(keyv, keysv1);
				__m256i maskgtv2 = _mm256_cmpgt_epi32(keyv, keysv2);
				maskgt1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv1));
				maskgt2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv2));
				maskgt1 &= sizemask;
				maskgt2 &= (sizemask >> 8);
			}
			else
			{				
				__m256i maskgtv1 = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv1, keyv), keysv1);
				__m256i maskgtv2 = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv2, keyv), keysv2);
				maskgt1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv1));
				maskgt2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv2));
				maskgt1 = ~maskgt1 & 0xFF;
				maskgt2 = ~maskgt2 & 0xFF;
			}

			maskgt1 &= sizemask;
			maskgt2 &= (sizemask >> 8);
			
			return _mm_popcnt_u32(maskgt2 << 8 | maskgt1);
		}

		static uint8_t equal(const T* keys, uint64_t size, uint32_t key)
		{
			auto sizemask = (1u << size) - 1;

			__m256i keyv = _mm256_set1_epi32(key);
			__m256i keysv1 = _mm256_loadu_si256((__m256i*)keys);
			__m256i keysv2 = _mm256_loadu_si256((__m256i*)(keys + 8));

			__m256i maskeqv1 = _mm256_cmpeq_epi32(keyv, keysv1);
			int maskeq1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskeqv1));
			maskeq1 &= sizemask;

			__m256i maskeqv2 = _mm256_cmpeq_epi32(keyv, keysv2);
			int maskeq2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskeqv2));
			maskeq2 &= (sizemask >> 8);

			return _tzcnt_u32(maskeq2 << 8 | maskeq1);
		}

		static void move(T* keys, uint8_t size, uint8_t target, uint8_t source)
		{
			// TODO: SSE

			BTREENG_ASSERT(source + 1 == target);

			// 13 is here to not overwrite node data.
			std::memmove(keys + target, keys + source, sizeof(T) * (size - source));
		}

		static void copy(const T* source, T* target)
		{
			_mm256_storeu_si256((__m256i*)target, _mm256_loadu_si256((__m256i*)source));
			_mm256_storeu_si256((__m256i*)(target + 8), _mm256_loadu_si256((__m256i*)(source + 8)));
		}

		static void copy_lane(const T* source, T* target)
		{
			_mm256_storeu_si256((__m256i*)target, _mm256_loadu_si256((__m256i*)source));
		}
	};

	struct metadata
	{
		enum {
			static_node = 0,
			index_node = 1,
			value_node = 2,
		};

		static uint8_t get_node_type(uint8_t metadata)
		{
			BTREENG_ASSERT(metadata <= value_node);
			return metadata;
		}
	};

	template< typename T > struct btree_traits;

	template < typename T, size_t N > struct value_node;

	template < typename T, size_t N, size_t NodeN > struct index_node;
	template < typename T, size_t N, size_t NodeN > struct index_node_group;
	template < typename T, size_t N, size_t NodeN > struct value_node_group;

	template < size_t N > struct value_node< uint32_t, N >
	{
		enum { capacity = 2 * N };

		using value_type = uint32_t;
		using traits_type = array_traits< uint32_t, capacity >;

		uint32_t keys[capacity];
	};	

	template < size_t NodeN > struct index_node< uint32_t, 7, NodeN >
	{
		enum { capacity = 2 * 7 - 1 };

		using traits_type = array_traits< uint32_t, 16 >;
		using value_type = uint32_t;

		using value_node_group_type = value_node_group< uint32_t, 7, NodeN >;
		using index_node_group_type = index_node_group< uint32_t, 7, NodeN >;

		uint32_t keys[capacity];
		uint16_t size;
		uint16_t metadata;
				
		union {
			index_node_group_type* index_group;
			value_node_group_type* value_group;
		};
	};

	template < typename T, size_t N, size_t IndexNodeN > struct index_node_group
	{
		enum { capacity = 2 * N };
		using node_type = index_node< T, N, IndexNodeN >;

		//uint8_t size[capacity];
		alignas(64) node_type node[capacity];
	};

	template < typename T, size_t N, size_t ValueNodeN > struct value_node_group
	{
		enum { capacity = 2 * N };
		using node_type = value_node< T, ValueNodeN >;

		uint8_t size[capacity];

		value_node_group< T, N, ValueNodeN >* left;
		value_node_group< T, N, ValueNodeN >* right;
		alignas(64) node_type node[capacity];
	};
	
	template < typename T > struct btree_static_node;

	// metadata needs to overlap between static and dynamic node
	template<> struct btree_static_node< uint32_t >
	{
		enum { capacity = 7 };
		using value_type = uint32_t;
		using traits_type = array_traits< uint32_t, capacity + 1 >;

		uint32_t keys[capacity];
		uint32_t size : 16;
		uint32_t metadata : 16;
	};

	struct btree_dynamic_node
	{
		void* first; // value_node_group< T, 14 >* first;
		void* last;  // value_node_group< T, 14 >* last;
		void* root;
		uint64_t size : 48;
		uint64_t metadata : 16;
	};

	// This is interesting, it is not a node trait, but insert algorithm that needs to be used with
	// different types of nodes in case they use SIMD.
	template < typename Node, typename Traits = typename Node::traits_type > struct btree_node_default_traits
	{
		using node_type = Node;

		static std::pair< uint8_t, bool > insert(Node& node, uint64_t size, typename Node::value_type key)
		{
			if (size > 0)
			{
				if (size < Node::capacity)
				{
				#if defined(BTREENG_FAST_APPEND)
					// This is not needed, we will either have hint, or not.
					// Check for an append.

					if (node.keys[size - 1] < key)
					{
						node.keys[size] = key;
						return { size, true };
					}
				#endif

					auto index = Traits::equal(node.keys, size, key);
					if (index < size)
					{
						return { index, false };
					}

					index = Traits::count_lt(node.keys, size, key);
					if (index < size)
					{
						Traits::move(node.keys, size, index + 1, index);
					}

					node.keys[index] = key;
					return { index, true };
				}

				return { Node::capacity, false };
			}
			else
			{
				node.keys[0] = key;
				return { 0, true };
			}
		}

		static void insert_separator(Node & node, uint64_t size, typename Node::value_type key)
		{
			BTREENG_ASSERT(size > 0);
			BTREENG_ASSERT(size < Node::capacity);

			auto index = Traits::count_lt(node.keys, size, key);
			if (index < size)
			{
				Traits::move(node.keys, node.size, index + 1, index);
			}

			node.keys[index] = key;
		}
	
		static bool find(const Node& node, uint64_t size, typename Node::value_type key)
		{			
			return Traits::equal(node.keys, size, key) < size;
		}

		static void move(const uint32_t* source, uint32_t* target)
		{
			Traits::copy(source, target);
		}

		static bool full(const Node& node)
		{
			return node.size == Node::capacity;
		}

		static bool full(uint64_t size)
		{
			return size == Node::capacity;
		}
	};


	template < typename Node, typename Traits = typename Node::traits_type > struct btree_node_traits
		: btree_node_default_traits< Node, Traits >
	{};	

	/*
	template < typename T, size_t Size > struct btree_node_traits< value_node< T, Size > >
		: btree_node_default_traits< value_node< T, Size > >
	{
	
	};
	*/

	template < typename T, size_t IndexNodeSize, size_t ValueNodeSize > struct btree_node_traits< index_node< T, IndexNodeSize, ValueNodeSize > >
		: btree_node_default_traits< index_node< T, IndexNodeSize, ValueNodeSize > >
	{
		using index_node_type = typename btree_node_default_traits< index_node< T, IndexNodeSize, ValueNodeSize > >::node_type;
		
		static bool find(const index_node_type& node, T key)
		{
			using traits = typename index_node_type::traits_type;

			// TODO: we want count_lte
			auto index = traits::count_lt(node.keys, node.size, key);
			if (index < node.size)
			{
				index += node.keys[index] == key;
			}

			BTREENG_ASSERT(node.size > 0);
			BTREENG_ASSERT(index <= node.size);

			switch (metadata::get_node_type(node.metadata))
			{
			case metadata::value_node:
				return btree_node_traits< typename btree_traits< T >::value_node_type >::find(node.value_group->node[index], node.value_group->size[index], key);
			case metadata::index_node:
				return btree_node_traits< typename btree_traits< T >::index_node_type >::find(node.index_group->node[index], key);
			default:
				BTREENG_ABORT("unreachable");
			}
		}

		template< typename Allocator > static void destroy(index_node_type& node, Allocator&& alloc)
		{
		
		}
	};

	template < typename Node > struct btree_node_group_traits;

	/*
	template < typename T, size_t IndexNodeSize, size_t ValueNodeSize > struct btree_node_group_traits< index_node_group< T, IndexNodeSize, ValueNodeSize > >
	{
		using node_type = index_node_group< T, IndexNodeSize, ValueNodeSize >;

		static void split(node_type& lnode, node_type& rnode)
		{}
	};

	template < typename T, size_t IndexNodeSize, size_t ValueNodeSize > struct btree_node_group_traits< value_node_group< T, IndexNodeSize, ValueNodeSize > >
	{
		using node_group_type = value_node_group< T, IndexNodeSize, ValueNodeSize >;

		static void split(const node_group_type& lgroup, node_group_type& rgroup)
		{				
			for (size_t i = 0; i < node_group_type::capacity / 2; ++i)
			{
				rgroup.size[i] = lgroup.size[i + node_group_type::capacity / 2];

				btree_node_traits< typename btree_traits< T >::value_node_type >::move(
					lgroup.node[i + node_group_type::capacity / 2].keys,
					rgroup.node[i].keys
				);
			}
		}
	};
	*/

	// template < typename Node, typename Traits = typename Node::traits_type > struct btree_node_traits

	/*
	template < typename IndexNode > struct btree_node_group_traits
	{
		template < typename Node > static void move(IndexNode& inode, Node* node, uint8_t target, uint8_t source)
		{
			using traits = Node::traits_type;

			BTREENG_ASSERT(inode.size > 0);

			if (target > source)
			{
				uint8_t diff = target - source;
				uint8_t count = inode.size + 1 - source;

				Node* end = node + inode.size;
				while (count--)
				{
					traits::copy(end->keys, (end + diff)->keys);
					--end;
				}			
			}
		}
	};
	*/
			
	template<> struct btree_traits< uint32_t >
	{
		enum {
			static_node_size = 7,
			value_node_size = 8,
			index_node_size = 7,			
		};
		
		using value_node_type = value_node< uint32_t, value_node_size >;
		using index_node_type = index_node< uint32_t, index_node_size, value_node_size >;
		using index_node_group_type = index_node_group< uint32_t, index_node_size, value_node_size >;
		using value_node_group_type = value_node_group< uint32_t, index_node_size, value_node_size >;

		static_assert(sizeof(btree_static_node< uint32_t >) == 32);
		static_assert(sizeof(btree_dynamic_node) == 32);
		static_assert(sizeof(value_node_type) == 64);
		static_assert(sizeof(index_node_type) == 64);
	};

	template< typename T, typename Allocator = std::allocator< T >, typename Traits = btree_traits< T > > struct btree
	{	
		using value_node_type = typename Traits::value_node_type;
		using index_node_type = typename Traits::index_node_type;
		using index_node_group_type = typename Traits::index_node_group_type;
		using value_node_group_type = typename Traits::value_node_group_type;

		struct iterator {};

		btree() = default;

		btree(Allocator allocator)
			: allocator_(allocator)
		{}

		~btree()
		{
			switch (get_root_type())
			{
			case metadata::value_node:
				get_allocator< value_node_type >().deallocate(reinterpret_cast<value_node_type*>(dynamic_.root), 1);
				break;
			case metadata::index_node:
				break;
			}
		}

		std::pair< iterator, bool > insert(T key)
		{
			switch (get_root_type())
			{
			case metadata::static_node:
				return full(static_) ? promote(static_, key) : insert(static_, key);
			case metadata::value_node:
			{
				auto node = reinterpret_cast<value_node_type*>(dynamic_.root);
				if (full(*node, dynamic_.size))
				{
					auto pb = promote(*node, key);
					get_allocator< value_node_type >().deallocate(node, 1);
					return pb;
				}
				else
				{
					return insert(*node, key);
				}
			}
			case metadata::index_node:
				return insert(reinterpret_cast<index_node_type*>(dynamic_.root), key, nullptr);
			default:
				BTREENG_ABORT("unreachable");
			}
		}

		size_t size() const
		{
			// TODO: it would be cool to have size aligned in a way that we do not have to branch here.
			switch (get_root_type())
			{
			case metadata::static_node:
				return static_.size;
			default:
				return dynamic_.size;
			}
		}

		bool find(T key)
		{
			switch (get_root_type())
			{
			case metadata::static_node:
				return btree_node_traits< btree_static_node< T > >::find(static_, static_.size, key);
			case metadata::value_node:
				return btree_node_traits< value_node_type >::find(*reinterpret_cast<value_node_type*>(dynamic_.root), dynamic_.size, key);
			case metadata::index_node:
			{
				BTREENG_ASSERT(verify_node(*reinterpret_cast<index_node_type*>(dynamic_.root)));
				return btree_node_traits< index_node_type >::find(*reinterpret_cast<index_node_type*>(dynamic_.root), key);
			}
			default:
				BTREENG_ABORT("unreachable");
			}
		}

	private:
		std::pair< iterator, bool > insert(btree_static_node< T >& node, T key)
		{
			using traits = btree_node_traits< btree_static_node< T > >;
			auto [index, inserted] = traits::insert(node, node.size, key);
			if (index < traits::node_type::capacity)
			{
				if (inserted)
				{
					node.size += 1;
				}
			}
				
			return { iterator(), inserted };
		}

		std::pair< iterator, bool > promote(btree_static_node< T >& node, T key)
		{
			using traits = btree_node_traits< btree_static_node< T > >;

			auto vnode = get_allocator< value_node_type >().allocate(1);
			traits::move(node.keys, vnode->keys);

			btree_node_traits< value_node_type >::insert(*vnode, traits::node_type::capacity, key);

			dynamic_.metadata = metadata::value_node;
			dynamic_.root = vnode;
			dynamic_.size = traits::node_type::capacity + 1;

			return { iterator(), true };
		}

		std::pair< iterator, bool > insert(value_node_type& node, T key)
		{
			using traits = btree_node_traits< value_node_type >;

			uint64_t size = dynamic_.size;
			auto [index, inserted] = traits::insert(node, size, key);
			if (index < traits::node_type::capacity)
			{
				if (inserted)
				{
					dynamic_.size += 1;
				}
			}
					
			return { iterator(), inserted };
		}

		// TODO: duplicate of above
		std::pair< iterator, bool > insert(value_node_type& node, uint8_t& size, T key)
		{
			using traits = btree_node_traits< value_node_type >;
						
			auto [index, inserted] = traits::insert(node, size, key);
			if (index < traits::node_type::capacity)
			{
				if (inserted)
				{
					size += 1;
					dynamic_.size += 1;
				}
			}

			return { iterator(), inserted };
		}

		T split(const value_node_type& node, value_node_group_type& group, T key)
		{
			// TODO
			static_assert(value_node_type::capacity == 16);

		#if defined(BTREENG_FAST_APPEND)
			if (node.keys[dynamic_.size - 1] < key)
			{
				group.size[0] = value_node_type::capacity;
				value_node_type::traits_type::copy(node.keys, group.node[0].keys);
				group.size[1] = 0;

				return key;
			}
		#endif

			// split node into empty group
			using traits = typename value_node_type::traits_type;

			traits::copy_lane(node.keys, group.node[0].keys);
			group.size[0] = value_node_type::capacity / 2;

			traits::copy_lane(node.keys + 8, group.node[1].keys);
			group.size[1] = value_node_type::capacity / 2;

			return group.node[1].keys[0];
		}
				
		std::pair< iterator, bool > promote(value_node_type& node, T key)
		{
			BTREENG_ASSERT(dynamic_.size == value_node_type::capacity);

			using traits = btree_node_traits< value_node_type >;

			auto inode = get_allocator< index_node_type >().allocate(1);
			auto vgroup = get_allocator< value_node_group_type >().allocate(1);

			inode->size = 1;
			inode->metadata = metadata::value_node;
			inode->value_group = vgroup;

			dynamic_.metadata = metadata::index_node;
			dynamic_.root = inode;
						
			inode->keys[0] = split(node, *vgroup, key);

			auto n = key < inode->keys[0] ? 0 : 1;
			auto [index, inserted] = traits::insert(vgroup->node[n], vgroup->size[n], key);
			if (inserted)
			{
				vgroup->size[n] += 1;
				dynamic_.size += 1;
			}
			return { iterator(), inserted };
		}
				
		template < typename NodeGroup > void move(NodeGroup& target, uint8_t tindex, const NodeGroup& source, uint8_t sindex)
		{
			// TODO: in case it is index_node that is moved, group pointer and metadata get copied by traits_type::copy.
			NodeGroup::node_type::traits_type::copy(source.node[sindex].keys, target.node[tindex].keys);

			if constexpr (std::is_same_v< NodeGroup, value_node_group_type >)
			{
				target.size[tindex] = source.size[sindex];
			}
		}

		void move(index_node_type* node, uint8_t target, uint8_t source)
		{
			BTREENG_ASSERT(!full(*node));
			BTREENG_ASSERT(source + 1 == target);
			BTREENG_ASSERT(target <= index_node_type::capacity);

			// TODO: this really should use index vectors to speed up moves.

			const auto count = target - source;

			switch (metadata::get_node_type(node->metadata))
			{
			case metadata::value_node:
				for (uint8_t i = node->size + 1; i > source; --i)
				{
					move(*node->value_group, i, *node->value_group, i - 1);
				}
				break;
			case metadata::index_node:
				for (uint8_t i = node->size + 1; i > source; --i)
				{
					move(*node->index_group, i, *node->index_group, i - 1);
				}
				break;
			default:
				BTREENG_ABORT("unreachable");
			}
		}

		template < typename NodeGroup > void split_group(const NodeGroup& group, NodeGroup& lgroup, NodeGroup& rgroup)
		{
			for (size_t i = 0; i < NodeGroup::capacity / 2; ++i)
			{
				move(lgroup, i, group, i);
				move(rgroup, i, group, i + NodeGroup::capacity / 2);

				if constexpr (std::is_same_v< NodeGroup, value_node_group_type >)
				{
					lgroup.size[i] = group.size[i];
					rgroup.size[i] = group.size[i + NodeGroup::capacity / 2];
				}
			}
		}

		template < typename NodeGroup > void split_group(const NodeGroup& lgroup, NodeGroup& rgroup)
		{
			for (size_t i = 0; i < NodeGroup::capacity / 2; ++i)
			{				
				move(rgroup, i, lgroup, i + NodeGroup::capacity / 2);
			}
		}

		T split(value_node_group_type& group, uint8_t index)
		{
			static_assert(value_node_type::capacity == 16);

			BTREENG_ASSERT(group.size[index] == value_node_type::capacity);

			using traits = typename value_node_type::traits_type;
			traits::copy_lane(group.node[index].keys + 8, group.node[index + 1].keys);

			group.size[index] = value_node_type::capacity / 2;
			group.size[index + 1] = value_node_type::capacity / 2;

			return group.node[index + 1].keys[0];
		}

		T split_node(const index_node_type& node, index_node_type& lnode, index_node_type& rnode)
		{
			// TODO: sse
			std::memcpy(lnode.keys, node.keys, sizeof(T) * index_node_type::capacity / 2);
			lnode.size = index_node_type::capacity / 2;
			
			std::memcpy(rnode.keys, node.keys + index_node_type::capacity / 2 + 1, sizeof(T) * index_node_type::capacity / 2);
			rnode.size = index_node_type::capacity / 2;

			lnode.metadata = rnode.metadata = node.metadata;

			switch (metadata::get_node_type(node.metadata))
			{
			case metadata::value_node:
				lnode.value_group = get_allocator< value_node_group_type >().allocate(1);
				rnode.value_group = get_allocator< value_node_group_type >().allocate(1);
				split_group(*node.value_group, *lnode.value_group, *rnode.value_group);
				break;
			case metadata::index_node:
				lnode.index_group = get_allocator< index_node_group_type >().allocate(1);
				rnode.index_group = get_allocator< index_node_group_type >().allocate(1);
				split_group(*node.index_group, *lnode.index_group, *rnode.index_group);
				break;
			default:
				BTREENG_ABORT("unreachable");
			}
			
			BTREENG_ASSERT(verify_node(lnode));
			BTREENG_ASSERT(verify_node(rnode));

			return node.keys[index_node_type::capacity / 2];
		}

		T split_node(index_node_type& lnode, index_node_type& rnode)
		{			
			lnode.size = index_node_type::capacity / 2;

			std::memcpy(rnode.keys, lnode.keys + index_node_type::capacity / 2 + 1, sizeof(T) * index_node_type::capacity / 2);
			rnode.size = index_node_type::capacity / 2;
			rnode.metadata = lnode.metadata;

			switch (metadata::get_node_type(lnode.metadata))
			{
			case metadata::value_node:
				rnode.value_group = get_allocator< value_node_group_type >().allocate(1);
				split_group(*lnode.value_group, *rnode.value_group);
				break;
			case metadata::index_node:
				rnode.index_group = get_allocator< index_node_group_type >().allocate(1);
				split_group(*lnode.index_group, *rnode.index_group);
				break;
			default:
				BTREENG_ABORT("unreachable");
			}
			
			BTREENG_ASSERT(verify_node(lnode));
			BTREENG_ASSERT(verify_node(rnode));

			return lnode.keys[index_node_type::capacity / 2];
		}

		uint8_t split(index_node_type* node, uint8_t index, T key)
		{
			BTREENG_ASSERT(!full(*node));
			BTREENG_ASSERT(metadata::get_node_type(node->metadata) == metadata::value_node);

			switch (metadata::get_node_type(node->metadata))
			{
			case metadata::value_node:
			{
			#if defined(BTREENG_FAST_APPEND)
				if (index == node->size && node->value_group->node[index].keys[node->value_group->size[index] - 1] < key)
				{
					node->keys[index] = key;
					node->size += 1;
					node->value_group->size[index + 1] = 0;

					BTREENG_ASSERT(verify_node(*node));

					return index + 1;
				}
			#endif

				if (index < node->size)
				{
					move(node, index + 2, index + 1);
				}

				T splitkey = split(*node->value_group, index);
				btree_node_traits< index_node_type >::insert_separator(*node, node->size, splitkey);
				node->size += 1;

				BTREENG_ASSERT(verify_node(*node));
				return key < splitkey ? index : index + 1;
			}
			case metadata::index_node:
			{
				if (index < node->size)
				{
					move(node, index + 2, index + 1);
				}

				T splitkey = split_node(node->index_group->node[index], node->index_group->node[index + 1]);
			}
			default:
				BTREENG_ABORT("unreachable");
			}
		}

		index_node_type* rebalance(index_node_type* node, T key, index_node_type* parent)
		{			
			BTREENG_ASSERT(full(*node));
			
			if (metadata::get_node_type(node->metadata) == metadata::value_node)
			{
				auto last = index_node_type::capacity - 1;
				if (node->value_group->node[last].keys[node->value_group->size[last]] < key)
				{
					int append(1);
				}
			}

			if (parent)
			{
				BTREENG_ASSERT(!full(*parent));
										
				T splitkey = node->keys[index_node_type::capacity / 2];

				using traits = typename index_node_type::traits_type;

				auto index = traits::count_lt(parent->keys, parent->size, splitkey);

				if (index < parent->size)
				{
					// Insert at proper place
					btree_node_traits< index_node_type >::insert_separator(*parent, parent->size, splitkey);

					// Make space for new node
					move(parent, index + 2, index + 1);
				}
				else
				{
					// Insert at end
					parent->keys[index] = splitkey;

					// Nothing to move
				}

				parent->size += 1;

				switch (metadata::get_node_type(parent->metadata))
				{
				case metadata::value_node:
					// TODO: this case
					split(*parent->value_group, index);
					break;
				case metadata::index_node:
					split_node(parent->index_group->node[index], parent->index_group->node[index + 1]);
					break;
				default:
					BTREENG_ABORT("unreachable");
				}

				BTREENG_ASSERT(verify_node(parent->index_group->node[index]));
				BTREENG_ASSERT(verify_node(parent->index_group->node[index + 1]));				
				BTREENG_ASSERT(verify_node(*parent));

				return key < parent->keys[index] ? &parent->index_group->node[index] : &parent->index_group->node[index + 1];
			}
			else
			{					
				auto root = get_allocator< index_node_type >().allocate(1);
				root->metadata = metadata::index_node;
				root->index_group = get_allocator< index_node_group_type >().allocate(1);
				root->size = 1;
				root->keys[0] = split_node(*node, root->index_group->node[0], root->index_group->node[1]);
				
				// Set as root so verify can be relaxed.
				dynamic_.root = root;

				BTREENG_ASSERT(verify_node(*root));			

				// TODO: delete node
				//btree_node_traits< index_node_type >::destroy(*node, get_allocator< index_node_type >());
				switch (metadata::get_node_type(node->metadata))
				{
				case metadata::index_node:
					get_allocator< index_node_group_type >().deallocate(node->index_group, 1);
					break;
				case metadata::value_node:
					get_allocator< value_node_group_type >().deallocate(node->value_group, 1);
					break;
				default:
					BTREENG_ABORT("unreachable");
				}

				get_allocator< index_node_type >().deallocate(node, 1);

				return key < root->keys[0] ? &root->index_group->node[0] : &root->index_group->node[1];
			}
		}
				
		std::pair< iterator, bool > insert(index_node_type* node, T key, index_node_type* parent)
		{
			using traits = typename index_node_type::traits_type;	
						
			switch (metadata::get_node_type(node->metadata))
			{
			case metadata::index_node:
			{
				if (full(*node))
				{
					node = rebalance(node, key, parent);
				}

				auto index = traits::count_lt(node->keys, node->size, key);
				return insert(&node->index_group->node[index], key, node);
			}
			case metadata::value_node:
			{				
				auto index = traits::count_lt(node->keys, node->size, key);
				if (full(node->value_group, index))
				{
					if (full(*node))
					{
						node = rebalance(node, key, parent);
						return insert(node, key, parent);
					}
					else
					{
						index = split(node, index, key);
						return insert(node->value_group->node[index], node->value_group->size[index], key);
					}
				}
				else
				{
					return insert(node->value_group->node[index], node->value_group->size[index], key);
				}
			}
			
			default:
				BTREENG_ABORT("unreachable");
			}			
		}

		template < typename Node > bool full(const Node& node) const
		{
			return btree_node_traits< Node >::full(node);
		}

		template < typename Node > bool full(const Node&, uint64_t size) const
		{
			return btree_node_traits< Node >::full(size);
		}

		bool full(value_node_group_type* group, uint8_t index)
		{
			return full(group->node[index], group->size[index]);
		}

		template < typename Node > auto get_allocator()
		{
			return std::allocator_traits< Allocator >::rebind_alloc< Node >(allocator_);
		}
				
		uint8_t get_root_type() const
		{
			return metadata::get_node_type(static_.metadata);
		}

		union
		{
			alignas(32) btree_static_node< T > static_;
			alignas(32) btree_dynamic_node dynamic_{};
		};
		
		bool verify_node(const index_node_type& node)
		{	
			if (&node != dynamic_.root)
			{
				BTREENG_ASSERT(node.size >= index_node_type::capacity / 2);
				BTREENG_ASSERT(node.size <= index_node_type::capacity);
			}

			switch (metadata::get_node_type(node.metadata))
			{
			case metadata::index_node:
			{				
				for (size_t i = 0; i < node.size + 1; ++i)
				{
					auto n = node.index_group->node[i];
					auto n0 = node.index_group->node[0];
					BTREENG_ASSERT(n.metadata != 0);
					BTREENG_ASSERT(metadata::get_node_type(n.metadata) == metadata::get_node_type(n0.metadata));		
					BTREENG_ASSERT(n.size >= index_node_type::capacity / 2);
					BTREENG_ASSERT(n.size <= index_node_type::capacity);
				}
				break;
			}
			case metadata::value_node:
			{				
				for (size_t i = 0; i < node.size + 1; ++i)
				{
					auto size = node.value_group->size[i];
					BTREENG_ASSERT(size <= value_node_type::capacity);

				#if defined(BTREENG_FAST_APPEND)
					// TODO: this is true for 'last last' node only
					if (i != node.size)	{
				#endif
						BTREENG_ASSERT(size >= value_node_type::capacity / 2);
				#if defined(BTREENG_FAST_APPEND)
					}
				#endif
				}
				break;
			}
			default:
				BTREENG_ABORT("unreachable");
			}

			return true;
		}

		Allocator allocator_;
	};
}