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
	//
	// Delta operation:
	// Each allocator will point to crdt::aggregate, this way each crdt instance will be able to access the aggregate.
	// Each operation will merge delta with delta instance in the aggregate.
	// Each instance will have replica-unique id so it can be found in the aggregate.
	// In the end, the aggregate will have merged deltas for all operations done so far, in form of crdt instances.
	// 

	template < typename ReplicaId, typename InstanceId > class replica
	{
	public:
		typedef ReplicaId replica_id_type;
		typedef InstanceId instance_id_type;

		replica(ReplicaId replica_id)
			: replica_id_(replica_id)
		{}

		const ReplicaId& get_replica_id() const { return replica_id_; }
		InstanceId generate_instance_id() { return 0; }

		// Nothing to do
		template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta) {}

	private:
		ReplicaId replica_id_;
	};

	// TODO: naming of instances need to be better enforced.
	// All instances explicitly created will need user-provided name, all automatically created can use generated name.
	template < typename ReplicaId, typename InstanceId > class aggregating_replica: public replica< ReplicaId, InstanceId >
	{
		struct aggregate_instance_base
		{
			virtual ~aggregate_instance_base() {}
		};

		template < typename Instance > struct aggregate_instance : aggregate_instance_base
		{
			template< typename Allocator > aggregate_instance(Allocator allocator)
				: instance_(allocator)
			{}

			template < typename DeltaInstance > void merge(const DeltaInstance& delta_instance)
			{
				instance_.merge(delta_instance);
			}

		private:
			Instance instance_;
		};

	public:
		aggregating_replica(ReplicaId replica_id)
			: replica< ReplicaId, InstanceId >(replica_id)
		{}

		InstanceId generate_instance_id()
		{
			return ++instance_id_counter_;
		}

		// This is called from local instances when they merge
		template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta)
		{			
			get_aggregate_instance(instance).merge(delta);
			// TODO: We will have to track removals so we can remove removed instances from instances_.
		}

	private:
		template < typename Instance > auto& get_aggregate_instance(const Instance& instance)
		{
			// So this gives us instance usable in aggregating_replica, but what about it's ids?
			// We need to take ids from what we are merging

			crdt::allocator < replica< ReplicaId, InstanceId > > allocator(*this);
			typedef typename Instance::rebind_allocator< decltype(allocator) >::type aggregated_type;

			auto& context = instances_[instance.get_instance_id()];
			if (!context)
			{
				// We will have to use separate different allocator for aggregated instances as the allocator they have got us here
				// while merging. That also means we will have to rebind instance type to different allocator.

				auto ptr = new aggregate_instance< aggregated_type >(allocator);
				context.reset(ptr);
				return *ptr;
			}
			else
			{
				return dynamic_cast< aggregate_instance< aggregated_type >& >(*context);
			}
		}

		InstanceId instance_id_counter_;
		std::map< InstanceId, std::unique_ptr< aggregate_instance_base > > instances_;
	};

	// Allocator is used to pass node inside containers
	template < typename Replica, typename T = unsigned char, typename Allocator = std::allocator< T > > class allocator 
		: public Allocator
	{
	public:
		template < typename Replica, typename U, typename AllocatorU > friend class allocator;

		template< typename U > struct rebind { 
			typedef allocator< Replica, U, 
				typename std::allocator_traits< Allocator >::template rebind_alloc< U > 
			> other;
		};

		template < typename... Args > allocator(Replica& replica, Args&&... args)
			: Allocator(std::forward< Args >(args)...)
			, replica_(replica)
		{}

		template < typename U, typename AllocatorU > allocator(const allocator< Replica, U, AllocatorU >& other)
			: Allocator(other)
			, replica_(other.replica_)
		{}

		const auto& get_replica_id() const { return replica_.get_replica_id(); }

		template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta_instance)
		{
			replica_.merge(instance, delta_instance);
		}

	private:
		Replica& replica_;
	};

	template < typename Replica, typename Counter, typename Allocator > struct traits_base
	{
		typedef Replica replica_type;
		typedef typename Replica::replica_id_type replica_id_type;
		typedef Counter counter_type;
		typedef Allocator allocator_type;
	};

	struct traits : traits_base< replica< uint64_t, uint64_t >, uint64_t, allocator< replica< uint64_t, uint64_t >, unsigned char > > {};

	template < typename ReplicaId, typename Counter > class dot
	{
	public:
		ReplicaId replica_id;
		Counter counter;

		bool operator < (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) < std::make_tuple(other.replica_id, other.counter); }
		bool operator > (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) > std::make_tuple(other.replica_id, other.counter); }
		bool operator == (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) == std::make_tuple(other.replica_id, other.counter); }
	
		size_t hash() const
		{
			std::size_t h1 = std::hash< replica_id_ >{}(replica_id);
			std::size_t h2 = std::hash< Counter >{}(counter);
			return h1 ^ (h2 << 1);
		}
	};
}

