#pragma once

#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <scoped_allocator>

#include <fluidstore/allocators/arena_allocator.h>

namespace crdt
{
	// Allocator is used to pass node inside containers
	template < typename Node, typename T = void, typename Allocator = std::allocator< T > > class allocator 
		: public Allocator
	{
	public:
		template< typename U > struct rebind { 
			typedef allocator< Node, U, 
				typename std::allocator_traits< Allocator >::template rebind_alloc< U > 
			> other;
		};

		template < typename... Args > allocator(Node node, Args&&... args)
			: node_(node)
			, Allocator(std::forward< Args >(args)...)
		{}

		template < typename U, typename AllocatorU > allocator(const allocator< Node, U, AllocatorU >& other)
			: node_(other.get_node())
			, Allocator(other)
		{}

		const Node& get_node() const { return node_; }

	private:
		Node node_;
	};

	template < typename Node, typename Counter, typename Allocator > struct traits_base
	{
		typedef Node node_type;
		typedef Counter counter_type;
		typedef Allocator allocator_type;
	};

	struct traits : traits_base< uint64_t, uint64_t, allocator< uint64_t, unsigned char > > {};

	template < typename Node, typename Counter > class dot
	{
	public:
		Node node;
		Counter counter;

		bool operator < (const dot< Node, Counter >& other) const { return std::make_tuple(node, counter) < std::make_tuple(other.node, other.counter); }
		bool operator > (const dot< Node, Counter >& other) const { return std::make_tuple(node, counter) > std::make_tuple(other.node, other.counter); }
		bool operator == (const dot< Node, Counter >& other) const { return std::make_tuple(node, counter) == std::make_tuple(other.node, other.counter); }
	
		size_t hash() const
		{
			std::size_t h1 = std::hash< Node >{}(node);
			std::size_t h2 = std::hash< Counter >{}(counter);
			return h1 ^ (h2 << 1);
		}
	};
}

namespace std
{
	template< typename Node, typename Counter > struct hash< crdt::dot< Node, Counter > >
	{
		std::size_t operator()(const crdt::dot< Node, Counter >& dot) const noexcept
		{
			return dot.hash();
		}
	};
}

namespace crdt {

	template < typename Node, typename Counter, typename Allocator > class dot_context
	{
		template < typename Node, typename Counter, typename Allocator > friend class dot_context;

	public:
		typedef Allocator allocator_type;

		dot_context(allocator_type allocator)
			: counters_(allocator)
		{}

		template < typename Dot > void emplace(Dot&& dot)
		{
			counters_.emplace(std::forward< Dot >(dot));
		}

		template < typename It > void insert(It begin, It end)
		{
			counters_.insert(begin, end);
		}

		bool find(const dot< Node, Counter >& dot) const
		{
			return counters_.find(dot) != counters_.end();
		}

		void remove(const dot< Node, Counter >& dot)
		{
			counters_.erase(dot);
		}

		Counter get(const Node& node) const
		{
			auto counter = Counter();
			auto it = counters_.upper_bound(dot< Node, Counter >{node, 0});
			while (it != counters_.end() && it->node == node)
			{
				counter = it++->counter;
			}

			return counter;
		}

		template < typename AllocatorT > void merge(const dot_context< Node, Counter, AllocatorT >& other)
		{
			insert(other.counters_.begin(), other.counters_.end());
			collapse();
		}

		const auto& get() const { return counters_; }

		void collapse()
		{
			if (counters_.size() > 1)
			{
				auto it = counters_.begin();
				auto d = *it++;
				for (; it != counters_.end();)
				{
					if (it->node == d.node)
					{
						if (it->counter == d.counter + 1)
						{
							it = counters_.erase(it);
						}
						else
						{
							it = counters_.upper_bound({ d.node, std::numeric_limits< Counter >::max() });
							if (it != counters_.end())
							{
						    	d = *it;
								++it;
							}
						}
					}
					else
					{
						d = *it;
						++it;
					}
				}
			}
		}

	private:
		std::set< dot< Node, Counter >, std::less< dot< Node, Counter > >, allocator_type > counters_;
	};

	template < typename Value, typename Allocator, typename Node, typename Counter > class dot_kernel_value
	{
	public:
		typedef Allocator allocator_type;
		typedef Value value_type;

		dot_kernel_value(allocator_type allocator)
			: value(allocator)
			, dots(allocator)
		{}

		template < typename ValueT, typename AllocatorT, typename NodeT, typename CounterT > 
		void merge(
			const dot_kernel_value< ValueT, AllocatorT, NodeT, CounterT >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
			value.merge(other.value);
		}

		std::set< dot< Node, Counter >, std::less< dot< Node, Counter > >, allocator_type > dots;
		Value value;
	};

