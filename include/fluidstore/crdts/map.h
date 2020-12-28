#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/set.h>
#include <fluidstore/allocators/arena_allocator.h>

#include <memory>
#include <stdexcept>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Tag = tag_state, typename Hook = default_hook > class map
        : public Hook::template hook< Allocator, map< Key, Value, Allocator, Tag, Hook > >
        , public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, Tag, Hook >, Tag >
    {
        typedef map< Key, Value, Allocator, Tag, Hook > map_type;
        typedef dot_kernel< Key, Value, Allocator, map_type, Tag > dot_kernel_type;
    
    public:
        typedef Allocator allocator_type;
        typedef typename allocator_type::replica_type replica_type;

        typedef typename Hook::template hook< allocator_type, map_type > hook_type;
        
        template < typename AllocatorT, typename TagT, typename HookT > struct rebind
        {
            typedef map< Key, typename Value::template rebind< AllocatorT, TagT, HookT >::type, AllocatorT, TagT, HookT > type;
        };

        map(allocator_type allocator)
            : hook_type(allocator, allocator.get_replica().generate_instance_id())
            , dot_kernel_type(allocator)
        {}

        map(allocator_type allocator, typename allocator_type::replica_type::id_type id)
            : hook_type(allocator, id)
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            arena< 1024 * 2 > buffer;
            arena_allocator< void, allocator< typename replica_type::delta_replica_type > > allocator(buffer, this->get_allocator().get_replica());
            typename rebind< decltype(allocator), tag_delta, default_hook >::type delta(allocator, this->get_id());
            //map_type delta(this->allocator_, this->get_id());

            insert(delta, key, value);

            // TODO:
            // When this is insert of one element, merge should change internal state only when an insert operation would.
            // But we already got id. So the id would have to be returned, otherwise dot sequence will not collapse.
            
            insert_context context;
            this->merge(delta, context);
            this->merge_hook(*this, delta);
            return { context.result.first, context.result.second };
        }

        Value& operator[](const Key& key)
        {
            arena< 1024 * 2 > buffer;
            arena_allocator< void, allocator< typename replica_type::delta_replica_type > > allocator(buffer, this->get_allocator().get_replica());
            typename rebind< decltype(allocator), tag_delta, default_hook >::type delta(allocator, this->get_id());

            if constexpr (std::uses_allocator_v< Value, Allocator >)
            {
                // TODO: this Value should not be tracked, it is temporary.
                auto pairb = insert(key, Value(this->get_allocator()));
                return (*pairb.first).second;
            }
            else
            {
                auto pairb = insert(key, Value());
                return (*pairb.first).second;
            }
        }

        Value& at(const Key& key)
        {
            auto it = find(key);
            if (it != end())
            {
                return (*it).second;
            }
            else
            {
                throw std::out_of_range(__FUNCTION__);
            }
        }

        const Value& at(const Key& key) const
        {
            return const_cast<decltype(this)>(this)->at(key);
        }

    private:
        template < typename Delta, typename ValueT > void insert(Delta& delta, const Key& key, const ValueT& value)
        {
            auto replica_id = this->get_allocator().get_replica().get_id();
            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(replica_id, counter);
            auto& data = delta.values_[key];
            data.dots.emplace(replica_id, counter);
            data.value.merge(value);
        }
    };
}