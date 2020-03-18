#pragma once

#include <sqlstl/allocator.h>
#include <sqlstl/storage/set.h>

namespace sqlstl
{
    template < typename Key, typename Allocator > class set
    {
    public:
        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            // insert variant
            template < typename K > iterator(const set< Key, Allocator >* set, K&& key)
                : set_(set)
                , key_(std::forward< K >(key))
            {}

            // find variant
            template < typename K > iterator(const set< Key, Allocator >* set, K&& key, typename set_storage< Key >::iterator&& it)
                : it_(std::move(it))
                , set_(set)
                , key_(std::forward< K >(key))
            {}

            // begin / end variant
            iterator(const set< Key, Allocator >* set, typename set_storage< Key >::iterator&& it)
                : it_(std::move(it))
                , set_(set)
            {}

            iterator(iterator&& other)
                : it_(std::move(other.it_))
                , set_(other.set_)
                , key_(std::move(other.key_))
            {}

            const Key& operator*() const 
            {
                init_key();
                return *key_;
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
                key_.reset();
                ++it_; 
                return const_cast<iterator&>(*this);
            }
            
        private:
            void init_iterator() const
            {
                if(it_.result() == SQLITE_OK)
                {
                    it_ = std::move(set_->storage_.find(set_->name_, key_));
                }
            }

            void init_key() const
            {
                if(!key_)
                {
                    key_ = *it_;
                }
            }

            const set< Key, Allocator >* set_;
            mutable typename set_storage< Key >::iterator it_;
            mutable std::optional< Key > key_;
        };

        typedef iterator const_iterator;

        set(Allocator& allocator, const std::string& name)
            : storage_(allocator.template create_storage< set_storage< Key > >())
            , name_(name)
        {}

        template < typename K > iterator find(K&& key) 
        {
            auto it = storage_.find(name_, key);
            return { this, std::forward< K >(key), std::move(it) };
        }
        
        template < typename K > std::pair< iterator, bool > insert(K&& key)
        {
            auto inserted = storage_.insert(name_, key);
            return { iterator(this, std::forward< K >(key)), inserted };
        }
        
        template < typename It > void insert(It&& begin, It&& end)
        {
            while (begin != end)
            {
                insert(*begin);
                ++begin;
            }
        }

        iterator begin() { return { this, storage_.begin(name_) }; }
        const_iterator begin() const { return { this, storage_.begin(name_) }; }

        iterator end() { return { this, storage_.end(name_) }; }
        const_iterator end() const { return { this, storage_.end(name_) }; }

        size_t size() const { return storage_.size(name_); }
        bool empty() const { return size() == 0; }

    private:
        set_storage< Key >& storage_;
        std::string name_;        
    };

    template < typename Key, typename Allocator > struct container_traits< sqlstl::set< Key, Allocator > >
    {
        static const bool has_storage_type = true;
    };
}