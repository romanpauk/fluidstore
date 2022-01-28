#pragma once

#include <vector>
#include <cstdlib>

template < typename T > void shuffle(std::vector< T >& data, size_t count = 1)
{
    assert(!data.empty());

    srand(1);

    while (count--)
    {
        for (auto i = data.size(); i > 1; i--)
        {
            int j = rand() % i;
            std::swap(data[i - 1], data[j]);
        }
    }
}
