#pragma once

#include <crdt/allocator.h>
#include <crdt/counter_g.h>

namespace crdt
{
/*
    template < typename Node, typename T, typename Traits > class counter_pn
    {
        template < typename Node, typename T, typename Traits > friend class counter_pn;

    public:
        typedef typename Traits::template Allocator<void> allocator_type;

        counter_pn(allocator_type allocator)
            : allocator_(allocator)
            , inc_(allocator_type(allocator, "inc"))
            , dec_(allocator_type(allocator, "dec"))
        {}

        void increment(const Node& node, T value)
        {
            inc_.increment(node, value);
        }

        void decrement(const Node& node, T value)
        {
            dec_.increment(node, value);
        }

        T value() const
        {
            return inc_.value() - dec_.value();
        }

        T value(const Node& node) const
        {
            return inc_.value(node) - dec_.value(node);
        }

        template < typename Counter > void merge(const Counter& other)
        {
            inc_.merge(other.inc_);
            dec_.merge(other.dec_);
        }

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        counter_g< Node, T, Traits > inc_, dec_;
    };
*/

    template < typename Node, typename T, typename Traits > class counter_pn
    {
        template < typename Node, typename T, typename Traits > friend class counter_pn;

    public:
        typedef map_g< Node, typename Traits::template Tuple< T, T >, Traits > container_type;
        typedef typename Traits::template Allocator<void> allocator_type;

        counter_pn(allocator_type allocator)
            : allocator_(allocator)
            , container_(allocator_type(allocator, "values"))
        {}

        void update(const Node& node, T value)
        {
            // auto initial = std::make_tuple(value > 0 ? value : 0, value > 0 ? 0 : value);
            // auto pairb = container_.emplace(node, initial);

            auto&& result = container_[node];
            auto tv = static_cast<std::tuple< T, T >>(result);

            if (value > 0)
            {
                result = std::make_tuple(std::get< 0 >(tv) + value, std::get< 1 >(tv));
            }
            else
            {
                result = std::make_tuple(std::get< 0 >(tv), std::get< 1 >(tv) - value);
            }
        }

        void update(typename container_type::iterator& it, T value)
        {
            auto tv = static_cast<std::tuple< T, T >>(it->second);

            if (value > 0)
            {
                it->second = std::make_tuple(std::get< 0 >(tv) + value, std::get< 1 >(tv));
            }
            else
            {
                it->second = std::make_tuple(std::get< 0 >(tv), std::get< 1 >(tv) - value);
            }
        }

        T value() const
        {
            T result = 0;
            return sqlstl::accumulate(container_.begin(), container_.end(), result, [](T init, auto&& p) 
            {
                auto tv = static_cast<std::tuple< T, T >>(p.second);
                return init + std::get< 0 >(tv) - std::get< 1 >(tv);
            });
        }

        typename container_type::iterator find(const Node& node)
        {
            return container_.find(node);
        }

        typename container_type::iterator end()
        {
            return container_.end();
        }

        template < typename Counter > void merge(const Counter& other)
        {
            for (auto&& otherValue : other.container_)
            {
                auto&& localValue = container_[otherValue.first];
                //std::get< 0 >(localValue) = std::max((T)std::get< 0 >(localValue), std::get< 0 >(otherValue.second));
                //std::get< 1 >(localValue) = std::max((T)std::get< 1 >(localValue), std::get< 1 >(otherValue.second));
            }
        }

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        container_type container_;
    };

    template < typename Node, typename T, typename Traits > class counter_pn_delta
    {
    public:
        typedef typename Traits::template Allocator<void> allocator_type;

        counter_pn_delta(allocator_type allocator)
            : delta_container_(allocator_type(allocator, "delta"))
        {}

        template < typename StateContainer > void update(StateContainer& state_container, const Node& node, T value)
        {
            auto delta_it = delta_container_.find(node);
            if (delta_it != delta_container_.end())
            {
                delta_container_.update(delta_it, value);
            }
            else
            {
                auto state_it = state_container.find(node);
                if (state_it != state_container.end())
                {
                    // delta_container_.update(node, state_it->second + value);
                }
                else
                {
                    delta_container_.update(node, value);
                }
            }
        }

        counter_pn< Node, T, Traits > delta_container_;
    };
}