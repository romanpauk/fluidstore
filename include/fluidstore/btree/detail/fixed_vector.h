#pragma once

namespace btree::detail
{
    template < typename T > struct is_fixed_vector_trivial
    {
        static const bool value =
            std::is_trivially_copy_constructible<T>::value &&
            std::is_trivially_move_constructible<T>::value &&
            std::is_trivially_destructible<T>::value;
    };

    // TODO:
    // template < typename T > static const bool is_fixed_vector_trivial_v = is_fixed_vector_trivial< T >::value;

    // TODO: this could use at least one test...
    template < typename T, typename Descriptor, bool IsTrivial = is_fixed_vector_trivial< T >::value > struct fixed_vector
    {
    public:
        using size_type = typename Descriptor::size_type;
        using value_type = T;
        using iterator = value_type*;

        fixed_vector(Descriptor desc)
            : desc_(desc)
        {
            checkvec();
        }

        fixed_vector(Descriptor desc, size_type size)
            : desc_(desc)
        {
            desc_.set_size(size);
            checkvec();
        }

        fixed_vector(fixed_vector&&) = default;
        fixed_vector< T, Descriptor, IsTrivial >& operator = (fixed_vector< T, Descriptor, IsTrivial >&&) = default;

        template < typename Allocator, typename... Args > void emplace_back(Allocator& alloc, Args&&... args)
        {
            emplace(alloc, end(), std::forward< Args >(args)...);
        }

        template < typename Allocator > iterator erase(Allocator& alloc, iterator index)
        {
            assert(begin() <= index && index < end());

            bool last = index == end() - 1;
            std::move(index + 1, end(), index);
            destroy(alloc, end() - 1, end());

            desc_.set_size(size() - 1);

            checkvec();

            return last ? end() : index;
        }

        template < typename Allocator > void erase(Allocator& alloc, iterator from, iterator to)
        {
            assert(begin() <= from && from <= to);
            assert(from <= to && to <= end());

            if (to == end())
            {
                destroy(alloc, from, to);
                desc_.set_size(size() - static_cast<size_type>(to - from));
            }
            else
            {
                std::abort(); // TODO
            }

            checkvec();
        }

        size_type size() const { return desc_.size(); }
        constexpr size_type capacity() const { return desc_.capacity(); }
        bool empty() const { return desc_.size() == 0; }

        template < typename Allocator > void clear(Allocator& alloc)
        {
            destroy(alloc, begin(), end());
            desc_.set_size(0);
        }

        T* begin() { return reinterpret_cast<T*>(desc_.data()); }
        const T* begin() const { return reinterpret_cast<const T*>(desc_.data()); }

        T* end() { return reinterpret_cast<T*>(desc_.data()) + desc_.size(); }
        const T* end() const { return reinterpret_cast<const T*>(desc_.data()) + desc_.size(); }

        T& front() { return *begin(); }
        const T& front() const { return *begin(); }

        T& back() { return *(end() - 1); }
        const T& back() const { return *(end() - 1); }

        template < typename Allocator, typename... Args > void emplace(Allocator& alloc, iterator it, Args&&... args)
        {
            assert(size() < capacity());
            assert(it >= begin());
            assert(it <= end());

            if (it < end())
            {
                move_backward(alloc, it, end(), end() + 1);

                // TODO: does this make sense?
                if constexpr (std::is_move_assignable_v< T >)
                {
                    const_cast<T&>(*it) = T{ std::forward< Args >(args)... };
                }
                else
                {
                    std::allocator_traits< Allocator >::destroy(alloc, it);
                    std::allocator_traits< Allocator >::construct(alloc, it, std::forward< Args >(args)...);
                }
            }
            else
            {
                std::allocator_traits< Allocator >::construct(alloc, it, std::forward< Args >(args)...);
            }

            desc_.set_size(desc_.size() + 1);

            checkvec();
        }

        template < typename Allocator, typename U > void insert(Allocator& alloc, iterator it, U from, U to)
        {
            assert(begin() <= it && it <= end());
            assert((uintptr_t)(to - from + it - begin()) <= capacity());

            if (it < end())
            {
                move_backward(alloc, it, end(), end() + (to - from));
            }

            copy(alloc, from, to, it);
            desc_.set_size(size() + static_cast<size_type>(to - from));

            checkvec();
        }

        value_type& operator[](size_type index)
        {
            assert(index < size());
            return *(begin() + index);
        }

        const value_type& operator[](size_type index) const
        {
            assert(index < size());
            return *(begin() + index);
        }

    private:
        template < typename Allocator > void destroy(Allocator& alloc, iterator first, iterator last)
        {
            assert(last >= first);
            if constexpr (!std::is_trivially_destructible_v< T >)
            {
                while (first != last)
                {
                    std::allocator_traits< Allocator >::destroy(alloc, first++);
                }
            }
        }

        template < typename Allocator > void move_backward(Allocator& alloc, iterator first, iterator last, iterator dest)
        {
            assert(last > first);
            assert(dest > last);

            if constexpr (std::is_trivially_copyable_v< T >)
            {
                std::move_backward(first, last, dest);
            }
            else
            {
                if (dest > end())
                {
                    size_type uninitialized_count = static_cast<size_type>(std::min(last - first, dest - end()));
                    while (uninitialized_count--)
                    {
                        std::allocator_traits< Allocator >::construct(alloc, --dest, std::move(*--last));
                    }
                }

                std::move_backward(first, last, dest);
            }
        }

        template < typename Allocator, typename U > void copy(Allocator& alloc, U first, U last, iterator dest)
        {
            assert(last > first);

            if constexpr (std::is_trivially_copyable_v< T >)
            {
                std::copy(first, last, dest);
            }
            else
            {
                size_type cnt = 0;
                if (dest < end())
                {
                    cnt = static_cast<size_type>(std::min(last - first, end() - dest));
                    std::copy(first, first + cnt, dest);
                }

                std::uninitialized_copy(first + cnt, last, dest + cnt);
            }
        }

        void checkvec()
        {
        #if defined(BTREE_CHECK_VECTOR_INVARIANTS)
            assert(end() - begin() == size());
            assert(size() <= capacity());

            vec_.assign(begin(), end());
            assert(std::is_sorted(vec_.begin(), vec_.end()));
        #endif
        }

        Descriptor desc_;

    #if defined(BTREE_CHECK_VECTOR_INVARIANTS)
        std::vector< T > vec_;
    #endif
    };

