#pragma once

#include <map>

//
// Container takes a tuple< Arg1, ... ArgN > and provides interface to access data that shares common prefix.
// So given prefix tuple< Arg1, ... ArgK >, we return map< ArgK + 1, std::tuple< ArgK + 2, ... ArgN > >.
//

namespace column
{    
    template < typename Container, typename Iterator, size_t N, typename Head, typename... Tail > class tree_index_iterator;

    template < typename Container, typename Iterator, typename Head > class tree_index_iterator< Container, Iterator, 1, Head >
    {
    public:
        tree_index_iterator(Container* container, Iterator it);

        const Head* operator* () const;
        //tree_index_iterator< 1, Head > operator ++(int);
    };

    template < typename Container, typename Iterator, size_t N, typename Head, typename... Tail > class tree_index_iterator
        : public tree_index_iterator< typename Container::nested_tree_type, typename Container::nested_tree_type::container_type::iterator, N - 1, Tail... >
    {
        using base_type = tree_index_iterator< Container, Iterator, N, Tail... >;

    public:
        tree_index_iterator(Container* container, Iterator it)
            : tree_index_iterator< typename Container::nested_tree_type, typename Container::nested_tree_type::container_type::iterator, N - 1, Tail... >(
                container ? container->prefix(it) : nullptr
                , typename Container::nested_tree_type::container_type::iterator()
            )
            , container_(container)
            , it_(it)
        {}
        
        tree_index_iterator< Container, Iterator, N, Head, Tail... >& operator++()
        {
            return *this;
        }

    private:
        Container* container_;
        Iterator it_;
    };

    template < size_t N, typename Head, typename... Tail > class tree_index;

    template < typename Head > class tree_index< 1, Head > 
    {
    public:
        using key_type = Head;
        using value_type = Head;
        using container_type = std::set< Head >;
        using iterator = tree_index_iterator< tree_index < 1, Head >, typename container_type::iterator, 1, Head >;

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
        auto end() { return iterator(this, values_.end());; }
        
        size_t size() const { return values_.size(); }

    private:
        container_type values_;
    };

    template < size_t N, typename Head, typename... Tail > class tree_index
    {
    public:
        using key_type = Head;

        using nested_tree_type = tree_index< N - 1, Tail... >;
        using container_type = std::map< key_type, tree_index< N - 1, Tail... > >;
    
        using mapped_type = std::tuple< Tail&... >;
        using value_type = std::pair< const key_type&, mapped_type >;
        using iterator = tree_index_iterator< tree_index< N, Head, Tail... >, typename container_type::iterator, N, Head, Tail... >;

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
            // TODO: this does 'hidden insert', it should not modify the struct in case there is nothing.
            if constexpr(sizeof... (tail) == 0)
                return &values_[std::forward< HeadT >(head)];
            else
                return values_[std::forward< HeadT >(head)].prefix(std::forward< TailT >(tail)...);
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