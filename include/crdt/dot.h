#pragma once

// TODO: fix
#undef _ENFORCE_MATCHING_ALLOCATORS
#define _ENFORCE_MATCHING_ALLOCATORS 0

#include <set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <vector>
#include <scoped_allocator>

#include <crdt/arena_allocator.h>
#include <boost/container/flat_set.hpp>

namespace crdt
{
	// Allocator is used to pass node inside containers
	template < typename Node, typename T, template < typename > typename Allocator > class allocator : public Allocator< T >
	{
	public:
		template< typename U > struct rebind { typedef allocator< Node, U, Allocator > other; };

		//allocator(Node node)
		//	: node_(node)
		//{}

		template < typename... Args > allocator(Node node, Args&&... args)
			: node_(node)
			, Allocator< T >(std::forward< Args >(args)...)
		{}

		template < typename U > allocator(const allocator< Node, U, Allocator >& other)
			: node_(other.get_node())
			//, Allocator< T >(other)
		{}

		const Node& get_node() const { return node_; }

	private:
		Node node_;
	};

	template < typename Node, typename T > class allocator2 : public arena_allocator< T, std::allocator< void > >
	{
	public:
		template< typename U > struct rebind { typedef allocator2< Node, U> other; };

		template < typename... Args > allocator2(Node node, Args&&... args)
			: node_(node)
			, arena_allocator< T, std::allocator< void > >(std::forward< Args >(args)...)
		{}

		template < typename U > allocator2(const allocator2< Node, U >& other)
			: node_(other.get_node())
			, arena_allocator< T, std::allocator< void > >(other)
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

	struct traits : traits_base< uint64_t, uint64_t, allocator< uint64_t, void, std::allocator > > {};

	template < typename Node, typename Counter > class dot
	{
	public:
		Node node;
		Counter counter;

		bool operator < (const dot< Node, Counter >& other) const { return std::make_tuple(node, counter) < std::make_tuple(other.node, other.counter); }
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
	template < typename Counter, typename Allocator > class counters
	{
	public:
		typedef Allocator allocator_type;

		counters(allocator_type allocator)
			: counters_(allocator)
		{}

		Counter next()
		{
			if (counters_.empty())
			{
				counters_.insert(1);
				return 1;
			}
			else
			{
				// This is local replica that is always in sync, always collapsed.
				assert(counters_.size() == 1);
				auto counter = counters_.first() + 1;
				counters_.clear();
				counters_.insert(counter);
				return counter;
			}
		}

		Counter get() const
		{
			if (counters_.empty())
			{
				return 0;
			}
			else
			{
				// This is local replica that is always in sync, always collapsed.
				assert(counters_.size() == 1);
				return *counters_.begin();
			}
		}

		void add(const Counter& counter)
		{
			if (!counters_.empty())
			{
				auto& first = *counters_.begin();
				if (first >= counter)
				{
					// Already there.
					return;
				}
				else if (first + 1 == counter)
				{
					counters_.erase(counters_.begin());
					counters_.insert(counter);
					collapse();
					return;
				}
			}

			counters_.insert(counter);
		}

		bool find(const Counter& counter) const
		{
			if (!counters_.empty())
			{
				if (counters_.begin() >= counter)
				{
					return true;
				}
				else if (counters_.find(counter) != counters_.end())
				{
					return true;
				}
			}

			return false;
		}

		template < typename AllocatorT > void merge(const counters< Counter, AllocatorT >& other)
		{
			counters_.insert(other.counters_.begin(), other.counters_.end());
			collapse();
		}

		void collapse()
		{
			auto counter = counters_.begin();
			if (counter != counters_.end())
			{
				auto it = counter;
				++it;

				for (; it != counters_.end(); ++it)
				{
					if (*counter + 1 == *it)
					{
						counter = counters_.erase(counter);
					}
					else
					{
						break;
					}
				}
			}
		}

		auto begin() const { return counters_.begin(); }
		auto end() const { return counters_.end(); }

	// private:
		std::set< Counter, std::less< Counter >, allocator_type > counters_;
		// boost::container::flat_set< Counter, std::less< Counter >, allocator_type > counters_;
	};


	template < typename Counter, typename Allocator > struct counters2
	{
		typedef Allocator allocator_type;

		counters2(allocator_type allocator)
			: counters_(allocator)
		{}

		Counter get() const
		{
			if (counters_.empty())
			{
				return Counter();
			}
			else
			{
				// This is local replica that is always in sync, always collapsed.
				assert(counters_.size() == 1);
				return *counters_.begin();
			}
		}

		Counter next()
		{
			// Next is called on local replica only. That is always in sync, so it has always 1 element as it has seen all operations.
			if (counters_.empty())
			{
				counters_.push_back(0);
			}

			assert(counters_.size() == 1);
			return ++counters_[0];
		}

		void add(const Counter& counter)
		{
			if (counters_.empty())
			{
				counters_.push_back(counter);
			}
			else
			{
				if (counters_[0] >= counter)
				{
					// Already there in the collapsed consecutive sequence
				}
				else
				{
					for (size_t i = counters_.size() - 1; i > 0; --i)
					{
						if (counters_[i] == counter)
						{
							// Already there, duplicate
							break;
						}
						else if (counters_[i] < counter)
						{
							counters_.insert(counters_.begin() + i + 1, counter);
							
							// Check if sequence can be collapsed
							collapse(i + 1);

							break;
						}
					}
				}
			}
		}

		void collapse(size_t index)
		{
			for (size_t i = index; i > 1; --i)
			{
				if (counters_[i] - 1 != counters_[i - 1])
				{
					return;
				}
			}

			counters_.erase(counters_.begin(), counters_.begin() + index);
		}

		bool find(const Counter& counter) const
		{
			if (!counters_.empty())
			{
				if (counters_[0] >= counter)
				{
					return true;
				}

				for (size_t i = 1; i < counters_.size(); ++i)
				{
					if (counters_[i] == counter)
					{
						return true;
					}
					else if (counters_[i] > counter)
					{
						break;
					}
				}
			}

			return false;
		}

		template< typename AllocatorT > void merge(const counters2< Counter, AllocatorT >& other)
		{
			size_t l_index = 0;
			size_t r_index = 0;

			while (l_index < counters_.size() && r_index < other.counters_.size())
			{
				auto l_e = counters_[l_index];
				auto r_e = other.counters_[r_index];
				if (l_e > r_e)
				{
					counters_.insert(counters_.begin() + l_index, r_e);
					++l_index;
					++r_index;					
				}
				else if (l_e == r_e)
				{
					++l_index;
					++r_index;
				}
				else
				{
					++l_index;
				}
			}

			if (r_index < other.counters_.size())
			{
				counters_.insert(counters_.end(), other.counters_.begin(), other.counters_.end());
			}

			collapse();
		}

		void collapse()
		{
			int same = 0;
			for (size_t i = 1; i < counters_.size(); ++i)
			{
				if (counters_[i - 1] + 1 == counters_[i])
				{
					++same;
				}
				else
				{
					break;
				}
			}

			if (same)
			{
				counters_.erase(counters_.begin(), counters_.begin() + same);
			}
		}

		auto begin() const { return counters_.begin(); }
		auto end() const { return counters_.end(); }

		std::vector< Counter, allocator_type > counters_;
	};

	template < typename Node, typename Counter, typename Allocator > class dot_context
	{
	public:
		typedef Allocator allocator_type;

		dot_context(allocator_type allocator)
			: counters_(allocator)
		{}

		void add(const Node& node, const Counter& counter)
		{
			counters_[node].add(counter);	
		}

		bool find(const Node& node, const Counter& counter) const
		{
			auto it = counters_.find(node);
			if (it != counters_.end())
			{
				return it->second.find(counter);
			}

			return false;
		}

		void remove(const Node& node, const Counter& counter)
		{
			auto it = counters_.find(node);
			if (it != counters_.end())
			{
				it->second.remove(counter);
				if (it->second.empty())
				{
					counters_.erase(it);
				}
			}
		}

		Counter next(const Node& node)
		{
			return counters_[node].next();
		}

		Counter get(const Node& node) const
		{
			auto it = counters_.find(node);
			if (it != counters_.end())
			{
				return it->second.get();
			}

			return Counter();
		}

		template < typename AllocatorT > void merge(const dot_context< Node, Counter, AllocatorT >& other)
		{
			for (auto& p : other.counters_)
			{
				counters_[p.first].merge(p.second);
			}
		}

		template < typename DotSet > void get_dots(DotSet& dots) const
		{
			for (auto& [node, counters] : counters_)
			{
				for (auto& counter : counters)
				{
					dots.insert({ node, counter });
				}
			}
		}

	// private:
		std::map< Node, counters< Counter, allocator_type >, std::less< Node >, std::scoped_allocator_adaptor< allocator_type > > counters_;
		// std::unordered_map< Node, counters< Counter, allocator_type >, std::hash< Node >, std::equal_to< Node >, std::scoped_allocator_adaptor< allocator_type > > counters_;
	};	

	template < typename Value, typename Allocator, typename Node, typename Counter > class dot_kernel_value
	{
	public:
		typedef Allocator allocator_type;

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
		// boost::container::flat_set< dot< Node, Counter >, std::less< dot< Node, Counter > >, allocator_type > dots;
		Value value;
	};

	template < typename Allocator, typename Node, typename Counter > class dot_kernel_value< void, Allocator, Node, Counter >
	{
	public:
		typedef Allocator allocator_type;

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
		// boost::container::flat_set< dot< Node, Counter >, std::less< dot< Node, Counter > >, allocator_type > dots;
	};

	template < typename Iterator, typename Adaptor > class dot_kernel_iterator
	{
	public:
		dot_kernel_iterator(Iterator it)
			: it_(it)
		{}

		bool operator == (const dot_kernel_iterator< Iterator, Adaptor >& other) const { return it_ == other.it_; }
		bool operator != (const dot_kernel_iterator< Iterator, Adaptor >& other) const { return it_ != other.it_; }

	private:
		Iterator it_;
	};

	template < typename Key, typename Value, typename Allocator, typename Node, typename Counter > class dot_kernel_base
	{
	protected:
	public: // TODO
		Allocator allocator_;
		dot_context< Node, Counter, Allocator > counters_;
		std::map< Key, Value, std::less< Key >, std::scoped_allocator_adaptor< Allocator > > values_;
		std::map< dot< Node, Counter >, Key, std::less< dot< Node, Counter > >, Allocator > dots_;
		// std::unordered_map< dot< Node, Counter >, Key, std::hash< dot< Node, Counter > >, std::equal_to< dot< Node, Counter > >, Allocator > dots_;

		typedef dot_kernel_base< Key, Value, Allocator, Node, Counter > dot_kernel_type;
		typedef dot_kernel_iterator< typename decltype(values_)::const_iterator, void > const_iterator;

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
			// For each value
			//    We allocate value - 1
			//	  And we allocate all its dots - 2
			//		Solution: when allocating value, allocate a block for a few dots too
			//         Allocator that starts from the buffer (later changed to pool allocator so it can be reused).
			//    And we allocate dot -> value link - 3
			//
			// Counters

			typedef std::set < dot< Node, Counter >, std::less< dot< Node, Counter > >, arena_allocator< void > > dot_set_type;
			
			// TODO: size based on input
			arena< 1024 > buffer;
			arena_allocator< void > arena(buffer);

			dot_set_type rdotsvisited(arena);

			// Merge values
			for (const auto& [rkey, rdata] : other.values_)
			{
				auto& ldata = values_[rkey];
				ldata.merge(rdata);

				// Track visited dots
				for (const auto& rdot : rdata.dots)
				{
					rdotsvisited.insert(rdot);
					dots_[rdot] = rkey;
				}
			}

			dot_set_type rdots(arena);
			other.counters_.get_dots(rdots);

			dot_set_type rdotsvalueless(arena);

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
				for (auto& dot : data.dots)
				{
					delta.counters_.add(dot.node, dot.counter);
				}
			}

			merge(delta);
		}

		void erase(const Key& key)
		{
			dot_kernel_type delta(allocator_);

			auto values_it = values_.find(key);
			if (values_it != values_.end())
			{
				for (auto& dot : values_it->second.dots)
				{
					delta.counters_.add(dot.node, dot.counter);
				}

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
		: public dot_kernel_base< Key, dot_kernel_value< void, Allocator, Node, Counter >, Allocator, Node, Counter
		>
	{
		typedef dot_kernel_set< Key, Allocator, Node, Counter > dot_kernel_type;

	public:
		dot_kernel_set(Allocator allocator)
			: dot_kernel_base< Key, dot_kernel_value< void, Allocator, Node, Counter >, Allocator, Node, Counter >(allocator)
		{}

		/*std::pair< const_iterator, bool >*/ void insert(const Key& key)
		{
			auto node = this->allocator_.get_node();

			arena< 1024 > buffer;
			arena_allocator< void > arena(buffer);
			crdt::allocator2< Node, void > allocator(node, arena);

			dot_kernel_set< Key, decltype(allocator), Node, Counter > delta(allocator);

			// dot_kernel_type delta(this->allocator_);

			auto counter = this->counters_.get(node) + 1;
			delta.counters_.add(node, counter);
			delta.values_[key].dots.insert({ node, counter });

			this->merge(delta);
		}
	};

	template < typename Key, typename Value, typename Allocator, typename Node, typename Counter > struct dot_kernel_map
		: public dot_kernel_base< Key, dot_kernel_value<  Value, Allocator, Node, Counter >, Allocator, Node, Counter >
	{
		typedef dot_kernel_map< Key, Value, Allocator, Node, Counter > dot_kernel_type;

	public:
		dot_kernel_map(Allocator allocator)
			: dot_kernel_base< Key, dot_kernel_value< Value, Allocator, Node, Counter >, Allocator, Node, Counter >(allocator)
		{}

		void insert(const Key& key, const Value& value)
		{
			dot_kernel_type delta(this->allocator_);

			auto node = this->allocator_.get_node();
			auto counter = this->counters_.get(node) + 1;
			delta.counters_.add(node, counter);

			auto& data = delta.values_[key];
			data.dots.insert({ node, counter });
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

	private:
		crdt::set< Value, Traits > values_;
	};

}