    // TODO: exception safety
    template < typename... Args > struct fixed_split_vector
        : std::tuple< Args... >
    {
    public:
        // TODO: extend to more than 2 fields
        // using value_type = std::tuple< typename Args::value_type&... >;

        using value_type = std::pair< typename Args::value_type&... >;
        using base_iterator = typename std::tuple_element_t< 0, std::tuple< Args... > >::iterator;
        using size_type = typename std::tuple_element_t< 0, std::tuple< Args... > >::size_type;
        using container_type = fixed_split_vector< Args... >;

        // TODO: to avoid end dereference, this should be a tuple of iterators
        struct iterator
        {
            using difference_type = std::ptrdiff_t;
            using value_type = value_type;
            using pointer = value_type*;
            using reference = value_type;
            using iterator_category = std::random_access_iterator_tag;

            iterator(container_type& container, base_iterator it)
                : container_(container)
                , it_(it)
            {}

            reference operator *()
            {
                return container_[static_cast<size_type>(it_ - container_.base().begin())];
            }

            reference operator *() const
            {
                return container_[static_cast<size_type>(it_ - container_.base().begin())];
            }

            // TODO: how should those +/- methods be implemented?
            difference_type operator - (iterator it) const { return it_ - it.it_; }

            iterator operator + (size_type n) const { return { container_, it_ + n }; }
            iterator operator - (size_type n) const { return { container_, it_ - n }; }

            bool operator == (const iterator& other) { return it_ == other.it_; }

        private:
            container_type& container_;
            base_iterator it_;
        };

        template < typename... ArgsT > fixed_split_vector(ArgsT... args)
            : std::tuple< Args... >(std::forward< ArgsT >(args)...)
        {}

        fixed_split_vector(fixed_split_vector&&) = default;
        fixed_split_vector& operator = (fixed_split_vector&&) = default;

        template < typename Allocator, typename Ty > void emplace_back(Allocator& alloc, Ty&& value)
        {
            emplace(alloc, end(), std::forward< Ty >(value));
        }

        template < typename Allocator > iterator erase(Allocator& alloc, iterator index)
        {
            bool last = (index + 1 == end());
            erase_impl(alloc, index, sequence());
            return last ? end() : index;
        }

        template < typename Allocator > void erase(Allocator& alloc, iterator from, iterator to)
        {
            erase_impl(alloc, from, to, sequence());
        }

        size_type size() const { return base().size(); }
        size_type capacity() const { return base().capacity(); }
        bool empty() const { return base().size() == 0; }

        iterator begin() { return { *this, base().begin() }; }
        iterator end() { return { *this, base().end() }; }

        template < typename Allocator > void clear(Allocator& alloc)
        {
            clear_impl(alloc, sequence());
        }

        template < typename Allocator, typename Ty > void emplace(Allocator& alloc, iterator it, Ty&& value)
        {
            emplace_impl(alloc, it, std::forward< Ty >(value), sequence());
        }

        template < typename Allocator, typename U > void insert(Allocator& alloc, iterator it, U from, U to)
        {
            insert_impl(alloc, it, from, to, sequence());
        }

        auto operator[](size_type index)
        {
            return at_impl(index, sequence());
        }

        const auto operator[](size_type index) const
        {
            return at_impl(index, sequence());
        }

    private:
        static constexpr auto sequence() { return std::make_integer_sequence< size_t, sizeof...(Args) >(); }

        auto& base() { return std::get< 0 >(*this); }
        const auto& base() const { return std::get< 0 >(*this); }

        template < typename Allocator, size_t... Ids > void clear_impl(Allocator& alloc, std::integer_sequence< size_t, Ids... >)
        {
            (std::get< Ids >(*this).clear(alloc), ...);
        }

        template < typename Allocator, size_t... Ids > void erase_impl(Allocator& alloc, iterator index, std::integer_sequence< size_t, Ids... >)
        {
            auto offset = index - begin();
            (std::get< Ids >(*this).erase(alloc, std::get< Ids >(*this).begin() + offset), ...);
        }

        template < typename Allocator, size_t... Ids > void erase_impl(Allocator& alloc, iterator from, iterator to, std::integer_sequence< size_t, Ids... >)
        {
            auto first = from - begin();
            auto last = to - begin();
            (std::get< Ids >(*this).erase(alloc, std::get< Ids >(*this).begin() + first, std::get< Ids >(*this).begin() + last), ...);
        }

        template < typename Allocator, typename Ty, size_t... Ids > void emplace_impl(Allocator& alloc, iterator index, Ty&& value, std::integer_sequence< size_t, Ids... >)
        {
            auto offset = index - begin();

            if constexpr (std::is_rvalue_reference_v< decltype(value) >)
            {
                // TODO: the move is required to compile crdts.
                (std::get< Ids >(*this).emplace(alloc, std::get< Ids >(*this).begin() + offset, std::move(std::get< Ids >(std::forward< Ty >(value)))), ...);
            }
            else
            {
                (std::get< Ids >(*this).emplace(alloc, std::get< Ids >(*this).begin() + offset, std::get< Ids >(std::forward< Ty >(value))), ...);
            }
        }

        template < typename Allocator, typename U, size_t... Ids > void insert_impl(Allocator& alloc, iterator index, std::move_iterator<U> from, std::move_iterator<U> to, std::integer_sequence< size_t, Ids... >)
        {
            auto offset = index - begin();
            (std::get< Ids >(*this).insert(
                alloc, std::get< Ids >(*this).begin() + offset,
                // TODO: this quite crudely expects continuous storage
                std::make_move_iterator(&std::get< Ids >(*from)),
                std::make_move_iterator(&std::get< Ids >(*to))
            ), ...);
        }

        template < size_t... Ids > auto at_impl(size_type index, std::integer_sequence< size_t, Ids... >)
        {
            // TODO: this dereferences end().
            return value_type(*(std::get< Ids >(*this).begin() + index)...);
        }

        template < size_t... Ids > auto at_impl(size_type index, std::integer_sequence< size_t, Ids... >) const
        {
            return value_type(*(std::get< Ids >(*this).begin() + index)...);
        }
    };
}