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
#include "tstate.h"

#include "types.h"
#include "user_assert.h"

#include <iostream>
using namespace std;

Thread_State::Thread_State(unsigned s, unsigned l): shared(s), local(l)
{
}

unsigned Thread_State::unique_id(shared_t S, local_t L) const
{
	return (shared * L) + local;
}

bool Thread_State::operator == (const Thread_State& r) const
{
	return (this->shared == r.shared) && (this->local == r.local);
}

bool Thread_State::operator != (const Thread_State& r) const
{ 
	return !(*this==r); 
}

bool Thread_State::operator < (const Thread_State& r) const
{
	if(this->shared != r.shared)
		return (this->shared < r.shared);
	
	invariant(this->shared == r.shared);
	
	return (this->local < r.local);
}

std::ostream& operator << (std::ostream& out, const Thread_State& t)
{
	if(!out.rdbuf()) return out;
	out << t.shared << " " << t.local;
	return out;
}
