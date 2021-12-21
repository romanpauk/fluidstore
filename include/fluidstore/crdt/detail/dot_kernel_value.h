# pragma once

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename DotContext, typename DotKernel > class dot_kernel_value
    {
    public:
        using allocator_type = Allocator;
        using dot_kernel_value_type = dot_kernel_value< Key, Value, Allocator, DotContext, DotKernel >;
        using value_allocator_type = dot_kernel_allocator< Allocator, dot_kernel_value_type >;
        using value_type = typename Value::template rebind_t< value_allocator_type >;

        struct nested_value
        {
            nested_value(value_allocator_type& allocator, DotKernel* parent)
                : value(allocator)
                , parent(parent)
            {}

            nested_value(nested_value&& other) = default;

            DotKernel* parent;
            DotContext dots;
            value_type value;
        };

        dot_kernel_value(const dot_kernel_value_type&) = delete;
        dot_kernel_value_type& operator = (const dot_kernel_value_type&) = delete;

        // TODO: allocator points to 'this' so we can invoke this->parent->update() with key stored on this.
        // Not sure yet how to change it so it does not need two pointers.

        template < typename AllocatorT > dot_kernel_value(AllocatorT& allocator, Key key, DotKernel* p)
            : first(key)
        #if defined(DOTKERNEL_BTREE)
            , value(value_allocator_type(allocator, this))
            , parent(p)
        #else
            , second(value_allocator_type(allocator, this), p)
        #endif
        {}

        dot_kernel_value(dot_kernel_value_type&& other)
            : first(std::move(other.first))
        #if defined(DOTKERNEL_BTREE)
            , value(std::move(other.value))
            , dots(std::move(other.dots))
            , parent(other.parent)
        #else
            , second(std::move(other.second))
        #endif
        {
        #if defined(DOTKERNEL_BTREE)
            value.get_allocator().set_container(this);
        #else
            second.value.get_allocator().set_container(this);
        #endif
        }

        dot_kernel_value_type& operator = (dot_kernel_value_type&& other)
        {
            std::swap(first, other.first);

        #if defined(DOTKERNEL_BTREE)
            std::swap(value, other.value);
            std::swap(dots, other.dots);
            std::swap(parent, other.parent);
            value.get_allocator().set_container(this);
        #else
            std::swap(second, other.second);
            second.value.get_allocator().set_container(this);
        #endif

            return *this;
        }

        template < typename AllocatorT, typename DotKernelValue, typename Context > void merge(AllocatorT& allocator, const DotKernelValue& other, Context& context)
        {
        #if defined(DOTKERNEL_BTREE)
            dots.merge(allocator, other.dots, context);
            value.merge(other.value);
        #else
            second.dots.merge(allocator, other.dots, context);
            second.value.merge(other.value);
        #endif
        }

        void update()
        {
        #if defined(DOTKERNEL_BTREE)
            parent->update(first);
        #else
            second.parent->update(first);
        #endif
        }

        bool operator == (const Key& other) const { return first == other; }
        bool operator == (const dot_kernel_value_type& other) const { return first == other.first; }

        bool operator < (const Key& other) const { return first < other; }
        bool operator < (const dot_kernel_value_type& other) const { return first < other.first; }

        Key first;

    #if defined(DOTKERNEL_BTREE)
        DotKernel* parent;
        DotContext dots;
        value_type value;
    #else
        nested_value second;
    #endif
    };

    template < typename Key, typename Allocator, typename DotContext, typename DotKernel > class dot_kernel_value< Key, void, Allocator, DotContext, DotKernel >
    {
    public:
        using allocator_type = Allocator;
        using value_type = void;
        using dot_kernel_value_type = dot_kernel_value< Key, void, Allocator, DotContext, DotKernel >;

        template < typename AllocatorT > dot_kernel_value(AllocatorT&, Key key, DotKernel*)
            : first(key)
        {}

        dot_kernel_value() = default;
        dot_kernel_value(dot_kernel_value_type&&) = default;
        dot_kernel_value_type& operator = (dot_kernel_value_type&&) = default;

        dot_kernel_value(const dot_kernel_value_type&) = delete;
        dot_kernel_value_type& operator = (const dot_kernel_value_type&) = delete;

        template < typename AllocatorT, typename DotKernelValue, typename Context > void merge(AllocatorT& allocator, const DotKernelValue& other, Context& context)
        {
        #if defined(DOTKERNEL_BTREE)
            dots.merge(allocator, other.dots, context);
        #else
            second.dots.merge(allocator, other.dots, context);
        #endif
        }

        void update() {}

        bool operator == (const Key& other) const { return first == other; }
        bool operator == (const dot_kernel_value_type& other) const { return first == other.first; }

        bool operator < (const Key& other) const { return first < other; }
        bool operator < (const dot_kernel_value_type& other) const { return first < other.first; }

        // TODO: need a way how to determine key from value without storing duplicate key (if is not small enough).
        Key first;

    #if defined(DOTKERNEL_BTREE)
        DotContext dots;
    #else
        struct nested_value
        {
            DotContext dots;
        };

        nested_value second;
    #endif
    };
}
