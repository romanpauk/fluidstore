#pragma once

#include <fluidstore/crdts/dot_context.h>

#include <algorithm>
#include <scoped_allocator>

namespace crdt
{
    template < typename Value, typename Allocator > class dot_kernel_value
    {
        typedef dot_kernel_value< Value, Allocator > dot_kernel_value_type;

    public:
        typedef Allocator allocator_type;
        typedef Value value_type;
        typedef typename Allocator::replica_type replica_type;
        typedef typename replica_type::replica_id_type replica_id_type;
        typedef typename replica_type::counter_type counter_type;

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator)
            : value(allocator)
            , dots(allocator)
        {}

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator, typename replica_type::id_type id)
            : value(allocator, id)
            , dots(allocator)
        {}

        template < typename ValueT, typename AllocatorT >
        void merge(
            const dot_kernel_value< ValueT, AllocatorT >& other)
        {
            dots.insert(other.dots.begin(), other.dots.end());
            value.merge(other.value);
        }

        const typename replica_type::id_type& get_id() const { return value.get_id(); }

        std::set< dot< replica_id_type, counter_type >, std::less< dot< replica_id_type, counter_type > >, allocator_type > dots;
        Value value;
    };

    template < typename Allocator > class dot_kernel_value< void, Allocator >
    {
    public:
        typedef Allocator allocator_type;
        typedef void value_type;
        typedef typename Allocator::replica_type replica_type;
        typedef typename replica_type::replica_id_type replica_id_type;
        typedef typename replica_type::counter_type counter_type;

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator)
            : dots(allocator)
        {}

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator, typename replica_type::id_type)
            : dots(allocator)
        {}

        template < typename AllocatorT >
        void merge(
            const dot_kernel_value< void, AllocatorT >& other)
        {
            dots.insert(other.dots.begin(), other.dots.end());
        }

        typename const replica_type::id_type& get_id() const { static typename replica_type::id_type id; return id; }

        std::set< dot< replica_id_type, counter_type >, std::less< dot< replica_id_type, counter_type > >, allocator_type > dots;
    };

    template < typename Iterator, typename Outer > class dot_kernel_iterator_base
    {
        template < typename Key, typename Value, typename Allocator, typename Container > friend class dot_kernel;

    public:
        dot_kernel_iterator_base(Iterator it)
            : it_(it)
        {}

        bool operator == (const dot_kernel_iterator_base< Iterator, Outer >& other) const { return it_ == other.it_; }
        bool operator != (const dot_kernel_iterator_base< Iterator, Outer >& other) const { return it_ != other.it_; }

        Outer& operator++() { ++it_; return static_cast< Outer& >(*this); }
        Outer& operator--() { --it_; return static_cast< Outer& >(*this); }
        Outer operator++(int) { return it_++; }
        Outer operator--(int) { return it_--; }

    protected:
        Iterator it_;
    };

    template < typename Iterator, typename Key, typename Value > class dot_kernel_iterator
        : public dot_kernel_iterator_base< Iterator, dot_kernel_iterator< Iterator, Key, Value > >
    {
    public:
        dot_kernel_iterator(Iterator it)
            : dot_kernel_iterator_base< Iterator, dot_kernel_iterator< Iterator, Key, Value > >(it)
        {}

        std::pair< const Key&, Value& > operator *() { return { this->it_->first, this->it_->second.value }; } 
    };

    template < typename Iterator, typename Key > class dot_kernel_iterator< Iterator, Key, void >
        : public dot_kernel_iterator_base< Iterator, dot_kernel_iterator< Iterator, Key, void > >
    {
    public:
        dot_kernel_iterator(Iterator it)
            : dot_kernel_iterator_base< Iterator, dot_kernel_iterator< Iterator, Key, void > >(it)
        {}

        const Key& operator *() { return this->it_->first; }
    };

    template < typename Result > struct merge_context: public Result
    {};

    template < typename Key, typename Value, typename Allocator, typename Container > class dot_kernel
    {
        template < typename Key, typename Value, typename Allocator, typename Container > friend class dot_kernel;
        template < typename Key, typename Allocator > friend class set;
        template < typename Key, typename Value, typename Allocator > friend class map;

    protected:
        typedef typename Allocator::replica_type replica_type;
        typedef typename replica_type::replica_id_type replica_id_type;
        typedef typename replica_type::counter_type counter_type;

        typedef std::map< Key, dot_kernel_value< Value, Allocator >, std::less< Key >, std::scoped_allocator_adaptor< Allocator > > values_type;

        typedef dot< replica_id_type, counter_type > dot_type;
        typedef dot_kernel< Key, Value, Allocator, Container > dot_kernel_type;
        typedef dot_kernel_iterator< typename values_type::iterator, Key, Value > iterator;
        typedef dot_kernel_iterator< typename values_type::const_iterator, Key, Value > const_iterator;

        Allocator allocator_;
        dot_context< replica_id_type, counter_type, Allocator > counters_;

        values_type values_;
        std::map< dot_type, typename values_type::iterator, std::less< dot_type >, Allocator > dots_;
        
        struct merge_result
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
        void merge(const DotKernel& other, merge_context< merge_result >* context = nullptr)
        {
            arena< 1024 > buffer;
            typedef std::set < dot_type, std::less< dot_type >, arena_allocator<> > dot_set_type;
            dot_set_type rdotsvisited(buffer);
            dot_set_type rdotsvalueless(buffer);

            const auto& rdots = other.counters_.get();

            // Merge values
            for (const auto& [rkey, rdata] : other.values_)
            {
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
                if (context)
                {
                    ++context->count;
                    context->iterator = lpb.first;
                    context->inserted = lpb.second;
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
                        auto it = values_.erase(values_it);
                        if (context)
                        {
                            context->iterator = it;
                            ++context->count;
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
        iterator begin() { return values_.begin(); }
        const_iterator end() const { return values_.end(); }
        iterator end() { return values_.end(); }
        const_iterator find(const Key& key) const { return values_.find(key); }
        iterator find(const Key& key) { return values_.find(key); }

        void clear()
        {
            if (!empty())
            {
                dot_kernel_type delta(allocator_);

                for (auto& [value, data] : values_)
                {
                    delta.counters_.insert(data.dots.begin(), data.dots.end());
                }

                merge(delta);
                this->allocator_.merge(*static_cast<Container*>(this), delta);
            }
        }

        size_t erase(const Key& key)
        {
            auto values_it = values_.find(key);
            if (values_it != values_.end())
            {
                merge_context< merge_result > context;
                erase(values_it, &context);
                return context.count;
            }

            return 0;
        }

        iterator erase(iterator it)
        {
            merge_context< merge_result > context;
            erase(it.it_, &context);
            return context.iterator;
        }

        bool empty() const
        {
            return values_.empty();
        }

        size_t size() const
        {
            return values_.size();
        }

    private:
        void erase(typename values_type::iterator it, merge_context< merge_result >* context = nullptr)
        {
            dot_kernel_type delta(allocator_);

            auto& dots = it->second.dots;
            delta.counters_.insert(dots.begin(), dots.end());

            merge(delta, context);
            this->allocator_.merge(*static_cast<Container*>(this), delta);
        }
    };
}