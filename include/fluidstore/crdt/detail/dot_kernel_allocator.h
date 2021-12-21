# pragma once

namespace crdt
{
    template < typename Allocator, typename Container > struct dot_kernel_allocator
        : public Allocator
    {
        typedef Container container_type;

        dot_kernel_allocator(Allocator& allocator, Container* container)
            : Allocator(allocator)
            , container_(container)
        {}

        dot_kernel_allocator(const dot_kernel_allocator< Allocator, Container >& other) = default;
        dot_kernel_allocator< Allocator, Container >& operator = (dot_kernel_allocator< Allocator, Container >&& other) = default;

        void set_container(container_type* container) { container_ = container; }
        void update() { container_->update(); }

    private:
        container_type* container_;
    };
}