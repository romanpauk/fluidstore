#pragma once

namespace sqlstl
{
    template < typename Iterator, typename Value, typename BinaryFunction > Value accumulate(Iterator&& begin, Iterator&& end, Value&& init, BinaryFunction&& fn)
    {
        for (; begin != end; ++begin)
        {
            init = fn(std::move(init), *begin);
        }
        return init;
    }

    template<class InputIt, class OutputIt>
    void copy(InputIt&& first, InputIt&& last, OutputIt&& d_first)
    {
        while (first != last)
        {
            *d_first++ = *first;
            ++first;
        }
    }

    template<class InputIt1, class InputIt2, class OutputIt>
    void set_difference(InputIt1&& first1, InputIt1&& last1, InputIt2&& first2, InputIt2&& last2, OutputIt&& d_first)
    {
        while (first1 != last1)
        {
            if (first2 == last2) 
            {
                sqlstl::copy(first1, last1, d_first);
                return;
            }

            if (*first1 < *first2)
            {
                *d_first++ = *first1;
                ++first1;
            }
            else
            {
                if (!(*first2 < *first1))
                {
                    ++first1;
                }
                ++first2;
            }
        }
    }

    template < typename Container > struct insert_iterator
    {
        insert_iterator(Container& container)
            : container_(container)
        {}

        template < typename T > void operator = (T&& value)
        {
            container_.insert(std::forward< T >(value));
        }

        insert_iterator< Container >& operator ++(int) { return *this; }
        insert_iterator< Container >& operator* () { return *this; }

    private:
        Container& container_;
    };

    template < typename Container > insert_iterator< Container > inserter(Container& container)
    {
        return insert_iterator< Container >(container);
    }

}