#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/allocators/arena_allocator.h>

#include <memory>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Tag = tag_state > class map
        : public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator >, Tag >
        , public Allocator::replica_type::template hook< map< Key, Value, Allocator > >
    {
        typedef map< Key, Value, Allocator > map_type;
        typedef dot_kernel< Key, Value, Allocator, map_type, Tag > dot_kernel_type;
    
    public:
        typedef Allocator allocator_type;
        typedef typename Allocator::replica_type replica_type;
       
        template < typename AllocatorT, typename TagT > struct rebind
        {
            typedef map< Key, typename Value::template rebind< AllocatorT, TagT >::type, AllocatorT, TagT > type;
        };

        map(Allocator allocator)
            : replica_type::template hook< map_type >(allocator.get_replica(), allocator.get_replica().generate_instance_id())
            , dot_kernel_type(allocator)
        {}

        map(Allocator allocator, typename Allocator::replica_type::id_type id)
            : replica_type::template hook< map_type >(allocator.get_replica(), id)
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            arena< 1024 * 2 > buffer;
            arena_allocator< void, allocator< typename replica_type::delta_replica_type > > allocator(buffer, this->allocator_.get_replica());
            typename rebind< decltype(allocator), tag_delta >::type delta(allocator, this->get_id());
            //map_type delta(this->allocator_, this->get_id());

            insert(delta, key, value);

            // TODO:
            // When this is insert of one element, merge should change internal state only when an insert operation would.
            // But we already got id. So the id would have to be returned, otherwise dot sequence will not collapse.
            
            insert_context context;
            this->merge(delta, context);
            this->allocator_.merge(*this, delta);
            return { context.result.first, context.result.second };
        }

        Value& operator[](const Key& key)
        {
            arena< 1024 * 2 > buffer;
            arena_allocator< void, allocator< typename replica_type::delta_replica_type > > allocator(buffer, this->allocator_.get_replica());
            typename rebind< decltype(allocator), tag_delta >::type delta(allocator, this->get_id());

            if constexpr (std::uses_allocator_v< Value, Allocator >)
            {
                // TODO: this Value should not be tracked, it is temporary.
                auto pairb = insert(key, Value(allocator_));
                return (*pairb.first).second;
            }
            else
            {
                auto pairb = insert(key, Value());
                return (*pairb.first).second;
            }
        }

    private:
        template < typename Delta, typename ValueT > void insert(Delta& delta, const Key& key, const ValueT& value)
        {
            auto replica_id = this->allocator_.get_replica().get_id();
            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(replica_id, counter);
            auto& data = delta.values_[key];
            data.dots.emplace(replica_id, counter);
            data.value.merge(value);
        }
    };
}