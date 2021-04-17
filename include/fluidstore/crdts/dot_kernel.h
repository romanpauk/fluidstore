// Merge algorithm is based on the article "An Optimized Conflict-free Replicated Set"
// https://pages.lip6.fr/Marek.Zawirski/papers/RR-8083.pdf

#pragma once

#include <fluidstore/crdts/dot_context.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/allocators/arena_allocator.h>
#include <fluidstore/flat/map.h>

#include <algorithm>
#include <scoped_allocator>
#include <ostream>

namespace crdt
{
    template < typename Allocator, typename Dot > struct dot_kernel_value_context
    {
        dot_kernel_value_context(Allocator& allocator)
            : erased_dots(allocator)
        {}
         
        void register_erase(const Dot& dot) { erased_dots.push_back(dot); }

        flat::vector < Dot, Allocator > erased_dots;
    };

    template < typename Key, typename Value, typename Allocator, typename DotContext, typename DotKernel > class dot_kernel_value
    {
    public:
        using allocator_type = Allocator;
        using dot_kernel_value_type = dot_kernel_value< Key, Value, Allocator, DotContext, DotKernel >;

        using value_allocator_type = typename allocator_type::template rebind< typename allocator_type::value_type, allocator_container< dot_kernel_value_type > >::other;
        using value_type = typename Value::template rebind< value_allocator_type >::other;
        
        struct nested_value
        {
            nested_value(value_allocator_type& allocator, DotKernel* parent)
                : value(allocator)
                , parent(parent)
            {}

            nested_value(nested_value&& other) = default;

            DotKernel* parent;
            DotContext dots;
            value_type value;
        };

        template < typename AllocatorT > dot_kernel_value(AllocatorT& allocator, Key key, DotKernel* parent)
            : first(key)
            , second(value_allocator_type(allocator, this), parent)
        {}

        dot_kernel_value(dot_kernel_value_type&& other)
            : second(std::move(other.second))
            , first(std::move(other.first))
        {
            second.value.get_allocator().set_container(this);
        }

        template < typename Allocator, typename DotKernelValue, typename Context > void merge(Allocator& allocator, const DotKernelValue& other, Context& context)
        {
            second.dots.merge(allocator, other.dots, context);
            second.value.merge(other.value);
        }

        void update()
        {
            second.parent->update(first);
        }

        bool operator == (const Key& other) const { return first == other; }
        bool operator == (const dot_kernel_value_type& other) const { return first == other.first; }

        bool operator < (const Key& other) const { return first < other; }
        bool operator < (const dot_kernel_value_type& other) const { return first < other.first; }

        const Key first;
        nested_value second;
    };

    template < typename Key, typename Allocator, typename DotContext, typename DotKernel > class dot_kernel_value< Key, void, Allocator, DotContext, DotKernel >
    {
    public:        
        using allocator_type = Allocator;
        using value_type = void;
        using dot_kernel_value_type = dot_kernel_value< Key, void, Allocator, DotContext, DotKernel >;

        struct nested_value
        {
            DotContext dots;
        };

        template < typename AllocatorT > dot_kernel_value(AllocatorT&, Key key, DotKernel*)
            : first(key)
        {}

        dot_kernel_value() = default;
        dot_kernel_value(dot_kernel_value_type&& other) = default;
      
        template < typename Allocator, typename DotKernelValue, typename Context > void merge(Allocator& allocator, const DotKernelValue& other, Context& context)
        {
            second.dots.merge(allocator, other.dots, context);
        }

        void update() {}

        bool operator == (const Key& other) const { return first == other; }
        bool operator == (const dot_kernel_value_type& other) const { return first == other.first; }

        bool operator < (const Key& other) const { return first < other; }
        bool operator < (const dot_kernel_value_type& other) const { return first < other.first; }

        const Key first;
        nested_value second;
    };

