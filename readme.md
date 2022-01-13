# Journey into CRDT Types

Conflict-free replicated data types are data structures that can be replicated over multiple computers in a network, where replicas can be updated independently and concurrently without coordination between the replicas.

# Contents of the Repository

Code in this repository implements stl-like set, map and multi-value register as a header only library. The stl interface was chosen as a standard, yet the code definitely does not met all stl requirements for the containers. Merging algorithm is based on the article ["An Optimized Conflict-free Replicated Set"](https://pages.lip6.fr/Marek.Zawirski/papers/RR-8083.pdf).

This algorithm was chosen for its elegance and optimization that means that the structure behaves naturally how one would expect - it keeps just minimal amount of metadata and can shrink during erase operations. The algorithm also looked super simple to implement.

In a nutshell, CRDT type needs to hold additional metadata related to operations that mutate the data structure so it can resolve the conflicts later when state between replicas is synchronzied. The algorithm is able to drop metadata that was observed by other replicas and is no longer required to resolve the conflicts or the metadata thats effects are no longer observable because of other operations.

# The way CRDTs operate

Any operation on CRDT data type is done through 'merging' of CRDT types.

Lets say we want to add an element E to the set S: we create an empty set D (delta) and add element E there. We merge set S with D. S now contains E. This allows us to distribute the delta D to other replicas that will perform the same merge operation. We can distribute it now or later, but the modifications comming from single replica needs to be ordered. This approach allows for buffering the changes naturally, we simply do all modifications to D and when we are done, we just merge (again, does not matter if we merge localy or on remote replicas) with latest version of D. So if ten elements are added, five of them deleted, final version that will be distributed will contain just the remaining five elements. If we add millions of elements and delete them all, we will distribute just the 'deletion', not the addition, as the effect of the deletion hides the effect of the addition. The erasure works similarily: we create empty set D, perform erasure of E, merge with S. S now does not have the element E.

# Design Description

Algorithm for conflict-free merging and base implementation for stl-like containers is in [crdtr::detail::dot_kernel](include/fluidstore/crdt/detail/dot_kernel.h). This allows types that have different implementations to be merged together. The dot_kernel class uses tags to select concrete representation for convenience. Currently, set and map operations are implemented as merges with stack-based temporary crdt types and different implementations exist for performance testing:

 - [crdt::detail::dot_kernel_metadata_stl](include/fluidstore/crdt/detail/dot_kernel_metadata_stl.h) - implemented using std::set and std::map
 - [crdt::detail::dot_kernel_metadata_btree](include/fluidstore/crdt/detail/dot_kernel_metadata_btree.h) - implemented using [btree::set](include/fluidstore/btree/set.h) and [btree::map](include/fluidstore/btree/map.h)
 - [crdt::detail::dot_kernel_metadata_flat](include/fluidstore/crdt/detail/dot_kernel_metadata_flat.h) - implemented using boost's flat_set and flat_map.

The general idea is that it should be possible to merge in-memory crdt type with on-disk crdt type efficiently and using the same algorithm, without imposing restrictions on size of the data.

# Implemented Data Types

- [crdt::set](include/fluidstore/crdt/set.h) - stl-like set
- [crdt::map](include/fluidstore/crdt/map.h) - stl-like map
- [crdt::value_mv](include/fluidstore/crdt/value_mv.h) - multi-value register

# Performance 

It is hard to put here any numbers right now as there are some unexplained and quite significant differences between msvc/clang compilers and windows/linux systems, yet fastest crdt implementation seems to be the one based on btree and the performance of crdt::set with btree is 5x-10x slower than the one of std::set.

# Tests

Sure, [here](src/tests) they are.

# The End

Thank you, interested reader. 

As the structures are recursive and the combination of CRDT structures is again a CRDT structure, graphs emerge naturally, or one can imagine json documents, all having the deterministic merging ability on all their fields, perhaps defined by user-selected merge strategy (observed remove, last write wins, etc), mapped from the persistent storage.








