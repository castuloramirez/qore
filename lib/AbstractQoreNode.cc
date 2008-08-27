/*
  AbstractQoreNode.cc

  Qore Programming Language

  Copyright 2003 - 2008 David Nichols

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define TRACK_REFS 1

#if TRACK_REFS
#endif

AbstractQoreNode::AbstractQoreNode(qore_type_t t, bool n_value, bool n_needs_eval, bool n_there_can_be_only_one, bool n_custom_reference_handlers) : type(t), value(n_value), needs_eval_flag(n_needs_eval), there_can_be_only_one(n_there_can_be_only_one), custom_reference_handlers(n_custom_reference_handlers)
{
#if TRACK_REFS
   printd(5, "AbstractQoreNode::ref() %08p type=%d (0->1)\n", this, type);
#endif
}

AbstractQoreNode::~AbstractQoreNode()
{
#if 0
   printd(5, "AbstractQoreNode::~AbstractQoreNode() type=%d (%s)\n", type, getTypeName());
#endif
}

void AbstractQoreNode::ref() const
{
#ifdef DEBUG
#if TRACK_REFS
   if (type == NT_OBJECT) {
      const QoreObject *o = reinterpret_cast<const QoreObject *>(this);
      printd(5, "AbstractQoreNode::ref() %08p type=object (%d->%d) object=%08p, class=%s\n", this, references, references + 1, o, o->getClass()->getName());
   }
   else
      printd(5, "AbstractQoreNode::ref() %08p type=%s (%d->%d)\n", this, getTypeName(), references, references + 1);
#endif
#endif
   if (!there_can_be_only_one) {
      if (custom_reference_handlers)
	 customRef();
      else
	 ROreference();
   }
}

AbstractQoreNode *AbstractQoreNode::refSelf() const
{
   ref();
   return const_cast<AbstractQoreNode *>(this);
}

bool AbstractQoreNode::derefImpl(ExceptionSink *xsink)
{
   return true;
}

void AbstractQoreNode::customRef() const
{
   assert(false);
}

void AbstractQoreNode::customDeref(ExceptionSink *xsink)
{
   assert(false);
}

void AbstractQoreNode::deref(ExceptionSink *xsink)
{
   //QORE_TRACE("AbstractQoreNode::deref()");
#ifdef DEBUG
#if TRACK_REFS
   if (type == NT_STRING) printd(5, "AbstractQoreNode::deref() %08p (%d->%d) string='%s'\n", this, references, references - 1, ((QoreStringNode *)this)->getBuffer());
   else if (type == NT_OBJECT)
      printd(5, "QoreObject::deref() %08p class=%s (%d->%d) %d\n", this, ((QoreObject *)this)->getClassName(), references, references - 1, custom_reference_handlers);
   else
      printd(5, "AbstractQoreNode::deref() %08p type=%s (%d->%d)\n", this, getTypeName(), references, references - 1);

#endif
   if (references > 51200 || references <= 0)
   {
      if (type == NT_STRING)
	 printd(0, "AbstractQoreNode::deref() WARNING, node %08p references=%d (type=%s) (val=\"%s\")\n",
		this, references, getTypeName(), ((QoreStringNode *)this)->getBuffer());
      else
	 printd(0, "AbstractQoreNode::deref() WARNING, node %08p references=%d (type=%s)\n",
		this, references, getTypeName());
      assert(false);
   }
#endif
   assert(references > 0);

   if (there_can_be_only_one) {
      assert(is_unique());
      return;
   }
   
   if (custom_reference_handlers) {
      customDeref(xsink);
   }
   else if (ROdereference()) {
      if (type < NUM_SIMPLE_TYPES || derefImpl(xsink))
	 delete this;
   }


}

// AbstractQoreNode::eval(): return value requires a dereference
AbstractQoreNode *AbstractQoreNode::eval(ExceptionSink *xsink) const
{
   if (!needs_eval_flag)
      return refSelf();
   return evalImpl(xsink);
}

// AbstractQoreNode::eval(): return value requires a dereference if needs_deref is true
AbstractQoreNode *AbstractQoreNode::eval(bool &needs_deref, ExceptionSink *xsink) const
{
   if (!needs_eval_flag) {
      needs_deref = false;
      return const_cast<AbstractQoreNode *>(this);
   }

   return evalImpl(needs_deref, xsink);
}

int64 AbstractQoreNode::bigIntEvalImpl(ExceptionSink *xsink) const
{
   ReferenceHolder<AbstractQoreNode> rv(eval(xsink), xsink);
   return rv ? rv->getAsBigInt() : 0;
}

int AbstractQoreNode::integerEvalImpl(ExceptionSink *xsink) const
{
   ReferenceHolder<AbstractQoreNode> rv(eval(xsink), xsink);
   return rv ? rv->getAsInt() : 0;
}

bool AbstractQoreNode::boolEvalImpl(ExceptionSink *xsink) const
{
   ReferenceHolder<AbstractQoreNode> rv(eval(xsink), xsink);
   return rv ? rv->getAsBool() : 0;
}

double AbstractQoreNode::floatEvalImpl(ExceptionSink *xsink) const
{
   ReferenceHolder<AbstractQoreNode> rv(eval(xsink), xsink);
   return rv ? rv->getAsFloat() : 0;
}

int64 AbstractQoreNode::bigIntEval(ExceptionSink *xsink) const
{
   if (needs_eval_flag)
      return bigIntEvalImpl(xsink);
   return getAsBigInt();
}

int AbstractQoreNode::integerEval(ExceptionSink *xsink) const
{
   if (needs_eval_flag)
      return integerEvalImpl(xsink);
   return getAsInt();
}

bool AbstractQoreNode::boolEval(ExceptionSink *xsink) const
{
   if (needs_eval_flag)
      return boolEvalImpl(xsink);
   return getAsBool();
}

double AbstractQoreNode::floatEval(ExceptionSink *xsink) const
{
   if (needs_eval_flag)
      return floatEvalImpl(xsink);
   return getAsFloat();
}

bool AbstractQoreNode::getAsBool() const
{
   if (type == NT_BOOLEAN)
      return reinterpret_cast<const QoreBoolNode *>(this)->getValue();
   return getAsBoolImpl();
}

int AbstractQoreNode::getAsInt() const
{
   if (type == NT_INT)
      return reinterpret_cast<const QoreBigIntNode *>(this)->val;
   return getAsIntImpl();
}

int64 AbstractQoreNode::getAsBigInt() const
{
   if (type == NT_INT)
      return reinterpret_cast<const QoreBigIntNode *>(this)->val;
   return getAsBigIntImpl();
}

double AbstractQoreNode::getAsFloat() const
{
   if (type == NT_FLOAT)
      return reinterpret_cast<const QoreFloatNode *>(this)->f;
   return getAsFloatImpl();
}

// for getting relative time values or integer values
int getSecZeroInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsInt();
}

int64 getSecZeroBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsBigInt();
}

// for getting relative time values or integer values
int getSecMinusOneInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsInt();
}

int64 getSecMinusOneBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsBigInt();
}

int getMsZeroInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsInt();
}

int64 getMsZeroBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsBigInt();
}

// for getting relative time values or integer values
int getMsMinusOneInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsInt();
}

int64 getMsMinusOneBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsBigInt();
}

int getMicroSecZeroInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->getType() == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds() * 1000;

   return a->getAsInt();
}

static inline QoreListNode *crlr_list_copy(const QoreListNode *n, ExceptionSink *xsink)
{
   // if it's not an immediate list, then there can't be any
   // variable references in it at any level, so return copy
   if (!n->needs_eval()) {
      n->ref();
      return const_cast<QoreListNode *>(n);
   }

   // otherwise process each list element
   ReferenceHolder<QoreListNode> l(new QoreListNode(true), xsink);
   for (unsigned i = 0; i < n->size(); i++) {
      l->push(copy_and_resolve_lvar_refs(n->retrieve_entry(i), xsink));
      if (*xsink)
	 return 0;
   }
   return l.release();
}

static inline AbstractQoreNode *crlr_hash_copy(const QoreHashNode *n, ExceptionSink *xsink)
{
   // if it's not an immediate hash, then there can't be any
   // variable references in it at any level, so return copy
   if (!n->needs_eval())
      return n->refSelf();

   ReferenceHolder<QoreHashNode> h(new QoreHashNode(true), xsink);
   ConstHashIterator hi(n);
   while (hi.next()) {
      h->setKeyValue(hi.getKey(), copy_and_resolve_lvar_refs(hi.getValue(), xsink), xsink);
      if (*xsink)
	 return 0;
   }
   return h.release();
}

static inline AbstractQoreNode *crlr_tree_copy(const QoreTreeNode *n, ExceptionSink *xsink)
{
   return new QoreTreeNode(copy_and_resolve_lvar_refs(n->left, xsink), n->op,
			   n->right ? copy_and_resolve_lvar_refs(n->right, xsink) : 0);
}

static inline AbstractQoreNode *crlr_fcall_copy(const FunctionCallNode *n, ExceptionSink *xsink)
{
   QoreListNode *na = n->getArgs() ? crlr_list_copy(n->getArgs(), xsink) : 0;

   switch (n->getFunctionType())
   {
      case FC_USER:
	 return new FunctionCallNode(n->f.ufunc, na);
      case FC_BUILTIN:
	 return new FunctionCallNode(n->f.bfunc, na);
      case FC_SELF:
	 return new FunctionCallNode(n->f.sfunc->func, na);
      case FC_UNRESOLVED:
	 return new FunctionCallNode(strdup(n->f.c_str), na);
      case FC_IMPORTED:
	 return new FunctionCallNode(n->f.ifunc->pgm, n->f.ifunc->func, na);
      case FC_METHOD: {
	 FunctionCallNode *nn = new FunctionCallNode(strdup(n->f.c_str), na);
	 nn->ftype = FC_METHOD;
	 return nn;
      }
   }
   assert(false);
   return 0;
}

static inline AbstractQoreNode *call_ref_call_copy(const CallReferenceCallNode *n, ExceptionSink *xsink)
{
   ReferenceHolder<AbstractQoreNode> exp(copy_and_resolve_lvar_refs(n->getExp(), xsink), xsink);
   if (*xsink)
      return 0;

   QoreListNode *args = const_cast<QoreListNode *>(n->getArgs());
   if (args) {
      ReferenceHolder<QoreListNode> args_holder(crlr_list_copy(args, xsink), xsink);
      if (*xsink)
	 return 0;

      args = args_holder.release();
   }

   return new CallReferenceCallNode(exp.release(), args);
}

static inline AbstractQoreNode *eval_notnull(const AbstractQoreNode *n, ExceptionSink *xsink)
{
   n = n->eval(xsink);
   if (!xsink->isEvent() && !n)
      return nothing();
   return const_cast<AbstractQoreNode *>(n);
}

AbstractQoreNode *copy_and_resolve_lvar_refs(const AbstractQoreNode *n, ExceptionSink *xsink)
{
   if (!n) return 0;

   qore_type_t ntype = n->getType();

   if (ntype == NT_LIST)
      return crlr_list_copy(reinterpret_cast<const QoreListNode *>(n), xsink);

   if (ntype == NT_HASH)
      return crlr_hash_copy(reinterpret_cast<const QoreHashNode *>(n), xsink);

   if (ntype == NT_TREE)
      return crlr_tree_copy(reinterpret_cast<const QoreTreeNode *>(n), xsink);

   if (ntype == NT_FUNCTION_CALL)
      return crlr_fcall_copy(reinterpret_cast<const FunctionCallNode *>(n), xsink);

   // must make sure to return a value here or it could cause a segfault - parse expressions expect non-NULL values for the operands
   if (ntype == NT_FIND || ntype == NT_SELF_VARREF)
      return eval_notnull(n, xsink);

   if (ntype == NT_VARREF && reinterpret_cast<const VarRefNode *>(n)->type == VT_LOCAL)
      return eval_notnull(n, xsink);

   if (ntype == NT_FUNCREFCALL)
      return call_ref_call_copy(reinterpret_cast<const CallReferenceCallNode *>(n), xsink);

   return n->refSelf();
}

// get the value of the type in a string context, empty string for complex types (default implementation)
QoreString *AbstractQoreNode::getStringRepresentation(bool &del) const
{
   del = false;
   return NullString;
}

// empty default implementation
void AbstractQoreNode::getStringRepresentation(QoreString &str) const
{
}

// if del is true, then the returned DateTime * should be deleted, if false, then it should not
DateTime *AbstractQoreNode::getDateTimeRepresentation(bool &del) const
{
   del = false;
   return ZeroDate;
}

// assign date representation to a DateTime (no action for complex types = default implementation)
void AbstractQoreNode::getDateTimeRepresentation(DateTime &dt) const
{
   dt.setDate(0LL);
}

void SimpleQoreNode::deref()
{
   if (there_can_be_only_one) {
      assert(is_unique());
      return;
   }

   if (ROdereference())
      delete this;   
}

AbstractQoreNode *SimpleValueQoreNode::evalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

AbstractQoreNode *SimpleValueQoreNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

int64 SimpleValueQoreNode::bigIntEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

int SimpleValueQoreNode::integerEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

bool SimpleValueQoreNode::boolEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return false;
}

double SimpleValueQoreNode::floatEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0.0;
}

AbstractQoreNode *UniqueValueQoreNode::realCopy() const
{
   return const_cast<UniqueValueQoreNode *>(this);
}
