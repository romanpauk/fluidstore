#pragma once

namespace crdt
{
    namespace detail 
    {     
        template < typename Container, typename Metadata, typename MetadataTag = typename Metadata::metadata_tag_type > struct metadata;        
        
        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct dot_kernel_metadata;

        template < typename T, typename Id > struct instance_base
        {
            instance_base()
                : id_()
            {}

            ~instance_base()
            {
                if (id_)
                {
                    // id_ = static_cast<T*>(this)->get_allocator().get_replica().release_instance_id(id_);
                }
            }

            Id get_id()
            {
                if (!id_)
                {
                    // id_ = static_cast<T*>(this)->get_allocator().get_replica().acquire_instance_id();
                }

                return id;
            }

        private:
            Id id_;
        };
    }
}