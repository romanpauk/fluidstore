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

    template < typename ReplicaId, typename InstanceRegistry = empty_instance_registry< std::pair< ReplicaId, uint64_t > > > class replica
        : noncopyable
        , public InstanceRegistry
    {
    public:
        typedef replica< ReplicaId, InstanceRegistry > replica_type;
        typedef ReplicaId replica_id_type;
        typedef typename InstanceRegistry::id_type instance_id_type;

        template < typename Instance > struct hook 
        {
            hook(replica_type&) {}
            
            hook(replica_type&, const instance_id_type& id) 
                : id_(id)
            {}

            const instance_id_type& get_id() const { return id_; }

        private:
            instance_id_type id_;
        };

        replica(const ReplicaId& id)
            : id_(id)
        {}

        const ReplicaId& get_id() const { return id_; }
        
        template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta) {}

    private:
        ReplicaId id_;
    };

    template < typename Id > class instance_registry
    {
        struct registered_instance_base
        {
            virtual ~registered_instance_base() {}
            virtual void merge(const void*) = 0;
        };

        template < typename Instance, typename Allocator, typename Replica > struct registered_instance : registered_instance_base
        {
            registered_instance(Instance& instance)
                : instance_(instance)
            {}

            void merge(const void* instance)
            {
                typedef typename Instance::rebind< Allocator, Replica >::type delta_type;
                auto instance_ptr = reinterpret_cast<const delta_type*>(instance);
                instance_.merge(*instance_ptr);
            }

            Instance& instance_;
        };

        typedef std::map< Id, std::unique_ptr< registered_instance_base > > instances_type;
        
    public:
        typedef Id id_type;
        typedef typename instances_type::iterator iterator;

        template < typename Allocator, typename Replica, typename Instance > iterator insert(const Id& id, Instance& instance) 
        {
            auto [it, inserted] = instances_.emplace(id, std::make_unique< registered_instance< Instance, Allocator, Replica > >(instance));
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

    template < typename ReplicaId, typename InstanceRegistry, typename Visitor = empty_visitor > class aggregating_replica
        : public replica< ReplicaId, InstanceRegistry > 
    {
    public:
        typedef replica< ReplicaId, empty_instance_registry< typename InstanceRegistry::id_type > > delta_replica_type;
        typedef crdt::allocator < delta_replica_type > delta_allocator_type;

        using replica< ReplicaId, InstanceRegistry >::replica_id_type;
        using replica< ReplicaId, InstanceRegistry >::instance_id_type;
        
    private:
        struct delta_instance_base
        {
            virtual ~delta_instance_base() {}
            virtual void visit(Visitor&) const {}
        };

        template < typename Instance > struct delta_instance : delta_instance_base
        {
            delta_instance(delta_allocator_type allocator, typename replica_type::instance_id_type id)
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
                it_ = replica_.insert< delta_allocator_type, delta_replica_type >(id_, *static_cast<Instance*>(this));
            }

            hook(replica_type& replica, instance_id_type id)
                : replica_(replica)
                , id_(id)
            {
                it_ = replica_.insert< delta_allocator_type, delta_replica_type >(id_, *static_cast<Instance*>(this));
            }

            ~hook()
            {
                replica_.erase(it_);
            }

            const instance_id_type& get_id() const { return id_; }

        private:
            replica_type& replica_;
            typename replica_type::iterator it_;
            instance_id_type id_;
        };

        aggregating_replica(ReplicaId replica_id)
            : replica_type(replica_id)
            , delta_replica_(replica_id)
        {}

        template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        {			
            // TODO: we will have to maintain the order of merges so they can be reapplied on different replica without sorting there.
            get_delta_instance(target).merge(source);
            // TODO: We will have to track removals so we can remove removed instances from instances_.
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
            typedef typename Instance::rebind< delta_allocator_type, delta_replica_type >::type delta_type;

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
        std::map< typename replica_type::instance_id_type, std::unique_ptr< delta_instance_base > > delta_instances_;
    };

    template < typename Replica, typename Counter = uint64_t, typename Allocator = allocator< Replica > > struct traits_base
    {
        typedef Replica replica_type;
        typedef typename Replica::replica_id_type replica_id_type;
        typedef typename Replica::instance_id_type instance_id_type;
        typedef Counter counter_type;
        typedef Allocator allocator_type;
    };

    struct traits : traits_base< 
        replica< uint64_t, empty_instance_registry< std::pair< uint64_t, uint64_t > > >
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
            std::size_t h1 = std::hash< replica_id_ >{}(replica_id);
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

namespace crdt {

    template < typename Key, typename Allocator, typename Replica, typename Counter > class set_base
        : public dot_kernel<
            Key, void, Allocator, typename Replica::replica_id_type, Counter,
            set_base< Key, Allocator, Replica, Counter >
        >
        , public Replica::template hook< set_base< Key, Allocator, Replica, Counter > >
    {
        typedef set_base< Key, Allocator, Replica, Counter > set_base_type;

    public:
        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            typedef set_base< Key, AllocatorT, ReplicaT, Counter > type;
        };

        set_base(Allocator allocator, typename Replica::instance_id_type id)
            : dot_kernel<
                Key, void, Allocator, typename Replica::replica_id_type, Counter, set_base_type
            >(allocator)
            , Replica::template hook< set_base_type >(allocator.get_replica(), id)
        {}

        set_base(Allocator allocator)
            : dot_kernel<
                Key, void, Allocator, typename Replica::replica_id_type, Counter, set_base_type
            >(allocator)
            , Replica::template hook< set_base_type >(allocator.get_replica())
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
            set_base< Key, decltype(allocator3), replica< typename Replica::replica_id_type >, Counter > delta(allocator3, this->get_id());

            // set_base_type delta(this->allocator_, this->get_id());

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });
            delta.values_[key].dots.emplace(dot_type{ replica_id, counter });

            this->merge(delta);
        }
    };

    template< typename Key, typename Traits > class set
        : public set_base< Key,
            typename Traits::allocator_type,
            typename Traits::replica_type,
            typename Traits::counter_type
        >
        , noncopyable
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        set(allocator_type allocator, typename Traits::instance_id_type id)
            : set_base< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator, id)
        {}

        set(std::allocator_arg_t, allocator_type allocator)
            : set_base< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator)
        {}
    };

    template < typename Key, typename Value, typename Allocator, typename Replica, typename Counter > class map_base
        : public dot_kernel<
            Key, Value, Allocator, typename Replica::replica_id_type, Counter,
            map_base< Key, Value, Allocator, Replica, Counter >
        >
        , public Replica::template hook< map_base< Key, Value, Allocator, Replica, Counter > >
    {
        typedef map_base< Key, Value, Allocator, Replica, Counter > map_base_type;

    public:
        // TODO: ugh - we need to rebind value recursively.

        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            typedef map_base< Key, Value, AllocatorT, ReplicaT, Counter > type;
        };

        map_base(Allocator allocator, typename Replica::instance_id_type id)
            : dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, Counter, map_base_type >(allocator)
            , Replica::template hook< map_base_type >(allocator.get_replica(), id)
        {}

        map_base(Allocator allocator)
            : dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, Counter, map_base_type >(allocator)
            , Replica::template hook< map_base_type >(allocator.get_replica())
        {}

        void insert(const Key& key, const Value& value)
        {
            arena< 1024 > buffer;

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename Replica::replica_id_type > rep(replica_id);
            allocator< replica< typename Replica::replica_id_type > > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            map_base< Key, Value, decltype(allocator3), replica< typename Replica::replica_id_type >, Counter > delta(allocator3, this->get_id());

            // dot_kernel_type delta(this->allocator_, this->get_id());

            // dot_kernel_map< Key, Value, AllocatorT, ReplicaT, Counter >

            // dot_kernel_type delta(this->allocator_);

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });

            auto& data = delta.values_[key];
            data.dots.emplace(dot_type{ replica_id, counter });
            data.value = value;

            this->merge(delta);
        }
    };

    template< typename Key, typename Value, typename Traits > class map
        : public map_base< Key, Value, 
            typename Traits::allocator_type, 
            typename Traits::replica_type, 
            typename Traits::counter_type
        >
        , noncopyable
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        map(allocator_type allocator, typename Traits::instance_id_type id)
            : map_base< Key, Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator, id)
        {}

        map(std::allocator_arg_t, allocator_type allocator)
            : map_base< Key, Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator)
        {}
    };

    template < typename Value, typename Traits > class value
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        value(allocator_type)
            : value_()
        {}

        value(allocator_type, const Value& value)
            : value_(value)
        {}

        void merge(const value< Value, Traits >& other)
        {
            value_ = other.value_;
        }

        bool operator == (const Value& value) const { return value_ == value; }

    private:
        Value value_;
    };

    template < typename Value, typename Traits > class value_mv
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        value_mv(allocator_type allocator)
            : values_(allocator)
        {}

        value_mv(allocator_type allocator, const Value& value)
            : values_(allocator)
        {
            set(value);
        }

        Value get() const
        {
            switch (values_.size())
            {
            case 0:
                return Value();
            case 1:
                return *values_.begin();
            default:
                // TODO: this needs some better interface
                std::abort();
            }
        }

        void set(Value value)
        {
            values_.clear();
            values_.insert(value);
        }

        void merge(const value_mv< Value, Traits >& other)
        {
            values_.merge(other.values_);
        }

        bool operator == (const Value& value) const { return get() == value; }

    private:
        crdt::set< Value, Traits > values_;
    };
}
