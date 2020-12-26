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

    template < typename ReplicaId, typename InstanceId, typename Counter, typename DeltaAllocator, typename Visitor = empty_visitor > class delta_replica
        : public replica< ReplicaId, InstanceId, Counter >
    {
        typedef delta_replica< ReplicaId, InstanceId, Counter, DeltaAllocator, Visitor > this_type;

    public:
        typedef replica< ReplicaId, InstanceId, Counter > delta_replica_type;
        
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
                    typedef typename Instance::template rebind< Allocator, tag_delta >::type delta_type;
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
            
            instance_base* get_instance_ptr(id_type id) 
            {
                auto it = instances_.find(id);
                if (it != instances_.end())
                {
                    return it->second;
                }

                return nullptr;
            }

            // private:
            instances_type instances_;
        };

        class delta_registry
        {
        public:
            delta_registry(std::function< DeltaAllocator() > factory)
                : delta_allocator_(factory())
                , delta_allocator_factory_(factory)
            {}

            struct instance_base
            {
                virtual ~instance_base() {}
                virtual void visit(Visitor&) const = 0;
                virtual void clear_instance_ptr() = 0;
            };

            template < typename Instance, typename DeltaInstance, typename Allocator > struct instance : instance_base
            {
                instance(Allocator allocator, const Instance& instance)
                    : delta_instance_(allocator, instance.get_id())
                    , instance_(&instance)
                {
                    instance.delta_instance_ = this;
                }

                ~instance()
                {
                    clear_instance_ptr();
                }

                template < typename DeltaInstanceT > void merge(const DeltaInstanceT& delta)
                {
                    delta_instance_.merge(delta);
                }

                void visit(Visitor& visitor) const override
                {
                    visitor.visit(delta_instance_);
                }

                void clear_instance_ptr()
                {
                    if (instance_)
                    {
                        instance_->delta_instance_ = nullptr;
                        instance_ = nullptr;
                    }
                }

            private:
                const Instance* instance_;
                DeltaInstance delta_instance_;
            };

            template < typename Instance > auto& get_instance(const Instance& i)
            {
                typedef typename Instance::template rebind< DeltaAllocator, tag_delta >::type delta_type;
                typedef instance< Instance, delta_type, DeltaAllocator > delta_instance_type;

                if (!i.delta_instance_)
                {
                    auto& context = instances_[i.get_id()];
                    if (!context)
                    {
                        context.reset(new delta_instance_type(delta_allocator_, i));
                        deltas_.push_back(context.get());
                    }
                }

                return dynamic_cast< delta_instance_type& >(*i.delta_instance_);                
            }

            void clear()
            {
                deltas_.clear();
                instances_.clear();
                delta_allocator_ = delta_allocator_factory_();
            }

            auto begin() { return instances_.begin(); }
            auto end() { return instances_.end(); }

            DeltaAllocator delta_allocator_;
            std::function< DeltaAllocator() > delta_allocator_factory_;

            std::map< id_type, std::unique_ptr< instance_base > > instances_;
            std::deque< instance_base* > deltas_;
        };

    public:
        template< typename Instance > struct hook
        {
            hook(id_type id)
                : instance_(*static_cast<Instance*>(this))
                , instance_it_(static_cast<Instance*>(this)->get_allocator().get_replica().instance_registry_.insert(id, instance_))
                , delta_instance_()
            {}

            ~hook()
            {
                static_cast<Instance*>(this)->get_allocator().get_replica().instance_registry_.erase(instance_it_);
                if (delta_instance_)
                {
                    delta_instance_->clear_instance_ptr();
                }
            }

            const id_type& get_id() const { return instance_it_->first; }

        private:
            typename instance_registry::template instance< Instance, DeltaAllocator > instance_;
            typename instance_registry::iterator instance_it_;

            friend class delta_registry;
            mutable typename delta_registry::instance_base* delta_instance_;
        };

        delta_replica(
            ReplicaId replica_id, 
            id_sequence< InstanceId >& sequence, 
            std::function< DeltaAllocator() > delta_allocator_factory
        )
            : replica_type(replica_id, sequence)
            , delta_registry_(delta_allocator_factory)
        {}

        template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        {
            delta_registry_.get_instance< Instance >(target).merge(source);
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