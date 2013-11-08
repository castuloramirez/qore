/*
 CallReferenceNode.cpp
 
 Qore Programming Language
 
 Copyright 2003 - 2013 David Nichols
 
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
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/qore_program_private.h>
#include <qore/intern/QoreNamespaceIntern.h>

#include <stdlib.h>
#include <string.h>

CallReferenceCallNode::CallReferenceCallNode(AbstractQoreNode *n_exp, QoreListNode *n_args) : ParseNode(NT_FUNCREFCALL), exp(n_exp), args(n_args) {
}

CallReferenceCallNode::~CallReferenceCallNode() {
   if (exp)
      exp->deref(0);
   if (args)
      args->deref(0);
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int CallReferenceCallNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("call reference call (0x%p)", this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *CallReferenceCallNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString;
   getAsString(*rv, foff, xsink);
   return rv;
}

// returns the type name as a c string
const char *CallReferenceCallNode::getTypeName() const {
   return "call reference call";
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *CallReferenceCallNode::evalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> lv(exp->eval(xsink), xsink);
   if (*xsink)
      return 0;

   ResolvedCallReferenceNode *r = dynamic_cast<ResolvedCallReferenceNode *>(*lv);
   if (!r) {
      xsink->raiseException("REFERENCE-CALL-ERROR", "expression does not evaluate to a call reference (evaluated to type '%s')", lv ? lv->getTypeName() : "NOTHING"); 
      return 0;
   }
   return r->exec(args, xsink);
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *CallReferenceCallNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return CallReferenceCallNode::evalImpl(xsink);
}

int64 CallReferenceCallNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(CallReferenceCallNode::evalImpl(xsink), xsink);
   return rv ? rv->getAsBigInt() : 0;
}

int CallReferenceCallNode::integerEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(CallReferenceCallNode::evalImpl(xsink), xsink);
   return rv ? rv->getAsInt() : 0;
}

bool CallReferenceCallNode::boolEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(CallReferenceCallNode::evalImpl(xsink), xsink);
   return rv ? rv->getAsBool() : 0;
}

double CallReferenceCallNode::floatEvalImpl(ExceptionSink *xsink) const {
   ReferenceHolder<AbstractQoreNode> rv(CallReferenceCallNode::evalImpl(xsink), xsink);
   return rv ? rv->getAsFloat() : 0;
}

AbstractQoreNode *CallReferenceCallNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   // call references calls can return any value
   typeInfo = 0;

   pflag &= ~(PF_RETURN_VALUE_IGNORED);

   const QoreTypeInfo *expTypeInfo = 0;
   if (exp) {
      exp = exp->parseInit(oflag, pflag, lvids, expTypeInfo);

      if (expTypeInfo->hasType() && !codeTypeInfo->parseAccepts(expTypeInfo)) {
	 // raise parse exception
	 QoreStringNode *desc = new QoreStringNode("invalid call; expression gives ");
	 expTypeInfo->getThisType(*desc);
	 desc->concat(", but a call reference or closure is required to make a call");
	 qore_program_private::makeParseException(getProgram(), "PARSE-TYPE-ERROR", desc);
      }
   }

   if (args) {
      bool needs_eval = args->needs_eval();

      // turn off PF_RETURN_VALUE_IGNORED
      pflag &= ~PF_RETURN_VALUE_IGNORED;
      
      ListIterator li(args);
      while (li.next()) {
	 AbstractQoreNode **n = li.getValuePtr();
	 if (*n) {
	    const QoreTypeInfo *argTypeInfo = 0;
	    (*n) = (*n)->parseInit(oflag, pflag, lvids, argTypeInfo);
	    
	    if (!needs_eval && (*n)->needs_eval()) {
	       args->setNeedsEval();
	       needs_eval = true;
	    }
	 }
      }
   }
   
   return this;
}

AbstractCallReferenceNode::AbstractCallReferenceNode(bool n_needs_eval, qore_type_t n_type) : AbstractQoreNode(n_type, false, n_needs_eval) {
}

AbstractCallReferenceNode::AbstractCallReferenceNode(bool n_needs_eval, bool n_there_can_be_only_one, qore_type_t n_type) : AbstractQoreNode(n_type, false, n_needs_eval, n_there_can_be_only_one) {
}

AbstractCallReferenceNode::~AbstractCallReferenceNode() {
}

// parse types should never be copied
AbstractQoreNode *AbstractCallReferenceNode::realCopy() const {
   assert(false);
   return 0;
}

bool AbstractCallReferenceNode::is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const {
   return is_equal_hard(v, xsink);
}

bool AbstractCallReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
   assert(false);
   return false;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int AbstractCallReferenceNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.sprintf("function reference (0x%p)", this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *AbstractCallReferenceNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

AbstractQoreNode *AbstractCallReferenceNode::evalImpl(ExceptionSink *xsink) const {
   assert(false);
   return 0;
}

AbstractQoreNode *AbstractCallReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   assert(false);
   return 0;
}

int64 AbstractCallReferenceNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   assert(false);
   return 0;
}

int AbstractCallReferenceNode::integerEvalImpl(ExceptionSink *xsink) const {
   assert(false);
   return 0;
}

bool AbstractCallReferenceNode::boolEvalImpl(ExceptionSink *xsink) const {
   assert(false);
   return false;
}

double AbstractCallReferenceNode::floatEvalImpl(ExceptionSink *xsink) const {
   assert(false);
   return 0.0;
}

// returns the type name as a c string
const char *AbstractCallReferenceNode::getTypeName() const {
   return getStaticTypeName();
}

bool AbstractCallReferenceNode::getAsBoolImpl() const {
   // check if we should do perl-style boolean evaluation
   if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL))
      return false;
   return true;
}

ParseObjectMethodReferenceNode::ParseObjectMethodReferenceNode(AbstractQoreNode *n_exp, char *n_method) : exp(n_exp), method(n_method), qc(0), m(0) {
}

ParseObjectMethodReferenceNode::~ParseObjectMethodReferenceNode() {
   if (exp)
      exp->deref(0);
   if (method)
      free(method);
}

// returns a RunTimeObjectMethodReference or NULL if there's an exception
AbstractQoreNode *ParseObjectMethodReferenceNode::evalImpl(ExceptionSink *xsink) const {
   // evaluate lvalue expression
   ReferenceHolder<AbstractQoreNode> lv(exp->eval(xsink), xsink);
   if (*xsink)
      return 0;

   QoreObject *o = dynamic_cast<QoreObject *>(*lv);
   if (!o) {
      xsink->raiseException("OBJECT-METHOD-REFERENCE-ERROR", "expression does not evaluate to an object");
      return 0;
   }

   // return class with method already found at parse time if known
   if (m) {
      const QoreClass *oc = o->getClass();
      if (oc == m->getClass() || oc == qc)
	 return new RunTimeResolvedMethodReferenceNode(o, m);
   }

   return new RunTimeObjectMethodReferenceNode(o, method);
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *ParseObjectMethodReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return ParseObjectMethodReferenceNode::evalImpl(xsink);
}

int64 ParseObjectMethodReferenceNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

int ParseObjectMethodReferenceNode::integerEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

bool ParseObjectMethodReferenceNode::boolEvalImpl(ExceptionSink *xsink) const {
   return false;
}

double ParseObjectMethodReferenceNode::floatEvalImpl(ExceptionSink *xsink) const {
   return 0.0;
}

AbstractQoreNode *ParseObjectMethodReferenceNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = callReferenceTypeInfo;
   if (exp) {
      const QoreTypeInfo *argTypeInfo = 0;
      exp = exp->parseInit(oflag, pflag, lvids, argTypeInfo);

      if (argTypeInfo->hasType()) {
	 if (!objectTypeInfo->parseAccepts(argTypeInfo)) {
	    // raise parse exception
	    QoreStringNode *desc = new QoreStringNode("invalid call; object expression gives ");
	    argTypeInfo->getThisType(*desc);
	    desc->concat(", but should resolve to an object to make a call with this syntax");
	    qore_program_private::makeParseException(getProgram(), "PARSE-TYPE-ERROR", desc);
	 }
	 else {
	    const QoreClass *n_qc = argTypeInfo->getUniqueReturnClass();
	    if (n_qc) {
	       m = const_cast<QoreClass *>(n_qc)->parseFindMethodTree(method);
	       if (!m)
		  parseException("PARSE-ERROR", "method %s::%s() cannot be found", n_qc->getName(), method);
	       else
		  qc = n_qc;
	    }
	 }
      }
   }
   return this;
}

// returns a RunTimeObjectMethodReferenceNode or NULL if there's an exception
AbstractQoreNode *ParseSelfMethodReferenceNode::evalImpl(ExceptionSink *xsink) const {
   return new RunTimeResolvedMethodReferenceNode(runtime_get_stack_object(), meth);
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *ParseSelfMethodReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return ParseSelfMethodReferenceNode::evalImpl(xsink);
}

int64 ParseSelfMethodReferenceNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

int ParseSelfMethodReferenceNode::integerEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

bool ParseSelfMethodReferenceNode::boolEvalImpl(ExceptionSink *xsink) const {
   return false;
}

double ParseSelfMethodReferenceNode::floatEvalImpl(ExceptionSink *xsink) const {
   return 0.0;
}

AbstractQoreNode *ParseSelfMethodReferenceNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = callReferenceTypeInfo;
   if (!oflag)
      parse_error("reference to object member '%s' out of a class member function definition", method);
   else {
      assert(method);
      meth = qore_class_private::parseResolveSelfMethod(*(getParseClass()), method);
      free(method);
      method = 0;
   }
   return this;
}

ParseScopedSelfMethodReferenceNode::ParseScopedSelfMethodReferenceNode(NamedScope *n_nscope) : nscope(n_nscope), method(0) {
}

ParseScopedSelfMethodReferenceNode::~ParseScopedSelfMethodReferenceNode() {
   delete nscope;
}

// returns a RunTimeObjectMethodReference or NULL if there's an exception
AbstractQoreNode *ParseScopedSelfMethodReferenceNode::evalImpl(ExceptionSink *xsink) const {
   return new RunTimeResolvedMethodReferenceNode(runtime_get_stack_object(), method);
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *ParseScopedSelfMethodReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return ParseScopedSelfMethodReferenceNode::evalImpl(xsink);
}

int64 ParseScopedSelfMethodReferenceNode::bigIntEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

int ParseScopedSelfMethodReferenceNode::integerEvalImpl(ExceptionSink *xsink) const {
   return 0;
}

bool ParseScopedSelfMethodReferenceNode::boolEvalImpl(ExceptionSink *xsink) const {
   return false;
}

double ParseScopedSelfMethodReferenceNode::floatEvalImpl(ExceptionSink *xsink) const {
   return 0.0;
}

AbstractQoreNode *ParseScopedSelfMethodReferenceNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = callReferenceTypeInfo;
   if (!oflag)
      parse_error("reference to object member '%s' out of a class member function definition", method);
   else {
      method = qore_class_private::parseResolveSelfMethod(*(getParseClass()), nscope);
      delete nscope;
      nscope = 0;
   }

   return this;
}

RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode(QoreObject *n_obj, const QoreMethod *n_method) : obj(n_obj), method(n_method) {
   printd(5, "RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode() this=%p obj=%p\n", this, obj);
   obj->tRef();
}

RunTimeResolvedMethodReferenceNode::~RunTimeResolvedMethodReferenceNode() {
   printd(5, "RunTimeResolvedMethodReferenceNode::~RunTimeResolvedMethodReferenceNode() this=%p obj=%p\n", this, obj);
   obj->tDeref();
}

AbstractQoreNode *RunTimeResolvedMethodReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   return qore_method_private::eval(*method, obj, args, xsink);
}

QoreProgram *RunTimeResolvedMethodReferenceNode::getProgram() const {
   return obj->getProgram();
}

bool RunTimeResolvedMethodReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
   const RunTimeResolvedMethodReferenceNode *vc = dynamic_cast<const RunTimeResolvedMethodReferenceNode *>(v);
   return vc && vc->obj == obj && vc->method == method;
}

RunTimeObjectMethodReferenceNode::RunTimeObjectMethodReferenceNode(QoreObject *n_obj, char *n_method) : obj(n_obj), method(strdup(n_method)) {
   printd(5, "RunTimeObjectMethodReferenceNode::RunTimeObjectMethodReferenceNode() this=%p obj=%p (method=%s)\n", this, obj, method);
   obj->tRef();
}

RunTimeObjectMethodReferenceNode::~RunTimeObjectMethodReferenceNode() {
   printd(5, "RunTimeObjectMethodReferenceNode::~RunTimeObjectMethodReferenceNode() this=%p obj=%p (method=%s)\n", this, obj, method);
   obj->tDeref();
   free(method);
}

AbstractQoreNode *RunTimeObjectMethodReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   return obj->evalMethod(method, args, xsink);
}

QoreProgram *RunTimeObjectMethodReferenceNode::getProgram() const {
   return obj->getProgram();
}

bool RunTimeObjectMethodReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
   const RunTimeObjectMethodReferenceNode *vc = dynamic_cast<const RunTimeObjectMethodReferenceNode *>(v);
   return vc && obj == vc->obj && !strcmp(vc->method, method);
}

UnresolvedProgramCallReferenceNode::UnresolvedProgramCallReferenceNode(char *n_str) : AbstractUnresolvedCallReferenceNode(false), str(n_str) {
}

UnresolvedProgramCallReferenceNode::~UnresolvedProgramCallReferenceNode() {
   free(str);
}

AbstractQoreNode *UnresolvedProgramCallReferenceNode::parseInit(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = callReferenceTypeInfo;
   return qore_root_ns_private::parseResolveCallReference(this);
}

AbstractQoreNode *UnresolvedCallReferenceNode::parseInit(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = callReferenceTypeInfo;

   // try to resolve a method call if bare references are allowed
   // and we are parsing in an object context
   if (parse_check_parse_option(PO_ALLOW_BARE_REFS) && oflag) {
      const QoreClass *qc = oflag->getTypeInfo()->getUniqueReturnClass();
      const QoreMethod *m = qore_class_private::parseFindSelfMethod(const_cast<QoreClass *>(qc), str);
      if (m) {
	 ParseSelfMethodReferenceNode *rv = new ParseSelfMethodReferenceNode(m);
	 delete this;
	 return rv;
      }
   }

   return qore_root_ns_private::parseResolveCallReference(this);
}

AbstractQoreNode *LocalStaticMethodCallReferenceNode::evalImpl(ExceptionSink *xsink) const {
   return new StaticMethodCallReferenceNode(method, ::getProgram());
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *LocalStaticMethodCallReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return new StaticMethodCallReferenceNode(method, ::getProgram());
}

AbstractQoreNode *LocalStaticMethodCallReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   return qore_method_private::eval(*method, 0, args, xsink);
}

bool LocalStaticMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {   
   const LocalStaticMethodCallReferenceNode *vc = dynamic_cast<const LocalStaticMethodCallReferenceNode *>(v);
   //printd(5, "LocalStaticMethodCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v, v ? v->getTypeName() : "n/a");
   return vc && method == vc->method;
}

AbstractQoreNode *LocalMethodCallReferenceNode::evalImpl(ExceptionSink *xsink) const {
   return new MethodCallReferenceNode(method, ::getProgram());
}

// evalImpl(): return value requires a deref(xsink) if not 0
AbstractQoreNode *LocalMethodCallReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return new MethodCallReferenceNode(method, ::getProgram());
}

AbstractQoreNode *LocalMethodCallReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   return qore_method_private::eval(*method, runtime_get_stack_object(), args, xsink);
}

bool LocalMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {   
   const LocalMethodCallReferenceNode *vc = dynamic_cast<const LocalMethodCallReferenceNode *>(v);
   //printd(5, "LocalMethodCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v, v ? v->getTypeName() : "n/a");
   return vc && method == vc->method;
}

StaticMethodCallReferenceNode::StaticMethodCallReferenceNode(const QoreMethod *n_method, QoreProgram *n_pgm) : LocalStaticMethodCallReferenceNode(n_method, false), pgm(n_pgm) {
   assert(pgm);
   //printd(5, "StaticMethodCallReferenceNode::StaticMethodCallReferenceNode() this=%p calling QoreProgram::depRef() pgm=%p\n", this, pgm);
   pgm->depRef();
}

bool StaticMethodCallReferenceNode::derefImpl(ExceptionSink *xsink) {
   //printd(5, "StaticMethodCallReferenceNode::deref() this=%p pgm=%p refs: %d -> %d\n", this, pgm, reference_count(), reference_count() - 1);
   pgm->depDeref(xsink);
   return true;
}

AbstractQoreNode *StaticMethodCallReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   // do not set pgm context here before evaluating args
   return qore_method_private::eval(*method, 0, args, xsink);
}

MethodCallReferenceNode::MethodCallReferenceNode(const QoreMethod *n_method, QoreProgram *n_pgm) : LocalMethodCallReferenceNode(n_method, false), pgm(n_pgm) {
   assert(pgm);
   //printd(5, "MethodCallReferenceNode::MethodCallReferenceNode() this=%p calling QoreProgram::depRef() pgm=%p\n", this, pgm);
   pgm->depRef();
}

bool MethodCallReferenceNode::derefImpl(ExceptionSink *xsink) {
   //printd(5, "MethodCallReferenceNode::deref() this=%p pgm=%p refs: %d -> %d\n", this, pgm, reference_count(), reference_count() - 1);
   pgm->depDeref(xsink);
   return true;
}

AbstractQoreNode *MethodCallReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   ProgramThreadCountContextHelper tch(xsink, pgm, true);
   if (*xsink)
      return 0;
   return qore_method_private::eval(*method, runtime_get_stack_object(), args, xsink);
}

UnresolvedStaticMethodCallReferenceNode::UnresolvedStaticMethodCallReferenceNode(NamedScope *n_scope) : AbstractUnresolvedCallReferenceNode(false), scope(n_scope) {
   //printd(0, "UnresolvedStaticMethodCallReferenceNode::UnresolvedStaticMethodCallReferenceNode(%s) this=%p\n", n_scope->ostr, this);
}

UnresolvedStaticMethodCallReferenceNode::~UnresolvedStaticMethodCallReferenceNode() {
   //printd(0, "UnresolvedStaticMethodCallReferenceNode::~UnresolvedStaticMethodCallReferenceNode() this=%p\n", this);
   delete scope;
}

AbstractQoreNode *UnresolvedStaticMethodCallReferenceNode::parseInit(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   typeInfo = callReferenceTypeInfo;

   QoreClass *qc = qore_root_ns_private::parseFindScopedClassWithMethod(*scope, false);
   if (!qc) {
      // see if this is a function call to a function defined in a namespace
      const QoreFunction* f = qore_root_ns_private::parseResolveFunction(*scope);
      if (f) {
	 LocalFunctionCallReferenceNode* fr = new LocalFunctionCallReferenceNode(f);
         deref();
         return fr->parseInit(oflag, pflag, lvids, typeInfo);
      }
      parse_error("reference to undefined class '%s' in '%s()'", scope->get(scope->size() - 2), scope->ostr);
      return this;
   }

   const QoreMethod *qm = 0;
   // try to find a pointer to a non-static method if parsing in the class' context
   // and bare references are enabled
   if (oflag && parse_check_parse_option(PO_ALLOW_BARE_REFS) && oflag->getTypeInfo()->getUniqueReturnClass()->parseCheckHierarchy(qc)) {
      qm = qc->parseFindMethodTree(scope->getIdentifier());
      assert(!qm || !qm->isStatic());
   }
   if (!qm) {
      qm = qc->parseFindStaticMethodTree(scope->getIdentifier());
      if (!qm) {
	 parseException("INVALID-METHOD", "class '%s' has no static method '%s'", qc->getName(), scope->getIdentifier());
	 return this;
      }
      assert(qm->isStatic());
   }

   // check class capabilities against parse options
   if (qore_program_private::parseAddDomain(getProgram(), qc->getDomain())) {
      parseException("class '%s' implements capabilities that are not allowed by current parse options", qc->getName());
      return this;
   }

   AbstractQoreNode *rv = qm->isStatic() ? new LocalStaticMethodCallReferenceNode(qm) : new LocalMethodCallReferenceNode(qm);
   deref();
   return rv;
}

LocalFunctionCallReferenceNode::LocalFunctionCallReferenceNode(const QoreFunction *n_uf, bool n_needs_eval) : ResolvedCallReferenceNode(n_needs_eval), uf(n_uf) {
}

LocalFunctionCallReferenceNode::LocalFunctionCallReferenceNode(const QoreFunction *n_uf) : ResolvedCallReferenceNode(true), uf(n_uf) {
}

AbstractQoreNode *LocalFunctionCallReferenceNode::evalImpl(ExceptionSink *xsink) const {
   return new FunctionCallReferenceNode(uf, ::getProgram());
}

AbstractQoreNode *LocalFunctionCallReferenceNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = true;
   return new FunctionCallReferenceNode(uf, ::getProgram());
}

AbstractQoreNode *LocalFunctionCallReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   return uf->evalFunction(0, args, 0, xsink);
}

bool LocalFunctionCallReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {   
   const LocalFunctionCallReferenceNode *vc = dynamic_cast<const LocalFunctionCallReferenceNode *>(v);
   //printd(0, "LocalFunctionCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v, v ? v->getTypeName() : "n/a");
   return vc && uf == vc->uf;
}

bool FunctionCallReferenceNode::derefImpl(ExceptionSink *xsink) {
   //printd(5, "FunctionCallReferenceNode::deref() this=%p pgm=%p refs: %d -> %d\n", this, pgm, reference_count(), reference_count() - 1);
   //pgm->depDeref(xsink);
   pgm->deref(xsink);
   return true;
}

AbstractQoreNode *FunctionCallReferenceNode::exec(const QoreListNode *args, ExceptionSink *xsink) const {
   return uf->evalFunction(0, args, pgm, xsink);
}

ResolvedCallReferenceNode::ResolvedCallReferenceNode(bool n_needs_eval, qore_type_t n_type) : AbstractCallReferenceNode(n_needs_eval, n_type) {
}

QoreProgram *ResolvedCallReferenceNode::getProgram() const {
   return 0;
}
