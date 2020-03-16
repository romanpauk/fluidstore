#pragma once
// #include <iterator>
#include <crdt/allocator.h>
#include <crdt/set_g.h>
#include <sqlstl/algorithm.h>

namespace crdt
{
    template < typename T, typename Traits > class set_or
    {
        typedef uint64_t Tag;

        struct set_or_tags
        {
            set_or_tags(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
                : added(allocator, name + ".added")
                , removed(allocator, name + ".removed")
            {}

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

    public:
        set_or(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : values_(allocator.template create_container< typename Traits::template Map< T, set_or_tags, typename Traits::Allocator > >(name))
            , name_(name)
            , allocator_(allocator)
            , empty_tags_(allocator, "none")
        {}

        typedef iterator_base< typename Traits::template Map< T, set_or_tags, typename Traits::Allocator >::iterator > iterator;
        typedef iterator_base< typename Traits::template Map< T, set_or_tags, typename Traits::Allocator >::const_iterator > const_iterator;

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
            auto it = values_.emplace(std::forward< K >(value), empty_tags_);
            it.first->second.added.insert(get_tag());
            return { iterator(*this, std::move(it.first)), it.second };
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
        typename Traits::Allocator& allocator_;
        typename Traits::template Map< T, set_or_tags, typename Traits::Allocator > values_;
        std::string name_;
        const set_or_tags empty_tags_;
    };
}