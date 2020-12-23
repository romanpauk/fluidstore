#pragma once

#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <scoped_allocator>
#include <memory>

#include <fluidstore/allocators/arena_allocator.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/noncopyable.h>

namespace crdt
{
    template < typename Id > class empty_instance_registry
    {
    public:
        typedef Id id_type;
    };

    template < typename ReplicaId, typename InstanceId, typename Counter, typename InstanceRegistry = empty_instance_registry< std::pair< ReplicaId, InstanceId > > > class replica
        : noncopyable
        , public InstanceRegistry
    {
    public:
        typedef replica< ReplicaId, InstanceId, Counter, InstanceRegistry > replica_type;
        typedef ReplicaId replica_id_type;
        typedef ReplicaId instance_id_type;
        typedef Counter counter_type;
        typedef typename InstanceRegistry::id_type id_type;

        template < typename Instance > struct hook 
        {
            hook(replica_type&)
                : id_()
            {}
            
            hook(replica_type&, const id_type& id) 
                : id_(id)
            {}

            const id_type& get_id() const { return id_; }

        private:
            id_type id_;
        };

        replica(const ReplicaId& id)
            : id_(id)
            , instance_id_(1000)
        {}

        const ReplicaId& get_id() const { return id_; }
        id_type generate_instance_id() { return { id_, ++instance_id_ }; }

        template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta) {}

    private:
        ReplicaId id_;
        InstanceId instance_id_;
    };

    template < typename Id > class instance_registry
    {
        struct registered_instance_base
        {
            virtual ~registered_instance_base() {}
            virtual void merge(const void*) = 0;
        };

        template < typename Instance, typename Allocator > struct registered_instance : registered_instance_base
        {
            registered_instance(Instance& instance)
                : instance_(instance)
            {}

            void merge(const void* instance)
            {
                typedef typename Instance::rebind< Allocator >::type delta_type;
                auto instance_ptr = reinterpret_cast<const delta_type*>(instance);
                instance_.merge(*instance_ptr);
            }

            Instance& instance_;
        };

        typedef std::map< Id, std::unique_ptr< registered_instance_base > > instances_type;
        
    public:
        typedef Id id_type;
        typedef typename instances_type::iterator iterator;

        template < typename Allocator, typename Instance > iterator insert(const Id& id, Instance& instance) 
        {
            auto [it, inserted] = instances_.emplace(id, std::make_unique< registered_instance< Instance, Allocator > >(instance));
            assert(inserted);

            if (!inserted)
            {
                // TODO:
                std::abort();
            }

            return it;
        }
        
        void erase(const iterator& it) 
        {
            instances_.erase(it);
        }

        auto begin() const { return instances_.begin(); }
        auto end() const { return instances_.end(); }

    // private:
        instances_type instances_;
    };

    struct empty_visitor
    {
        template < typename T > void visit(T) {}
    };

    template < typename ReplicaId, typename InstanceId, typename Counter, typename InstanceRegistry, typename Visitor = empty_visitor > class aggregating_replica
        : public replica< ReplicaId, InstanceId, Counter, InstanceRegistry > 
    {
    public:
        typedef replica< ReplicaId, InstanceId, Counter, empty_instance_registry< typename InstanceRegistry::id_type > > delta_replica_type;
        typedef crdt::allocator < delta_replica_type > delta_allocator_type;

        typedef replica< ReplicaId, InstanceId, Counter, InstanceRegistry > replica_type;
        using replica_type::replica_id_type;
        using replica_type::instance_id_type;
        using replica_type::id_type;
        using replica_type::counter_type;
        
    private:
        struct delta_instance_base
        {
            virtual ~delta_instance_base() {}
            virtual void visit(Visitor&) const {}
        };

        template < typename Instance > struct delta_instance : delta_instance_base
        {
            delta_instance(delta_allocator_type allocator, typename replica_type::id_type id)
                : instance_(allocator, id)
            {}

            template < typename DeltaInstance > void merge(const DeltaInstance& delta)
            {
                instance_.merge(delta);
            }

            void visit(Visitor& visitor) const override
            {
                visitor.visit(instance_);
            }

        private:
            Instance instance_;
        };

    public:
        template< typename Instance > struct hook
        {
            hook(replica_type& replica)
                : replica_(replica)
                , id_(replica.generate_instance_id())
            {
                it_ = replica_.insert< delta_allocator_type >(id_, *static_cast< Instance* >(this));
            }

            hook(replica_type& replica, id_type id)
                : replica_(replica)
                , id_(id)
            {
                it_ = replica_.insert< delta_allocator_type >(id_, *static_cast< Instance* >(this));
            }

            ~hook()
            {
                replica_.erase(it_);
            }

            const id_type& get_id() const { return id_; }

        private:
            replica_type& replica_;
            typename replica_type::iterator it_;
            id_type id_;
        };

        aggregating_replica(ReplicaId replica_id)
            : replica_type(replica_id)
            , delta_replica_(replica_id)
        {}

        template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        {			
            // TODO: we will have to maintain the order of merges so they can be reapplied on different replica without sorting there.
            get_delta_instance(target).merge(source);
            // TODO: We will have to track removals so we can remove removed instances from delta_instances_.
        }
                
        template< typename Instance > void merge(const Instance& source)
        {
            // TODO: do something with the interface
            instances_.at(source.get_id())->merge(&source);
        }

        void visit(Visitor& visitor) const
        {
            for (const auto& [id, instance] : delta_instances_)
            {
                instance->visit(visitor);
            }
        }

        void clear()
        {
            delta_instances_.clear();
        }

    private:
        template < typename Instance > auto& get_delta_instance(const Instance& instance)
        {
            typedef typename Instance::rebind< delta_allocator_type >::type delta_type;

            auto& context = delta_instances_[instance.get_id()];
            if (!context)
            {
                delta_allocator_type delta_allocator(delta_replica_);
                auto ptr = new delta_instance< delta_type >(delta_allocator, instance.get_id());
                context.reset(ptr);
                return *ptr;
            }
            else
            {
                return dynamic_cast< delta_instance< delta_type >& >(*context);
            }
        }

        delta_replica_type delta_replica_;
        std::map< typename replica_type::id_type, std::unique_ptr< delta_instance_base > > delta_instances_;
    };

    template < typename Replica, typename Allocator = allocator< Replica > > struct traits_base
    {
        typedef Replica replica_type;
        typedef typename Replica::replica_id_type replica_id_type;
        typedef typename Replica::id_type id_type;
        typedef typename Replica::counter_type counter_type;
        typedef Allocator allocator_type;
    };

    struct traits : traits_base< 
        replica< uint64_t, uint64_t, uint64_t, empty_instance_registry< std::pair< uint64_t, uint64_t > > >
    > {};

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
            std::size_t h1 = std::hash< ReplicaId >{}(replica_id);
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

#include <fluidstore/crdts/dot_context.h>
#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/value_mv.h>

namespace crdt {

    template < typename Value, typename Allocator > class value
    {
    public:
        typedef Allocator allocator_type;
        
        template < typename AllocatorT > struct rebind
        {
            // TODO: this can also be recursive in Value... sometimes.
            typedef value< Value, AllocatorT > type;
        };
        
        value(allocator_type)
            : value_()
        {}

        value(allocator_type, const Value& value)
            : value_(value)
        {}

        template < typename ValueT > void merge(const ValueT& other)
        {
            value_ = other.value_;
        }

        bool operator == (const Value& value) const { return value_ == value; }

    private:
        Value value_;
    };
}
