#pragma once

#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>

#include <deque>

namespace crdt
{
    struct empty_visitor
    {
        template < typename T > void visit(T) {}
    };

    template < typename ReplicaId, typename InstanceId, typename Counter, typename Visitor = empty_visitor > class aggregating_replica
        : public replica< ReplicaId, InstanceId, Counter >
    {
    public:
        typedef aggregating_replica< ReplicaId, InstanceId, Counter, Visitor > aggregating_replica_type;
        typedef replica< ReplicaId, InstanceId, Counter > replica_type;
        
        typedef replica_type delta_replica_type;
        typedef crdt::allocator < delta_replica_type > delta_allocator_type;

        using replica_type::replica_id_type;
        using replica_type::instance_id_type;
        using replica_type::id_type;
        using replica_type::counter_type;
        typedef id_sequence< InstanceId > id_sequence_type;

    private:
        class instance_registry
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

            typedef std::map< id_type, instance_base* > instances_type;

        public:
            typedef typename instances_type::iterator iterator;

            iterator insert(const id_type& id, instance_base& i)
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

            instance_base& get_instance(id_type id) { return *instances_.at(id); }

            // private:
            instances_type instances_;
        };

        class delta_registry
        {
        public:
            delta_registry(ReplicaId replica_id, id_sequence< InstanceId >& sequence)
                : delta_replica_(replica_id, sequence)
            {}

            struct instance_base
            {
                virtual ~instance_base() {}
                virtual void visit(Visitor&) const {}
            };

            template < typename Instance > struct instance : instance_base
            {
                instance(delta_allocator_type allocator, id_type id)
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

            template < typename Instance > auto& get_instance(const id_type& id)
            {
                typedef typename Instance::template rebind< delta_allocator_type >::type delta_type;

                auto& context = instances_[id];
                if (!context)
                {
                    delta_allocator_type delta_allocator(delta_replica_);
                    auto ptr = new instance< delta_type >(delta_allocator, id);
                    context.reset(ptr);
                    deltas_.push_back(ptr);
                    return *ptr;
                }
                else
                {
                    return dynamic_cast<instance< delta_type >&>(*context);
                }
            }

            void clear()
            {
                instances_.clear();
                deltas_.clear();
            }

            delta_replica_type delta_replica_;
            std::map< id_type, std::unique_ptr< instance_base > > instances_;
            std::deque< instance_base* > deltas_;
        };

    public:
        template< typename Instance > struct hook
        {
            hook(aggregating_replica_type& replica)
                : replica_(replica)
                , registry_instance_(*static_cast<Instance*>(this))
                , it_(replica_.instance_registry_.insert(replica.generate_instance_id(), registry_instance_))
            {}

            hook(aggregating_replica_type& replica, id_type id)
                : replica_(replica)
                , registry_instance_(*static_cast<Instance*>(this))
                , it_(replica_.instance_registry_.insert(id, registry_instance_))
            {}

            ~hook()
            {
                replica_.instance_registry_.erase(it_);
            }

            const id_type& get_id() const { return it_->first; }

        private:
            aggregating_replica_type& replica_;
            typename instance_registry::template instance< Instance, delta_allocator_type > registry_instance_;
            typename instance_registry::iterator it_;
        };

        aggregating_replica(ReplicaId replica_id, id_sequence< InstanceId >& seq)
            : replica_type(replica_id, seq)
            , delta_registry_(replica_id, seq)
        {}

        template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        {
            delta_registry_.get_instance< Instance >(target.get_id()).merge(source);
            // TODO: We will have to track removals so we can remove removed instances from delta_instances_.
        }

        template< typename Instance > void merge(const Instance& source)
        {
            this->instance_registry_.get_instance(source.get_id()).merge(&source);
        }

        void visit(Visitor& visitor) const
        {
            for (const auto& instance : delta_registry_.deltas_)
            {
                instance->visit(visitor);
            }
        }

        void clear()
        {
            delta_registry_.clear();
        }

    private:
        instance_registry instance_registry_;
        delta_registry delta_registry_;
    };
}