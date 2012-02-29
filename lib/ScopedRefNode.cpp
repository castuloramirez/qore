/*
 ScopedRefNode.cpp
 
 Qore Programming Language
 
 Copyright 2003 - 2012 David Nichols
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <qore/Qore.h>

#include <qore/intern/QoreNamespaceIntern.h>

// object takes over ownership of NamedScope
ScopedRefNode::ScopedRefNode(char *ref) : ParseNoEvalNode(NT_CONSTANT), scoped_ref(new NamedScope(ref)) {
}

ScopedRefNode::~ScopedRefNode() {
   delete scoped_ref;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int ScopedRefNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("namespace-scoped reference '%s' (%p)", scoped_ref ? scoped_ref->ostr : "<null>", this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *ScopedRefNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString;
   getAsString(*rv, foff, xsink);
   return rv;
}

// returns the data type
qore_type_t ScopedRefNode::getType() const {
   return NT_CONSTANT;
}

// returns the type name as a c string
const char *ScopedRefNode::getTypeName() const {
   return "namespace-scoped reference";
}

NamedScope *ScopedRefNode::takeName() {
   assert(scoped_ref);
   NamedScope *n = scoped_ref;
   scoped_ref = 0;
   return n;
}

AbstractQoreNode *ScopedRefNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   printd(5, "ScopedRefNode::parseInit() resolving scoped constant \"%s\"\n", scoped_ref->ostr);

   AbstractQoreNode* rv = qore_root_ns_private::parseFindConstantValue(scoped_ref, typeInfo, true);
   if (!rv)
      return this;

   deref(0);
   rv->ref();
   return rv->parseInit(oflag, pflag, lvids, typeInfo);
}
