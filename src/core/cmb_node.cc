/******************************************************************************
  Synopsis		[Bfc - Greedy Analysis of Multi-Threaded Programs with 
				Non-Blocking Communication.]

  Author		[Alexander Kaiser]

(C) 2011 - 2014 Alexander Kaiser, University of Oxford, United Kingdom

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
#include "cmb_node.h"

#include "user_assert.h"

#include <boost/functional/hash.hpp>
#include <boost/foreach.hpp>

using namespace std;

cmb_node::cmb_node(const cmb_node& r, bool alloc): c(r.c), lt(alloc?new list<cmb_node_p>():nullptr), rc(alloc?new unsigned(0):nullptr)
{
	if(alloc)
		*this->lt = *r.lt, *this->rc = *r.rc;
}

cmb_node::cmb_node(size_t count): c(count), lt(new list<cmb_node_p>()), rc(new unsigned(0))
{
}

cmb_node::cmb_node(size_t count, local_t val): c(count, val), lt(new list<cmb_node_p>()), rc(new unsigned(0))
{
}

cmb_node::~cmb_node(){
	if(lt != nullptr) delete lt, lt = nullptr;
	if(rc != nullptr) delete rc, rc = nullptr;
}

/* ---- Operators ---- */
cmb_node& cmb_node::operator=(const cmb_node& r){
	if(this != &r) this->c = r.c, *this->lt = *r.lt, *this->rc = *r.rc;
	return *this;
}

std::ostream& cmb_node::operator << (std::ostream& out) const
{
	string sep;
	foreach(local_t l, c)
		out << sep << l, sep = ",";
	return out;
}

std::ostream& operator << (std::ostream& out, const cmb_node& t)
{
	return t.operator<<(out);
}

/* ---- Helpers for unordered containers ---- */
std::size_t cmb_node__husher::operator()(const cmb_node_p& v) const {
	size_t seed = 0;
	boost::hash_combine(seed, v->c.size());
	foreach(const local_t& l, v->c){
		boost::hash_combine(seed, l);
	}
	return seed;
}

bool cmb_node__eqstr::operator()(const cmb_node_p& v1, const cmb_node_p& v2) const {
	unsigned v1_sz = v1->c.size();
	if(v1_sz != v2->c.size()) return 0;
	for(unsigned i = v1_sz-1; i != 0u-1; --i){
		if(v1->c[i] != v2->c[i]) return 0;
	}
	return 1;
}
