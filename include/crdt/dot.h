#pragma once

// TODO: fix
#undef _ENFORCE_MATCHING_ALLOCATORS
#define _ENFORCE_MATCHING_ALLOCATORS 0

#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <vector>
#include <scoped_allocator>

namespace crdt
{
	// Allocator is used to pass node inside containers
	template < typename Node, typename T > class allocator : public std::allocator< T >
	{
	public:
		template< typename U > struct rebind { typedef allocator< Node, U > other; };

		allocator(Node node)
			: node_(node)
		{}

		template < typename U > allocator(const allocator< Node, U >& other)
			: node_(other.get_node())
		{}

		const Node& get_node() const { return node_; }

	private:
		Node node_;
	};

	struct traits
	{
		typedef uint64_t node_type;
		typedef uint64_t counter_type;

		template < typename T > using allocator = allocator< node_type, T >;
	};

	template < typename Node, typename Counter > struct dot
	{
		Node node;
		Counter counter;

		bool operator < (const dot< Node, Counter >& other) const { return std::make_tuple(node, counter) < std::make_tuple(other.node, other.counter); }
	};

	template < typename Counter > struct counters
	{
		counters()
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
				auto first = *counters_.begin();
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

		void merge(const counters< Counter >& other)
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

	private:
		std::set< Counter > counters_;
	};

	/*
	template < typename Counter > struct dot_node2
	{
		dot_node2()
		{}

		Counter next()
		{
			// Next is called on local replica only. That is always in sync, so it has always 1 element as it has seen all operations.
			if (counters_.empty())
			{
				counters_.push_back(0);
			}

			assert(counters_.size() == 1)
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
				if (counters_[i] != counters_[i - 1])
				{
					return;
				}
			}

			counters_.erase(counters_.begin(), counters_.begin() + index);
		}

		bool has(const Counter& counter) const
		{
			if (!counters_.empty())
			{
				if (counters_[0] >= counter)
				{
					return true;
				}

				// Could use binary search
				for (size_t i = 1; i < counters_.size())
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

		void merge(const dot_node< Counter >& other)
		{
			counter_ = std::max(counter_, other.counter_);
			cloud_.insert(other.cloud_.begin(), other.cloud_.end());
		}

		void compact()
		{
			for (auto it = cloud_.begin(); it != cloud_.end();)
			{
				if (counter_ + 1 == *it)
				{
					it = cloud_.erase(it);
					++counter_;
					continue;
				}
				else
				{
					break;
				}
			}
		}

		std::vector< Counter > counters_;
	};
	*/

	template < typename Node, typename Counter > class node_counters
	{
	public:
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

		void merge(const node_counters< Node, Counter >& other)
		{
			for (auto& p : other.counters_)
			{
				counters_[p.first].merge(p.second);
			}
		}

		std::set< dot< Node, Counter > > dots() const
		{
			std::set< dot< Node, Counter > > dots;
			for (auto& [node, counters] : counters_)
			{
				for (auto& counter : counters)
				{
					dots.insert({ node, counter });
				}
			}

			return dots;
		}

	private:
		std::map< Node, counters< Counter > > counters_;
	};	

	template < typename Value, typename Allocator, typename Node, typename Counter > class dot_kernel_value
	{
	public:
		typedef Allocator allocator_type;

		dot_kernel_value(allocator_type allocator)
			: value(allocator)
		{}

		void merge(const dot_kernel_value< Value, Allocator, Node, Counter >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
			value.merge(other.value);
		}

		std::set< dot< Node, Counter > > dots;
		Value value;
	};

