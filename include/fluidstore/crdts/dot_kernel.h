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

#define REPARENT_DISABLED

namespace crdt
{
    //
    // TODO: we need two strategies to solve conflicts:
    // remove-wins and add-wins. Right now it is a mess, if map is accessed through operator [], it does insert in the background,
    // but if it is accessed through iterator, it does not. So the way code is written has an impact on final merge.
    // For remove-wins, [] should not do insert and increase sequences, for add-wins (or, update-wins), we need to increase parent
    // sequences with each mutation operation of the value. Link to parent will have to be passed to value so parent can
    // update dots_[newdot] and value can add newdot to dots. The same will go for it's parent...
    // 

    struct tag_update_wins {};
    struct tag_remove_wins {};

    template < typename Allocator > struct dot_kernel_value_context
    {
        typedef Allocator allocator_type;
        typedef typename Allocator::replica_type replica_type;
        typedef typename replica_type::replica_id_type replica_id_type;
        typedef typename replica_type::counter_type counter_type;
        typedef dot< replica_id_type, counter_type > dot_type;

        dot_kernel_value_context(Allocator allocator)
            : erased_dots(allocator)
        {}
         
        void register_erase(const dot_type& dot) { erased_dots.insert(dot); }

        std::set < dot_type, std::less< dot_type >, Allocator > erased_dots;
    };

    template < typename Key, typename Value, typename Allocator, typename Tag > class dot_kernel_value
    {
        typedef dot_kernel_value< Key, Value, Allocator, Tag > dot_kernel_value_type;

    public:
        typedef Allocator allocator_type;
        typedef typename Allocator::replica_type replica_type;
        typedef typename replica_type::replica_id_type replica_id_type;
        typedef typename replica_type::counter_type counter_type;

    #if !defined(REPARENT_DISABLED)
        // BUG: this value_type typedef is causing big slowdown while compiling map_map_merge test
        typedef typename allocator_type::template rebind< typename allocator_type::value_type, allocator_container< dot_kernel_value_type > >::other value_allocator_type;
        typedef typename Value::template rebind< value_allocator_type >::other value_type;
    #else
        typedef Value value_type;
        typedef Allocator value_allocator_type;
    #endif
        dot_kernel_value(std::allocator_arg_t, allocator_type allocator)
        #if !defined(REPARENT_DISABLED)
            : value(value_allocator_type(allocator, this))
        #else
            : value(allocator)
        #endif
            , key()
            , container(allocator.get_container())
        {}

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator, typename replica_type::id_type id)
        #if !defined(REPARENT_DISABLED)
            : value(value_allocator_type(allocator, this))
        #else
            : value(allocator)
        #endif
            , key()
            , container(allocator.get_container())
        {}

        template < typename Allocator, typename DotKernelValue, typename Context > void merge(Allocator& allocator, const DotKernelValue& other, Context& context)
        {
            dots.merge(allocator, other.dots, context);
            value.merge(other.value);
        }

        const typename replica_type::id_type& get_id() const { return value.get_id(); } 
        void set_key(const Key& k) { key = k; }
        void update() { container.update(key); }

