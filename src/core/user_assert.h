#ifndef USER_ASSERT_H
#define USER_ASSERT_H

#include <assert.h>

#ifdef _DEBUG
#define debug_stmt(e) e
#define debug_assert(e) assert(e)
#define release_assert(e) debug_assert(e)
#define debug_assert_for(a,b,c) for(auto a : b) assert(c)
#define debug_assert_foreach(a,b,c) foreach(a,b)assert(c)
#define debug_assert_foreach_foreach(a,b,c,d,e) foreach(a,b){foreach(c,d) assert(e)}

#define invariant(e) debug_assert(e)
#define invariant_for(a,b,c) debug_assert_for(a,b,c)
#define invariant_foreach(a,b,c) debug_assert_foreach(a,b,c)
#define non_invariant(e) ((void)0)
#define non_invariant_foreach(a,b,c) ((void)0)
#else

#ifdef WIN32
extern "C" {_CRTIMP void __cdecl _wassert(_In_z_ const wchar_t * _Message, _In_z_ const wchar_t *_File, _In_ unsigned _Line);}
#define release_assert(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), __LINE__), 0) )
#else
#define release_assert(e) (void)( (!!(e)) || (exit(99), 0) )
#endif

#define release_assert_foreach(a,b,c) foreach(a,b)release_assert(c)
#define debug_stmt(e) ((void)0)
#define debug_assert(e) ((void)0)
#define debug_assert_for(a,b,c) ((void)0)
#define debug_assert_foreach(a,b,c) ((void)0)
#define debug_assert_foreach_foreach(a,b,c,d,e) ((void)0)

#define invariant(e) ((void)0)
#define invariant_for(a,b,c) ((void)0)
#define invariant_foreach(a,b,c) ((void)0)
#define non_invariant(e) ((void)0)
#define non_invariant_foreach(a,b,c) ((void)0)
#endif

#define precondition(e) debug_assert(e)
#define postcondition(e) debug_assert(e)
#define precondition_foreach(a,b,c) debug_assert_foreach(a,b,c)
#define postcondition_foreach(a,b,c) debug_assert_foreach(a,b,c)
#define implies(a,b) (!(a) || (b))
#define impliedby(a,b) (implies(b,a))
#define iff(a,b) ((a) == (b))

#endif