# Journey into CRDT types

Conflict-free replicated data types are data structures that can be replicated over multiple computers in a network, where replicas can be updated independently and concurrently without coordination between the replicas.

# Contents of this repository

Code in this repository implements stl-like set, map and multi-value register. The stl interface was chosen as a standard, yet the code definitely does not met all stl requirements for the containers. Merging algorithm is based on the article "An Optimized Conflict-free Replicated Set".

This algorithm was chosen for its elegance and 'optimized' property which means that the structure does not only grow, but can also shrink when possible (some types of those structures just grow and accumulate the history).

In a nutshell, CRDT type needs to hold additional metadata related to operations that mutate the data structure so it can resolve the conflicts properly. The algorithm is able to drop metadata that was observed by other replicas and is no longer required to resolve the conflicts or the metadata thats effects were superseeded by other metadata.

# The way CRDTs operate

Any operation on CRDT data type is done through 'merging' of CRDT types.

Lets say we want to add an element E to the set S: we create set D (delta) and add element E there. We merge set S with D. D now contains E. This allows us to distribute the delta D to other replicas that will perform the same merge operation. We can distribute it now or later, but the modifications comming from single replica needs to be ordered. This approach allows for buffering the changes naturally, we simply do all modifications to D and when we are done, we just merge (again, does not matter if we merge localy or on remote replicas) with latest version of D. So if ten elements are added, five of them deleted, final version that will be distributed will contain just the remaining five elements. If we add millions of elements and delete them all, we will distribute just the 'deletion', not the addition, as the effect of the deletion hides the effect of the addition.

# Design decisions

The code is using templates to keep core algorithm in one place yet detaches it from the actual storage - set S and set D can be implemented differently, each having different lifetime. D can be on the stack, while S can be on the heap (currently used for all merging operations, see crdt::set/crdt::map insert for an example).

Each struct has 2 variants, for map that would be map and map_base. The reason is that map is composed from multiple differently configured map_bases. The core algorighm is split between dot_context/dot_kernel.
Also, the classes do not hold allocator but receive it as a parameter as there would be a lot of space wasted if every class would have it (as there is multiple maps/sets in CRDT maps/sets :))

Nice example is multi-value register (a variable that usually holds one value, but sometimes two, in case of conflict). The maps and sets are not so big, but the value_mv done through this way is an interesting excercise.

- crdt::value_mv class is based on two different crdt::value_mv_base classes
- crdt::value_mv_base class is based on crdt::set
- crdt::set is based on two different crdt::set_base classes
- crdt::set_base is based on crdt::dot_counters and crdt::dot_kernel
- crtd::dot_kernel is using map to track (dot_context, sets and maps) per-replica and a map to track the actual keys and values, where each value is again using dot_context internally.
- crdt::dot_context is using map of sets

To add to the fun, the merge algorithm very slightly differs for delta/non-delta variants (D and S) in a most inner class, dot_context. It is very nicely recursive - different parts of the code reuse itself differently with slight modifications.

https://github.com/romanpauk/fluidstore/blob/master/include/fluidstore/crdts/map.h
https://github.com/romanpauk/fluidstore/blob/master/include/fluidstore/crdts/dot_kernel.h
https://github.com/romanpauk/fluidstore/blob/master/include/fluidstore/crdts/dot_context.h

As the structures internally needs to use more maps/sets, I've implemented flat set/map to save memory. B+Tree implementation is in the works that will hopefully give the performance of flat arrays for small number of values, yet performance of trees for modifications of larger sets. Other reason for B+Tree is its ability to effectively persist the structure on the disk, either indepentently or in some existing storage engine, so only the necessary parts of it can be accessed/modified. The B+Tree is here: https://github.com/romanpauk/fluidstore/blob/feature/btree2/include/fluidstore/btree/btree.h, not yet merged, still work in progress, but the algorithm is correct.

# Performance

The performance of current implementation of crdt::map/crdt::set is comparable to performance of default configuration of std::map/std::set but this of course cheats a lot - I am comparing custom container configured with various optimizations to stl's container configured without optimizations. This is simply to show that even the above implementation does not have to be as slow as it seems when done carefully as it is as fast as default use of stl (or as slow).

# Last words

As the structures are recursive and the combination of CRDT structures is again a CRDT structure, graphs emerge naturally, or one can imagine json documents, all having the deterministic merging ability on all fields, perhaps user-defined merge strategy (observed remove, last write wins, etc), stored in B+Tree...