namespace std
{
	template< typename ReplicaId, typename Counter > struct hash< crdt::dot< ReplicaId, Counter > >
	{
		std::size_t operator()(const crdt::dot< ReplicaId, Counter >& dot) const noexcept
		{
			return dot.hash();
		}
	};
}

namespace crdt {

	template < typename ReplicaId, typename Counter, typename Allocator > class dot_context
	{
		template < typename ReplicaId, typename Counter, typename Allocator > friend class dot_context;

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

		bool find(const dot< ReplicaId, Counter >& dot) const
		{
			return counters_.find(dot) != counters_.end();
		}

		void remove(const dot< ReplicaId, Counter >& dot)
		{
			counters_.erase(dot);
		}

		Counter get(const ReplicaId& replica_id) const
		{
			auto counter = Counter();
			auto it = counters_.upper_bound(dot< ReplicaId, Counter >{ replica_id, 0 });
			while (it != counters_.end() && it->replica_id == replica_id)
			{
				counter = it++->counter;
			}

			return counter;
		}

		template < typename AllocatorT > void merge(const dot_context< ReplicaId, Counter, AllocatorT >& other)
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
					if (it->replica_id == d.replica_id)
					{
						if (it->counter == d.counter + 1)
						{
							it = counters_.erase(it);
						}
						else
						{
							it = counters_.upper_bound({ d.replica_id, std::numeric_limits< Counter >::max() });
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
		std::set< dot< ReplicaId, Counter >, std::less< dot< ReplicaId, Counter > >, allocator_type > counters_;
	};

	template < typename Value, typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_value
	{
	public:
		typedef Allocator allocator_type;
		typedef Value value_type;

		dot_kernel_value(allocator_type allocator)
			: value(allocator)
			, dots(allocator)
		{}

		template < typename ValueT, typename AllocatorT, typename ReplicaIdT, typename CounterT > 
		void merge(
			const dot_kernel_value< ValueT, AllocatorT, ReplicaIdT, CounterT >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
			value.merge(other.value);
		}

		std::set< dot< ReplicaId, Counter >, std::less< dot< ReplicaId, Counter > >, allocator_type > dots;
		Value value;
	};

	template < typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_value< void, Allocator, ReplicaId, Counter >
	{
	public:
		typedef Allocator allocator_type;
		typedef void value_type;

		dot_kernel_value(allocator_type allocator)
			: dots(allocator)
		{}

		template < typename AllocatorT, typename ReplicaIdT, typename CounterT >
		void merge(
			const dot_kernel_value< void, AllocatorT, ReplicaIdT, CounterT >& other)
		{
			dots.insert(other.dots.begin(), other.dots.end());
		}
		
		std::set< dot< ReplicaId, Counter >, std::less< dot< ReplicaId, Counter > >, allocator_type > dots;
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

	template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter, typename Container > class dot_kernel_base
	{
		template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter, typename Container > friend class dot_kernel_base;
		template < typename Key, typename Allocator, typename ReplicaId, typename Counter > friend class dot_kernel_set;
		template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter > friend class dot_kernel_map;

	protected:
		typedef dot< ReplicaId, Counter > dot_type;
		typedef dot_kernel_base< Key, Value, Allocator, ReplicaId, Counter, Container > dot_kernel_type;
		
		Allocator allocator_;
		dot_context< ReplicaId, Counter, Allocator > counters_;
			
		std::map< Key, dot_kernel_value< Value, Allocator, ReplicaId, Counter >, std::less< Key >, std::scoped_allocator_adaptor< Allocator > > values_;
		std::map< dot_type, Key, std::less< dot_type >, Allocator > dots_;

		typedef dot_kernel_iterator< typename decltype(values_)::iterator, Key, Value > iterator;
		typedef dot_kernel_iterator< typename decltype(values_)::const_iterator, Key, Value > const_iterator;

	protected:
		dot_kernel_base(Allocator allocator)
			: allocator_(allocator)
			, values_(allocator)
			, counters_(allocator)
			, dots_(allocator)
		{}

		// TODO:
	public:
		uint64_t get_instance_id() const { return 0; }
	
		template < typename DotKernelBase > 
		void merge(const DotKernelBase& other)
		{
			arena< 1024 > buffer;
			typedef std::set < dot_type, std::less< dot_type >, arena_allocator<> > dot_set_type;
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

			// Merge into global context using outermost type.
			this->allocator_.merge(*static_cast< Container* >(this), other);
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

	template < typename Key, typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_set
		: public dot_kernel_base< 
		    Key, void, Allocator, ReplicaId, Counter, 
		    dot_kernel_set< Key, Allocator, ReplicaId, Counter >
		>
	{
		typedef dot_kernel_set< 
			Key, Allocator, ReplicaId, Counter
		> dot_kernel_type;

	public:
		template < typename AllocatorT > struct rebind_allocator
		{
			typedef dot_kernel_set< Key, AllocatorT, ReplicaId, Counter > type;
		};

		dot_kernel_set(Allocator allocator)
			: dot_kernel_base<
			    Key, void, Allocator, ReplicaId, Counter,
			    dot_kernel_set< Key, Allocator, ReplicaId, Counter >
			>(allocator)
		{}

		/*std::pair< const_iterator, bool >*/ void insert(const Key& key)
		{
			arena< 1024 > buffer;
			arena_allocator< void, Allocator > allocator(buffer, this->allocator_);
			dot_kernel_set< Key, decltype(allocator), ReplicaId, Counter > delta(allocator);

			//dot_kernel_set_type delta(this->allocator_);

			auto replica_id = this->allocator_.get_replica_id();
			auto counter = this->counters_.get(replica_id) + 1;
			delta.counters_.emplace(dot_type{ replica_id, counter });
			delta.values_[key].dots.emplace(dot_type{ replica_id, counter });

			this->merge(delta);
		}
	};

	template< typename Key, typename Traits > class set
		: public dot_kernel_set< Key, typename Traits::allocator_type, typename Traits::replica_id_type, typename Traits::counter_type >
	{
	public:
		typedef typename Traits::allocator_type allocator_type;

		set(allocator_type allocator)
			: dot_kernel_set< Key, typename Traits::allocator_type, typename Traits::replica_id_type, typename Traits::counter_type >(allocator)
		{}
	};

	template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_map
		: public dot_kernel_base< 
		    Key, Value, Allocator, ReplicaId, Counter, 
		    dot_kernel_map< Key, Value, Allocator, ReplicaId, Counter >
		>
	{
		typedef dot_kernel_map< Key, Value, Allocator, ReplicaId, Counter > dot_kernel_map_type;

		template < typename AllocatorT > struct rebind_allocator
		{
			typedef dot_kernel_map< Key, Value, AllocatorT, ReplicaId, Counter > type;
		};

	public:
		dot_kernel_map(Allocator allocator)
			: dot_kernel_base< Key, Value, Allocator, ReplicaId, Counter, dot_kernel_map< Key, Value, Allocator, ReplicaId, Counter > >(allocator)
		{}

		void insert(const Key& key, const Value& value)
		{
			dot_kernel_map_type delta(this->allocator_);

			auto replica_id = this->allocator_.get_replica_id();
			auto counter = this->counters_.get(replica_id) + 1;
			delta.counters_.emplace(dot_type{replica_id, counter});

			auto& data = delta.values_[key];
			data.dots.emplace(dot_type{ replica_id, counter });
			data.value = value;

			this->merge(delta);
		}
	};

	template< typename Key, typename Value, typename Traits > class map
		: public dot_kernel_map< Key, Value, typename Traits::allocator_type, typename Traits::replica_id_type, typename Traits::counter_type >
	{
	public:
		typedef typename Traits::allocator_type allocator_type;

		map(allocator_type allocator)
			: dot_kernel_map< Key, Value, typename Traits::allocator_type, typename Traits::replica_id_type, typename Traits::counter_type >(allocator)
		{}
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
