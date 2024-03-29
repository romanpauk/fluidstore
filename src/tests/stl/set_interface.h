#define STATIC_ASSERT(...) static_assert(__VA_ARGS__)
// #define STATIC_ASSERT(...) BOOST_TEST((__VA_ARGS__))

template < typename Container > class set_interface
{
public:
	static void constructor_default()
	{
		Container container;
	}

	// TODO
	// explicit set(const key_compare& comp, const allocator_type& alloc = allocator_type());
	
	static void constructor_allocator(const typename Container::allocator_type& allocator)
	{
		Container container(allocator);
	}

	// TODO
	// template <class InputIterator> set(InputIterator first, InputIterator last, const key_compare& comp = key_compare(), const allocator_type & = allocator_type());

	// TODO
	// template <class InputIterator> set(InputIterator first, InputIterator last, const allocator_type & = allocator_type()); 

	// TODO
	//set(const set& x);
	//set(const set& x, const allocator_type& alloc);
	//set(set&& x);
	//set(set&& x, const allocator_type& alloc);

	static void clear(Container& container)
	{		
		container.clear();
	}

	// TODO
	// count
	
	template< typename... Args > static void emplace(Container& container, Args&&... args)
	{		
		STATIC_ASSERT(std::is_same_v< 
			std::pair< typename Container::iterator, bool >, 
			decltype(std::declval< Container& >().emplace(std::forward< Args >(args)...)) 
		>);

		auto result = container.emplace(std::forward< Args >(args)...);
		STATIC_ASSERT(std::is_same_v< std::pair< typename Container::iterator, bool >, decltype(result) >);
	}

	static void empty(Container& container)
	{		
		STATIC_ASSERT(std::is_same_v< 
			bool, 
			decltype(std::declval< const Container& >().empty()) 
		>);

		auto result = container.empty();
		STATIC_ASSERT(std::is_same_v< bool, decltype(result) >);
	}

	static void erase_const_lvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v< 
			typename Container::size_type, 
			decltype(std::declval< Container& >().erase(std::declval< const typename Container::value_type& >()))
		>);

		typename Container::value_type value;
		auto result = container.erase(static_cast< const typename Container::value_type& >(value));
		STATIC_ASSERT(std::is_same_v< typename Container::size_type, decltype(result) >);
	}

	static void erase_const_iterator(Container& container)
	{
		STATIC_ASSERT(std::is_same_v< 
			typename Container::size_type, 
			decltype(std::declval< Container& >().erase(std::declval< Container& >().cbegin())) 
		>);

		container.insert(typename Container::value_type());
		auto result = container.erase(container.cbegin());
		STATIC_ASSERT(std::is_same_v< typename Container::size_type, decltype(result) >);
	}

	// TODO
	// void erase(iterator first, iterator last);
	// iterator erase(const_iterator first, const_iterator last);

	static void find_iterator(Container& container)
	{		
		STATIC_ASSERT(std::is_same_v< 
			typename Container::iterator, 
			decltype(std::declval< Container& >().find(std::declval< const typename Container::value_type& >()))
		>);

		auto result = container.find(typename Container::value_type());
		STATIC_ASSERT(std::is_same_v< typename Container::iterator, decltype(result) >);
	}

	static void find_const_iterator(const Container& container)
	{		
		STATIC_ASSERT(std::is_same_v< 
			typename Container::const_iterator, 
			decltype(std::declval< const Container& >().find(std::declval< const typename Container::value_type& >()))
		>);

		auto result = container.find(typename Container::value_type());
		STATIC_ASSERT(std::is_same_v< typename Container::const_iterator, decltype(result) >);
	}

	static void insert_const_lvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v< 
			std::pair< typename Container::iterator, bool >, 
			decltype(std::declval< Container& >().insert(std::declval< const typename Container::value_type& >()))
		>);

		typename Container::value_type value;
		auto result = container.insert(static_cast< const typename Container::value_type& >(value));
		STATIC_ASSERT(std::is_same_v< std::pair< typename Container::iterator, bool >, decltype(result) >);
	}

	static void insert_rvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v< 
			std::pair< typename Container::iterator, bool >, 
			decltype(std::declval< Container& >().insert(std::declval< typename Container::value_type&& >()))
		>);

		typename Container::value_type value;
		auto result = container.insert(std::move(value));
		STATIC_ASSERT(std::is_same_v< std::pair< typename Container::iterator, bool >, decltype(result) >);
	}

	static void insert_hint_const_lvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v< 
			typename Container::iterator, 
			decltype(std::declval< Container& >().insert(std::declval< Container& >().end(), std::declval< const typename Container::value_type& >()))
		>);

		typename Container::value_type value;
		auto result = container.insert(container.end(), static_cast<const typename Container::value_type&>(value));
		STATIC_ASSERT(std::is_same_v< typename Container::iterator, decltype(result) >);
	}

	static void insert_const_hint_const_lvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< Container& >().insert(std::declval< const Container& >().end(), std::declval< const typename Container::value_type& >()))
		>);

		typename Container::value_type value;
		auto result = container.insert(container.cend(), static_cast<const typename Container::value_type&>(value));
		STATIC_ASSERT(std::is_same_v< typename Container::iterator, decltype(result) >);
	}

	static void insert_hint_rvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< Container& >().insert(std::declval< Container& >().end(), std::declval< typename Container::value_type&& >()))
		>);

		typename Container::value_type value;
		auto result = container.insert(container.end(), std::move(value));
		STATIC_ASSERT(std::is_same_v< typename Container::iterator, decltype(result) >);
	}

	static void insert_const_hint_rvalue(Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< Container& >().insert(std::declval< const Container& >().cend(), std::declval< typename Container::value_type&& >()))
		>);

		typename Container::value_type value;
		auto result = container.insert(container.cend(), std::move(value));
		STATIC_ASSERT(std::is_same_v< typename Container::iterator, decltype(result) >);
	}

	static void insert_range(Container& container)
	{
		std::array< typename Container::value_type, 10 > range;
		container.insert(range.begin(), range.end());
	}

	// TODO
	// static void insert_initializer_list()
	// static void insert_node()
	// static void insert_const_hint_node()

	static void iterator_begin(Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< Container& >().begin())
		>);

		typename Container::iterator it = container.begin();
	}

	static void iterator_begin_const(const Container& container)
	{		
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< const Container& >().begin())
		>);

		typename Container::const_iterator it = container.begin();
	}

	// TODO
	// iterator_cbegin
	// iterator_crbegin

	static void iterator_end(Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< Container& >().end())
		>);

		typename Container::iterator it = container.end();
	}

	static void iterator_end_const(const Container& container)
	{	
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< const Container& >().end())
		>);

		typename Container::const_iterator it = container.begin();
	}

	// TODO
	// iterator_cend
	// iterator_crend

	static void lower_bound_iterator(Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::iterator,
			decltype(std::declval< Container& >().lower_bound(std::declval< const typename Container::value_type& >()))
		>);

		typename Container::value_type value;
		auto result = container.lower_bound(static_cast< const typename Container::value_type& >(value));
		STATIC_ASSERT(std::is_same_v< typename Container::iterator, decltype(result) >);
	}

	static void lower_bound_const_iterator(const Container& container)
	{
		STATIC_ASSERT(std::is_same_v<
			typename Container::const_iterator,
			decltype(std::declval< const Container& >().lower_bound(std::declval< const typename Container::value_type& >()))
		>);

		typename Container::value_type value;
		auto result = container.lower_bound(static_cast< const typename Container::value_type& >(value));
		STATIC_ASSERT(std::is_same_v< typename Container::const_iterator, decltype(result) >);
	}

	static void size(const Container& container)
	{		
		STATIC_ASSERT(std::is_same_v<
			typename Container::size_type, 
			decltype(std::declval< const Container& >().size())
		>);

		auto result = container.size();
		STATIC_ASSERT(std::is_same_v< typename Container::size_type, decltype(result) >);
	}

	// TODO: this is quite poor test
	static void typedefs()
	{
		static_assert(!std::is_same_v< typename Container::key_type, void >);
		static_assert(std::is_same_v< typename Container::value_type, typename Container::key_type >);
		static_assert(std::is_unsigned_v< typename Container::size_type >);

		// TODO
		// static_assert(std::is_signed_v< typename Container::difference_type >);

		static_assert(!std::is_same_v< typename Container::key_compare, void >);
		static_assert(std::is_same_v< typename Container::value_compare, typename Container::key_compare >);

		static_assert(!std::is_same_v< typename Container::allocator_type, void >);

		// TODO
		//Container::reference;
		//Container::const_reference;
		//Container::pointer;
		//Container::const_pointer;
		//Container::iterator;
		//Container::const_iterator;
		//Container::reverse_iterator;
		//Container::const_reverse_iterator

		// TODO
		//Container::node_type;
		//Container::insert_node_type;
	}

	static void test_suite(Container& container)
	{		
		//
		clear(container);

		//
		//set_interface< ContainerType >::emplace(container, 1, 1);

		//
		empty(container);

		//
		find_iterator(container);
		find_const_iterator(container);

		//
		insert_const_lvalue(container);
		insert_rvalue(container);
		insert_hint_const_lvalue(container);
		insert_hint_rvalue(container);

		// TODO
		// set_interface::insert_const_hint_const_lvalue();
		// set_interface::insert_const_hint_rvalue();

		insert_range(container);

		//
		iterator_begin(container);
		iterator_begin_const(container);
		iterator_end(container);
		iterator_end_const(container);

		//
		lower_bound_iterator(container);
		lower_bound_const_iterator(container);

		//
		size(container);

		//
		typedefs();
	}
};