    template < typename Iterator, typename Outer > class dot_kernel_iterator_base
    {
        template < typename Key, typename Value, typename Allocator, typename Container, typename Tag > friend class dot_kernel;

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

        std::pair< const Key&, Value& > operator *() { return { it_->first, it_->second.value }; }
        std::pair< const Key&, const Value& > operator *() const { return { it_->first, it_->second.value }; }
    };

    template < typename Iterator, typename Key > class dot_kernel_iterator< Iterator, Key, void >
        : public dot_kernel_iterator_base< Iterator, dot_kernel_iterator< Iterator, Key, void > >
    {
    public:
        dot_kernel_iterator(Iterator it)
            : dot_kernel_iterator_base< Iterator, dot_kernel_iterator< Iterator, Key, void > >(it)
        {}

        const Key& operator *() const { return this->it_->first; }
    };

    template < typename Key, typename Value, typename Allocator, typename Container, typename Tag > class dot_kernel
    {
    public:
        typedef Allocator allocator_type;
        
    private:
        template < typename Key, typename Value, typename Allocator, typename Container, typename Tag > friend class dot_kernel;
        
        template < typename Key, typename Value, typename Allocator, typename Container, typename Tag >
        friend std::ostream& operator << (std::ostream&, const dot_kernel< Key, Value, Allocator, Container, Tag >& kernel);

        // TODO: this is not exactly extensible... :(
        template < typename Key, typename Allocator, typename Tag, typename Hook, typename Delta > friend class set_base;
        template < typename Key, typename Value, typename Allocator, typename Tag, typename Hook, typename Delta > friend class map_base;
        template < typename Key, typename Allocator, typename Tag, typename Hook, typename Delta > friend class value_mv_base;

    protected:
        using replica_type = typename allocator_type::replica_type;
        using replica_id_type = typename replica_type::replica_id_type;
        using counter_type = typename replica_type::counter_type;

        using dot_type = dot< replica_id_type, counter_type >;
        using dot_kernel_type = dot_kernel< Key, Value, allocator_type, Container, Tag >;
        using dot_context_type = dot_context< dot_type, Tag >;
        using dots_type = flat::map_base< dot_type, Key >;

        using dot_kernel_value_allocator_type = typename allocator_type::template rebind< typename allocator_type::value_type, allocator_container< dot_kernel_type > >::other;
        using dot_kernel_value_type = dot_kernel_value< Key, Value, dot_kernel_value_allocator_type, dot_context_type, dot_kernel_type >;
        
        using values_type = flat::map_base< Key, Value, dot_kernel_value_type >;

        typedef dot_kernel_iterator< typename values_type::iterator, Key, typename dot_kernel_value_type::value_type > iterator;
        typedef dot_kernel_iterator< typename values_type::const_iterator, Key, typename dot_kernel_value_type::value_type > const_iterator;

        dot_context_type counters_;
        values_type values_;
        dots_type dots_;
        
        struct context
        {
            void register_insert(std::pair< typename values_type::iterator, bool >) {}
            void register_erase(typename values_type::iterator) {}
        };

        struct insert_context: public context
        {
            void register_insert(std::pair< typename values_type::iterator, bool > pb) { result = pb; }
            std::pair< typename values_type::iterator, bool > result;
        };

        struct erase_context: public context
        {
            void register_erase(typename values_type::iterator it) { iterator = it; ++count; }
            typename values_type::iterator iterator;
            size_t count = 0;
        };

    protected:
        dot_kernel() = default;
        dot_kernel(dot_kernel_type&& other) = default;

        ~dot_kernel()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            counters_.clear(allocator);
            for (auto& value : values_)
            {
                value.second.dots.clear(allocator);
            }
            values_.clear(allocator);
            dots_.clear(allocator);
        }

        // TODO:
    public:
        template < typename DotKernel >
        void merge(const DotKernel& other)
        {
            context ctx;
            merge(other, ctx);
        }

        template < typename DotKernel, typename Context >
        void merge(const DotKernel& other, Context& ctx)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > tmp(allocator.get_replica(), arenaallocator);

            typedef flat::set < dot_type, decltype(tmp) > dot_set_type;
            typedef flat::vector < dot_type, decltype(tmp) > dot_vec_type;