	template < typename Allocator, typename Node, typename Counter > class dot_kernel_value< void, Allocator, Node, Counter >
	{
	public:
		typedef Allocator allocator_type;
		typedef void value_type;

		dot_kernel_value(allocator_type allocator)
			: dots(allocator)
		{}

		template < typename AllocatorT, typename NodeT, typename CounterT >
		void merge(
			const dot_kernel_value< void, AllocatorT, NodeT, CounterT >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
		}
		
		std::set< dot< Node, Counter >, std::less< dot< Node, Counter > >, allocator_type > dots;
	};

	template < typename Iterator > class dot_kernel_iterator_base
	{
	public:
		dot_kernel_iterator_base(Iterator it)
			: it_(it)
		{}

		bool operator == (const dot_kernel_iterator_base< Iterator >& other) const { return it_ == other.it_; }
		bool operator != (const dot_kernel_iterator_base< Iterator >& other) const { return it_ != other.it_; }

	protected:
		Iterator it_;
	};

	template < typename Iterator, typename Key, typename Value > class dot_kernel_iterator
		: public dot_kernel_iterator_base< Iterator >
	{
	public:
		dot_kernel_iterator(Iterator it)
			: dot_kernel_iterator_base< Iterator >(it)
		{}

		std::pair< const Key&, const Value& > operator *() { return { this->it_->first, this->it_->second.value }; }
	};

	template < typename Iterator, typename Key > class dot_kernel_iterator< Iterator, Key, void >
		: public dot_kernel_iterator_base< Iterator >
	{
	public:
		dot_kernel_iterator(Iterator it)
			: dot_kernel_iterator_base< Iterator >(it)
		{}

		const Key& operator *() { return this->it_->first; }
	};

	template < typename Key, typename Value, typename Allocator, typename Node, typename Counter > class dot_kernel_base
	{
		template < typename Key, typename Value, typename Allocator, typename Node, typename Counter > friend class dot_kernel_base;
		template < typename Key, typename Allocator, typename Node, typename Counter > friend class dot_kernel_set;
		template < typename Key, typename Value, typename Allocator, typename Node, typename Counter > friend class dot_kernel_map;

	protected:
		Allocator allocator_;
		dot_context< Node, Counter, Allocator > counters_;
		std::map< Key, dot_kernel_value< Value, Allocator, Node, Counter >, std::less< Key >, std::scoped_allocator_adaptor< Allocator > > values_;
		std::map< dot< Node, Counter >, Key, std::less< dot< Node, Counter > >, Allocator > dots_;
		
		typedef dot_kernel_base< Key, Value, Allocator, Node, Counter > dot_kernel_type;

		typedef dot_kernel_iterator< typename decltype(values_)::iterator, Key, Value > iterator;
		typedef dot_kernel_iterator< typename decltype(values_)::const_iterator, Key, Value > const_iterator;

	protected:
		dot_kernel_base(Allocator allocator)
			: allocator_(allocator)
			, values_(allocator)
			, counters_(allocator)
			, dots_(allocator)
		{}

		template < typename DotKernelBase > 
		void merge(const DotKernelBase& other)
		{
			arena< 1024 > buffer;
			typedef std::set < dot< Node, Counter >, std::less< dot< Node, Counter > >, arena_allocator<> > dot_set_type;		
			dot_set_type rdotsvisited(buffer);
			dot_set_type rdotsvalueless(buffer);

			const auto& rdots = other.counters_.get();
			
			// Merge values
			for (const auto& [rkey, rdata] : other.values_)
			{
				auto& ldata = values_[rkey];
				ldata.merge(rdata);

				// Track visited dots
				rdotsvisited.insert(rdata.dots.begin(), rdata.dots.end());

				// Create dot -> key link
				for (const auto& rdot : rdata.dots)
				{
					dots_[rdot] = rkey;
				}
			}

			// Find dots that do not have values - those are removed
			std::set_difference(
				rdots.begin(), rdots.end(), 
				rdotsvisited.begin(), rdotsvisited.end(), 
				std::inserter(rdotsvalueless, rdotsvalueless.end())
			);

			for (auto& rdot : rdotsvalueless)
			{
				auto dots_it = dots_.find(rdot);
				if (dots_it != dots_.end())
				{
					auto& value = dots_it->second;
					
					auto values_it = values_.find(value);
					assert(values_it != values_.end());
					if (values_it != values_.end())
					{
						values_it->second.dots.erase(rdot);

						if (values_it->second.dots.empty())
						{
							values_.erase(values_it);
						}
					}
					
					dots_.erase(dots_it);
				}
			}

			// Merge counters
			counters_.merge(other.counters_);
		}

	public:
		const_iterator begin() const { return values_.begin(); }
		const_iterator end() const { return values_.end(); }
		const_iterator find(const Key& key) const { return values_.find(key); }

		void clear()
		{
			dot_kernel_type delta(allocator_);

			for(auto& [value, data]: values_)
			{
				delta.counters_.insert(data.dots.begin(), data.dots.end());
			}

			merge(delta);
		}

		void erase(const Key& key)
		{
			dot_kernel_type delta(allocator_);

			auto values_it = values_.find(key);
			if (values_it != values_.end())
			{
				auto& dots = values_it->second.dots;
				delta.counters_.insert(dots.begin(), dots.end());
				
				merge(delta);
			}
		}

		bool empty() const
		{
			return values_.empty();
		}

		size_t size() const
		{
			return values_.size();
		}
	};

