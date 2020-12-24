#pragma once

#include <fluidstore/crdts/noncopyable.h>
#include <fluidstore/crdts/allocator.h>

#include <deque>

namespace crdt
{
    template < typename Id > class empty_instance_registry
    {
    public:
        typedef Id id_type;
    };

    template < typename Id > struct id_sequence
        : noncopyable
    {
        id_sequence()
            : id_()
        {}

        Id next()
        {
            return ++id_;
        }

    private:
        Id id_;
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
        typedef id_sequence< InstanceId > id_sequence_type;

        template < typename Instance > struct hook
        {
            hook(replica_type& replica)
                : id_(replica.generate_instance_id())
            {}

            hook(replica_type&, const id_type& id)
                : id_(id)
            {}

            const id_type& get_id() const { return id_; }

        private:
            id_type id_;
        };

        replica(const ReplicaId& id, id_sequence< InstanceId >& seq)
            : id_(id)
            , sequence_(seq)
        {}

        const ReplicaId& get_id() const { return id_; }
        id_type generate_instance_id() { return { id_, sequence_.next() }; }
        id_sequence_type& get_sequence() { return sequence_; }

        template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta) {}

    private:
        ReplicaId id_;
        id_sequence< InstanceId >& sequence_;
    };

    template < typename Id > class instance_registry
    {
    public:
        struct instance_base
        {
            virtual ~instance_base() {}
            virtual void merge(const void*) = 0;
        };

        template < typename Instance, typename Allocator > struct instance : instance_base
        {
            instance(Instance& i)
                : instance_(i)
            {}

            void merge(const void* i)
            {
                typedef typename Instance::template rebind< Allocator >::type delta_type;
                auto instance_ptr = reinterpret_cast<const delta_type*>(i);
                instance_.merge(*instance_ptr);
            }

            Instance& instance_;
        };

        typedef std::map< Id, instance_base* > instances_type;

    public:
        typedef Id id_type;
        typedef typename instances_type::iterator iterator;

        iterator insert(const Id& id, instance_base& i)
        {
            auto [it, inserted] = instances_.emplace(id, &i);
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
        typedef id_sequence< InstanceId > id_sequence_type;

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
                , registry_instance_(*static_cast< Instance* >(this))
                , it_(replica_.insert(replica.generate_instance_id(), registry_instance_))
            {}

            hook(replica_type& replica, id_type id)
                : replica_(replica)
                , registry_instance_(*static_cast< Instance* >(this))
                , it_(replica_.insert(id, registry_instance_))
            {}

            ~hook()
            {
                replica_.erase(it_);
            }

            const id_type& get_id() const { return it_->first; }

        private:
            replica_type& replica_;
            typename InstanceRegistry::template instance< Instance, delta_allocator_type > registry_instance_;
            typename InstanceRegistry::iterator it_;
        };

        aggregating_replica(ReplicaId replica_id, id_sequence< InstanceId >& seq)
            : replica_type(replica_id, seq)
            , delta_replica_(replica_id, seq)
        {}

        template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        {
            get_delta_instance(target).merge(source);
            // TODO: We will have to track removals so we can remove removed instances from delta_instances_.
        }

        template< typename Instance > void merge(const Instance& source)
        {
            this->instances_.at(source.get_id())->merge(&source);
        }

        void visit(Visitor& visitor) const
        {
            for (const auto& instance : deltas_)
            {
                instance->visit(visitor);
            }
        }

        void clear()
        {
            delta_instances_.clear();
            deltas_.clear();
        }

    private:
        template < typename Instance > auto& get_delta_instance(const Instance& instance)
        {
            typedef typename Instance::template rebind< delta_allocator_type >::type delta_type;

            auto& context = delta_instances_[instance.get_id()];
            if (!context)
            {
                delta_allocator_type delta_allocator(delta_replica_);
                auto ptr = new delta_instance< delta_type >(delta_allocator, instance.get_id());
                context.reset(ptr);
                deltas_.push_back(ptr);
                return *ptr;
            }
            else
            {
                return dynamic_cast< delta_instance< delta_type >& >(*context);
            }
        }

        delta_replica_type delta_replica_;
        std::map< typename replica_type::id_type, std::unique_ptr< delta_instance_base > > delta_instances_;
        std::deque< delta_instance_base* > deltas_;
    };

    template < typename Replica, typename Allocator = allocator< Replica > > struct traits_base
    {
        typedef Replica replica_type;
        typedef typename Replica::replica_id_type replica_id_type;
        typedef typename Replica::id_type id_type;
        typedef typename Replica::counter_type counter_type;
        typedef typename Replica::id_sequence_type id_sequence_type;
        typedef Allocator allocator_type;
    };

    struct traits : traits_base<
        replica< uint64_t, uint64_t, uint64_t, empty_instance_registry< std::pair< uint64_t, uint64_t > > >
    >
    {};


}