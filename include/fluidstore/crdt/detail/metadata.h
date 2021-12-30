#pragma once

namespace crdt
{
    namespace detail 
    {
        struct tag_local {};
        struct tag_shared {};

        template < typename Container, typename Metadata, typename MetadataTag = typename Metadata::metadata_tag_type > struct metadata_base;
        
        template < typename Container, typename Metadata > struct metadata_base< Container, Metadata, tag_local >
        {            
            Metadata& get_metadata() { return metadata_; }
            const Metadata& get_metadata() const { return metadata_; }

        private:
            Metadata metadata_;
        };
        
        template < typename Container, typename Metadata > struct metadata_base< Container, Metadata, tag_shared >
        {
            Metadata& get_metadata() { return static_cast< Container* >(this)->get_replica().get_metadata(); }
            const Metadata& get_metadata() const { return return static_cast<Container*>(this)->get_replica().get_metadata(); }
        };

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