#pragma once

#include <fluidstore/crdts/noncopyable.h>

namespace crdt
{
    template < typename Id = uint64_t > struct id_sequence
        : noncopyable
    {
        id_sequence()
            : id_()
        {}

        Id next()
        {
            return ++id_;
        }

    private:
        Id id_;
    };
}