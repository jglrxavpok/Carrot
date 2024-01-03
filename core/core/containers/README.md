Containers for Carrot engine
====
Implementation of a container types for Carrot.

# Allocators
Like the EA STL, all containers made for Carrot reference an allocator 
and do no allocation on empty construction (ie empty containers should do not allocations).

While allocators are currently provided to containers via their constructor, they are NOT part of their type, 
and changing the allocator may be supported in the future.

# Moving
When moving the contents of a container to another, if the allocators are compatible (`Allocator::isCompatibleWith(const Allocator&)`),
the dynamic memory pointer(s) are moved to the container directly.

Otherwise, a new allocation is done in the target's allocator, and move constructors are called for each element. The source container will then deallocate its memory. 