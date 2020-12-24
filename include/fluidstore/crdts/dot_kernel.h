#pragma once

#include <fluidstore/crdts/dot_context.h>

#include <algorithm>
#include <scoped_allocator>

namespace crdt
{
    template < typename Value, typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_value
    {
        typedef dot_kernel_value< Value, Allocator, ReplicaId, Counter > dot_kernel_value_type;

    public:
        typedef Allocator allocator_type;
        typedef Value value_type;

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator)
            : value(allocator)
            , dots(allocator)
        {}

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator, typename allocator_type::replica_type::id_type id)
            : value(allocator, id)
            , dots(allocator)
        {}

        template < typename ValueT, typename AllocatorT, typename ReplicaIdT, typename CounterT >
        void merge(
            const dot_kernel_value< ValueT, AllocatorT, ReplicaIdT, CounterT >& other)
        {
            dots.insert(other.dots.begin(), other.dots.end());
            value.merge(other.value);
        }

        const typename allocator_type::replica_type::id_type& get_id() const { return value.get_id(); }

        std::set< dot< ReplicaId, Counter >, std::less< dot< ReplicaId, Counter > >, allocator_type > dots;
        Value value;
    };

    template < typename Allocator, typename ReplicaId, typename Counter > class dot_kernel_value< void, Allocator, ReplicaId, Counter >
    {
    public:
        typedef Allocator allocator_type;
        typedef void value_type;

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator)
            : dots(allocator)
        {}

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator, typename allocator_type::replica_type::id_type)
            : dots(allocator)
        {}

        template < typename AllocatorT, typename ReplicaIdT, typename CounterT >
        void merge(
            const dot_kernel_value< void, AllocatorT, ReplicaIdT, CounterT >& other)
        {
            dots.insert(other.dots.begin(), other.dots.end());
        }

        typename allocator_type::replica_type::id_type get_id() const { return typename allocator_type::replica_type::id_type(); }

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

        std::pair< const Key&, Value& > operator *() { return { this->it_->first, this->it_->second.value }; } 
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

    template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter, typename Container > class dot_kernel
    {
        template < typename Key, typename Value, typename Allocator, typename ReplicaId, typename Counter, typename Container > friend class dot_kernel;
        template < typename Key, typename Allocator > friend class set;
        template < typename Key, typename Value, typename Allocator > friend class map;

    protected:
        typedef std::map< Key, dot_kernel_value< Value, Allocator, ReplicaId, Counter >, std::less< Key >, std::scoped_allocator_adaptor< Allocator > > values_type;

        typedef dot< ReplicaId, Counter > dot_type;
        typedef dot_kernel< Key, Value, Allocator, ReplicaId, Counter, Container > dot_kernel_type;
        typedef dot_kernel_iterator< typename values_type::iterator, Key, Value > iterator;
        typedef dot_kernel_iterator< typename values_type::const_iterator, Key, Value > const_iterator;

        Allocator allocator_;
        dot_context< ReplicaId, Counter, Allocator > counters_;

        values_type values_;
        std::map< dot_type, typename values_type::iterator, std::less< dot_type >, Allocator > dots_;
        
        struct merge_info
        {
            typename values_type::iterator iterator;
            bool inserted = false;
            size_t count = 0;
        };

    protected:
        dot_kernel(Allocator allocator)
            : allocator_(allocator)
            , values_(allocator)
            , counters_(allocator)
            , dots_(allocator)
        {}

        // TODO:
    public:
        template < typename DotKernel >
        void merge(const DotKernel& other, merge_info* info = nullptr)
        {
            arena< 1024 > buffer;
            typedef std::set < dot_type, std::less< dot_type >, arena_allocator<> > dot_set_type;
            dot_set_type rdotsvisited(buffer);
            dot_set_type rdotsvalueless(buffer);

            const auto& rdots = other.counters_.get();

            // Merge values
            for (const auto& [rkey, rdata] : other.values_)
            {
                // TODO: if this is insert of one element, we should not continue if insertion was not done
                auto lpb = values_.emplace(rkey, rdata.get_id());
                auto& ldata = lpb.first->second;
                ldata.merge(rdata);

                // Track visited dots
                rdotsvisited.insert(rdata.dots.begin(), rdata.dots.end());

                // Create dot -> key link
                for (const auto& rdot : rdata.dots)
                {
                    dots_[rdot] = lpb.first;
                }

                // Support for insert / emplace pairb result
                if (info)
                {
                    ++info->count;
                    info->iterator = lpb.first;
                    info->inserted = lpb.second;
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
                    auto values_it = dots_it->second;
                    values_it->second.dots.erase(rdot);
                    if (values_it->second.dots.empty())
                    {
                        values_.erase(values_it);
                    }

                    dots_.erase(dots_it);
                }
            }

            // Merge counters
            counters_.merge(other.counters_);
        }

    public:
        const_iterator begin() const { return values_.begin(); }
        iterator begin() { return values_.begin(); }
        const_iterator end() const { return values_.end(); }
        iterator end() { return values_.end(); }
        const_iterator find(const Key& key) const { return values_.find(key); }
        iterator find(const Key& key) { return values_.find(key); }

        void clear()
        {
            dot_kernel_type delta(allocator_);

            for (auto& [value, data] : values_)
            {
                delta.counters_.insert(data.dots.begin(), data.dots.end());
            }

            merge(delta);
            this->allocator_.merge(*static_cast< Container* >(this), delta);
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
                this->allocator_.merge(*static_cast< Container* >(this), delta);
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
}