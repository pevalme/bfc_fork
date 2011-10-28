/******************************************************************************
  Synopsis		[Thread state object.]

  Author		[Alexander Kaiser]

(C) 2011 Alexander Kaiser, University of Oxford, United Kingdom

All rights reserved. Redistribution and use in source and binary forms, with
or without modification, are permitted provided that the following
conditions are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  3. All advertising materials mentioning features or use of this software
     must display the following acknowledgement:

     This product includes software developed by Alexander Kaiser, 
     University of Oxford, United Kingdom and its contributors.

  4. Neither the name of the University nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS `AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include "types.h"
#include <iostream>
using namespace std;

class Thread_State {
public:
	unsigned shared;
	unsigned local;
	Thread_State(unsigned s = -1, unsigned l = -1): shared(s), local(l) {}
	
	unsigned unique_id(shared_t, local_t) const;
};

unsigned Thread_State::unique_id(shared_t S, local_t L) const
{
	return (shared * L) + local;
}

bool operator == (const Thread_State& l, const Thread_State& r){
	return (l.shared == r.shared) && (l.local == r.local);
}

bool operator != (const Thread_State& l, const Thread_State& r){ return !(l==r); }

short compare(clong& x, clong& y) {
	if (x < y)
		return -1;
	if (x > y)
		return +1;
	return 0; 
}

bool operator < (const Thread_State& t1, const Thread_State& t2) {
	cshort compare_shareds = compare(t1.shared, t2.shared);
	if (compare_shareds != 0)
		return compare_shareds == -1;
	cshort compare_locals  = compare(t1.local,  t2.local);
	return compare_locals == -1; }

ostream& operator << (ostream& out, const Thread_State& t) {
	if(!out.rdbuf()) return out;
	out << t.shared << " " << t.local;
	return out;
}

