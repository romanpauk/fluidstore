#pragma once

#include <map>

namespace crdt
{
    struct registry_instance_base
    {
        virtual ~registry_instance_base() {}
        virtual void merge(const void*) = 0;
    };

    template < typename Instance, typename Allocator > struct registry_instance : registry_instance_base
    {
        registry_instance(Instance& i)
            : instance_(i)
        {}

        void merge(const void* i)
        {
            typedef typename Instance::template rebind< Allocator, default_state_hook, tag_delta >::other delta_type;
            auto instance_ptr = reinterpret_cast<const delta_type*>(i);
            instance_.merge(*instance_ptr);
        }

        Instance& instance_;
    };

    template < typename Registry, typename DeltaAllocator > class registry_hook
    {
    public:
        template < typename Allocator, typename Delta, typename Instance > struct hook
            : public default_state_hook::template hook< Allocator, Delta, Instance >
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(allocator_type allocator, const id_type& id)
                : default_state_hook::template hook< Allocator, Delta, Instance >(allocator, id)
                , registry_instance_(*static_cast<Instance*>(this))
                , registry_instance_it_(allocator.get_replica().get_registry().insert(id, registry_instance_))
            {}

            ~hook()
            {
                static_cast<Instance*>(this)->get_allocator().get_replica().get_registry().erase(registry_instance_it_);
            }

        private:
            registry_instance< Instance, DeltaAllocator > registry_instance_;
            typename Registry::iterator registry_instance_it_;
        };
    };

    template < typename Allocator = std::allocator< char >, typename System = crdt::system<> > class registry
    {
        using id_type = typename System::id_type;
        typedef std::map< id_type, registry_instance_base* > instances_type;

    public:
        typedef typename instances_type::iterator iterator;

    public:
        registry(Allocator allocator = Allocator())
            : instances_(allocator)
        {}
        
        iterator insert(const id_type& id, registry_instance_base& instance)
        {
            auto [it, inserted] = instances_.emplace(id, &instance);
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

        registry_instance_base& get_instance(id_type id) { return *instances_.at(id); }

        registry_instance_base* get_instance_ptr(id_type id)
        {
            auto it = instances_.find(id);
            if (it != instances_.end())
            {
                return it->second;
            }

            return nullptr;
        }

    private:
        instances_type instances_;
    };
}