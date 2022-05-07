#pragma once

#include <map>
#include <optional>

//
// Container takes a tuple< Key1, ..., KeyN, Value1, ..., ValueN > and provides interface to access data 
// sharing common prefix in a form of new container < KeyN, Value1, ..., ValueN >.
//

namespace column
{    
    template < typename Container, size_t N > class tree_index_iterator;

    template < typename Container > class tree_index_iterator< Container, 1 >
    {
    public:
        tree_index_iterator() {}

        tree_index_iterator(Container* container, typename Container::container_iterator it)
            : container_(container)
            , it_(it)
        {}

        const typename Container::value_type& operator* () const { return *it_; }
        const typename Container::value_type& operator* ()       { return *it_; }

    private:
        Container* container_;
        typename Container::container_iterator it_;
    };

    template < typename Container, size_t N > class tree_index_iterator
        : public tree_index_iterator< typename Container::nested_tree_type, N - 1 >
    {        
    public:
        tree_index_iterator()
        {}

        tree_index_iterator(Container* container)
            : tree_index_iterator< typename Container::nested_tree_type, N - 1 >()                
            , container_(container)
        {}
        
        tree_index_iterator(Container* container, typename Container::container_iterator it)
            : tree_index_iterator< typename Container::nested_tree_type, N - 1 >()
            , container_(container)
            , it_(it)
        {}

        tree_index_iterator< Container, N >& operator++()
        {
            return *this;
        }

        const typename Container::value_type operator*() const
        {
            //                                  This is pair< Head, std::tuple< Tail... > > but should be std::tuple< Head, Tail... >
            //return std::make_pair(it_->first, tree_index_iterator< typename Container::nested_tree_type, N - 1 >::operator*());
            return typename Container::value_type();
        }

    private:
        Container* container_;
        typename Container::container_iterator it_;
    };

    template < size_t N, typename Head, typename... Tail > class tree_index;

    template < typename Head > class tree_index< 1, Head > 
    {
    public:
        using container_type = std::set< Head >;
        using container_iterator = typename container_type::iterator;

        using key_type = Head;
        using value_type = Head;
        using iterator = tree_index_iterator< tree_index < 1, Head >, 1 >;

        template < typename T > auto& prefix(T&&) 
        {
            static_assert(false, "not callable");
            return *this;
        }

        auto prefix(typename container_type::iterator it)
        {
            static_assert(false, "not callable");
            return this;
        }

        template < typename T > auto emplace(T&& value)
        {
            return values_.emplace(std::forward< T >(value));
        }

        auto begin() { return iterator(this, values_.begin()); }
        auto end() { return iterator(this, values_.end()); }
        
        size_t size() const { return values_.size(); }

    private:
        container_type values_;
    };

    template < size_t N, typename Head, typename... Tail > class tree_index
    {
    public:
        using nested_tree_type = tree_index< N - 1, Tail... >;
        using container_type = std::map< Head, nested_tree_type >;
        using container_iterator = typename container_type::iterator;

        using key_type = Head;
        using mapped_type = std::tuple< Tail&... >;
        using value_type = std::pair< const key_type&, mapped_type >;
        using iterator = tree_index_iterator< tree_index< N, Head, Tail... >, N >;

        tree_index()
            : size_()
        {}

        template < typename HeadT, typename... TailT > auto emplace(HeadT&& head, TailT&&... tail)
        {
            auto it = values_.find(head);
            if (it != values_.end())
            {                
                if(it->second.emplace(std::forward< TailT >(tail)...).second)
                {
                    ++size_;
                    return std::make_pair(it, true);
                }

                return std::make_pair(it, false);
            }
            else
            {
                it = values_.emplace_hint(it, std::forward< HeadT >(head), nested_tree_type());
                it->second.emplace(std::forward< TailT >(tail)...);
                ++size_;
                return std::make_pair(it, true);
            }       
        }
     
        template< typename HeadT, typename... TailT > auto prefix(HeadT&& head, TailT&&... tail)
        {
            if constexpr (sizeof... (tail) == 0)
            {
                auto it = values_.find(head);
                if (it != values_.end()) 
                {
                    return &it->second;
                } 
                else
                {
                    // TODO: this is nullptr...
                    return std::add_pointer_t< decltype(it->second) >();
                }
            }                
            else
            {
                auto it = values_.find(head);
                if (it != values_.end())
                {
                    return it->second.prefix(std::forward< TailT >(tail)...);
                }
                else
                {
                    // TODO: this is nullptr...
                    return std::add_pointer_t< decltype(*it->second.prefix(std::forward< TailT >(tail)...)) >();
                }
            }                
        }

        auto prefix(typename container_type::iterator it)
        {
            return it != values_.end() ? &it->second : nullptr;
        }

        auto begin() { return iterator(this, values_.begin()); }
        auto end() { return iterator(this, values_.end()); }
        
        size_t size() const { return size_; }

    private:
        container_type values_;
        size_t size_;
    };

    template < typename... Args > class tree: public tree_index< sizeof... (Args), Args... > {};
}