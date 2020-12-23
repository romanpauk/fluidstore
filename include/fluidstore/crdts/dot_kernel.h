#pragma once

namespace crdt
{
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
        template < typename Key, typename Allocator, typename Replica, typename Counter > friend class dot_kernel_set;
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
            this->allocator_.merge(*static_cast<Container*>(this), other);
        }

    public:
        const_iterator begin() const { return values_.begin(); }
        const_iterator end() const { return values_.end(); }
        const_iterator find(const Key& key) const { return values_.find(key); }

        void clear()
        {
            dot_kernel_type delta(allocator_);

            for (auto& [value, data] : values_)
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

    template < typename Key, typename Allocator, typename Replica, typename Counter > class dot_kernel_set
        : public dot_kernel_base<
        Key, void, Allocator, typename Replica::replica_id_type, Counter,
        dot_kernel_set< Key, Allocator, Replica, Counter >
        >
        , public Replica::template hook< dot_kernel_set< Key, Allocator, Replica, Counter > >
    {
        typedef dot_kernel_set< Key, Allocator, Replica, Counter > dot_kernel_type;

    public:
        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            typedef dot_kernel_set< Key, AllocatorT, ReplicaT, Counter > type;
        };

        dot_kernel_set(Allocator allocator, typename Replica::instance_id_type id)
            : dot_kernel_base<
            Key, void, Allocator, typename Replica::replica_id_type, Counter,
            dot_kernel_set< Key, Allocator, Replica, Counter >
            >(allocator)
            , Replica::template hook< dot_kernel_type >(allocator.get_replica(), id)
        {}

        dot_kernel_set(std::allocator_arg_t, Allocator allocator)
            : dot_kernel_base<
            Key, void, Allocator, typename Replica::replica_id_type, Counter,
            dot_kernel_set< Key, Allocator, Replica, Counter >
            >(allocator)
            , Replica::template hook< dot_kernel_type >(allocator.get_replica())
        {}

        /*std::pair< const_iterator, bool >*/ void insert(const Key& key)
        {
            arena< 1024 > buffer;

            //arena_allocator< void, Allocator > alloc(buffer, this->allocator_);
            //dot_kernel_set< Key, decltype(alloc), ReplicaId, Counter, InstanceId > delta(alloc, this->get_id());

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename Replica::replica_id_type > rep(replica_id);
            allocator< replica< typename Replica::replica_id_type > > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            dot_kernel_set< Key, decltype(allocator3), replica< typename Replica::replica_id_type >, Counter > delta(allocator3, this->get_id());

            // dot_kernel_type delta(this->allocator_, this->get_id());

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });
            delta.values_[key].dots.emplace(dot_type{ replica_id, counter });

            this->merge(delta);
        }
    };

    template< typename Key, typename Traits > class set
        : public dot_kernel_set< Key,
        typename Traits::allocator_type,
        typename Traits::replica_type,
        typename Traits::counter_type
        >
        , noncopyable
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        set(allocator_type allocator, typename Traits::instance_id_type id)
            : dot_kernel_set< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator, id)
        {}

        set(std::allocator_arg_t, allocator_type allocator)
            : dot_kernel_set< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator)
        {}
    };

    template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_map
        : public dot_kernel_base<
        Key, Value, Allocator, ReplicaId, Counter,
        dot_kernel_map< Key, Value, Allocator, ReplicaId, Counter >
        >
    {
        typedef dot_kernel_map< Key, Value, Allocator, ReplicaId, Counter > dot_kernel_map_type;

        template < typename AllocatorT > struct rebind
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

            auto replica_id = this->allocator_.get_replica().get_id();
            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });

            auto& data = delta.values_[key];
            data.dots.emplace(dot_type{ replica_id, counter });
            data.value = value;

            this->merge(delta);
        }
    };
}