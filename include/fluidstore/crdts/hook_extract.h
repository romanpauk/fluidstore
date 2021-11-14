#pragma once

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_extract
    {
        hook_extract(Allocator& allocator)
            : allocator_(allocator)
            , delta_(allocator_)
        {}

        template < typename Delta > void commit_delta(Delta&& delta)
        {
            delta_.merge(delta);
            allocator_.update();
        }

        Delta extract_delta()
        {
            Delta delta(delta_.get_allocator());
            delta.merge(delta_);

            typename Container::delta_extractor extractor;
            extractor.apply(*static_cast<Container*>(this), delta);

            delta_.reset();
            return delta;
        }

        typename Allocator& get_allocator()
        {
            return allocator_;
        }

    private:
        Allocator allocator_;
        Delta delta_;
    };
}