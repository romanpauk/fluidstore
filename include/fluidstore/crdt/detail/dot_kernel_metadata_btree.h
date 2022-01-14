#pragma once

#include <fluidstore/btree/set.h>
#include <fluidstore/btree/map.h>

#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata.h>

// #define DOTKERNEL_METADATA_HINT

namespace crdt
{
    namespace detail
    {
        struct metadata_tag_btree_local {};
        struct metadata_tag_btree_shared {};
        struct metadata_tag_btree_test {};

        template <  typename Key, typename Tag, typename Allocator > struct dot_kernel_metadata
            < Key, Tag, Allocator, metadata_tag_btree_local >
        {
            using metadata_tag_type = metadata_tag_btree_local;

            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            using counters_type = btree::set_base< counter_type >;
            using value_type_dots_type = btree::map_base < replica_id_type, counters_type >;            

            template < typename KeyT, typename ValueT > using values_map_type = btree::map_base< KeyT, ValueT >;
            template < typename AllocatorT > using visited_map_type = btree::map< replica_id_type, btree::set_base< counter_type >, std::less< replica_id_type >, AllocatorT >;

            // Replicas
            struct replica_data
            {
                btree::set_base< counter_type > counters;
                btree::map_base< counter_type, Key > dots;
            };

            replica_data& get_replica_data(Allocator& allocator, replica_id_type id)
            {
                return replica_.emplace(allocator, id, replica_data()).first->second;
            }

            replica_data* get_replica_data(replica_id_type id)
            {
                auto it = replica_.find(id);
                return it != replica_.end() ? &it->second : nullptr;
            }

            counter_type replica_counters_get(replica_data& replica)
            {
                return !replica.counters.empty() ? *--replica.counters.end() : counter_type();
            }

            void replica_counters_add(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                auto& counters = get_replica_data(allocator, id).counters;
            #if defined(DOTKERNEL_METADATA_HINT)
                counters.emplace_hint(allocator, counters.end(), counter);
            #else
                counters.emplace(allocator, counter);
            #endif
            }
                        
            template < typename Counters > void replica_counters_add(Allocator& allocator, replica_id_type id, Counters& counters)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                get_replica_data(allocator, id).counters.insert(allocator, counters.begin(), counters.end());                
            }            

            auto counters_erase(Allocator& allocator, counters_type& counters, typename counters_type::iterator it)
            {
                return counters.erase(allocator, it);
            }

            void counters_update(Allocator& allocator, counters_type& counters, typename counters_type::iterator it, counter_type value)
            {
                // TODO: in-place update, this should not change the tree layout

                //counters.insert(allocator, it, value);  
                //counters.erase(allocator, it);
                //const_cast< counter_type& >(*it) = value;

                auto pos = counters.erase(allocator, it);
                counters.insert(allocator, pos, value);  
            }

            template < typename AllocatorT, typename It > void counters_insert(AllocatorT& allocator, counters_type& counters, It begin, It end)
            {
                counters.insert(allocator, begin, end);
            }

            template < typename AllocatorT > void counters_clear(AllocatorT& allocator, counters_type& counters)
            {
                counters.clear(allocator);
            }

            void replica_dots_add(Allocator& allocator, replica_data& replica, counter_type counter, Key key)
            {
            #if defined(DOTKERNEL_METADATA_HINT)
                replica.dots.emplace_hint(allocator, replica.dots.end(), counter, key);
            #else
                replica.dots.emplace(allocator, counter, key);
            #endif
            }

            void replica_dots_erase(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                auto replica = get_replica_data(id);
                if (replica)
                {
                    replica->dots.erase(allocator, counter);
                }
            }

            void replica_dots_erase(Allocator& allocator, replica_data& replica, typename btree::map_base< counter_type, Key >::iterator it)
            {
                replica.dots.erase(allocator, it);
            }

            auto replica_dots_find(replica_data& replica, counter_type counter)
            {
                return replica.dots.find(counter);
            }

            auto& value_counters_fetch(Allocator& allocator, value_type_dots_type& ldots, replica_id_type id)
            {
                auto& counters = ldots.emplace(allocator, id, btree::set_base< counter_type >()).first->second;
                return counters;
            }

