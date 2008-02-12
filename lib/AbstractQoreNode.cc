/*
  AbstractQoreNode.cc

  Qore Programming Language

  Copyright (C) 2003, 2004, 2005, 2006, 2007 David Nichols

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

AbstractQoreNode::AbstractQoreNode(const QoreType *t) : type(t)
{
#if TRACK_REFS
   printd(5, "AbstractQoreNode::ref() %08p type=%s (0->1)\n", this, getTypeName());
#endif
}

AbstractQoreNode::~AbstractQoreNode()
{
#if 0
   printd(5, "AbstractQoreNode::~AbstractQoreNode() type=%s\n", getTypeName());
#endif
}

void AbstractQoreNode::ref() const
{
#ifdef DEBUG
#if TRACK_REFS
   if (type == NT_OBJECT) {
      const QoreObject *o = reinterpret_cast<const QoreObject *>(this);
      printd(5, "AbstractQoreNode::ref() %08p type=%s (%d->%d) object=%08p, class=%s\n", this, getTypeName(), references, references + 1, o, o->getClass()->getName());
   }
   else
      printd(5, "AbstractQoreNode::ref() %08p type=%s (%d->%d)\n", this, getTypeName(), references, references + 1);
#endif
#endif
   ROreference();
}

AbstractQoreNode *AbstractQoreNode::refSelf() const
{
   ref();
   return const_cast<AbstractQoreNode *>(this);
}

void AbstractQoreNode::deref(ExceptionSink *xsink)
{
   //tracein("AbstractQoreNode::deref()");
#ifdef DEBUG
#if TRACK_REFS
   if (type == NT_STRING) printd(5, "AbstractQoreNode::deref() %08p (%d->%d) string='%s'\n", this, references, references - 1, ((QoreStringNode *)this)->getBuffer());
   else
      printd(5, "AbstractQoreNode::deref() %08p type=%s (%d->%d)\n", this, getTypeName(), references, references - 1);

#endif
   if (references > 51200 || references < 0)
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

   if (ROdereference())
   {
      // now delete this QoreNode
      delete this;
   }

   //traceout("AbstractQoreNode::deref()");
}

bool AbstractQoreNode::needs_eval() const
{
   return false;
}

bool AbstractQoreNode::is_value() const
{
   return true;
}

/*
  AbstractQoreNode::eval(): return value requires a dereference
 */
AbstractQoreNode *AbstractQoreNode::eval(ExceptionSink *xsink) const
{
   return refSelf();
}

/*
 AbstractQoreNode::eval(): return value requires a dereference if needs_deref is true
 */
AbstractQoreNode *AbstractQoreNode::eval(bool &needs_deref, ExceptionSink *xsink) const
{
   needs_deref = false;
   return const_cast<AbstractQoreNode *>(this);
}

int64 AbstractQoreNode::bigIntEval(ExceptionSink *xsink) const
{
   return getAsBigInt();
}

int AbstractQoreNode::integerEval(ExceptionSink *xsink) const
{
   return getAsInt();
}

bool AbstractQoreNode::boolEval(ExceptionSink *xsink) const
{
   return getAsBool();
}

double AbstractQoreNode::floatEval(ExceptionSink *xsink) const
{
   return getAsFloat();
}

bool AbstractQoreNode::getAsBool() const
{
   if (type == NT_BOOLEAN)
      return reinterpret_cast<const QoreBoolNode *>(this)->b;
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

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsInt();
}

int64 getSecZeroBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsBigInt();
}

// for getting relative time values or integer values
int getSecMinusOneInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsInt();
}

int64 getSecMinusOneBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeSeconds();

   return a->getAsBigInt();
}

int getMsZeroInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsInt();
}

int64 getMsZeroBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsBigInt();
}

// for getting relative time values or integer values
int getMsMinusOneInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsInt();
}

int64 getMsMinusOneBigInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return -1;

   if (a->type == NT_DATE)
      return reinterpret_cast<const DateTimeNode *>(a)->getRelativeMilliseconds();

   return a->getAsBigInt();
}

int getMicroSecZeroInt(const AbstractQoreNode *a)
{
   if (is_nothing(a))
      return 0;

   if (a->type == NT_DATE)
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
   QoreListNode *l = new QoreListNode(true);
   for (int i = 0; i < n->size(); i++)
      l->push(copy_and_resolve_lvar_refs(n->retrieve_entry(i), xsink));
   return l;
}

static inline AbstractQoreNode *crlr_hash_copy(const QoreHashNode *n, ExceptionSink *xsink)
{
   // if it's not an immediate hash, then there can't be any
   // variable references in it at any level, so return copy
   if (!n->needsEval())
      return n->refSelf();

   QoreHashNode *h = new QoreHashNode(1);
   ConstHashIterator hi(n);
   while (hi.next())
      h->setKeyValue(hi.getKey(), copy_and_resolve_lvar_refs(hi.getValue(), xsink), xsink);
   return h;
}

static inline AbstractQoreNode *crlr_tree_copy(const QoreTreeNode *n, ExceptionSink *xsink)
{
   return new QoreTreeNode(copy_and_resolve_lvar_refs(n->left, xsink), n->op,
			   n->right ? copy_and_resolve_lvar_refs(n->right, xsink) : NULL);
}

static inline AbstractQoreNode *crlr_fcall_copy(const FunctionCallNode *n, ExceptionSink *xsink)
{
   QoreListNode *na = n->args ? crlr_list_copy(n->args, xsink) : 0;

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

   const QoreType *ntype = n->type;

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

// returns the data type
const QoreType *AbstractQoreNode::getType() const
{
   return type;
}

const char *AbstractQoreNode::getTypeName() const
{
   return type->getName();
}

SimpleQoreNode::SimpleQoreNode(const QoreType *t) : AbstractQoreNode(t)
{
}

void SimpleQoreNode::deref()
{
   if (ROdereference())
      delete this;   
}
