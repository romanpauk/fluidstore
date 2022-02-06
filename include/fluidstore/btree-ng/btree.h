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


	// if key is trivial, use Key[]
	// if key is trivial and SSE, align array to 32
	// so we have sort of selector...
	// and we can have trivial/sse key and non-trivial value
	//

	/*
	template < typename T > struct array_descriptor
	{
		static constexpr uint8_t capacity = std::max(std::size_t(8), std::size_t(64 / sizeof(T))) / 2;
		static constexpr uint8_t alignment = alignof(T);
		using type = uint8_t[sizeof(uint8_t) * sizeof(T) * capacity];

		static_assert((1ull << sizeof(uint8_t) * 8) > 2 * capacity);
	};

	template <> struct array_descriptor< uint32_t >
	{
		static constexpr uint8_t capacity = 8;
		static constexpr uint8_t alignment = 32;
		using type = uint32_t[capacity];
	};

	template < typename T, size_t N > struct array
	{
		// alignas(array_descriptor< T >::alignment) typename array_descriptor< T >::type data_;
	};

	template <> struct array< uint32_t, 16 >
	{
		uint32_t data[16];
	};

	template <> struct array< uint32_t, 8 >
	{
		uint32_t data[8];
	};
	*/

	template < typename T, size_t N > struct array_traits;
	
	template <> struct array_traits< uint32_t, 8 >
	{
		static std::pair< uint8_t, bool > count_lt(const uint32_t* keys, uint8_t size, uint32_t key)
		{			
			auto sizemask = (1u << size) - 1;
			
			__m256i keyv = _mm256_set1_epi32(key);
			__m256i keysv = _mm256_loadu_si256((__m256i*)keys);		
						
			// signed case
			__m256i maskgtv = _mm256_cmpgt_epi32(keyv, keysv);
			int maskgt = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv));
			maskgt &= sizemask;
			
			__m256i maskeqv = _mm256_cmpeq_epi32(keyv, keysv);
			int maskeq = _mm256_movemask_ps(_mm256_castsi256_ps(maskeqv));
			maskeq &= sizemask;

			return { _mm_popcnt_u32(maskgt), maskeq};

			/*
				// TODO: the template needs to be signed/unsigned aware
				// unsigned case

			__m256i maskgtv = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv, keyv), keysv);			
			int maskgt = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv));
			maskgt = ~maskgt;			
			maskgt &= sizemask;
			return _mm_popcnt_u32(maskgt);
			*/
		}

		static void move(uint32_t* keys, uint8_t target, uint8_t source)
		{
			static const int permindex[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7 };

			__m256i keysv = _mm256_loadu_si256((__m256i*)keys);			
			__m256i permindexv = _mm256_loadu_si256((__m256i*)(permindex + 8 - (target - source)));
			__m256i keysv2 = _mm256_permutevar8x32_epi32(keysv, permindexv);
			
			// TODO: mask and store?
			uint32_t last = keys[7];
			_mm256_storeu_si256((__m256i*)keys, keysv2);
			keys[7] = last;
		}

		static void copy(const uint32_t* source, uint32_t* target)
		{			
			_mm256_storeu_si256((__m256i*)target, _mm256_loadu_si256((__m256i*)source));
		}
	};

	template <> struct array_traits< uint32_t, 16 >
	{
		static std::pair< uint8_t, bool > count_lt(const uint32_t* keys, uint8_t size, uint32_t key)
		{
			auto sizemask = (1u << size) - 1;

			__m256i keyv = _mm256_set1_epi32(key);

			__m256i keysv1 = _mm256_loadu_si256((__m256i*)keys);
			__m256i keysv2 = _mm256_loadu_si256((__m256i*)keys + 8);

			// signed case
			__m256i maskgtv1 = _mm256_cmpgt_epi32(keyv, keysv1);
			__m256i maskgtv2 = _mm256_cmpgt_epi32(keyv, keysv2);

			int maskgt1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv1));
			maskgt1 &= sizemask;
			
			int maskgt2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskgtv2));
			maskgt2 &= (sizemask >> 8);
			
			__m256i maskeqv1 = _mm256_cmpeq_epi32(keyv, keysv1);
			int maskeq1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskeqv1));
			maskeq1 &= sizemask;

			__m256i maskeqv2 = _mm256_cmpeq_epi32(keyv, keysv2);
			int maskeq2 = _mm256_movemask_ps(_mm256_castsi256_ps(maskeqv2));
			maskeq2 &= (sizemask >> 8);			

			return { _mm_popcnt_u32(maskgt2 << 8 | maskgt1), maskeq2 | maskeq1 };

			/*
				// TODO: the template needs to be signed/unsigned aware
				// unsigned case

			__m256i maskv1 = _mm256_cmpeq_epi32(_mm256_max_epi32(keysv, keyv), keysv);
			int cmpmask1 = _mm256_movemask_ps(_mm256_castsi256_ps(maskv1));
			cmpmask1 = ~cmpmask1;
			cmpmask1 &= sizemask;
			return _mm_popcnt_u32(cmpmask);
			*/
		}

		static void copy(const uint32_t* source, uint32_t* target)
		{
			_mm256_storeu_si256((__m256i*)target, _mm256_loadu_si256((__m256i*)source));
			_mm256_storeu_si256((__m256i*)target + 8, _mm256_loadu_si256((__m256i*)source + 8));
		}
	};

	template < typename T, size_t N > struct value_node_group;
	template < typename T, size_t N > struct index_node_group;
		
	template < typename T, size_t N > struct index_node;

	template <> struct index_node< uint32_t, 7 >
	{
		// For 13 keys, to support split, we need to layout 6 + 1 + 6 into two lanes:
		// lane 1: size + 6 + sep
		// lane 2: 6

		alignas(64) uint32_t keys_size;
		uint32_t keys[13];
				
		union {
			index_node_group< uint32_t, 14 >* index_group;
			value_node_group< uint32_t, 14 >* value_group;
		};
	};

	template < typename T, size_t N > struct value_node;

	template < typename T, size_t N > struct value_node_group
	{
		uint8_t node_size[N];
		value_node_group< T, N >* left;
		value_node_group< T, N >* right;
		alignas(64) value_node< T, 8 > node[N];
	};

	template < typename T, size_t N > struct index_node_group
	{
		uint8_t node_size[2*N];
		alignas(64) index_node< T, 7 > node[2*N];
	};

	template < size_t N > struct value_node< uint32_t, N >
	{
		enum { capacity = 2*N };
		using value_type = uint32_t;

		uint32_t keys[capacity];
	};

	template < typename Node > struct btree_value_node_traits
	{
		using node_type = Node;
		
		static std::pair< uint8_t, bool > insert(Node& node, uint8_t size, typename Node::value_type key)
		{
			using traits = array_traits< Node::value_type, Node::capacity >;

			if (size > 0)
			{
				if (size < Node::capacity)
				{
					auto [index, duplicate] = traits::count_lt(node.keys, size, key);
					if (index < size)
					{
						if (duplicate)
						{
							return { index, false };
						}

						// TODO
						// traits::move(node.keys, index + 1, index);
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
	};

	template < typename T > struct btree_static_node;

	// root and meta needs to overlap
	template<> struct btree_static_node< uint32_t >
	{	
		enum { capacity = 7 };
		using value_type = uint32_t;

		uint32_t keys[capacity];
		uint32_t size : 16;
		uint32_t metadata : 16;
	};

	template < typename Node > struct btree_static_node_traits
	{
		using node_type = Node;

		static std::pair< uint8_t, bool > insert(Node& node, typename Node::value_type key)
		{
			using traits = array_traits< Node::value_type, Node::capacity + 1 >;
			
			if (node.size > 0)
			{
				if (node.size < Node::capacity)
				{									
					auto [index, duplicate] = traits::count_lt(node.keys, node.size, key);
					if (index < node.size)
					{			
						if (duplicate)
						{
							return { index, false };
						}

						traits::move(node.keys, index + 1, index);
					}

					node.keys[index] = key;
					node.size += 1;
					return { index, true };
				}

				return { Node::capacity, false };
			}
			else
			{
				node.keys[0] = key;
				node.size = 1;
				return { 0, true };
			}
		}

		static void move(const uint32_t* source, uint32_t* target)
		{
			array_traits< Node::value_type, Node::capacity + 1 >::copy(source, target);
		}
	};

	struct btree_dynamic_node
	{		
		void* first; // value_node_group< T, 14 >* first;
		void* last;  // value_node_group< T, 14 >* last;
		void* root; 
		uint64_t size : 48;
		uint64_t metadata : 16;
	};

	template< typename T, typename Allocator = std::allocator< T > > struct btree
	{
		struct iterator {};

		std::pair< iterator, bool > insert(T key)
		{
			if (is_static())
			{
				using static_traits = btree_static_node_traits< btree_static_node< T > >;
				auto [index, inserted] = static_traits::insert(static_, key);
				if (index < static_traits::node_type::capacity)
				{
					static_.size += 1;
					return { iterator(), inserted };
				}
								
				auto node = get_allocator< value_node< T, 8 > >().allocate(1);
				static_traits::move(static_.keys, node->keys);
				node->keys[static_traits::node_type::capacity] = key;

				dynamic_.metadata |= 1;
				dynamic_.root = node;		
				dynamic_.size = static_traits::node_type::capacity + 1;

				return { iterator(), true };
			}
			else
			{
				// decode dynamic_.root type
				// can be internal_node or value_node.
				// first bit - 1: dynamic
				// second bit - 1: internal_node (otherwise value_node)

				auto node = reinterpret_cast< value_node< T, 8 >* >(dynamic_.root);
								
				using value_node_traits = btree_value_node_traits< value_node< T, 8 > >;

				uint64_t size = dynamic_.size;
				auto [index, inserted] = value_node_traits::insert(*node, size, key);
				if(index < value_node_traits::node_type::capacity)
				{
					if (inserted)
					{						
						dynamic_.size += 1;
					}

				    return { iterator(), inserted };
				}
				// 
				// grow node to internal_node
			}
						
			return { iterator(), false };
		}
						
		bool is_static() const
		{
			return (static_.metadata & 1) == 0;
		}

		template < typename Node > auto get_allocator()
		{
			return std::allocator_traits< Allocator >::rebind_alloc< Node >();
		}

		union
		{
			alignas(32) btree_static_node< T > static_;
			alignas(32) btree_dynamic_node dynamic_{};
		};
	};
}