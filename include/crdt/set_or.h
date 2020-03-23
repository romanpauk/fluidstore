#pragma once

#include <crdt/allocator.h>
#include <crdt/set_g.h>
#include <sqlstl/algorithm.h>

namespace crdt
{
    template < typename T, typename Traits > class set_or
    {
    public:
        typedef uint64_t Tag;

        struct set_or_tags
        {
            template < typename Allocator > set_or_tags(Allocator&& allocator)
                : added(Allocator(allocator, "added"))
                , removed(Allocator(allocator, "removed"))
            {}

            set_or_tags(set_or_tags&& other)
                : added(std::move(other.added))
                , removed(std::move(other.removed))
            {}

            set_or_tags& operator = (set_or_tags&& other)
            {
                std::swap(added, other.added);
                std::swap(removed, other.removed);
            }

            set_g< Tag, Traits > added;
            set_g< Tag, Traits > removed;

            template < typename Container > void merge(const Container& other)
            {
                added.merge(other.added);
                removed.merge(other.removed);
            }
        };

        template < typename It > class iterator_base
        {
        public:
            iterator_base(set_or< T, Traits >& set, It&& it)
                : it_(std::move(it))
                , set_(set)
            {}

            bool operator == (const iterator_base< It >& other) const { return it_ == other.it_; }
            bool operator != (const iterator_base< It >& other) const { return it_ != other.it_; }
            const T& operator*() { return it_->first; }

            iterator_base< It >& operator++() 
            {
                do
                {
                    ++it_;
                }
                while(*this != set_.end() && !set_.is_added(it_->second));
                
                return *this;
            }
            
        private:
            set_or< T, Traits >& set_;
            typename It it_;
        };

        typedef typename Traits::template Map< T, set_or_tags > container_type;
        typedef typename container_type::allocator_type allocator_type;

    public:
        template < typename Allocator > set_or(Allocator&& allocator)
            : allocator_(std::forward< Allocator >(allocator))
            , values_(allocator_type(allocator_, "values"))
        {}

        set_or(set_or&& other)
            : allocator_(std::move(other.allocator_))
            , values_(std::move(other.values_))
        {}

        typedef iterator_base< typename container_type::iterator > iterator;
        typedef iterator_base< typename container_type::const_iterator > const_iterator;

        iterator begin() { return {*this, advance(values_.begin())}; }
        const_iterator begin() const { return {*this, advance(values_.begin())}; }

        iterator end() { return {*this, values_.end()}; }
        const_iterator end() const { return {*this, values_.end()}; }
        
        void clear()
        {
            for (auto&& value : values_)
            {
                value.second.removed.insert(value.second.added.begin(), value.second.added.end());
            }
        }
        
        template < typename K > std::pair< iterator, bool > insert(K&& value)
        {
            auto pairb = values_.emplace(
                std::forward< K >(value), 
                set_or_tags(allocator_type(allocator_, std::to_string(value))));

            pairb.first->second.added.insert(get_tag());
            return { iterator(*this, std::move(pairb.first)), pairb.second };
        }

        template < typename K > void erase(K&& key)
        {
            auto it = values_.find(std::forward< K >(key));
            if (it != values_.end())
            {
                auto& tags = it->second;
                sqlstl::set_difference(
                    tags.added.begin(), tags.added.end(),
                    tags.removed.begin(), tags.removed.end(),
                    sqlstl::inserter(tags.removed)
                );
            }
        }

        template < typename K > iterator find(K&& key)
        {
            auto it = values_.find(std::forward< K >(key));
            if (it != values_.end())
            {
                if (is_added(it->second))
                {
                    return { *this, std::move(it) };
                }
            }

            return end();
        }

        size_t size() const
        {
            size_t count = 0;
            for (auto&& value : values_)
            {
                count += is_added(value.second);
            }
            return count;
        }

        template < typename Set > void merge(const Set& other)
        {
            for (auto&& element : other.values_)
            {
                values_[element.first].merge(element.second);
            }
        }

        template < typename Set > bool operator == (const Set& other) const
        {
            return values_ == other.values_;
        }

    private:
        template < typename It > void insert_erase_node(It& it)
        {
            const auto& data = (*it);
            values_[data.first].merge(data.second);
            erase(data.first);
        }

        template < typename K > auto find_node(K&& key)
        {
            return { *this, values_.find(std::forward< K >(key)) };
        }

        auto end_node()
        {
            return { *this, values_.end() };
        }

        template < typename It > It advance(It&& it)
        {
            while (it != values_.end() && !is_added(it->second))
            {
                ++it;
            }

            return std::move(it);
        }

        Tag get_tag() const { return rand(); }

        template < typename Tags > bool is_added(const Tags& tags) const
        {
            return tags.added.size() > tags.removed.size();
        }

    public:
        allocator_type allocator_;
        container_type values_;
    };
}