            dot_set_type rdotsvisited(tmp);
            dot_vec_type rdotsvalueless(tmp);
            dot_kernel_value_context< decltype(tmp), dot_type > value_ctx(tmp);

            const auto& rdots = other.counters_.get();

            // Merge values
            for (const auto& [rkey, rdata] : other.values_)
            {
                auto lpb = values_.emplace(allocator, allocator, rkey, this);
                auto& ldata = *lpb.first;
             
                ldata.merge(allocator, rdata, value_ctx);
                
                // Track visited dots
                rdotsvisited.insert(rdata.dots.begin(), rdata.dots.end());

                // Create dot -> key link
                for (const auto& rdot : rdata.dots)
                {
                    dots_.emplace(allocator, rdot, rkey);
                }

                // Support for insert / emplace pairb result
                ctx.register_insert(lpb);
            }

            // Find dots that do not have values - those are removed
            std::set_difference(
                rdots.begin(), rdots.end(),
                rdotsvisited.begin(), rdotsvisited.end(),
                std::back_inserter(rdotsvalueless)
            );

            for (const auto& rdot : rdotsvalueless)
            {
                auto dots_it = dots_.find(rdot);
                if (dots_it != dots_.end())
                {
                    auto lkey = dots_it->second;
                    auto values_it = values_.find(lkey);
                    values_it->second.dots.erase(allocator, rdot);
                    if (values_it->second.dots.empty())
                    {
                        auto it = values_.erase(allocator, values_it);
                        ctx.register_erase(it);
                    }

                    dots_.erase(allocator, dots_it);
                }
            }
            
            for (const auto& ldot : value_ctx.erased_dots)
            {
                dots_.erase(allocator, ldot);
            }

            // Merge counters
            counters_.merge(allocator, other.counters_);
        }

        void update(const Key& key)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            auto delta = static_cast<Container*>(this)->mutable_delta(deltaallocator);

            auto replica_id = static_cast<Container*>(this)->get_allocator().get_replica().get_id();
            auto counter = counters_.get(replica_id) + 1;
            delta.counters_.emplace(delta.get_allocator(), replica_id, counter);
            delta.values_.emplace(delta.get_allocator(), delta.get_allocator(), key, nullptr).first->second.dots.emplace(delta.get_allocator(), replica_id, counter);
            merge(delta);
            static_cast<Container*>(this)->commit_delta(delta);
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
                auto allocator = static_cast<Container*>(this)->get_allocator();
                arena< 8192 > arena;
                arena_allocator< void > arenaallocator(arena);
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
                auto delta = static_cast<Container*>(this)->mutable_delta(deltaallocator);

                clear(delta);
                merge(delta);
                static_cast< Container* >(this)->commit_delta(delta);
            }
        }

        template < typename Delta > void clear(Delta& delta)
        {
            for (const auto& [value, data] : values_)
            {
                delta.counters_.insert(delta.get_allocator(), data.dots.begin(), data.dots.end());
            }
        }

        size_t erase(const Key& key)
        {
            auto values_it = values_.find(key);
            if (values_it != values_.end())
            {
                erase_context context;
                erase(values_it, context);
                return context.count;
            }

            return 0;
        }

        iterator erase(iterator it)
        {
            erase_context context;
            erase(it.it_, context);
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
        template < typename Context > void erase(typename values_type::iterator it, Context& context)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            auto delta = static_cast<Container*>(this)->mutable_delta(deltaallocator);

            const auto& dots = it->second.dots;
            delta.counters_.insert(delta.get_allocator(), dots.begin(), dots.end());
            merge(delta, context);
            static_cast< Container* >(this)->commit_delta(delta);
        }
    };

    /*
    template < typename Key, typename Value, typename Allocator, typename Container, typename Tag >
    std::ostream& operator << (std::ostream& out, const dot_kernel< Key, Value, Allocator, Container, Tag >& kernel)
    {
        return out;
    }
    */
}