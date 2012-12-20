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

#ifndef TRANS_H
#define TRANS_H

#include <map>
#include <set>

#include "types.h"
#include "tstate.h"

enum trans_type
{
	thread_transition, 
	transfer_transition, 
	spawn_transition, 
	dummy
};

enum trans_dir_t
{
	unset,
	hor,
	nonhor
};

typedef std::map<local_t,std::set<local_t> > transfers_t; //TODO: this should be a multimap

struct Transition
{	
	Thread_State		source;
	Thread_State		target;
	transfers_t			bcs;
	mutable transfers_t	bcs2;
	
	Transition(const Thread_State& s = Thread_State(0,0), const Thread_State& t = Thread_State(0,0), transfers_t trns = transfers_t(), transfers_t trns2 = transfers_t());

	bool operator < (const Transition&) const;
	bool operator ==(const Transition&) const;

	std::ostream& extended_print(std::ostream&) const;
};

std::ostream& operator << (std::ostream&, const Transition& t);

#endif