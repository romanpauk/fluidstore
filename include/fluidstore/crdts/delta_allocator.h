#pragma once

namespace crdt
{
    template < typename T > class delta_allocator
        : public T::allocator_type
    {
    public:
        typedef typename T::allocator_type::replica_type replica_type;
        typedef typename replica_type::id_type id_type;

        template < typename Instance > struct hook
            : public replica_type::template hook< Instance >
        {
            friend class delta_allocator< T >;

            hook(const id_type& id)
                : replica_type::hook< Instance >(id)
                , delta_(static_cast<Instance*>(this)->get_allocator(), id)
            {}

            T extract_delta()
            {
                T delta(static_cast<Instance*>(this)->get_allocator(), static_cast<Instance*>(this)->get_id());
                std::swap(delta, delta_);
                return delta;
            }

        private:
            T delta_;
        };

        template < typename... Args > delta_allocator(Args&&... args)
            : T::allocator_type(std::forward< Args >(args)...)
        {}

        template < typename Instance, typename DeltaInstance > void merge(Instance& target, const DeltaInstance& source)
        {
            target.delta_.merge(source);
        }
    };
}