#pragma once

namespace crdt
{
    class allocator
    {
    public:
        template < typename Container > Container create_container(const std::string&)
        {
            return Container();
        }

        static allocator& static_allocator() { static allocator al; return al; }
    };
}