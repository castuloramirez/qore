/*
  ReferenceNode.h
  
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

AbstractQoreNode* ParseReferenceNode::doPartialEval(AbstractQoreNode* n, QoreObject*& self, ExceptionSink* xsink) const {
   qore_type_t ntype = n->getType();

   //printd(5, "ParseReferenceNode::doPartialEval() this: %p type: '%s' %d\n", this, get_type_name(n), ntype);

   if (ntype == NT_TREE) {
      QoreTreeNode* tree = reinterpret_cast<QoreTreeNode*>(n);
      ReferenceHolder<> nn(tree->right->eval(xsink), xsink);
      if (*xsink)
         return 0;

      SimpleRefHolder<QoreTreeNode> t(new QoreTreeNode(doPartialEval(tree->left, self, xsink), tree->getOp(), nn ? nn.release() : nothing()));
      return t->left ? t.release() : 0;
   }

   if (ntype == NT_SELF_VARREF) {
      assert(!self);
      self = runtime_get_stack_object();
   }
   else if (ntype == NT_VARREF) {
      VarRefNode* v = reinterpret_cast<VarRefNode*>(n);
      //printd(5, "ParseReferenceNode::doPartialEval() this: %p v: '%s' type: %d\n", this, v->getName(), v->getType());
      if (v->getType() == VT_CLOSURE) {
         const char* name = v->ref.id->getName();
         ClosureVarValue* cvv = thread_get_runtime_closure_var(v->ref.id);
         //printd(5, "ParseReferenceNode::doPartialEval() this: %p '%s' cvv: %p\n", this, name, cvv);
         return new VarRefImmediateNode(strdup(name), cvv, v->ref.id->getTypeInfo());
      }
      else if (v->getType() == VT_LOCAL_TS) {
         const char* name = v->ref.id->getName();
         ClosureVarValue* cvv = thread_find_closure_var(name);
         //printd(5, "ParseReferenceNode::doPartialEval() this: %p '%s' cvv: %p\n", this, name, cvv);
         return new VarRefImmediateNode(strdup(name), cvv, v->ref.id->getTypeInfo());
      }
   }

   return n->refSelf();
}

ReferenceNode* ParseReferenceNode::evalToRef(ExceptionSink* xsink) const {
   QoreObject* self = 0;
   AbstractQoreNode* nv = doPartialEval(lvexp, self, xsink);
   return nv ? new ReferenceNode(nv, self) : 0;
}

IntermediateParseReferenceNode* ParseReferenceNode::evalToIntermediate(ExceptionSink* xsink) const {
   QoreObject* self = 0;
   AbstractQoreNode* nv = doPartialEval(lvexp, self, xsink);
   return nv ? new IntermediateParseReferenceNode(nv, self) : 0;
}

AbstractQoreNode* ParseReferenceNode::parseInitImpl(LocalVar* oflag, int pflag, int& lvids, const QoreTypeInfo*& typeInfo) {
   typeInfo = referenceTypeInfo;
   if (!lvexp)
      return this;

   const QoreTypeInfo* argTypeInfo = 0;
   lvexp = lvexp->parseInit(oflag, pflag, lvids, argTypeInfo);
   if (!lvexp)
      return this;

   if (check_lvalue(lvexp)) {
      parse_error("the reference operator was expecting an lvalue, got '%s' instead", lvexp->getTypeName());
      return this;
   }

   // check lvalue, and convert "normal" local vars to thread-safe local vars
   AbstractQoreNode* n = lvexp;
   while (true) {
      qore_type_t ntype = n->getType();
      // references to objects members and static class vars are already thread-safe
      if (ntype == NT_SELF_VARREF || ntype == NT_CLASS_VARREF)
         break;
      if (ntype == NT_VARREF) {
         reinterpret_cast<VarRefNode*>(n)->setThreadSafe();
         break;
      }
      assert(ntype == NT_TREE);
      // must be a tree
      n = reinterpret_cast<QoreTreeNode*>(n)->left;
   }

   return this;
}

ReferenceNode::ReferenceNode(AbstractQoreNode* exp, QoreObject* self) : AbstractQoreNode(NT_REFERENCE, false, true), priv(new lvalue_ref(exp, self)) {
}

ReferenceNode::ReferenceNode(lvalue_ref* p) : AbstractQoreNode(NT_REFERENCE, false, true), priv(p) {
}

ReferenceNode::~ReferenceNode() {
   delete priv;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int ReferenceNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
   str.sprintf("reference expression (%p)", this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString* ReferenceNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
   del = true;
   QoreString* rv = new QoreString;
   getAsString(*rv, foff, xsink);
   return rv;
}

AbstractQoreNode* ReferenceNode::evalImpl(ExceptionSink* xsink) const {
   LValueHelper lvh(this, xsink);
   return lvh ? lvh.getReferencedValue() : 0;
}

AbstractQoreNode* ReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
   needs_deref = true;
   return ReferenceNode::evalImpl(xsink);
}

int64 ReferenceNode::bigIntEvalImpl(ExceptionSink* xsink) const {
   LValueHelper lvh(this, xsink);
   return lvh ? lvh.getAsBigInt() : 0;
}

int ReferenceNode::integerEvalImpl(ExceptionSink* xsink) const {
   LValueHelper lvh(this, xsink);
   return lvh ? (int)lvh.getAsBigInt() : 0;
}

bool ReferenceNode::boolEvalImpl(ExceptionSink *xsink) const {
   LValueHelper lvh(this, xsink);
   return lvh ? lvh.getAsBool() : false;
}

double ReferenceNode::floatEvalImpl(ExceptionSink *xsink) const {
   LValueHelper lvh(this, xsink);
   return lvh ? lvh.getAsFloat() : 0.0;
}

AbstractQoreNode* ReferenceNode::realCopy() const {
   return new ReferenceNode(new lvalue_ref(*priv));
}

// the type passed must always be equal to the current type
bool ReferenceNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
   ReferenceHolder<> val(ReferenceNode::evalImpl(xsink), xsink);
   if (*xsink)
      return false;
   if (!val)
      return is_nothing(v) ? true : false;
   return val->is_equal_soft(v, xsink);
}

bool ReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
   ReferenceHolder<> val(ReferenceNode::evalImpl(xsink), xsink);
   if (*xsink)
      return false;
   if (!val)
      return is_nothing(v) ? true : false;
   return val->is_equal_hard(v, xsink);
}

// returns the type name as a c string
const char* ReferenceNode::getTypeName() const {
   return "runtime reference to lvalue";
}

bool ReferenceNode::derefImpl(ExceptionSink* xsink) {
   priv->del(xsink);
   return true;
}
