#define NOMINMAX

#include <iostream>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <numeric>

#define BOOST_TEST_MODULE fluidstore
#include <boost/test/unit_test.hpp>

#include <sqlstl/allocator.h>
#include <sqlstl/map.h>
#include <sqlstl/set.h>
#include <sqlstl/tuple.h>

#include <crdt/counter_g.h>
#include <crdt/counter_pn.h>
#include <crdt/set_g.h>
#include <crdt/set_or.h>
#include <crdt/map_g.h>
#include <crdt/value_lww.h>
#include <crdt/value_mv.h>
#include <crdt/traits.h>

struct delta_context
{
    struct delta_container
    {
        std::shared_ptr< void > container;
        std::function< void() > merge;
    };

    template < typename DeltaContainer, typename StateContainer, typename Allocator > 
    DeltaContainer& create(StateContainer& state_container, Allocator& allocator)
    {
        auto& rec = containers_[state_container.get_allocator().get_name()];
        if (!rec.container)
        {
            auto container = std::make_shared< DeltaContainer >(allocator);
            
            rec.container = container;
            rec.merge = [container, &state_container]
            {
                state_container.merge(container->delta_container_);
            };
        }

        return *reinterpret_cast< DeltaContainer* >(rec.container.get());
    }

    void commit(bool merge)
    {
        if (merge)
        {
            for (auto& rec : containers_)
            {
                rec.second.merge();
            }
        }

        containers_.clear();
    }

private:
    std::map< std::string, delta_container > containers_;
};

template < typename Traits > struct schema
{
    typedef typename Traits::template Allocator<void> allocator_type;

    schema(allocator_type allocator)
        : counter_g_(allocator)
        , set_g_(allocator)
        , counter_pn_(allocator)
        , allocator_(allocator)
    {}

    crdt::set_g< int, Traits > set_g_;
    crdt::counter_g< int, int, Traits > counter_g_;
    crdt::counter_pn< int, int, Traits > counter_pn_;
    allocator_type allocator_;

    void run()
    {
        delta_context context;

        {
            auto& delta = context.create<
                crdt::counter_g_delta< int, int, crdt::traits< crdt::memory > >
            >(counter_g_, allocator_);

            delta.update(counter_g_, 1, 1);
        }

        {
            auto& delta = context.create<
                crdt::counter_pn_delta< int, int, crdt::traits< crdt::memory > >
            >(counter_pn_, allocator_);

            delta.update(counter_pn_, 1, 1);
            delta.update(counter_pn_, 1, -2);
        }

        context.commit(true);
    }
};

BOOST_AUTO_TEST_CASE(main)
//int main()
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);

    sqlstl::allocator<void> allocator(factory);

    // Sql-based map of delta-counters that themselves are sql-based.
    sqlstl::map< 
        int, 
        crdt::counter_pn< int, int,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::allocator<void>
    > mapcounterg(allocator);

    mapcounterg[1].update(1, 4);
    mapcounterg[1].update(2, 4);
    BOOST_ASSERT(mapcounterg[0].value() == 0);
    BOOST_ASSERT(mapcounterg[1].value() == 8);

    sqlstl::map<
        int,
        crdt::set_g< int,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::allocator<void>
    > mapsetg(allocator);

    mapsetg[1].insert(1);
    mapsetg[1].insert(2);
    BOOST_ASSERT(mapsetg[0].size() == 0);
    BOOST_ASSERT(mapsetg[1].size() == 2);

    //crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg1(allocator);
    //crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg2(allocator);

    //mapg1[1].insert(1);
    //mapg1[1].insert(2);
    //mapg2[1].insert(3);
    //mapg2[2].insert(1);
    //mapg1.merge(mapg2);
    //BOOST_ASSERT(mapg1[1].size() == 3);

    schema< crdt::traits< crdt::sqlite > > schema(allocator);
    // schema.run();
} 
