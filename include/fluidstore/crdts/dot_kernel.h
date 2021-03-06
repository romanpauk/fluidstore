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

    protected:
        using replica_type = typename allocator_type::replica_type;
        using replica_id_type = typename replica_type::replica_id_type;
        using counter_type = typename replica_type::counter_type;

        using dot_type = dot< replica_id_type, counter_type >;
        using dot_kernel_type = dot_kernel< Key, Value, allocator_type, Container, Tag >;
        using dot_context_type = dot_context< dot_type, Tag >;
        
        using dot_kernel_value_allocator_type = typename allocator_type::template rebind< typename allocator_type::value_type, allocator_container< dot_kernel_type > >::other;
        using dot_kernel_value_type = dot_kernel_value< Key, Value, dot_kernel_value_allocator_type, dot_context_type, dot_kernel_type >;
        
        using values_type = flat::map_base< Key, Value, dot_kernel_value_type >;

        typedef dot_kernel_iterator< typename values_type::iterator, Key, typename dot_kernel_value_type::value_type > iterator;
        typedef dot_kernel_iterator< typename values_type::const_iterator, Key, typename dot_kernel_value_type::value_type > const_iterator;

        values_type values_;

        struct replica_data
        {
            // Persistent data
            dot_counters_base< counter_type, Tag > counters;
            flat::map_base< counter_type, Key > dots;
            
            // Temporary merge data
            flat::set_base< counter_type > visited;
            const flat::set_base< counter_type >* other_counters;
        };
        
        flat::map_base< replica_id_type, replica_data > replica_;

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

        template < typename Allocator > struct value_context
        {
            value_context(Allocator& allocator, flat::map_base< replica_id_type, replica_data >& replica)
                : allocator_(allocator)
                , replica_(replica)
            {}

            void register_erase(const dot_type& dot) 
            { 
                auto it = replica_.find(dot.replica_id);
                if (it != replica_.end())
                {
                    it->second.dots.erase(allocator_, dot.counter);
                }
            }

        private:
            Allocator& allocator_;
            flat::map_base< replica_id_type, replica_data >& replica_;
        };

    protected:
        dot_kernel() = default;
        dot_kernel(dot_kernel_type&& other) = default;

        ~dot_kernel()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();

            for (auto& value : values_)
            {
                value.second.dots.clear(allocator);
            }
            values_.clear(allocator);

            for (auto& [replica_id, data] : replica_)
            {
                data.counters.clear(allocator);
                data.dots.clear(allocator);
            }
            replica_.clear(allocator);
        }

        // TODO: public
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

            for (const auto& [replica_id, rdata] : other.replica_)
            {
                // Merge counters
                auto& ldata = replica_.emplace(allocator, replica_id, replica_data()).first->second;
                ldata.counters.merge(allocator, replica_id, rdata.counters);

                ldata.other_counters = &rdata.counters.counters_;
            }

            // Merge values
            for (const auto& [rkey, rvalue] : other.values_)
            {
                auto lpb = values_.emplace(allocator, allocator, rkey, this);

                auto& lvalue = *lpb.first;

                value_context value_ctx(allocator, replica_);
                lvalue.merge(allocator, rvalue, value_ctx);

                for (const auto& [replica_id, rdots] : rvalue.dots)
                {
                    auto& ldata = replica_.emplace(allocator, replica_id, replica_data()).first->second;
                    
                    // Track visited dots
                    ldata.visited.insert(tmp, rdots.counters_);
                    
                    for (const auto& counter : rdots.counters_)
                    {
                        // Create dot -> key link
                        ldata.dots.emplace(allocator, counter, rkey);
                    }
                }

                // Support for insert / emplace pairb result
                ctx.register_insert(lpb);
            }

            for (auto& [replica_id, rdata] : other.replica_)
            {
                auto replica_it = replica_.find(replica_id);
                if (replica_it != replica_.end())
                {
                    auto& ldata = replica_it->second;

                    // Find dots that were not visited during processing of values (thus valueless). Those are the ones to be removed.
                    flat::vector < counter_type, decltype(tmp) > rdotsvalueless(tmp);
                    std::set_difference(
                        ldata.other_counters->begin(), ldata.other_counters->end(),
                        ldata.visited.begin(), ldata.visited.end(),
                        std::back_inserter(rdotsvalueless)
                    );

                    if (!rdotsvalueless.empty())
                    {
                        for (const auto& counter : rdotsvalueless)
                        {
                            auto counter_it = ldata.dots.find(counter);
                            if (counter_it != ldata.dots.end())
                            {
                                auto& lkey = counter_it->second;
                                auto values_it = values_.find(lkey);
                                values_it->second.dots.erase(allocator, dot_type{ replica_id, counter });
                                if (values_it->second.dots.empty())
                                {
                                    auto it = values_.erase(allocator, values_it);
                                    ctx.register_erase(it);
                                }

                                ldata.dots.erase(allocator, counter_it);
                            }
                        }
                    }

                    ldata.visited.clear(tmp);
                }
            }
        }

        void update(const Key& key)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            auto delta = static_cast<Container*>(this)->mutable_delta(deltaallocator);

            auto dot = get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot);
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
                delta.add_counter_dots(data.dots);
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
            delta.add_counter_dots(dots);
            merge(delta, context);
            static_cast< Container* >(this)->commit_delta(delta);
        }

    public:
        template < typename Dots > void add_counter_dots(const Dots& dots)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            for (auto& [replica_id, counters] : dots)
            {
                replica_.emplace(allocator, replica_id, replica_data()).first->second.counters.insert(allocator, counters);
            }
        }

        void add_counter_dot(const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            replica_.emplace(allocator, dot.replica_id, replica_data()).first->second.counters.emplace(allocator, dot.counter);
        }

        // TODO: const
        dot_type get_next_dot()
        {
            auto replica_id = static_cast<Container*>(this)->get_allocator().get_replica().get_id();
            counter_type counter = 1;
            auto it = replica_.find(replica_id);
            if (it != replica_.end())
            {
                counter = it->second.counters.get() + 1;
            }
            
            return { replica_id, counter };
        }

        void add_value(const Key& key, const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto& data = *values_.emplace(allocator, allocator, key, nullptr).first;
            data.second.dots.emplace(allocator, dot);
        }

        template < typename ValueT > void add_value(const Key& key, const dot_type& dot, ValueT&& value)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto& data = *values_.emplace(allocator, allocator, key, nullptr).first;
            data.second.dots.emplace(allocator, dot);
            data.second.value.merge(value);
        }
    };
}