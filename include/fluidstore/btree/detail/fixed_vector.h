#pragma once

namespace btree::detail
{
    template < typename T > struct is_fixed_vector_trivial
    {
        static constexpr bool value =
            std::is_trivially_copy_constructible<T>::value &&
            std::is_trivially_move_constructible<T>::value &&
            std::is_trivially_destructible<T>::value;
    };

    // TODO:
    // template < typename T > static const bool is_fixed_vector_trivial_v = is_fixed_vector_trivial< T >::value;
        
    template < typename T, typename Descriptor > struct fixed_vector_base
    {
        using size_type = typename Descriptor::size_type;

        template < typename... Args > fixed_vector_base(Args&&... args)
            : desc_(std::forward< Args >(args)...)
        {}
        
        size_type size() const { return desc_.size(); }
        constexpr size_type capacity() const { return desc_.capacity(); }
        bool empty() const { return size() == 0; }

        T* begin() { return reinterpret_cast<T*>(desc_.data()); }
        const T* begin() const { return reinterpret_cast<const T*>(desc_.data()); }

        T* end() { return begin() + size(); }
        const T* end() const { return begin() + size(); }

        T& front() { return *begin(); }
        const T& front() const { return *begin(); }

        T& back() { return *(end() - 1); }
        const T& back() const { return *(end() - 1); }

        T& operator[](size_type index)
        {
            assert(index < size());
            return *(begin() + index);
        }

        const T& operator[](size_type index) const
        {
            return const_cast<fixed_vector_base< T, Descriptor >&>(*this).operator [](index);
        }

    protected:
        Descriptor desc_;
    };
        
    // TODO: the trivial code is not faster than the normal code...
    template < typename T, typename Descriptor, bool IsTrivial = is_fixed_vector_trivial< T >::value > struct fixed_vector
        : public fixed_vector_base< T, Descriptor >
    {
    public:
        using size_type = typename Descriptor::size_type;
        using value_type = T;
        using iterator = value_type*;

        template < typename... Args > fixed_vector(Args&&... args)
            : fixed_vector_base< T, Descriptor >(std::forward< Args >(args)...)
        {}
                
        template < typename Allocator, typename... Args > void emplace_back(Allocator& alloc, Args&&... args)
        {
            emplace(alloc, this->end(), std::forward< Args >(args)...);
        }

        template < typename Allocator > iterator erase(Allocator& alloc, iterator index)
        {
            assert(this->begin() <= index && index < this->end());

            bool last = index == this->end() - 1;
            std::move(index + 1, this->end(), index);
            destroy(alloc, this->end() - 1, this->end());

            this->desc_.set_size(this->size() - 1);

            return last ? this->end() : index;
        }

        template < typename Allocator > void erase(Allocator& alloc, iterator from, iterator to)
        {
            assert(this->begin() <= from && from <= to);
            assert(from <= to && to <= this->end());

            if (to == this->end())
            {
                destroy(alloc, from, to);
                this->desc_.set_size(this->size() - static_cast<size_type>(to - from));
            }
            else
            {
                std::abort(); // TODO
            }
        }
               
        template < typename Allocator > void clear(Allocator& alloc)
        {
            destroy(alloc, this->begin(), this->end());
            this->desc_.set_size(0);
        }       

        template < typename Allocator, typename... Args > void emplace(Allocator& alloc, iterator it, Args&&... args)
        {
            assert(this->size() < this->capacity());
            assert(it >= this->begin());
            assert(it <= this->end());

            if (it < this->end())
            {
                move_backward(alloc, it, this->end(), this->end() + 1);

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

            this->desc_.set_size(this->desc_.size() + 1);
        }

        template < typename Allocator, typename U > void insert(Allocator& alloc, iterator it, U from, U to)
        {
            assert(this->begin() <= it && it <= this->end());
            assert((uintptr_t)(to - from + it - this->begin()) <= this->capacity());

            if (it < this->end())
            {
                move_backward(alloc, it, this->end(), this->end() + (to - from));
            }

            copy(alloc, from, to, it);
            this->desc_.set_size(this->size() + static_cast<size_type>(to - from));
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
                if (dest > this->end())
                {
                    size_type uninitialized_count = static_cast<size_type>(std::min(last - first, dest - this->end()));
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
                if (dest < this->end())
                {
                    cnt = static_cast<size_type>(std::min(last - first, this->end() - dest));
                    std::copy(first, first + cnt, dest);
                }

                std::uninitialized_copy(first + cnt, last, dest + cnt);
            }
        }
    };
        
    template < typename T, typename Descriptor > struct fixed_vector< T, Descriptor, true >
        : public fixed_vector_base< T, Descriptor >
    {
        using value_type = T;
        using iterator = value_type*;
        using size_type = typename Descriptor::size_type;

        template < typename... Args > fixed_vector(Args&&... args)
            : fixed_vector_base< T, Descriptor >(std::forward< Args >(args)...)
        {}
              
        template < typename Allocator > void clear(Allocator& alloc)
        {
            this->desc_.set_size(0);
        }

        template < typename Allocator, typename... Args > void emplace_back(Allocator& alloc, Args&&... args)
        {
            emplace(alloc, this->end(), std::forward< Args >(args)...);
        }

        template < typename Allocator, typename... Args > void emplace(Allocator&, iterator it, Args&&... args)
        {
            assert(this->size() < this->capacity());
            assert(it >= this->begin());
            assert(it <= this->end());

            if (it < this->end())
            {
                move(it + 1, it, static_cast< size_type >(this->end() - it));
            }

            new (it) T(std::forward< Args >(args)...);

            this->desc_.set_size(this->size() + 1);
        }

        template < typename Allocator > iterator erase(Allocator&, iterator it)
        {
            assert(this->begin() <= it && it < this->end());

            bool last = it == this->end() - 1;
            copy(it, it + 1, static_cast< size_type >(this->end() - it - 1));
            this->desc_.set_size(this->size() - 1);
            return last ? this->end() : it;
        }

        template < typename Allocator > void erase(Allocator&, iterator from, iterator to)
        {
            assert(this->begin() <= from && from <= to);
            assert(from <= to && to <= this->end());

            if (to == this->end())
            {                
                this->desc_.set_size(this->size() - static_cast<size_type>(to - from));
            }
            else
            {
                std::abort(); // TODO
            }
        }

        template < typename Allocator, typename U > void insert(Allocator& alloc, iterator it, std::move_iterator< U > from, std::move_iterator< U > to)
        {
            insert(alloc, it, from.base(), to.base());
        }

        template < typename Allocator, typename U > void insert(Allocator&, iterator it, U from, U to)
        {
            assert(this->begin() <= it && it <= this->end());
            assert((uintptr_t)(to - from + it - this->begin()) <= this->capacity());

            const auto count = static_cast<size_type>(to - from);

            if (it < this->end())
            {                
                move(it + count, it, count);
            }

            copy(it, from, count);
                        
            this->desc_.set_size(this->size() + count);
        }       

    private:
        void move(T* dest, const T* source, size_type count)
        {
            std::memmove(dest, source, sizeof(T) * count);
        }

        void copy(T* dest, const T* source, size_type count)
        {
            std::memcpy(dest, source, sizeof(T) * count);
        }
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