            void value_counters_emplace(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto& counters = ldots.emplace(allocator, dot.replica_id, btree::set_base< counter_type >()).first->second;
            #if defined(DOTKERNEL_METADATA_HINT)
                counters.emplace_hint(allocator, counters.end(), dot.counter);
            #else
                counters.emplace(allocator, dot.counter);
            #endif
            }

            void value_counters_erase(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto it = ldots.find(dot.replica_id);
                if (it != ldots.end())
                {
                    it->second.erase(allocator, dot.counter);
                    if (it->second.empty())
                    {
                        ldots.erase(allocator, it);
                    }
                }                
            }

            template < typename Values, typename Value > auto values_emplace(Allocator& allocator, Values& values, const Key& key, Value&& value)
            {
            #if defined(DOTKERNEL_METADATA_HINT)
                return values.emplace_hint(allocator, values.end(), key, std::forward< Value >(value));
            #else
                return values.emplace(allocator, key, std::forward< Value >(value));
            #endif
            }

            template < typename Values > auto values_find(Values& values, const Key& key)
            {
                return values.find(key);
            }

            template < typename Values > auto values_erase(Allocator& allocator, Values& values, typename Values::iterator it)
            {
                return values.erase(allocator, it);
            }

            template < typename Values > void values_clear(Allocator& allocator, Values& values)
            {
                for (auto&& value : values)
                {
                    // TODO: this should not run if we have trivially destructible elements.
                    for (auto&& [replica_id, dots] : value.second.dots)
                    {
                        dots.clear(allocator);
                    }

                    value.second.dots.clear(allocator);
                }
                values.clear(allocator);
            }
            
            const btree::map_base < replica_id_type, replica_data >& get_replica_map() const { return replica_; }
            btree::map_base < replica_id_type, replica_data >& get_replica_map() { return replica_; }

            void clear(Allocator& allocator)
            {
                for (auto&& [replica_id, data] : replica_)
                {
                    data.counters.clear(allocator);
                    data.dots.clear(allocator);
                }
                replica_.clear(allocator);
            }

        private:
            btree::map_base < replica_id_type, replica_data > replica_;
        };

        template < typename Container, typename Metadata > struct dot_kernel_metadata_base< Container, Metadata, metadata_tag_btree_local >
        {
            Metadata& get_metadata() { return metadata_; }
            const Metadata& get_metadata() const { return metadata_; }

        private:
            Metadata metadata_;
        };

