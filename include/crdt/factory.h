#pragma once

namespace crdt
{
    class factory
    {
    public:
        template < typename Container > Container create_container(const std::string&)
        {
            return Container();
        }

        static factory& static_factory() { static factory fy; return fy; }
    };
}