#pragma once

#include <map>

namespace column
{    
    namespace detail
    {
        template < typename Head > auto value_type_tuple(const Head& p)
        {
            return std::make_tuple(p);
        }

        template < typename Head, typename Tail > auto value_type_tuple(const std::pair< const Head&, Tail& >& p)
        {
            return std::make_tuple(p.first, p.second);
        }

        template < typename Head, typename... Tail > auto value_type_tuple(const std::pair< const Head&, std::tuple< Tail... > >& p)
        {
            return std::tuple_cat(std::make_tuple(p.first), p.second);
        }

        template < typename Container, size_t N > class set_iterator;

        template < typename Container > class set_iterator< Container, 1 >
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = typename Container::value_type;
            using pointer = value_type*;
            using reference = value_type&;

            set_iterator()
                : it_()
            {}

            set_iterator(typename Container::container_iterator it)
                : it_(it)
            {}

            const typename Container::value_type& operator* () const { return *it_; }
            const typename Container::value_type& operator* ()       { return *it_; }

            set_iterator< Container, 1 >& operator++() { ++it_; return *this; }
            set_iterator< Container, 1 >  operator++(int) { auto it = *this; operator ++(); return it; }

            bool operator == (set_iterator< Container, 1 > it) const { return it_ == it.it_; }
            bool operator != (set_iterator< Container, 1 > it) const { return it_ != it.it_; }

            set_iterator< Container, 1 >& operator = (set_iterator< Container, 1 > it) { it_ = it.it_; return *this; }

        private:
            typename Container::container_iterator it_;
        };

        template < typename Container, size_t N > class set_iterator
        {        
            using nested_iterator_type = set_iterator< typename Container::nested_tree_type, N - 1 >;

        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = typename Container::value_type;
            using pointer = value_type*;
            using reference = value_type;

            set_iterator()
                : container_()
                , it_()
            {}

            set_iterator(
                typename Container::container_type* container, 
                typename Container::container_iterator it
            )
                : container_(container)
                , it_(it, it != container->end() ? it->second.begin() : nested_iterator_type())
            {}

            set_iterator< Container, N >& operator++()
            {            
                if (++it_.second == it_.first->second.end())
                {
                    if(++it_.first == container_->end())
                    {
                        it_.second = nested_iterator_type(); 
                    }
                    else
                    {
                        it_.second = it_.first->second.begin();
                    }
                }
            
                return *this;
            }

            set_iterator< Container, N > operator++(int)
            {
                auto it = *this;
                operator++();
                return it;
            }

            const typename value_type operator*() const
            {                   
                return std::make_pair< typename value_type::first_type, typename value_type::second_type >(it_.first->first, value_type_tuple(it_.second.operator *()));
            }

            bool operator == (set_iterator< Container, N > it) const 
            {            
                assert(it.container_ == container_);
                return it_ == it.it_;
            }

            bool operator != (set_iterator< Container, N > it) const 
            {
                assert(it.container_ == container_);
                return it_ != it.it_;
            }

            set_iterator< Container, N >& operator = (set_iterator< Container, N > it) 
            {
                container_ = it.container_;
                it_ = it.it_; 
                return *this;
            }

        private:
            typename Container::container_type* container_;

            mutable std::pair< 
                typename Container::container_iterator, 
                nested_iterator_type
            > it_;
        };

        template < size_t N, typename Head, typename... Tail > class set;

        template < typename Head > class set< 1, Head > 
        {
        public:
            using container_type = std::set< Head >;
            using container_iterator = typename container_type::iterator;

            using key_type = Head;
            using value_type = Head;
            using iterator = set_iterator< set < 1, Head >, 1 >;

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

            auto begin() { return iterator(values_.begin()); }
            auto end() { return iterator(values_.end()); }
        
            size_t size() const { return values_.size(); }

        private:
            container_type values_;
        };

        template < size_t N, typename Head, typename... Tail > class set
        {
        public:
            using nested_tree_type = set< N - 1, Tail... >;
            using container_type = std::map< Head, nested_tree_type >;
            using container_iterator = typename container_type::iterator;

            using key_type = Head;
            using mapped_type = std::tuple< Tail... >;
            using value_type = std::pair< const key_type&, mapped_type >;
            using iterator = set_iterator< set< N, Head, Tail... >, N >;

            set()
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
                        // This is type-safe nullptr
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
                        // This is type-safe nullptr
                        return std::add_pointer_t< decltype(*it->second.prefix(std::forward< TailT >(tail)...)) >();
                    }
                }                
            }

            auto begin() { return iterator(&values_, values_.begin()); }
            auto end() { return iterator(&values_, values_.end()); }
        
            size_t size() const { return size_; }

        private:
            container_type values_;
            size_t size_;
        };
    }

    template < typename... Args > class set: public detail::set< sizeof... (Args), Args... > {};
}