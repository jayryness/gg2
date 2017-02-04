#if defined(_VERTEX_)

#define VERTEX_PROGRAM(code) code
#pragma shader_stage(vertex)

#elif defined(_FRAGMENT_)

#define FRAGMENT_PROGRAM(code) code
#pragma shader_stage(fragment)

#else

#error "_VERTEX_ or _FRAGMENT_ must be #defined"

#endif

#ifndef VERTEX_PROGRAM
#define VERTEX_PROGRAM(ignored)
#endif

#ifndef FRAGMENT_PROGRAM
#define FRAGMENT_PROGRAM(ignored)
#endif
