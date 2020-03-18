#pragma once

namespace sqlstl
{
    template < typename Container, typename StorageIterator > struct iterator
    {
        iterator(const iterator&) = delete;
        iterator& operator = (const iterator&) = delete;

        // Insert
        template < typename V > iterator(const Container* container, typename V&& value)
            : container_(container)
            , value_(std::forward< V >(value))
        {}

        // Find variant
        template < typename V > iterator(const Container* container, V&& value, typename StorageIterator&& it)
            : container_(container)
            , value_(std::forward< V >(value))
            , it_(std::move(it))
        {}

        // Begin / end variant
        iterator(const Container* container, typename StorageIterator&& it)
            : container_(container)
            , it_(std::move(it))
        {}

        iterator(iterator&& other)
            : container_(other.container_)
            , value_(std::move(other.value_))
            , it_(std::move(other.it_))
        {}

        const typename Container::value_type& operator*() const
        {
            init_value();
            return *value_;
        }

        const typename Container::value_type* operator->() const
        {
            return &operator*();
        }

        bool operator == (const iterator& other) const
        {
            // We can have any combination out of those on both sides of ==:
            //    uninitialized (SQLITE_OK), initailized (SQLITE_ROW) and end (SQLITE_END)
            // Deal with trivial cases that do not need initialized iterator to compare

            if (it_.result() == other.it_.result())
            {
                if (it_.result() == SQLITE_DONE)
                {
                    return true;
                }
            }
            else
            {
                if (it_.result() == SQLITE_DONE || other.it_.result() == SQLITE_DONE)
                {
                    return false;
                }
            }

            init_iterator();
            other.init_iterator();
            return it_ == other.it_;
        }

        bool operator != (const iterator& other) const
        {
            return !(*this == other);
        }

        iterator& operator++() const
        {
            init_iterator();
            value_.reset();
            ++it_;
            return const_cast<iterator&>(*this);
        }

    protected:
        void init_iterator() const
        {
            if (it_.result() == SQLITE_OK)
            {
                it_ = std::move(container_->find_value(*value_));
            }
        }

        void init_value() const
        {
            if (!value_)
            {
                value_.emplace(*it_);
            }
        }

        const Container* container_;
        mutable std::optional< typename Container::value_type > value_;
        mutable StorageIterator it_;
    };
}