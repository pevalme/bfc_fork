/******************************************************************************
  Synopsis		[Transition objects.]

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
#include "trans.h"

#include <iostream>

using namespace std;

Transition::Transition(const Thread_State& s, const Thread_State& t, transfers_t b, transfers_t b2)
	: source(s), target(t), bcs(b), bcs2(b2)
{
}

bool Transition::operator < (const Transition& t2) const
{
	if(this->source < t2.source) return 1;
	if(t2.source < this->source) return 0;
	if(this->target < t2.target) return 1;
	if(t2.target < this->target) return 0;

	return this->bcs < t2.bcs;
}

bool Transition::operator ==(const Transition& t2) const
{
	return this->source == t2.source && this->target == t2.target && this->bcs == t2.bcs;
}

ostream& operator << (ostream& out, const Transition& r)
{ 
	if(!out.rdbuf()) return out;
	out << "(" << r.source << ")" << " -> " << "(" << r.target << ")";
	return out;
}

ostream& Transition::extended_print(ostream& out) const
{ 
	out << "(" << this->source << ")" << " -> " << "(" << this->target << ")";
	return out;
}
