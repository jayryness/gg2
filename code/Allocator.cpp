#include "Allocator.h"

namespace gg {

IAllocatorShim<Mallocator> Mallocator::s_iallocatorImpl;

}