	template < typename Key, typename Allocator, typename Node, typename Counter > class dot_kernel_set
		: public dot_kernel_base< Key, void, Allocator, Node, Counter
		>
	{
		typedef dot_kernel_set< Key, Allocator, Node, Counter > dot_kernel_type;

	public:
		dot_kernel_set(Allocator allocator)
			: dot_kernel_base< Key, void, Allocator, Node, Counter >(allocator)
		{}

		/*std::pair< const_iterator, bool >*/ void insert(const Key& key)
		{
			arena< 1024 > buffer;
			arena_allocator< void, Allocator > allocator(buffer, this->allocator_);
			dot_kernel_set< Key, decltype(allocator), Node, Counter > delta(allocator);

			//dot_kernel_type delta(this->allocator_);

			auto node = this->allocator_.get_node();
			auto counter = this->counters_.get(node) + 1;
			delta.counters_.emplace(dot< Node, Counter >{ node, counter });
			delta.values_[key].dots.emplace(dot< Node, Counter >{ node, counter });

			this->merge(delta);
		}
	};

	template < typename Key, typename Value, typename Allocator, typename Node, typename Counter > class dot_kernel_map
		: public dot_kernel_base< Key, Value, Allocator, Node, Counter >
	{
		typedef dot_kernel_map< Key, Value, Allocator, Node, Counter > dot_kernel_type;

	public:
		dot_kernel_map(Allocator allocator)
			: dot_kernel_base< Key, Value, Allocator, Node, Counter >(allocator)
		{}

		void insert(const Key& key, const Value& value)
		{
			dot_kernel_type delta(this->allocator_);

			auto node = this->allocator_.get_node();
			auto counter = this->counters_.get(node) + 1;
			delta.counters_.emplace(dot< Node, Counter >{node, counter});

			auto& data = delta.values_[key];
			data.dots.emplace(dot< Node, Counter >{ node, counter });
			data.value = value;

			this->merge(delta);
		}
	};

	template< typename Key, typename Traits > class set
		: public dot_kernel_set< Key, typename Traits::allocator_type, typename Traits::node_type, typename Traits::counter_type >
	{
		typedef dot_kernel_set< Key, typename Traits::allocator_type, typename Traits::node_type, typename Traits::counter_type > dot_kernel_type;

	public:
		typedef typename Traits::allocator_type allocator_type;

		set(allocator_type allocator)
			: dot_kernel_type(allocator)
		{}

		void merge(const set< Key, Traits >& other)
		{
			dot_kernel_type::merge(other);
		}
	};

	template< typename Key, typename Value, typename Traits > class map
		: public dot_kernel_map< Key, Value, typename Traits::allocator_type, typename Traits::node_type, typename Traits::counter_type >
	{
		typedef dot_kernel_map< Key, Value, typename Traits::allocator_type, typename Traits::node_type, typename Traits::counter_type > dot_kernel_type;

	public:
		typedef typename Traits::allocator_type allocator_type;

		map(allocator_type allocator)
			: dot_kernel_type(allocator)
		{}

		void merge(const map< Key, Value, Traits >& other)
		{
			dot_kernel_type::merge(other);
		}
	};

	template < typename Value, typename Traits > class value
	{
	public:
		typedef typename Traits::allocator_type allocator_type;

		value(allocator_type)
			: value_()
		{}

		value(allocator_type, const Value& value)
			: value_(value)
		{}

		void merge(const value< Value, Traits >& other)
		{
			value_ = other.value_;
		}

		bool operator == (const Value& value) const { return value_ == value; }

	private:
		Value value_;
	};

	template < typename Value, typename Traits > class value_mv
	{
	public:
		typedef typename Traits::allocator_type allocator_type;

		value_mv(allocator_type allocator)
			: values_(allocator)
		{}

		value_mv(allocator_type allocator, const Value& value)
			: values_(allocator)
		{
			set(value);
		}

		Value get() const
		{
			switch (values_.size())
			{
			case 0:
				return Value();
			case 1:
				return *values_.begin();
			default:
				// TODO: this needs some better interface
				std::abort();
			}
		}

		void set(Value value)
		{
			values_.clear();
			values_.insert(value);
		}

		void merge(const value_mv< Value, Traits >& other)
		{
			values_.merge(other.values_);
		}

		bool operator == (const Value& value) const { return get() == value; }

	private:
		crdt::set< Value, Traits > values_;
	};
}
