# Journey into CRDT Types

Conflict-free replicated data types are data structures that can be replicated over multiple computers in a network, where replicas can be updated independently and concurrently without coordination between the replicas.

# Contents of the Repository

Code in this repository implements stl-like set, map and multi-value register as a header only library. The stl interface was chosen as a standard, yet the code definitely does not met all stl requirements for the containers. Merging algorithm is based on the article ["An Optimized Conflict-free Replicated Set"](https://pages.lip6.fr/Marek.Zawirski/papers/RR-8083.pdf).

This algorithm was chosen for its elegance and optimization that means that the structure behaves naturally how one would expect - it keeps just minimal amount of metadata and can shrink during erase operations. The algorithm also looked super simple to implement.

In a nutshell, CRDT type needs to hold additional metadata related to operations that mutate the data structure so it can resolve the conflicts later when state between replicas is synchronzied. The algorithm is able to drop metadata that was observed by other replicas and is no longer required to resolve the conflicts or the metadata thats effects are no longer observable because of other operations.

# The way CRDTs operate

Any operation on CRDT data type is done through 'merging' of CRDT types.

Lets say we want to add an element E to the set S: we create an empty set D (delta) and add element E there. We merge set S with D. S now contains E. This allows us to distribute the delta D to other replicas that will perform the same merge operation. We can distribute it now or later, but the modifications comming from single replica needs to be ordered. This approach allows for buffering the changes naturally, we simply do all modifications to D and when we are done, we just merge (again, does not matter if we merge localy or on remote replicas) with latest version of D. So if ten elements are added, five of them deleted, final version that will be distributed will contain just the remaining five elements. If we add millions of elements and delete them all, we will distribute just the 'deletion', not the addition, as the effect of the deletion hides the effect of the addition. The erasure works similarily: we create empty set D, perform erasure of E, merge with S. S now does not have element E.

# Design Decisions

The code is using templates to keep core algorithm in one place yet detaches it from the actual storage - set S and set D can be implemented differently, each having different lifetime. D used in the above example can be fully stack-based without any heap usage, while S can be on the heap as it lives longer (this is currently used for all merging operations so they are cheaper). The general idea is that we will receive data from network, deserialize it into temporary D that lives on stack and merge it with S that lives in database. Or with cached S' that lives in RAM, that will be merged with database S later. All this through the same code, just by parametrizing the algorithm.

Lets look at how crdt::set looks like with respect to inheritance and map/sets usage:

- [crdt::set](include/fluidstore/crdts/set.h) - based on two different crdt::set_base classes, one for delta temporary for mutating operations and other for the data structure itself
    - [crtd::dot_kernel](include/fluidstore/crdts/dot_kernel.h), the core of the containers, shared between map and set implementation
        - map with keys/values and additional data
            - [crdt::dot_context](include/fluidstore/crdts/dot_context.h) is tracking dot data for each replica, for each value, for associative container version
                - using map of [crdt::dot_counters_base](include/fluidstore/crdts/dot_counters_base.h)
        - map with per-replica data
            - [crdt::dot_counters_base](include/fluidstore/crdts/dot_counters_base.h)
                - using set
            - temporary sets for merge operations

Sets and maps here correspond to 'normal', stl-like sets and maps, yet implemented in a flat memory buffer due to memory/performance issues. b+-tree implementation is comming to deal with performance issues with larger sets and to give the possibility to offload the data to persistent storage effectively (as we will be able to work with just portion of structure that is changing). There is not yet merged b+tree implementation here: [btree.h](https://github.com/romanpauk/fluidstore/blob/feature/btree2/include/fluidstore/btree/btree.h). The b+tree code avoids using virtual functions for internal/value nodes so those can be mapped from file.

To add to the fun, the merge algorithm very slightly differs for delta/non-delta variants (D and S) in a most inner class, crdt::dot_counters_base. The whole thing is very nicely recursive - different parts of the code reuse itself differently with slight modifications on different places, for rather different cases, yet with the same code dealing with sorted set of integers in sort of similar way, yet different. In the beginning, my idea was to create something normal, not sure where it all went haywire.

Different CRDT types implemented:    
- [crdt::set](include/fluidstore/crdts/set.h)
- [crdt::map](include/fluidstore/crdts/map.h)
- [crdt::value_mv](include/fluidstore/crdts/value_mv.h) - multivalue register, usually holding one value, but sometimes holding two values (in case of conflicting merge - here the conflict resolution means that we will not lose data, but propagate it to application layer). Based on crdt::set_base.

I've skipped counters as they are easy to implement.

# Performance

The performance of current implementation of crdt::map/crdt::set is comparable to performance of default configuration of std::map/std::set but this of course cheats a lot - I am comparing custom container configured with various optimizations to stl's container configured without optimizations. This is simply to show that even the above implementation does not have to be as slow as it seems when done carefully as it is as fast as default use of stl (or as slow).

# Tests

Sure, [here](src/tests) they go.

# The End

Thank you, interested reader. It had to be very painful to get here.

As the structures are recursive and the combination of CRDT structures is again a CRDT structure, graphs emerge naturally, or one can imagine json documents, all having the deterministic merging ability on all their fields, perhaps defined by user-selected merge strategy (observed remove, last write wins, etc).

What is possible right now is something like this (more can be seen in tests):

```
{
    // on PC 1:

    crdt::map< int, crdt::map< int, crdt::set < int > > > data;
    data[1][2].insert(33);
    auto delta = data.extract_delta();

    // serialize delta, as the types look 'normal', using boost::serialization should work
    // the extracted delta will contain what is required to add 33 to data[1][2].

    // send to PC 2
}

{
    // on PC 2:

    crdt::map< int, crdt::map< int, crdt::set < int > > > data;
    
    // deserialize delta
    
    data2.merge(delta);
    
    // and there is now 33 in data2[1][2]
}
```