	template < typename Allocator, typename Node, typename Counter > class dot_kernel_value< void, Allocator, Node, Counter >
	{
	public:
		dot_kernel_value()
		{}

		void merge(const dot_kernel_value< void, Allocator, Node, Counter >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
		}

		std::set< dot< Node, Counter > > dots;
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
		Allocator allocator_;
		node_counters< Node, Counter > counters_;
		std::map< Key, Value, std::less< Key >, std::scoped_allocator_adaptor< Allocator > > values_;
		std::map< dot< Node, Counter >, Key, std::less< dot< Node, Counter > > > dots_;

		typedef dot_kernel_base< Key, Value, Allocator, Node, Counter > dot_kernel_type;
		typedef dot_kernel_iterator< typename decltype(values_)::const_iterator, void > const_iterator;

	protected:
		dot_kernel_base(Allocator allocator)
			: allocator_(allocator)
			, values_(allocator)
		{}

		void remove(const Key& key)
		{
			dot_kernel_type delta(allocator_);

			auto values_it = values_.find(key);
			if (values_it != values_.end())
			{
				for(auto& dot: values_it->second.dots)
				{
					delta.counters_.add(dot.node, dot.counter);
				}
				
				merge(delta);
			}
		}

		void merge(const dot_kernel_base< Key, Value, Allocator, Node, Counter >& other)
		{
			std::set< dot< Node, Counter > > rdotsvisited;

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

			auto rdots = other.counters_.dots();
			std::set< dot< Node, Counter > > rdotsvalueless;

			std::set_difference(
				rdots.begin(), rdots.end(), 
				rdotsvisited.begin(), rdotsvisited.end(), 
				std::inserter(rdotsvalueless, rdotsvalueless.end())
			);

			for (auto& rdot : rdotsvalueless)
			{
				auto dots_it = dots_.find(rdot);
				assert(dots_it != dots_.end());
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
			dot_kernel_type delta;

			for(auto& [value, data]: values_)
			{
				for (auto& dot : data.dots)
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

		/*std::pair< const_iterator, bool >*/ void add(const Key& key)
		{
			dot_kernel_type delta(this->allocator_);

			auto node = this->allocator_.get_node();
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

		void add(const Key& key, const Value& value)
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

	template< typename Key, typename Allocator, typename Node, typename Counter > class set
		: public dot_kernel_set< Key, Allocator, Node, Counter >
	{
		typedef dot_kernel_set< Key, Allocator, Node, Counter > dot_kernel_type;

	public:
		set(Allocator allocator)
			: dot_kernel_type(allocator)
		{}

		void insert(Key k)
		{
			this->add(k);
		}

		void erase(Key k)
		{
			this->remove(k);
		}

		void merge(const set< Key, Allocator, Node, Counter >& other)
		{
			dot_kernel_type::merge(other);
		}
	};

	template< typename Key, typename Value, typename Allocator, typename Node, typename Counter > class map
		: public dot_kernel_map< Key, Value, Allocator, Node, Counter >
	{
		typedef dot_kernel_map< Key, Value, Allocator, Node, Counter > dot_kernel_type;

	public:
		map(Allocator allocator)
			: dot_kernel_type(allocator)
		{}

		void insert(Key k, Value v)
		{
			this->add(k, v);
		}

		void erase(Key k)
		{
			this->remove(k);
		}

		void merge(const map< Key, Value, Allocator, Node, Counter >& other)
		{
			dot_kernel_type::merge(other);
		}
	};

	template < typename Value, typename Allocator > class value
	{
	public:
		typedef Allocator allocator_type;

		value(Allocator)
			: value_()
		{}

		value(Allocator, const Value& value)
			: value_(value)
		{}

		void merge(const value< Value, Allocator >& other)
		{
			value_ = other.value_;
		}

	private:
		Value value_;
	};

	template < typename Value, typename Allocator, typename Node, typename Counter > class value_mv
	{
	public:
		typedef Allocator allocator_type;

		value_mv(Allocator allocator)
			: values_(allocator)
		{}

		value_mv(Allocator allocator, const Value& value)
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

		void merge(const value_mv< Value, Allocator, Node, Counter >& other)
		{
			values_.merge(other.values_);
		}

	private:
		crdt::set< Value, Allocator, Node, Counter > values_;
	};

}