        /*
        template <  typename Key, typename Tag, typename Allocator > struct dot_kernel_metadata
            < Key, Tag, Allocator, metadata_tag_btree_test >
        {
            using metadata_tag_type = metadata_tag_btree_test;

            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            using counters_type = btree::set_base< counter_type >;
            using value_type_dots_type = btree::map_base < replica_id_type, counters_type >;

            template < typename Key, typename Value > using values_map_type = btree::map_base< Key, Value >;

            // Instance ids
            // InstanceId acquire_instance_id();
            // void release_instance_id(InstanceId);
            
            // TODO: we need to know about all replicas.
            btree::set_base < replica_id_type > replicas;

            btree::set_base< std::pair< replica_id_type, counter_type > > counters;
            btree::map_base< std::pair< replica_id_type, counter_type >, Key > dots;

            // Replicas
            struct replica_data
            {};

            replica_id_type get_replica_data(Allocator&, replica_id_type id)
            {
                return id;
            }

            replica_id_type* get_replica_data(replica_id_type id)
            {
                // TODO
                return &id;
            }

            counter_type replica_counters_get(replica_id_type id)
            {
                auto counter = counter_type();
                auto begin = counters.lower_bound({ id, 0 });
                auto end = counters.lower_bound({ id, std::numeric_limits< counter_type >::max() });
                while (begin != end)
                {
                    counter = begin++->second;
                }
                return counter;
            }

            void replica_counters_add(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    // TODO
                    //assert(get_replica_data(allocator, id).counters.empty());
                }

                counters.emplace(allocator, { id, counter });
            }

            template < typename Counters > void replica_counters_add(Allocator& allocator, replica_id_type id, Counters& counters)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    // TODO
                    //assert(get_replica_data(allocator, id).counters.empty());
                }

                for (auto&& counter : counters)
                {
                    counters.emplace(allocator, { id, counter });
                }                                
            }

            template < typename Counters > auto counters_erase(Allocator& allocator, Counters& counters, typename Counters::iterator it)
            {
                return counters.erase(allocator, it);
            }

            template < typename Counters > void counters_update(Allocator& allocator, Counters& counters, typename Counters::iterator it, counter_type value)
            {
                // TODO: in-place update, this should not change the tree layout
                counters.insert(allocator, it, value);
                counters.erase(allocator, it);
            }

            template < typename AllocatorT, typename Counters, typename It > void counters_insert(AllocatorT& allocator, Counters& counters, It begin, It end)
            {
                counters.insert(allocator, begin, end);
            }

            template < typename AllocatorT, typename Counters > void counters_clear(AllocatorT& allocator, Counters& counters)
            {
                counters.clear(allocator);
            }

            template < typename Key > void replica_dots_add(Allocator& allocator, replica_id_type id, counter_type counter, Key key)
            {
                dots.emplace(allocator, { id, counter }, key);
            }

            void replica_dots_erase(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                dots.erase(allocator, { id, counter });
            }

            void replica_dots_erase(Allocator& allocator, replica_id_type id, typename btree::map_base< std::pair< replica_id_type, counter_type >, Key >::iterator it)
            {
                dots.erase(allocator, it);
            }

            auto replica_dots_find(replica_id_type id, counter_type counter)
            {
                return dots.find({ id, counter });
            }

            auto& value_counters_fetch(Allocator& allocator, value_type_dots_type& ldots, replica_id_type id)
            {
                auto& counters = ldots.emplace(allocator, id, btree::set_base< counter_type >()).first->second;
                return counters;
            }

            void value_counters_emplace(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto& counters = ldots.emplace(allocator, dot.replica_id, btree::set_base< counter_type >()).first->second;
                counters.emplace(allocator, dot.counter);
            }

            void value_counters_erase(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto it = ldots.find(dot.replica_id);
                if (it != ldots.end())
                {
                    it->second.erase(allocator, dot.counter);
                    if (it->second.empty())
                    {
                        ldots.erase(allocator, it);
                    }
                }
            }

            template < typename Values, typename Key, typename Value > auto values_emplace(Allocator& allocator, Values& values, const Key& key, Value&& value)
            {
                return values.emplace(allocator, key, std::forward< Value >(value));
            }

            template < typename Values > auto values_find(Values& values, const Key& key)
            {
                return values.find(key);
            }

            template < typename Values > auto values_erase(Allocator& allocator, Values& values, typename Values::iterator it)
            {
                return values.erase(allocator, it);
            }

            template < typename Values > void values_clear(Allocator& allocator, Values& values)
            {
                for (auto& value : values)
                {
                    // TODO: this should not run if we have trivially destructible elements.
                    for (auto&& [replica_id, dots] : value.second.dots)
                    {
                        dots.clear(allocator);
                    }

                    value.second.dots.clear(allocator);
                }
                values.clear(allocator);
            }

            const btree::map_base < replica_id_type, replica_data >& get_replica_map() const { return replica_; }
            btree::map_base < replica_id_type, replica_data >& get_replica_map() { return replica_; }

            void clear(Allocator& allocator)
            {
                for (auto& [replica_id, data] : replica_)
                {
                    data.counters.clear(allocator);
                    data.dots.clear(allocator);
                    assert(data.visited.empty());
                }
                replica_.clear(allocator);
            }

        private:
            btree::map_base < replica_id_type, replica_data > replica_;
        };
        
        template < typename Container, typename Metadata > struct dot_kernel_metadata_base< Container, Metadata, metadata_tag_btree_test >
        {
            Metadata& get_metadata() { return metadata_; }
            const Metadata& get_metadata() const { return metadata_; }

        private:
            Metadata metadata_;
        };
        */

        /*
        template < typename Container, typename Metadata > struct dot_kernel_metadata_base< Container, Metadata, tag_shared >
        {
            Metadata& get_metadata() { return static_cast<Container*>(this)->get_replica().get_metadata(); }
            const Metadata& get_metadata() const { return return static_cast<Container*>(this)->get_replica().get_metadata(); }
        };
        */
    }
}