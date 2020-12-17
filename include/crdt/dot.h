#pragma once

#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <vector>

namespace crdt
{
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
				counters_.insert(0);
				return 0;
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

		Counter get()
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

	template < typename Node, typename Counter > struct node_counters
	{
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

		Counter get(const Node& node)
		{
			return counters_[node].get();
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

	template < typename Node, typename Counter, typename Value > struct dot_kernel_value
	{
		void merge(const dot_kernel_value< Node, Counter, Value >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
			value.merge(other.value);
		}

		std::set< dot< Node, Counter > > dots;
		Value value;
	};

	template < typename Node, typename Counter > struct dot_kernel_value< Node, Counter, void >
	{
		void merge(const dot_kernel_value< Node, Counter, void >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
		}

		std::set< dot< Node, Counter > > dots;
	};

	template < typename Node, typename Counter, typename Key, typename Value > class dot_kernel_base
	{
		typedef dot_kernel_base< Node, Counter, Key, Value > dot_kernel_type;

	protected:
		void remove(const Node& node, const Key& key)
		{
			dot_kernel_type delta;

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

		void merge(const dot_kernel_base< Node, Counter, Key, Value >& other)
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
		bool find(const Key& key) const
		{
			return values_.find(key) != values_.end();
		}

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

	protected:
		node_counters< Node, Counter > counters_;
		std::map< Key, Value > values_;

		std::map< dot< Node, Counter >, Key > dots_;
	};

	template < typename Node, typename Counter, typename Key > class dot_kernel_set
		: public dot_kernel_base< Node, Counter, Key, dot_kernel_value< Node, Counter, void > >
	{
		typedef dot_kernel_set< Node, Counter, Key > dot_kernel_type;

	public:
		void add(const Node& node, const Key& key)
		{
			dot_kernel_type delta;

			auto counter = this->counters_.get(node) + 1;
			delta.counters_.add(node, counter);
			delta.values_[key].dots.insert({ node, counter });

			this->merge(delta);
		}
	};

	template < typename Node, typename Counter, typename Key, typename Value > struct dot_kernel_map
		: public dot_kernel_base< Node, Counter, Key, dot_kernel_value< Node, Counter, Value > >
	{
		typedef dot_kernel_map< Node, Counter, Key, Value > dot_kernel_type;

	public:
		void add(const Node& node, const Key& key, const Value& value)
		{
			dot_kernel_type delta;

			auto counter = this->counters_.get(node) + 1;
			delta.counters_.add(node, counter);

			auto& data = delta.values_[key];
			data.dots.insert({ node, counter });
			data.value = value;

			this->merge(delta);
		}
	};

	template< typename Node, typename Counter, typename Key > class set
		: public dot_kernel_set< Node, Counter, Key >
	{
	public:
		void insert(Key k)
		{
			this->add(Node(), k);
		}

		void erase(Key k)
		{
			this->remove(Node(), k);
		}

		void merge(const set< Node, Counter, Key >& other)
		{
			dot_kernel_set< Node, Counter, Key >::merge(other);
		}
	};

	template< typename Node, typename Counter, typename Key, typename Value > class map
		: public dot_kernel_map< Node, Counter, Key, Value >
	{
		typedef dot_kernel_map< Node, Counter, Key, Value > dot_kernel_type;

	public:
		void insert(Key k, Value v)
		{
			this->add(Node(), k, v);
		}

		void erase(Key k)
		{
			this->remove(Node(), k);
		}

		void merge(const set< Node, Counter, Key >& other)
		{
			dot_kernel_map< Node, Counter, Key >::merge(other);
		}
	};

	template < typename Value > class value
	{
	public:
		value()
			: value_()
		{}

		value(const Value& value)
			: value_(value)
		{}

		void merge(const value& other)
		{
			value_ = other.value_;
		}

	private:
		Value value_;
	};

	template < typename Node, typename Counter, typename Value > class value_mv
	{
	public:
		value_mv()
		{}

		value_mv(const Value& value)
		{
			set(value);
		}

		Value get()
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

		void merge(const value_mv& other)
		{
			values_.merge(other.values_);
		}

	private:
		crdt::set< Node, Counter, Value > values_;
	};

}