        // TODO: for keys bigger than pointers or the ones that are not POD, store pointers instead.
        Key key;
        typename allocator_type::container_type& container;
        value_type value;
        dot_context2< replica_id_type, counter_type, Tag > dots;
    };

    template < typename Key, typename Allocator, typename Tag > class dot_kernel_value< Key, void, Allocator, Tag >
    {
    public:
        typedef Allocator allocator_type;
        typedef void value_type;
        typedef typename Allocator::replica_type replica_type;
        typedef typename replica_type::replica_id_type replica_id_type;
        typedef typename replica_type::counter_type counter_type;

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator)
        {}

        dot_kernel_value(std::allocator_arg_t, allocator_type allocator, typename replica_type::id_type)
        {}

        dot_kernel_value(dot_kernel_value< Key, void, Allocator, Tag >&& other)
            : dots(std::move(other.dots))
        {}

        template < typename Allocator, typename DotKernelValue, typename Context > void merge(Allocator& allocator, const DotKernelValue& other, Context& context)
        {
            dots.merge(allocator, other.dots, context);
        }

        const typename replica_type::id_type& get_id() const { static typename replica_type::id_type id; return id; }

        void set_key(const Key&) {}
        void update() {}

        dot_context2< replica_id_type, counter_type, Tag > dots;
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

        std::pair< const Key&, Value& > operator *() { return { this->it_->first, this->it_->second.value }; }
        //std::pair< const Key&, const Value& > operator *() const { return { this->it_->first, this->it_->second.value }; }
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
        using dot_kernel_value_allocator_type = typename allocator_type::template rebind< typename allocator_type::value_type, allocator_container< dot_kernel_type > >::other;
        using dot_kernel_value_type = dot_kernel_value< Key, Value, dot_kernel_value_allocator_type, Tag >;
        
        /*
        typedef std::map< 
            Key, 
            dot_kernel_value_type,
            std::less< Key >, 
            std::scoped_allocator_adaptor< 
                allocator_type,
                dot_kernel_value_allocator_type
            >
        > values_type;
        */

        typedef flat::map<
            Key,
            dot_kernel_value_type,
            std::scoped_allocator_adaptor<
                allocator_type,
                dot_kernel_value_allocator_type
            >
        > values_type;
        
        typedef dot_kernel_iterator< typename values_type::iterator, Key, typename dot_kernel_value_type::value_type > iterator;
        typedef dot_kernel_iterator< typename values_type::const_iterator, Key, typename dot_kernel_value_type::value_type > const_iterator;

        dot_context2< replica_id_type, counter_type, Tag > counters_;
        values_type values_;
        flat::map_base< dot_type, Key > dots_;
        
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
        dot_kernel(allocator_type allocator)
            : values_(std::scoped_allocator_adaptor< allocator_type, dot_kernel_value_allocator_type >(
                allocator, 
                dot_kernel_value_allocator_type(allocator, this)
            ))
        {}

        dot_kernel(dot_kernel_type&& other)
            : values_(std::move(other.values_))
            , dots_(std::move(other.dots_))
            , counters_(std::move(other.counters_))
        {}

        ~dot_kernel()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            counters_.clear(allocator);
            for (auto& value : values_)
            {
                value.second.dots.clear(allocator);
            }

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
            auto tmp = allocator_traits< allocator_type >::get_allocator< crdt::tag_delta >(static_cast< Container* >(this)->get_allocator());
            //typedef flat::set < dot_type, decltype(tmp) > dot_set_type;
            typedef std::set < dot_type, std::less< dot_type >, decltype(tmp) > dot_set_type;

            dot_set_type rdotsvisited(tmp);
            dot_set_type rdotsvalueless(tmp);
            dot_kernel_value_context value_ctx(tmp);

            const auto& rdots = other.counters_.get();

            // Merge values
            for (const auto& [rkey, rdata] : other.values_)
            {
                auto lpb = values_.try_emplace(rkey, rdata.get_id());
                auto& ldata = lpb.first->second;
             
                // TODO: using iterator could be better for performance, issue is that iterator type and thus size is not known while dot_kernel_value
                // class is defined. Maybe it could be forward-declared somehow?
                ldata.set_key(rkey);
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
                std::inserter(rdotsvalueless, rdotsvalueless.end())
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
                        auto it = values_.erase(values_it);
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
            auto& delta = static_cast< Container* >(this)->mutable_delta();
            
            auto replica_id = static_cast<Container*>(this)->get_allocator().get_replica().get_id();
            auto counter = counters_.get(replica_id) + 1;
            delta.counters_.emplace(delta.get_allocator(), replica_id, counter);
            delta.values_[key].dots.emplace(delta.get_allocator(), replica_id, counter);
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
                auto& delta = static_cast<Container*>(this)->mutable_delta();
                clear(delta);
                merge(delta);
                static_cast< Container* >(this)->commit_delta(delta);
            }
        }

        template < typename Delta > void clear(Delta& delta)
        {
            for (const auto& [value, data] : values_)
            {
                delta.counters_.insert(delta.get_allocator(), data.dots.begin(), data.dots.end(), (unsigned)data.dots.size());
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
            auto& delta = static_cast<Container*>(this)->mutable_delta();

            const auto& dots = it->second.dots;
            delta.counters_.insert(delta.get_allocator(), dots.begin(), dots.end(), (unsigned)dots.size());
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