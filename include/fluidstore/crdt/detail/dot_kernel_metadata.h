#pragma once

#include <fluidstore/crdt/detail/metadata.h>


namespace crdt
{
    namespace detail
    {
        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct dot_kernel_metadata;

        template < typename Container, typename Metadata, typename MetadataTag = typename Metadata::metadata_tag_type > struct dot_kernel_metadata_base;
    }
}