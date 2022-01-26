#pragma once

#include <vector>
#include <cstdlib>

template < typename T > void shuffle(std::vector< T >& data)
{
    assert(!data.empty());

    srand(1);

    for (int i = data.size() - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        std::swap(data[i], data[j]);
    }
}
