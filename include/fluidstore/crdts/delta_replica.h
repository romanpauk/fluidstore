#pragma once

#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>

#include <deque>

namespace crdt
{
    struct empty_visitor
    {
        template < typename T > void visit(const T&) {}
    };

    template < typename Allocator, typename Visitor > class delta_registry
    {
    public:
        typedef Allocator allocator_type;
        typedef typename allocator_type::replica_type::id_type id_type;

        delta_registry(allocator_type allocator)
            : delta_allocator_(allocator)
        {}

        struct instance_base
        {
            virtual ~instance_base() {}
            virtual void visit(Visitor&) const = 0;
            virtual void clear_instance_ptr() = 0;
            virtual const void* get_delta_instance_ptr() = 0;
            virtual const id_type& get_id() = 0;
        };

        template < typename Instance, typename DeltaInstance > struct instance : instance_base
        {
            instance(allocator_type allocator, const Instance& instance)
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

            const void* get_delta_instance_ptr()
            {
                return &delta_instance_;
            }

            const id_type& get_id()
            {
                return instance_->get_id();
            }

        private:
            const Instance* instance_;
            DeltaInstance delta_instance_;
        };

        template < typename Instance > auto& get_instance(const Instance& i)
        {
            typedef typename Instance::template rebind< allocator_type, tag_delta, default_hook >::type delta_type;
            typedef instance< Instance, delta_type > delta_instance_type;

            if (!i.delta_instance_)
            {
                auto& context = instances_[i.get_id()];
                if (!context)
                {
                    context.reset(new delta_instance_type(delta_allocator_, i));
                    deltas_.push_back(context.get());
                }
            }

            return dynamic_cast<delta_instance_type&>(*i.delta_instance_);
        }

        void clear()
        {
            deltas_.clear();
            instances_.clear();
        }

        auto begin() { return instances_.begin(); }
        auto end() { return instances_.end(); }

        allocator_type delta_allocator_;

        std::map< id_type, std::unique_ptr< instance_base > > instances_;
        std::deque< instance_base* > deltas_;
    };

    template < typename Allocator, typename Id > class instance_registry
    {
        typedef Id id_type;

    public:
        struct instance_base
        {
            virtual ~instance_base() {}
            virtual void merge(const void*) = 0;
        };

        template < typename Instance, typename DeltaAllocator > struct instance : instance_base
        {
            instance(Instance& i)
                : instance_(i)
            {}

            void merge(const void* i)
            {
                typedef typename Instance::template rebind< Allocator, tag_delta, default_hook >::type delta_type;
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

    struct empty_delta_replica_callback 
    {
        template < typename Delta > void commit_delta(const Delta&) {}
    };

    template < typename Allocator = std::allocator< unsigned >, typename System = crdt::system<>, typename Visitor = empty_visitor > class delta_replica
        : public replica< System >
    {
    public:
        typedef replica< System > delta_replica_type;
        using typename System::replica_id_type;
        using typename System::instance_id_type;
        using typename System::id_type;
        using typename System::counter_type;
        using typename System::instance_id_sequence_type;
        
    public:
        delta_replica(replica_id_type id, instance_id_sequence_type& sequence, Allocator allocator = Allocator())
            : replica< System >(id, sequence)
            //, delta_registry_(allocator)
        {}

        template < typename Delta > void commit_delta(Delta& delta) 
        {

        }

        ///template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        //{
        //    delta_registry_.get_instance< Instance >(target).merge(source);
            // TODO: We will have to track removals so we can remove removed instances from delta_instances_.
        //}

        //template< typename Instance > void merge(const Instance& source)
        //{
        //    this->instance_registry_.get_instance(source.get_id()).merge(&source);
        //}

        //void merge(const id_type& id, const void* source)
        //{
        //    this->instance_registry_.get_instance(id).merge(source);
        //}

        // TODO: I would rather merge two objects togetger
        //template < typename Replica > void merge_with_replica(Replica& replica)
        //{
        //    for (const auto& instance : delta_registry_.deltas_)
        //    {
        //        replica.merge(instance->get_id(), instance->get_delta_instance_ptr());
        //    }
        //}

        void visit(Visitor& visitor) const
        {
            //for (const auto& instance : delta_registry_.deltas_)
            //{
            //    instance->visit(visitor);
            //}
        }

        void clear()
        {
            // delta_registry_.clear();
        }

    public: // TODO
        instance_registry< Allocator, id_type > instance_registry_;
        //delta_registry< Allocator, Visitor > delta_registry_;
    };

    class delta_replica_hook
    {
    public:
        template < typename Allocator, typename Delta, typename Instance > struct hook
            : public default_hook::template hook< Allocator, Delta, Instance >
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(allocator_type allocator, const id_type& id)
                : default_hook::template hook< Allocator, Delta, Instance >(allocator, id)
                //, instance_(*static_cast<Instance*>(this))
                //, instance_it_(allocator.get_replica().instance_registry_.insert(id, instance_))
                //, delta_instance_()
            {}

            ~hook()
            {
                //static_cast<Instance*>(this)->get_allocator().get_replica().instance_registry_.erase(instance_it_);
                //if (delta_instance_)
                //{  
                //    delta_instance_->clear_instance_ptr();
                //}
            }

            void commit_delta(Delta& delta)
            {
                auto& replica = static_cast<Instance*>(this)->get_allocator().get_replica();
                // replica.commit_delta(delta);
 
                default_hook::template hook< Allocator, Delta, Instance >::commit_delta(delta);
            }

        private:
            //typename instance_registry< Allocator, id_type >::template instance< Instance, Allocator > instance_;
            //typename instance_registry< Allocator, id_type >::iterator instance_it_;

            // template < typename Allocator, typename Visitor > friend class delta_registry;
            // mutable typename delta_registry< Allocator, Visitor >::instance_base* delta_instance_;
        };
    };
}