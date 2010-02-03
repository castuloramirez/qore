/*
  FunctionCallNode.cc

  Qore Programming Language

  Copyright 2003 - 2010 David Nichols

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

int FunctionCallBase::parseArgsFindVariant(LocalVar *oflag, int pflag, AbstractQoreFunction *func) {
   // number of local variables declared in arguments
   int lvids = 0;

   // turn off reference ok flag
   pflag &= ~PF_REFERENCE_OK;

   // number of arguments in call
   unsigned num_args = args ? args->size() : 0;

   // argument type list
   const QoreTypeInfo *argTypeInfo[num_args];

   // initialize arguments and setup argument type list (argTypeInfo)
   if (num_args) {
      // do arguments need to be evaluated?
      bool needs_eval = args->needs_eval();
      
      // loop through all args
      for (unsigned i = 0; i < num_args; ++i) {
	 AbstractQoreNode **n = args->get_entry_ptr(i);
	 assert(*n);
	 argTypeInfo[i] = 0;
	 //printd(5, "FunctionCallBase::parseArgsFindVariant() this=%p (%s) oflag=%p pflag=%d func=%p i=%d/%d arg=%p (%d %s)\n", this, func ? func->getName() : "n/a", oflag, pflag, func, i, num_args, *n, (*n)->getType(), (*n)->getTypeName());
	 if ((*n)->getType() == NT_REFERENCE)
	    (*n) = (*n)->parseInit(oflag, pflag | PF_REFERENCE_OK, lvids, argTypeInfo[i]);
	 else
	    (*n) = (*n)->parseInit(oflag, pflag, lvids, argTypeInfo[i]);
	 if (!needs_eval && (*n)->needs_eval()) {
	    args->setNeedsEval();
	    needs_eval = true;
	 }
      }
   }
   
   // find variant
   variant = func ? func->parseFindVariant(num_args, argTypeInfo) : 0;

   if (variant && variant->getFunctionality() & getProgram()->getParseOptions()) {
      // func will always be non-zero with builtin functions
      parse_error("parse options do not allow access to builtin function '%s()'", func->getName());
      return 0;
   }

   return lvids;
}
 
// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *AbstractFunctionCallNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return evalImpl(xsink);
}

int64 AbstractFunctionCallNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsBigInt() : 0;
}

int AbstractFunctionCallNode::integerEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsInt() : 0;
}

bool AbstractFunctionCallNode::boolEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsBool() : 0;
}

double AbstractFunctionCallNode::floatEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(evalImpl(xsink), xsink);
   return rv ? rv->getAsFloat() : 0;
}

AbstractQoreNode *SelfFunctionCallNode::evalImpl(ExceptionSink *xsink) const {
   QoreObject *self = getStackObject();
   
   //printd(0, "SelfFunctionCallNode::evalImpl() this=%p self=%p func=%p (name=%s ns=%s)\n", this, self, func, name ? name : "(null)", ns ? ns->ostr : "(null)");

   if (func)
      return self->evalMethod(*func, args, xsink);
   // otherwise exec copy method
   return self->getClass()->execCopy(self, xsink);
}

// called at parse time
AbstractQoreNode *SelfFunctionCallNode::parseInit(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   if (!oflag) {
      parse_error("cannot call member function '%s' out of an object member function definition", getName());
      return this;
   }

   assert(name || ns);

#ifdef DEBUG
   if (ns)
      printd(5, "SelfFunctionCallNode::parseInit() this=%p resolving base class call '%s'\n", this, ns->ostr);
   else 
      printd(5, "SelfFunctionCallNode::parseInit() this=%p resolving '%s'\n", this, name ? name : "(null)");
   assert(!func);
#endif
   if (name) {
      // copy method calls will be recognized by name = 0
      if (!strcmp(name, "copy")) {
	 free(name);
	 name = 0;
	 printd(5, "SelfFunctionCallNode::parseInit() this=%p resolved to copy constructor\n", this);
      }
      else
	 func = getParseClass()->parseResolveSelfMethod(name);
   }
   else
      func = getParseClass()->parseResolveSelfMethod(ns);

   lvids += parseArgsFindVariant(oflag, pflag, func ? func->getFunction() : 0);

   if (variant)
      returnTypeInfo = variant->getReturnTypeInfo();
   else if (func)
      returnTypeInfo = func->getUniqueReturnTypeInfo();
   else
      returnTypeInfo = 0;

   if (func) {
      printd(5, "SelfFunctionCallNode::parseInit() this=%p resolved '%s' to %p\n", this, func->getName(), func);
      if (name) {
	 free(name);
	 name = 0;
      }
      else if (ns) {
	 delete ns;
	 ns = 0;
      }

   }

   return this;
}

int SelfFunctionCallNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("in-object method call (0x%p) to %s::%s()", this, func->getClass()->getName(), func->getName());
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *SelfFunctionCallNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

AbstractQoreNode *SelfFunctionCallNode::makeReferenceNodeAndDeref() {
   AbstractQoreNode *rv;
   if (name)
      rv = new ParseSelfMethodReferenceNode(takeName());
   else {
      assert(ns);
      rv = new ParseScopedSelfMethodReferenceNode(takeNScope());
   }
   deref();
   return rv;
}

// makes a "new" operator call from a function call
AbstractQoreNode *FunctionCallNode::parseMakeNewObject() {
   assert(c_str);
   ScopedObjectCallNode *rv = new ScopedObjectCallNode(new NamedScope(c_str), args);
   c_str = 0;
   args = 0;
   return rv;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int FunctionCallNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("function call to '%s()' (0x%p)", getName(), this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *FunctionCallNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

// eval(): return value requires a deref(xsink)
AbstractQoreNode *FunctionCallNode::evalImpl(ExceptionSink *xsink) const {
   // if pgm is 0, then ProgramContextHelper does nothing
   ProgramContextHelper(pgm, xsink);
   return func->evalFunction(variant, args, xsink);
}

AbstractQoreNode *FunctionCallNode::parseInit(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo) {
   assert(!func);
   assert(c_str);

   // resolves the function and assigns pgm for imported code
   func = ::getProgram()->resolveFunction(c_str, pgm);
   free(c_str);
   c_str = 0;
   if (!func)
      return this;

   lvids += parseArgsFindVariant(oflag, pflag, const_cast<AbstractQoreFunction *>(func));

   // check variant functionality
   if (variant) {
      returnTypeInfo = variant->parseGetReturnTypeInfo();
   }
   else {
      returnTypeInfo = func->parseGetUniqueReturnTypeInfo();
   }

   return this;
}
