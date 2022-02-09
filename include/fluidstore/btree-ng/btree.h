#include <type_traits>

#include <immintrin.h>

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
		assert((value & get_tag_mask< T >()) == value);
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
				maskgt = ~maskgt;
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

		static void move(T* keys, uint8_t target, uint8_t source)
		{
			static const int permindex[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7 };

			__m256i keysv = _mm256_loadu_si256((__m256i*)keys);
			__m256i permindexv = _mm256_loadu_si256((__m256i*)(permindex + 8 - (target - source)));
			__m256i keysv2 = _mm256_permutevar8x32_epi32(keysv, permindexv);		
				
			static const __m256i storemask = _mm256_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0);
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

			int maskgt1 = 0, maskgt2 = 0;
			if constexpr (std::is_signed_v< T >)
			{
				__m256i maskgtv1 = _mm256_cmpgt_epi32(keyv, keysv1);
				__m256i maskgtv2 = _mm256_cmpgt_epi32(keyv, keysv2);
				maskgt1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv1));
				maskgt2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv2));

			}
			else
			{
				__m256i maskgtv1 = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv1, keyv), keysv1);
				__m256i maskgtv2 = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv2, keyv), keysv2);
				maskgt1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv1));
				maskgt2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv2));
				maskgt1 = ~maskgt1;
				maskgt2 = ~maskgt2;
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

		static void move(T* keys, uint8_t target, uint8_t source)
		{
			// TODO: 
			std::abort();
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

	template < typename T, size_t N, size_t NodeN > struct value_node_group;
	template < typename T, size_t N, size_t NodeN > struct index_node_group;
		
	template < typename T, size_t N, size_t NodeN > struct index_node;

	template < size_t NodeN > struct index_node< uint32_t, 7, NodeN >
	{
		enum { capacity = 2 * 7 - 1 };

		using traits_type = array_traits< uint32_t, 16 >;

		uint32_t keys[capacity];
		uint16_t size;
		uint16_t metadata;

		// Where to put info about node type? keys_size seems underused.
		union {
			index_node_group< uint32_t, 7, NodeN >* index_group;
			value_node_group< uint32_t, 7, NodeN >* value_group;
		};
	};

	template < typename T, size_t N > struct value_node;

	template < typename T, size_t N, size_t ValueNodeN > struct value_node_group
	{
		enum { capacity = 2 * N };

		uint8_t size[capacity];

		value_node_group< T, N, ValueNodeN >* left;
		value_node_group< T, N, ValueNodeN >* right;
		alignas(64) value_node< T, ValueNodeN > node[capacity];
	};

	template < typename T, size_t N, size_t IndexNodeN > struct index_node_group
	{
		enum { capacity = 2 * N };

		uint8_t size[capacity];
		alignas(64) index_node< T, N, IndexNodeN > node[capacity];
	};

	template < size_t N > struct value_node< uint32_t, N >
	{
		enum { capacity = 2*N };

		using value_type = uint32_t;
		using traits_type = array_traits< uint32_t, capacity >;

		uint32_t keys[capacity];
	};

	template < typename T > struct btree_static_node;

	// root and meta needs to overlap
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

	template < typename Node, typename Traits = typename Node::traits_type > struct btree_node_traits
	{
		using node_type = Node;

		static std::pair< uint8_t, bool > insert(Node& node, uint64_t size, typename Node::value_type key)
		{
			if (size > 0)
			{
				if (size < Node::capacity)
				{
					auto index = Traits::equal(node.keys, size, key);
					if (index < size)
					{
						return { index, false };
					}

					index = Traits::count_lt(node.keys, size, key);
					if (index < size)
					{
						Traits::move(node.keys, index + 1, index);
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

		static bool find(const Node& node, uint64_t size, typename Node::value_type key)
		{
			return Traits::equal(node.keys, size, key) < size;
		}

		static void move(const uint32_t* source, uint32_t* target)
		{
			Traits::copy(source, target);
		}
	};

	template< typename T > struct btree_traits;

	template<> struct btree_traits< uint32_t >
	{
		enum {
			static_node_size = 7,
			value_node_size = 8,
			index_node_size = 7,			
		};

		static_assert(sizeof(btree_static_node< uint32_t >) == 32);
		static_assert(sizeof(value_node< uint32_t, value_node_size >) == 64);
		static_assert(sizeof(index_node< uint32_t, index_node_size, value_node_size >) == 64);
	};

	template< typename T, typename Allocator = std::allocator< T >, typename Traits = btree_traits< T > > struct btree
	{
		enum class node_type : uint8_t
		{
			static_node = 0,
			value_node = 1,
			index_node = 2,
		};

		using value_node_type = value_node< T, Traits::value_node_size >;
		using value_node_group_type = value_node_group< T, Traits::index_node_size, Traits::value_node_size >;

		using index_node_type = index_node< T, Traits::index_node_size, Traits::value_node_size >;
		using index_node_group_type = index_node< T, Traits::index_node_size, Traits::value_node_size >;

		struct iterator {};

		std::pair< iterator, bool > insert(T key)
		{
			switch (get_root_type())
			{
			case node_type::static_node:
				return insert(static_, key);
			case node_type::value_node:
				return insert(*reinterpret_cast<value_node_type*>(dynamic_.root), key);
			case node_type::index_node:
				return insert(*reinterpret_cast<index_node_type*>(dynamic_.root), key);
			default:
				std::abort();
			}
		}

		size_t size() const
		{
			// TODO: it would be cool to have size aligned in a way that we do not have to branch here.
			switch (get_root_type())
			{
			case node_type::static_node:
				return static_.size;
			default:
				return dynamic_.size;
			}
		}

		bool find(T key)
		{
			switch (get_root_type())
			{
			case node_type::static_node:
				return find(static_, key);
			case node_type::value_node:
				return find(*reinterpret_cast<value_node_type*>(dynamic_.root), key);
			case node_type::index_node:
				return find(*reinterpret_cast<index_node_type*>(dynamic_.root), key);
			default:
				std::abort();
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
					static_.size += 1;
				}

				return { iterator(), inserted };
			}

			auto vnode = get_allocator< value_node_type >().allocate(1);
			traits::move(node.keys, vnode->keys);
			vnode->keys[traits::node_type::capacity] = key;

			dynamic_.metadata = static_cast<uint8_t>(node_type::value_node);
			dynamic_.root = vnode;
			dynamic_.size = traits::node_type::capacity + 1;

			return { iterator(), true };
		}

		bool find(const btree_static_node< T >& node, T key)
		{
			using traits = btree_node_traits< btree_static_node< T > >;
			return traits::find(node, node.size, key);
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

				return { iterator(), inserted };
			}

			auto inode = get_allocator< index_node_type >().allocate(1);
			auto vgroup = get_allocator< value_node_group_type >().allocate(1);

			inode->size = 1;
			inode->metadata = static_cast<uint8_t>(node_type::value_node);
			inode->keys[0] = split(node, *vgroup);
			inode->value_group = vgroup;

			dynamic_.metadata = static_cast<uint8_t>(node_type::index_node);
			dynamic_.root = inode;

			auto n = key < inode->keys[0] ? 0 : 1;
			std::tie(index, inserted) = traits::insert(vgroup->node[n], vgroup->size[n], key);
			if (inserted)
			{
				vgroup->size[n] += 1;
				dynamic_.size += 1;
			}

			// TODO: delete node
			return { iterator(), inserted };
		}

		bool find(const value_node_type& node, T key)
		{
			using traits = btree_node_traits< value_node_type >;
			return traits::find(node, dynamic_.size, key);
		}

		std::pair< iterator, bool > insert(index_node_type& node, T key)
		{
			using traits = typename index_node_type::traits_type;
						
			auto index = traits::count_lt(node.keys, node.size, key);

			// TODO: this is calling method that does the split from value_node to index node.
			// In this case we should not grow the node, but rather split elements between the nodes.

			// For this we need parent stack, too, or remember parents.
			// We can split forward or backward.
			
			switch (get_node_type(node.metadata))
			{
			case node_type::value_node:
				return insert(node.value_group->node[index], key);
			case node_type::index_node:
				return insert(node.index_group->node[index], key);
			default:
				std::abort();
			}			
		}

		bool find(const index_node_type& node, T key)
		{
			using traits = typename index_node_type::traits_type;
						
			auto index = traits::count_lt(node.keys, node.size, key);
			return find(node.value_group->node[index], key);
		}

		T split(value_node_type& node, value_node_group_type& group)
		{
			// TODO
			static_assert(value_node_type::capacity == 16);

			// split node into empty group
			using traits = typename value_node_type::traits_type;

			traits::copy_lane(node.keys, group.node[0].keys);
			group.size[0] = 8;

			traits::copy_lane(node.keys + 8, group.node[1].keys);			
			group.size[1] = 8;

			return group.node[1].keys[0];			
		}

		template < typename Node > auto get_allocator()
		{
			return std::allocator_traits< Allocator >::rebind_alloc< Node >();
		}

		node_type get_node_type(uint8_t metadata) const
		{
			return static_cast< node_type >(metadata);
		}

		node_type get_root_type() const
		{
			return get_node_type(static_.metadata);
		}

		union
		{
			alignas(32) btree_static_node< T > static_;
			alignas(32) btree_dynamic_node dynamic_{};
		};
	};
}