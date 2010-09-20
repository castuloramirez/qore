/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreTypeInfo.h

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

#ifndef _QORE_QORETYPEINFO_H

#define _QORE_QORETYPEINFO_H

#include <map>
#include <vector>

#define NO_TYPE_INFO "<no type info>"

// adds external types to global type map
DLLLOCAL void add_to_type_map(qore_type_t t, const QoreTypeInfo *typeInfo);
DLLLOCAL bool builtinTypeHasDefaultValue(qore_type_t t);
// returns the default value for any type >= 0 and < NT_OBJECT
DLLLOCAL AbstractQoreNode *getDefaultValueForBuiltinValueType(qore_type_t t);

static inline void concatClass(std::string &str, const char *cn) {
   str.append("<class: ");
   str.append(cn);
   str.push_back('>');
}

/*
 * if input_filter is true, then 
   + returns_mult must be false xxx <- REMOVE THIS RESTRICTION
   + accepts_mult must be true
 * if accepts_mult is false, then qc and qt apply to the type accepted
 * if returns_mult is false, then qc and qt apply to the type returned
 * if both accepts_mult and returns_mult are true, then qc and qt have no relevance to the type
 * if reverse_logic is true, then:
   + accepts_mult, returns_mult, and has_defval must be false
   + qc must be 0
   + only QTI_AMBIGUOUS and QTI_NOT_EQUAL are returned for matches
 * in a type list:
   + no entry may be NULL or have qt = NT_ALL
   + all entries must be different types
 * if exact_return is true then returns_mult must be false
 */

class QoreTypeInfo {
   friend class OrNothingTypeInfo;

protected:
   // class pointer
   const QoreClass *qc;
   // basic type
   qore_type_t qt : 11;
   // true if type indicates more than one return type can be returned
   bool returns_mult : 1;
   // true if type accepts multiple types
   bool accepts_mult : 1;
   // true if multiple types accepted on input that produce an output type
   bool input_filter : 1;
   // true if type has a subtype
   bool has_subtype : 1;
   // true if has a custom name
   bool has_name : 1;
   // true if the type has a default value implementation function
   bool has_defval : 1;
   // true if the acceptance or return logic should be reversed with simple types (not lists)
   bool reverse_logic : 1;
   // true if the type is an implementation of QoreBigIntNode (for ints and enums)
   bool is_int : 1;
   // true if the single return type makes an exact match or ambiguous on input
   bool exact_return : 1;
   // if true then any type with is_int sets matches NT_INT ambigously
   bool ambiguous_int_match : 1;

   DLLLOCAL qore_type_result_e parseReturnsType(qore_type_t t, bool n_is_int) const {
      if (!hasType())
         return QTI_AMBIGUOUS;

      if (returns_mult)
	 return parseReturnsTypeMult(t, n_is_int);

      return matchTypeIntern(t, n_is_int);
   }

   DLLLOCAL qore_type_result_e parseReturnsClass(const QoreClass *n_qc) const {
      if (!hasType())
         return QTI_AMBIGUOUS;

      if (returns_mult)
	 return parseReturnsClassMult(n_qc);

      return matchClassIntern(n_qc);
   }
   
   DLLLOCAL qore_type_result_e parseAcceptsType(qore_type_t t, bool n_is_int) const { 
      if (!hasType())
         return QTI_AMBIGUOUS;

      if (accepts_mult)
	 return parseAcceptsTypeMult(t, n_is_int);

      return matchTypeIntern(t, n_is_int);
   }

   DLLLOCAL qore_type_result_e runtimeAcceptsClass(const QoreClass *n_qc) const {
      if (!hasType())
         return QTI_AMBIGUOUS;

      if (accepts_mult)
	 return runtimeAcceptsClassMult(n_qc);

      return runtimeMatchClassIntern(n_qc);
   }

   DLLLOCAL qore_type_result_e parseAcceptsClass(const QoreClass *n_qc) const {
      if (!hasType())
         return QTI_AMBIGUOUS;

      if (accepts_mult)
	 return parseAcceptsClassMult(n_qc);

      return matchClassIntern(n_qc);
   }

   DLLLOCAL qore_type_result_e parseReturnsTypeMult(qore_type_t t, bool n_is_int) const {
      const type_vec_t &rt = getReturnTypeList();

      for (type_vec_t::const_iterator i = rt.begin(), e = rt.end(); i != e; ++i) {
	 if ((*i)->parseReturnsType(t, n_is_int))
	    return QTI_AMBIGUOUS;
      }

      // now check fundamental type
      return matchTypeIntern(t, n_is_int);
   }

   DLLLOCAL qore_type_result_e parseReturnsClassMult(const QoreClass *n_qc) const {
      const type_vec_t &rt = getReturnTypeList();

      for (type_vec_t::const_iterator i = rt.begin(), e = rt.end(); i != e; ++i) {
	 if ((*i)->parseReturnsClass(n_qc))
	    return QTI_AMBIGUOUS;
      }

      // now check fundamental type
      return matchClassIntern(n_qc);
   }

   DLLLOCAL qore_type_result_e parseAcceptsTypeMult(qore_type_t t, bool n_is_int) const {
      if (!returns_mult) {
         qore_type_result_e rc = matchTypeIntern(t, n_is_int);
         if (rc)
            return rc;
      }

      const type_vec_t &at = getAcceptTypeList();

      for (type_vec_t::const_iterator i = at.begin(), e = at.end(); i != e; ++i) {
	 if ((*i)->parseAcceptsType(t, n_is_int))
	    return QTI_AMBIGUOUS;
      }

      // now check fundamental type
      return matchTypeIntern(t, n_is_int);
   }

   DLLLOCAL qore_type_result_e parseAcceptsClassMult(const QoreClass *n_qc) const {
      if (!returns_mult && qc && qc->getID() == n_qc->getID())
         return exact_return ? QTI_IDENT : QTI_AMBIGUOUS;

      const type_vec_t &at = getAcceptTypeList();

      for (type_vec_t::const_iterator i = at.begin(), e = at.end(); i != e; ++i) {
	 if ((*i)->parseAcceptsClass(n_qc))
	    return QTI_AMBIGUOUS;
      }

      // now check fundamental type
      return matchClassIntern(n_qc);
   }

   DLLLOCAL qore_type_result_e runtimeAcceptsClassMult(const QoreClass *n_qc) const {
      if (!returns_mult && qc && qc->getID() == n_qc->getID())
         return exact_return ? QTI_IDENT : QTI_AMBIGUOUS;

      const type_vec_t &at = getAcceptTypeList();

      for (type_vec_t::const_iterator i = at.begin(), e = at.end(); i != e; ++i) {
	 if ((*i)->runtimeAcceptsClass(n_qc))
	    return QTI_AMBIGUOUS;
      }

      // now check fundamental type
      return runtimeMatchClassIntern(n_qc);
   }

   DLLLOCAL qore_type_result_e parseAcceptsBasic(const QoreTypeInfo *typeInfo) const {
      if (typeInfo->reverse_logic)
         return parseAcceptsType(typeInfo->qt, typeInfo->is_int) ? QTI_NOT_EQUAL : QTI_AMBIGUOUS;

      return typeInfo->qc ? parseAcceptsClass(typeInfo->qc) : parseAcceptsType(typeInfo->qt, typeInfo->is_int);
   }

   // see if any of of the types we accept match any of the types that can be returned by typeInfo
   DLLLOCAL qore_type_result_e parseAcceptsMult(const QoreTypeInfo *typeInfo) const {
      assert(accepts_mult);
      assert(typeInfo->returns_mult);

      const type_vec_t &at = getAcceptTypeList();
      const type_vec_t &rt = typeInfo->getReturnTypeList();

      for (type_vec_t::const_iterator i = at.begin(), e = at.end(); i != e; ++i) {
	 for (type_vec_t::const_iterator j = rt.begin(), je = rt.end(); j != je; ++j) {
	    if ((*i)->parseAccepts(*j))
	       return QTI_AMBIGUOUS;
	 }
      }

      // now check fundamental type
      return parseAcceptsBasic(typeInfo) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
   }

   DLLLOCAL qore_type_result_e matchTypeIntern(qore_type_t t, bool n_is_int) const {
      if (qt == NT_ALL || t == NT_ALL)
	 return QTI_AMBIGUOUS;

      if (reverse_logic)
         return qt == t ? QTI_NOT_EQUAL : QTI_AMBIGUOUS;

      if (qt == t)
         return exact_return ? QTI_IDENT : QTI_AMBIGUOUS;

      // if the type to compare is equivalent to int
      if (n_is_int) {
         if (is_int)
            return QTI_AMBIGUOUS;

         if (qt == NT_INT)
            return ambiguous_int_match ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
      }

      return QTI_NOT_EQUAL;
   }

   DLLLOCAL qore_type_result_e matchClassIntern(const QoreClass *n_qc) const {
      if (qt == NT_ALL)
	 return QTI_AMBIGUOUS;

      if (qt != NT_OBJECT)
	 return QTI_NOT_EQUAL;

      if (!qc)
	 return QTI_AMBIGUOUS;

      if (qc->getID() == n_qc->getID())
	 return exact_return ? QTI_IDENT : QTI_AMBIGUOUS;

      return parseCheckCompatibleClass(qc, n_qc) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
   }

   DLLLOCAL qore_type_result_e runtimeMatchClassIntern(const QoreClass *n_qc) const {
      if (qt == NT_ALL)
	 return QTI_AMBIGUOUS;

      if (qt != NT_OBJECT)
	 return QTI_NOT_EQUAL;

      if (!qc)
	 return QTI_AMBIGUOUS;

      if (qc->getID() == n_qc->getID())
	 return exact_return ? QTI_IDENT : QTI_AMBIGUOUS;

      bool priv;
      if (!n_qc->getClass(qc->getID(), priv))
         return QTI_NOT_EQUAL;

      if (!priv)
         return QTI_AMBIGUOUS;

      return runtimeCheckPrivateClassAccess(qc) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
   }

   DLLLOCAL int doTypeException(int param_num, const char *param_name, const AbstractQoreNode *n, ExceptionSink *xsink) const {
      // xsink may be null in case parse exceptions have been disabled in the QoreProgram object
      // for example if there was a "requires" error
      if (!xsink)
	 return -1;

      QoreStringNode *desc = new QoreStringNode;
      QoreTypeInfo::ptext(*desc, param_num, param_name);
      desc->concat("expects ");
      getThisType(*desc);
      desc->concat(", but got ");
      getNodeType(*desc, n);
      desc->concat(" instead");
      xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
      return -1;
   }

   DLLLOCAL int doPrivateClassException(int param_num, const char *param_name, const AbstractQoreNode *n, ExceptionSink *xsink) const {
      // xsink may be null in case that parse exceptions have been disabled in the QoreProgram object
      // for example if there was a "requires" error
      if (!xsink)
	 return -1;

      QoreStringNode *desc = new QoreStringNode;
      QoreTypeInfo::ptext(*desc, param_num, param_name);
      desc->concat("expects ");
      getThisType(*desc);
      desc->concat(", but got an object where this class is privately inherited instead");
      xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
      return -1;
   }

   DLLLOCAL int doObjectTypeException(const char *param_name, const AbstractQoreNode *n, ExceptionSink *xsink) const {
      assert(xsink);
      QoreStringNode *desc = new QoreStringNode;
      desc->sprintf("member '$.%s' expects ", param_name);
      getThisType(*desc);
      desc->concat(", but got ");
      getNodeType(*desc, n);
      desc->concat(" instead");
      xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
      return -1;
   }

   DLLLOCAL int doObjectPrivateClassException(const char *param_name, const AbstractQoreNode *n, ExceptionSink *xsink) const {
      assert(xsink);
      QoreStringNode *desc = new QoreStringNode;
      desc->sprintf("member '$.%s' expects ", param_name);
      getThisType(*desc);
      desc->concat(", but got an object where this class is privately inherited instead");
      xsink->raiseException("RUNTIME-TYPE-ERROR", desc);
      return -1;
   }

   // returns -1 for error encountered, 0 for OK
   // can only be called with accepts_mult is false
   DLLLOCAL int runtimeAcceptInputIntern(bool &priv_error, AbstractQoreNode *n) const;

   // returns -1 for error encountered, 0 for OK
   DLLLOCAL int acceptInputDefault(bool &priv_error, AbstractQoreNode *n) const;

   DLLLOCAL AbstractQoreNode *acceptInputIntern(bool obj, int param_num, const char *param_name, AbstractQoreNode *n, ExceptionSink *xsink) const {
      if (!input_filter) {
         bool priv_error = false;
         if (acceptInputDefault(priv_error, n))
            doAcceptError(priv_error, obj, param_num, param_name, n, xsink);
         return n;
      }

      // first check if input matches default type
      bool priv_error = false;
      if (!runtimeAcceptInputIntern(priv_error, n))
         return n;

      if (!acceptInputImpl(n, xsink))
         if (!*xsink)
            doAcceptError(false, obj, param_num, param_name, n, xsink);

      return n;
   }

   DLLLOCAL bool isTypeIdenticalIntern(const QoreTypeInfo *typeInfo) const {
      if (qt != typeInfo->qt)
	 return false;

      // both types are identical
      if (qt != NT_OBJECT)
	 return true;

      if (qc) {
	 if (!typeInfo->qc)
	    return false;
	 return qc->getID() == typeInfo->qc->getID();
      }
      return !typeInfo->qc;
   }

   // must be reimplemented in subclasses if input_filter is true
   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      assert(false);
      return false;
   }

   // must be reimplemented in subclasses if has_defval is true
   DLLLOCAL virtual AbstractQoreNode *getDefaultValueImpl() const {
      assert(false);
      return 0;
   }

   // must be reimplemented in subclasses if returns_mult is true
   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      assert(false);
      return *((type_vec_t*)0);
   }

   // must be reimplemented in subclasses if accepts_mult is true
   DLLLOCAL virtual const type_vec_t &getAcceptTypeList() const {
      assert(false);
      return *((type_vec_t*)0);
   }

   // must be reimplemented in subclasses if has_name is true
   DLLLOCAL virtual const char *getNameImpl() const {
      assert(false);
      return 0;
   }

   DLLLOCAL static void getNodeType(QoreString &str, const AbstractQoreNode *n) {
      if (is_nothing(n)) {
	 str.concat("no value");
	 return;
      }
      if (n->getType() != NT_OBJECT) {
	 str.sprintf("type '%s'", n->getTypeName());
	 return;
      }
      str.sprintf("an object of class '%s'", reinterpret_cast<const QoreObject *>(n)->getClassName());
   }

   DLLLOCAL static void ptext(QoreString &str, int param_num, const char *param_name) {
      if (!param_num && param_name && param_name[0] == '<') {
         str.concat(param_name);
         str.concat(' ');
         return;
      }
      if (param_name && param_name[0] == '<') {
         str.concat(param_name);
         str.concat(' ');
      }
      str.concat("parameter ");
      if (param_num) {
         str.sprintf("%d ", param_num);
         if (param_name && param_name[0] != '<')
            str.sprintf("('$%s') ", param_name);
      }
      else
         str.sprintf("'$%s' ", param_name);
   }

   DLLLOCAL QoreTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_returns_mult,
                         bool n_accepts_mult, bool n_input_filter, bool n_has_subtype,
                         bool n_has_name, bool n_has_defval, bool n_reverse_logic, 
                         bool n_is_int, bool n_exact_return) : 
      qc(n_qc), qt(n_qt), returns_mult(n_returns_mult), accepts_mult(n_accepts_mult), input_filter(n_input_filter), 
      has_subtype(n_has_subtype), has_name(n_has_name), has_defval(n_has_defval), reverse_logic(n_reverse_logic),
      is_int(n_is_int), exact_return(n_exact_return), ambiguous_int_match(false) {
      assert(!reverse_logic || (!returns_mult && !accepts_mult && !input_filter && !has_defval && !qc));
      assert(!is_int || !qc);
      assert(!(exact_return && returns_mult));
   }

public:
   DLLLOCAL QoreTypeInfo() : qc(0), qt(NT_ALL), returns_mult(false), accepts_mult(false), 
                             input_filter(false), has_subtype(false), has_name(false), has_defval(false),
                             reverse_logic(false), is_int(false), exact_return(false), 
                             ambiguous_int_match(false) {
   }

   DLLLOCAL QoreTypeInfo(qore_type_t n_qt) : qc(0), qt(n_qt), returns_mult(false), accepts_mult(false), 
                                             input_filter(false), has_subtype(false), has_name(false), has_defval(false),
                                             reverse_logic(false), is_int(n_qt == NT_INT),
                                             exact_return(true), ambiguous_int_match(false) {
   }

   DLLLOCAL QoreTypeInfo(const QoreClass *n_qc) : qc(n_qc), qt(NT_OBJECT), returns_mult(false), accepts_mult(false), 
                                                  input_filter(false), has_subtype(false), has_name(false), has_defval(false),
                                                  reverse_logic(false), is_int(false),
                                                  exact_return(true), ambiguous_int_match(false) {
   }

   DLLLOCAL virtual ~QoreTypeInfo() {
   }

   DLLLOCAL bool parseAcceptsReturns(qore_type_t t) const {
      if (!hasType())
         return true;

      bool n_is_int = (t == NT_INT);

      // see if type accepts given type
      if (!parseAcceptsType(t, n_is_int))
         return false;

      return parseReturnsType(t, n_is_int) ? true : false;
   }

   DLLLOCAL bool isType(qore_type_t t) const {
      if (!this || returns_mult)
         return false;

      return t == qt;
   }

   DLLLOCAL bool isClass(const QoreClass *n_qc) const {
      if (!this || returns_mult || !qc)
         return false;

      return qc->getID() == n_qc->getID();
   }

   DLLLOCAL qore_type_result_e runtimeAcceptsValue(const AbstractQoreNode *n) const {
      if (!hasType())
         return QTI_AMBIGUOUS;
      
      qore_type_t t = get_node_type(n);

      if (t == NT_OBJECT)
         return runtimeAcceptsClass(reinterpret_cast<const QoreObject *>(n)->getClass());

      return parseAcceptsType(t, t == NT_INT || (t >= QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode *>(n)));
   }

   DLLLOCAL qore_type_result_e parseAccepts(const QoreTypeInfo *typeInfo) const {
      if (!hasType() || !typeInfo->hasType())
         return QTI_AMBIGUOUS;

      if (!typeInfo->returnsSingle()) {
	 if (!accepts_mult)
	    return qc ? typeInfo->parseReturnsClass(qc) : typeInfo->parseReturnsType(qt, is_int);
	 return parseAcceptsMult(typeInfo);
      }

      return parseAcceptsBasic(typeInfo);
   }

   DLLLOCAL const QoreClass *getUniqueReturnClass() const {
      return !this || returns_mult ? 0 : qc;
   }

   DLLLOCAL bool returnsSingle() const {
      return !this || !returns_mult;
   }

   DLLLOCAL bool acceptsSingle() const {
      return !this || !accepts_mult;
   }

   DLLLOCAL bool hasType() const {
      return this && (accepts_mult || returns_mult || qt != NT_ALL);
   }

   DLLLOCAL bool hasInputFilter() const {
      return this && input_filter;
   }

   DLLLOCAL const char *getName() const {
      if (!hasType())
	 return NO_TYPE_INFO;

      if (has_name)
         return getNameImpl();

      return qc ? qc->getName() : getBuiltinTypeName(qt);
   }

   DLLLOCAL void getThisType(QoreString &str) const {
      if (!this || qt == NT_NOTHING) {
	 str.sprintf("no value");
	 return;
      }
      if (qc) {
	 str.sprintf("an object of class '%s'", qc->getName());
	 return;
      }
      str.sprintf("type '%s'", getName());
   }

   DLLLOCAL AbstractQoreNode *acceptInputParam(int param_num, const char *param_name, AbstractQoreNode *n, ExceptionSink *xsink) const {
      if (!hasType())
         return n;
      return acceptInputIntern(false, param_num, param_name, n, xsink);
   }

   DLLLOCAL AbstractQoreNode *acceptInputMember(const char *member_name, AbstractQoreNode *n, ExceptionSink *xsink) const {
      if (!hasType())
         return n;
      return acceptInputIntern(true, -1, member_name, n, xsink);
   }

   DLLLOCAL AbstractQoreNode *acceptAssignment(const char *text, AbstractQoreNode *n, ExceptionSink *xsink) const {
      assert(text && text[0] == '<');
      if (!hasType())
         return n;
      return acceptInputIntern(false, -1, text, n, xsink);
   }

   DLLLOCAL bool hasDefaultValue() const {
      if (!hasType())
         return false;

      return (qt >= 0 && qt < NT_OBJECT) || has_defval;
   }

   DLLLOCAL AbstractQoreNode *getDefaultValue() const {
      if (!hasType())
         return 0;

      if (!has_defval && qt >= 0 && qt < NT_OBJECT)
         return getDefaultValueForBuiltinValueType(qt);

      return has_defval ? getDefaultValueImpl() : 0;
   }

   // quick function to tell if the argument may be subject to an input filter for this type
   DLLLOCAL bool mayRequireFilter(const AbstractQoreNode *n) const {
      if (!hasType() || !input_filter)
         return false;

      qore_type_t nt = get_node_type(n);
      if (nt == NT_OBJECT && qc)
         return qc->getID() == reinterpret_cast<const QoreObject *>(n)->getClass()->getID() ? false : true;

      // only set n_is_int = true if our 'is_int' is true
      // only perform the dynamic cast if the type is external
      bool n_is_int = (is_int && (nt == NT_INT 
                                  || (nt >= QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode *>(n)))) ? true : false;
      if (n_is_int)
         return qt == nt ? false : true;

      return matchTypeIntern(nt, false) == QTI_IDENT ? false : true;
   }

   // used when parsing user code to find duplicate signatures after types are resolved
   DLLLOCAL bool isInputIdentical(const QoreTypeInfo *typeInfo) const;

   DLLLOCAL bool isOutputIdentical(const QoreTypeInfo *typeInfo) const;

   // returns false if there is no type or if the type can be converted to a numeric value, true if otherwise
   DLLLOCAL bool nonNumericValue() const {
      if (!hasType())
         return false;

      if (returns_mult) {
         const type_vec_t &rt = getReturnTypeList();

         // return true only if none of the return types are numeric
         for (type_vec_t::const_iterator i = rt.begin(), e = rt.end(); i != e; ++i) {
            if (!(*i)->nonNumericValue())
               return false;
         }
         return true;
      }

      return is_int || qt == NT_FLOAT || qt == NT_STRING || qt == NT_BOOLEAN || qt == NT_DATE ? false : true;
   }

   DLLLOCAL void doNonNumericWarning(const char *preface) const {
      QoreStringNode *desc = new QoreStringNode(preface);
      getThisType(*desc);
      desc->sprintf(", which does not evaluate to a numeric type, therefore will always evaluate to 0 at runtime");
      getProgram()->makeParseWarning(QP_WARN_INVALID_OPERATION, "INVALID-OPERATION", desc);
   }

   DLLLOCAL void doNonBooleanWarning(const char *preface) const {
      QoreStringNode *desc = new QoreStringNode(preface);
      getThisType(*desc);
      desc->sprintf(", which does not evaluate to a numeric or boolean type, therefore will always evaluate to False at runtime");
      getProgram()->makeParseWarning(QP_WARN_INVALID_OPERATION, "INVALID-OPERATION", desc);
   }

   DLLLOCAL void concatName(std::string &str) const {
      if (!hasType()) {
	 str.append(NO_TYPE_INFO);
	 return;
      }

      if (returns_mult || accepts_mult || has_name || !qc)
         str.append(getName());
      else
         concatClass(str, qc->getName());
   }

   DLLLOCAL int doAcceptError(bool priv_error, bool obj, int param_num, const char *param_name, AbstractQoreNode *n, ExceptionSink *xsink) const {
      if (priv_error) {
         if (obj)
            doObjectPrivateClassException(param_name, n, xsink);
         else
            doPrivateClassException(param_num + 1, param_name, n, xsink);
      }
      else {
         if (obj)
            doObjectTypeException(param_name, n, xsink);
         else
            doTypeException(param_num + 1, param_name, n, xsink);
      }
      return -1;
   }
};

// this is basically just a wrapper around NamedScope
class QoreParseTypeInfo {
protected:
   bool or_nothing;
   std::string tname;

   DLLLOCAL QoreParseTypeInfo(const NamedScope *n_cscope) : or_nothing(false), cscope(n_cscope->copy()) {
      setName();
   }

   DLLLOCAL void setName() {
      if (or_nothing)
         tname = "*";
      tname += cscope->getIdentifier();
   }

public:
   NamedScope *cscope; // namespace scope for class

   DLLLOCAL QoreParseTypeInfo(char *n_cscope, bool n_or_nothing = false) : or_nothing(n_or_nothing), cscope(new NamedScope(n_cscope)) {
      setName();
      assert(strcmp(n_cscope, "any"));
   }

   DLLLOCAL ~QoreParseTypeInfo() {
      delete cscope;
   }

   // prototype (expecting type) should be "this"
   // returns true if the prototype does not expect any type or the types are compatible, 
   // false if otherwise
   DLLLOCAL bool parseStageOneEqual(const QoreParseTypeInfo *typeInfo) const {
      return !strcmp(cscope->getIdentifier(), typeInfo->cscope->getIdentifier());
   }

   // used when parsing user code to find duplicate signatures
   DLLLOCAL bool parseStageOneIdenticalWithParsed(const QoreTypeInfo *typeInfo, bool &recheck) const {
      bool thisnt = !this;
      bool typent = !typeInfo->hasType();

      if (thisnt && typent)
	 return true;

      if (thisnt || typent)
	 return false;

      const QoreClass *qc = typeInfo->getUniqueReturnClass();
      if (!qc)
         return false;

      // both have class info
      if (!strcmp(cscope->getIdentifier(), qc->getName()))
         return recheck = true;
      else
         return false;
   }

   // used when parsing user code to find duplicate signatures
   DLLLOCAL bool parseStageOneIdentical(const QoreParseTypeInfo *typeInfo) const {
      bool thisnt = !this;
      bool typent = !typeInfo;

      if (thisnt && typent)
	 return true;

      if (thisnt || typent)
	 return false;

      return !strcmp(cscope->ostr, typeInfo->cscope->ostr);
   }

   // resolves the current type to a QoreTypeInfo pointer and deletes itself
   DLLLOCAL const QoreTypeInfo *resolveAndDelete();

#ifdef DEBUG
   DLLLOCAL const char *getCID() const { return this && cscope ? cscope->getIdentifier() : "n/a"; }
#endif

   DLLLOCAL QoreParseTypeInfo *copy() const {
      if (!this)
	 return 0;

      return new QoreParseTypeInfo(cscope);
   }

   DLLLOCAL const char *getName() const {
      if (!this)
	 return NO_TYPE_INFO;

      return tname.c_str();
   }

   DLLLOCAL void concatName(std::string &str) const {
      if (!this) {
	 str.append(NO_TYPE_INFO);
	 return;
      }

      concatClass(str, cscope->getIdentifier());
   }
};

class ReverseTypeInfo : public QoreTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const = 0;

public:
   DLLLOCAL ReverseTypeInfo(qore_type_t n_nt) : QoreTypeInfo(0, n_nt, false, false, false, false, 
                                                             true, false, true, false, false) {
   }
};

// SomethingTypeInfo, i.e. not NOTHING
class SomethingTypeInfo : public ReverseTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "something";
   }

public:
   DLLLOCAL SomethingTypeInfo() : ReverseTypeInfo(NT_NOTHING) {
   }
};

class AcceptsMultiTypeInfo : public QoreTypeInfo {
protected:
   type_vec_t at;

   DLLLOCAL virtual const type_vec_t &getAcceptTypeList() const {
      return at;
   }

public:
   DLLLOCAL AcceptsMultiTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_returns_mult, 
                                 bool n_input_filter = false, bool n_has_subtype = false, 
                                 bool n_has_name = false, bool n_has_defval = false, 
                                 bool n_is_int = false, bool n_exact_return = false) : 
      QoreTypeInfo(n_qc, n_qt, n_returns_mult, true, n_input_filter, n_has_subtype, n_has_name, 
                   n_has_defval, false, n_is_int, n_exact_return) {
   }
};

class AcceptsMultiFilterTypeInfo : public AcceptsMultiTypeInfo {
protected:
   // must be reimplemented in subclasses if input_filter is true
   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const = 0;

public:
   DLLLOCAL AcceptsMultiFilterTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_returns_mult, bool n_has_subtype = false, 
                                       bool n_has_name = false, bool n_has_defval = false, 
                                       bool n_is_int = false, bool n_exact_return = false) : 
      AcceptsMultiTypeInfo(n_qc, n_qt, n_returns_mult, true, n_has_subtype, n_has_name, n_has_defval, n_is_int, n_exact_return) {
   }
};

class AcceptsReturnsMultiFilterTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   type_vec_t rt;

   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return rt;
   }

public:
   DLLLOCAL AcceptsReturnsMultiFilterTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_has_subtype = false, 
                                              bool n_has_name = false, bool n_has_defval = false, 
                                              bool n_is_int = false) : 
      AcceptsMultiFilterTypeInfo(n_qc, n_qt, true, n_has_subtype, n_has_name, n_has_defval, n_is_int, false) {
   }
};

class FloatOrNothingTypeInfo : public AcceptsReturnsMultiFilterTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "*float";
   }

   DLLLOCAL bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_FLOAT || t == NT_NOTHING)
         return true;

      // only perform dynamic cast if type is external
      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n)))
         return false;

      QoreFloatNode *f = new QoreFloatNode(reinterpret_cast<const QoreBigIntNode *>(n)->val);
      n->deref(xsink);
      n = f;
      return true;
   }

public:
   DLLLOCAL FloatOrNothingTypeInfo() : AcceptsReturnsMultiFilterTypeInfo(0, NT_FLOAT, false, true, false, false) {
      assert(bigIntTypeInfo);
      at.push_back(bigIntTypeInfo);
      assert(floatTypeInfo);
      at.push_back(floatTypeInfo);
      assert(nothingTypeInfo);
      at.push_back(nothingTypeInfo);

      rt.push_back(floatTypeInfo);
      rt.push_back(nothingTypeInfo);
   }
};

class FloatTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   DLLLOCAL bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_FLOAT)
         return true;

      // only perform dynamic cast if type is external
      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n)))
         return false;

      QoreFloatNode *f = new QoreFloatNode(reinterpret_cast<const QoreBigIntNode *>(n)->val);
      n->deref(xsink);
      n = f;
      return true;
   }

public:
   DLLLOCAL FloatTypeInfo() : AcceptsMultiFilterTypeInfo(0, NT_FLOAT, false, false, false, false, false, true) {
      assert(bigIntTypeInfo);
      at.push_back(bigIntTypeInfo);
   }
};

class IntTypeInfo : public QoreTypeInfo {
public:
   DLLLOCAL IntTypeInfo(qore_type_t n_qt, bool n_accepts_mult = false, bool n_input_filter = false,
                        bool n_has_name = false, bool n_has_defval = false, bool n_exact_return = true) : 
      QoreTypeInfo(0, n_qt, false, n_accepts_mult, n_input_filter, false, n_has_name, n_has_defval, false, true, n_exact_return) {
   }
};

class BigIntTypeInfo : public IntTypeInfo {
public:
   DLLLOCAL BigIntTypeInfo() : IntTypeInfo(NT_INT) {
   }
};

class AcceptsReturnsSameMultiTypeInfo : public AcceptsMultiTypeInfo {
protected:
   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return at;
   }

   DLLLOCAL virtual const char *getNameImpl() const = 0;

public:
   DLLLOCAL AcceptsReturnsSameMultiTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, 
                                            bool n_input_filter = false, bool n_has_subtype = false, 
                                            bool n_is_int = false) :
      AcceptsMultiTypeInfo(n_qc, n_qt, true, n_input_filter, n_has_subtype, true, false, n_is_int) {
   }
};

class OrNothingTypeInfo : public AcceptsReturnsSameMultiTypeInfo {
protected:
   QoreString tname;

   DLLLOCAL virtual const char *getNameImpl() const {
      return tname.getBuffer();
   }

public:
   DLLLOCAL OrNothingTypeInfo(const QoreTypeInfo &ti) : AcceptsReturnsSameMultiTypeInfo(ti.qc, ti.qt, false, false, ti.qt == NT_INT) {
      assert(ti.hasType());

      tname = "*";
      tname += ti.getName();

      assert(!ti.input_filter);
      
      if (ti.accepts_mult)
         at = ti.getAcceptTypeList();
      else
         at.push_back(&ti);

      at.push_back(nothingTypeInfo);
   }
};

// expect a ResolvedCallReferenceNode with this type 
class CodeTypeInfo : public AcceptsReturnsSameMultiTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "code";
   }

public:
   DLLLOCAL CodeTypeInfo() : AcceptsReturnsSameMultiTypeInfo(0, NT_CODE) {
      at.push_back(callReferenceTypeInfo);
      at.push_back(runTimeClosureTypeInfo);
   }
};

class CodeOrNothingTypeInfo : public CodeTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "*code";
   }

public:
      DLLLOCAL CodeOrNothingTypeInfo() {
      at.push_back(nothingTypeInfo);
   }
};

// accepts any type
class UserReferenceTypeInfo : public ReverseTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "something";
   }

public:
   DLLLOCAL UserReferenceTypeInfo() : ReverseTypeInfo(NT_NONE) {
   }
};

// accepts QoreStringNode or BinaryNode and passes through
class DataTypeInfo : public AcceptsReturnsSameMultiTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "data";
   }

public:
   DLLLOCAL DataTypeInfo() : AcceptsReturnsSameMultiTypeInfo(0, NT_DATA) {
      at.push_back(stringTypeInfo);
      at.push_back(binaryTypeInfo);
   }
};

// accepts int, float, string, date, null, or boolean and returns an int
class SoftBigIntTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "softint";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_INT || (t >= QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode *>(n)))
         return true;

      if (t != NT_FLOAT
          && t != NT_STRING
          && t != NT_BOOLEAN
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      int64 rv = n->getAsBigInt();
      n->deref(xsink);
      n = new QoreBigIntNode(rv);
      return true;
   }

   // must be reimplemented in subclasses if has_defval is true
   DLLLOCAL virtual AbstractQoreNode *getDefaultValueImpl() const {
      return zero();
   }

   DLLLOCAL void init() {
      at.push_back(floatTypeInfo);
      at.push_back(stringTypeInfo);
      at.push_back(boolTypeInfo);
      at.push_back(dateTypeInfo);
      at.push_back(nullTypeInfo);
   }

public:
   DLLLOCAL SoftBigIntTypeInfo(bool n_returns_mult = false) : AcceptsMultiFilterTypeInfo(0, NT_INT, n_returns_mult, false, true, n_returns_mult ? false : true, n_returns_mult ? false : true, n_returns_mult ? false : true) {
      init();
   }
};

class SoftBigIntOrNothingTypeInfo : public SoftBigIntTypeInfo {
protected:
   type_vec_t rt;

   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return rt;
   }

   DLLLOCAL virtual const char *getNameImpl() const {
      return "*softint";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_NOTHING || t == NT_INT || (t >= QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode *>(n)))
         return true;

      if (t != NT_FLOAT
          && t != NT_STRING
          && t != NT_BOOLEAN
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      int64 rv = n->getAsBigInt();
      n->deref(xsink);
      n = new QoreBigIntNode(rv);
      return true;
   }

public:
   DLLLOCAL SoftBigIntOrNothingTypeInfo() : SoftBigIntTypeInfo(true) {
      at.push_back(nothingTypeInfo);
      rt.push_back(bigIntTypeInfo);
      rt.push_back(nothingTypeInfo);
   }
};

// accepts int, float, string, date, null, or boolean and returns a float
class SoftFloatTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "softfloat";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_FLOAT)
         return true;

      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n))
          && t != NT_STRING
          && t != NT_BOOLEAN
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      double rv = n->getAsFloat();
      n->deref(xsink);
      n = new QoreFloatNode(rv);
      return true;
   }

   // must be reimplemented in subclasses if has_defval is true
   DLLLOCAL virtual AbstractQoreNode *getDefaultValueImpl() const {
      return zero_float();
   }

public:
   DLLLOCAL SoftFloatTypeInfo(bool n_returns_mult = false) : AcceptsMultiFilterTypeInfo(0, NT_FLOAT, n_returns_mult, false, true, n_returns_mult ? false : true, false, n_returns_mult ? false : true) {
      at.push_back(bigIntTypeInfo);
      at.push_back(stringTypeInfo);
      at.push_back(boolTypeInfo);
      at.push_back(dateTypeInfo);
      at.push_back(nullTypeInfo);
   }
};

class SoftFloatOrNothingTypeInfo : public SoftFloatTypeInfo {
protected:
   type_vec_t rt;

   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return rt;
   }

   DLLLOCAL virtual const char *getNameImpl() const {
      return "*softfloat";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_FLOAT || t == NT_NOTHING)
         return true;

      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n))
          && t != NT_STRING
          && t != NT_BOOLEAN
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      double rv = n->getAsFloat();
      n->deref(xsink);
      n = new QoreFloatNode(rv);
      return true;
   }

public:
   DLLLOCAL SoftFloatOrNothingTypeInfo() : SoftFloatTypeInfo(true) {
      at.push_back(nothingTypeInfo);
      rt.push_back(floatTypeInfo);
      rt.push_back(nothingTypeInfo);
   }
};

// accepts int, float, string, date, null, or boolean and returns a boolean
class SoftBoolTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "softbool";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_BOOLEAN)
         return true;

      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n))
          && t != NT_FLOAT
          && t != NT_STRING
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      bool rv = n->getAsBool();
      n->deref(xsink);
      n = get_bool_node(rv);
      return true;
   }

   // must be reimplemented in subclasses if has_defval is true
   DLLLOCAL virtual AbstractQoreNode *getDefaultValueImpl() const {
      return &False;
   }

public:
   DLLLOCAL SoftBoolTypeInfo(bool n_returns_mult = false) : AcceptsMultiFilterTypeInfo(0, NT_BOOLEAN, n_returns_mult, false, true, n_returns_mult ? false : true, false, n_returns_mult ? false : true) {
      at.push_back(bigIntTypeInfo);
      at.push_back(floatTypeInfo);
      at.push_back(stringTypeInfo);
      at.push_back(dateTypeInfo);
      at.push_back(nullTypeInfo);
   }
};

class SoftBoolOrNothingTypeInfo : public SoftBoolTypeInfo {
protected:
   type_vec_t rt;

   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return rt;
   }

   DLLLOCAL virtual const char *getNameImpl() const {
      return "*softbool";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_BOOLEAN || t == NT_NOTHING)
         return true;

      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n))
          && t != NT_FLOAT
          && t != NT_STRING
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      bool rv = n->getAsBool();
      n->deref(xsink);
      n = get_bool_node(rv);
      return true;
   }

public:
   DLLLOCAL SoftBoolOrNothingTypeInfo() : SoftBoolTypeInfo(true) {
      at.push_back(nothingTypeInfo);
      rt.push_back(boolTypeInfo);
      rt.push_back(nothingTypeInfo);
   }
};

// accepts int, float, string, date, null, or boolean and returns a string
class SoftStringTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "softstring";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_STRING)
         return true;

      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n))
          && t != NT_FLOAT
          && t != NT_BOOLEAN
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      QoreStringNodeValueHelper str(n);
      QoreStringNode *rv = str.getReferencedValue();
      n->deref(xsink);
      n = rv;
      return true;
   }

   // must be reimplemented in subclasses if has_defval is true
   DLLLOCAL virtual AbstractQoreNode *getDefaultValueImpl() const {
      return &False;
   }

public:
   DLLLOCAL SoftStringTypeInfo(bool n_returns_mult = false) : AcceptsMultiFilterTypeInfo(0, NT_STRING, n_returns_mult, false, true, n_returns_mult ? false : true, false, n_returns_mult ? false : true) {
      at.push_back(bigIntTypeInfo);
      at.push_back(floatTypeInfo);
      at.push_back(boolTypeInfo);
      at.push_back(dateTypeInfo);
      at.push_back(nullTypeInfo);
   }
};

class SoftStringOrNothingTypeInfo : public SoftStringTypeInfo {
protected:
   type_vec_t rt;

   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return rt;
   }

   DLLLOCAL virtual const char *getNameImpl() const {
      return "*softstring";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_STRING || t == NT_NOTHING)
         return true;

      if (t != NT_INT && (t < QORE_NUM_TYPES || !dynamic_cast<const QoreBigIntNode *>(n))
          && t != NT_FLOAT
          && t != NT_BOOLEAN
          && t != NT_DATE
          && t != NT_NULL)
         return false;

      QoreStringNodeValueHelper str(n);
      QoreStringNode *rv = str.getReferencedValue();
      n->deref(xsink);
      n = rv;
      return true;
   }

public:
   DLLLOCAL SoftStringOrNothingTypeInfo() : SoftStringTypeInfo(true) {
      at.push_back(nothingTypeInfo);
      rt.push_back(stringTypeInfo);
      rt.push_back(nothingTypeInfo);
   }
};

// accepts int or date and returns an int representing time in milliseconds
class TimeoutTypeInfo : public AcceptsMultiFilterTypeInfo {
protected:
   DLLLOCAL virtual const char *getNameImpl() const {
      return "timeout";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_INT || (t < QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode *>(n)))
         return true;

      if (t != NT_DATE)
         return false;

      int64 ms = reinterpret_cast<const DateTimeNode *>(n)->getRelativeMilliseconds();
      n->deref(xsink);
      n = new QoreBigIntNode(ms);
      return true;
   }

   // must be reimplemented in subclasses if has_defval is true
   DLLLOCAL virtual AbstractQoreNode *getDefaultValueImpl() const {
      return &False;
   }

public:
   DLLLOCAL TimeoutTypeInfo(bool n_returns_mult = false) : AcceptsMultiFilterTypeInfo(0, NT_INT, n_returns_mult, false, true, n_returns_mult ? false : true, n_returns_mult ? false : true, n_returns_mult ? false : true) {
      at.push_back(dateTypeInfo);
   }
};

class TimeoutOrNothingTypeInfo : public TimeoutTypeInfo {
protected:
   type_vec_t rt;

   DLLLOCAL virtual const type_vec_t &getReturnTypeList() const {
      return rt;
   }

   DLLLOCAL virtual const char *getNameImpl() const {
      return "*timeout";
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      qore_type_t t = get_node_type(n);

      if (t == NT_INT || t == NT_NOTHING || (t < QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode *>(n)))
         return true;

      if (t != NT_DATE)
         return false;

      int64 ms = reinterpret_cast<const DateTimeNode *>(n)->getRelativeMilliseconds();
      n->deref(xsink);
      n = new QoreBigIntNode(ms);
      return true;
   }

public:
   DLLLOCAL TimeoutOrNothingTypeInfo() : TimeoutTypeInfo(true) {
      at.push_back(nothingTypeInfo);
      rt.push_back(bigIntTypeInfo);
      rt.push_back(nothingTypeInfo);
   }
};

/*
   DLLLOCAL QoreTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_returns_mult,
                         bool n_accepts_mult, bool n_input_filter, bool n_has_subtype,
                         bool n_has_name, bool n_has_defval, bool n_reverse_logic, 
                         bool n_is_int, bool n_exact_return) : {}
   DLLLOCAL AcceptsMultiTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_returns_mult, 
                                 bool n_input_filter = false, bool n_has_subtype = false, 
                                 bool n_has_name = false, bool n_has_defval = false, 
                                 bool n_is_int = false, bool n_exact_return = false) : 
   DLLLOCAL AcceptsMultiFilterTypeInfo(const QoreClass *n_qc, qore_type_t n_qt, bool n_returns_mult, bool n_has_subtype = false, 
                                       bool n_has_name = false, bool n_has_defval = false, 
                                       bool n_is_int = false, bool n_exact_return = false) : 
*/

class ExternalTypeInfo : public QoreTypeInfo {
protected:
   const char *tname;
   const QoreTypeInfoHelper &helper;
   type_vec_t at;

   DLLLOCAL virtual const char *getNameImpl() const {
      return tname;
   }

   DLLLOCAL virtual const type_vec_t &getAcceptTypeList() const {
      return at;
   }

   DLLLOCAL virtual bool acceptInputImpl(AbstractQoreNode *&n, ExceptionSink *xsink) const {
      return helper.acceptInputImpl(n, xsink);
   }

public:
   // used for base types
   DLLLOCAL ExternalTypeInfo(qore_type_t n_qt, const char *n_tname, const QoreTypeInfoHelper &n_helper, bool n_is_int = false, bool n_exact_return = true) : 
      QoreTypeInfo(0, n_qt, 
                   false, // returns_mult 
                   false, // accepts_mult
                   false, // input_filter
                   false, // has_subtype
                   true,  // has_name
                   false, // has_defval
                   false, // reverse_logic
                   n_is_int, n_exact_return), tname(n_tname), helper(n_helper) {
      assert(tname);
      
      assert(has_name);

      //printd(0, "ExternalTypeInfo::ExternalTypeInfo() this=%p qt=%n qc=%p name=%s\n", this, qt, qc, tname);
   }

   // used for classes
   DLLLOCAL ExternalTypeInfo(const QoreClass *n_qc, const QoreTypeInfoHelper &n_helper) : QoreTypeInfo(n_qc), tname(0), helper(n_helper) {
      //printd(0, "ExternalTypeInfo::ExternalTypeInfo() this=%p qt=%n qc=%p name=%s\n", this, qt, qc, qc->getName());
   }

   // used when assigning a base type after the fact
   DLLLOCAL ExternalTypeInfo(const char *n_tname, const QoreTypeInfoHelper &n_helper) : 
      QoreTypeInfo(0, NT_NOTHING,
                   false,  // returns_mult 
                   false,  // accepts_mult
                   false,  // input_filter
                   false,  // has_subtype
                   true,   // has_name
                   false,  // has_defval
                   false,  // reverse_logic
                   false,  // is_int
                   true), // exact_return
      tname(n_tname), helper(n_helper) {
   }
   // used for assigning a class after the fact
   DLLLOCAL ExternalTypeInfo(const QoreTypeInfoHelper &n_helper) : tname(0), helper(n_helper) {
   }
   DLLLOCAL void assign(qore_type_t n_qt, const char *n_tname = 0) {
      qt = n_qt;
      if (n_tname) {
         has_name = true;
         tname = n_tname;
      }
   }
   DLLLOCAL void assign(const QoreClass *n_qc) {
      assert(n_qc);
      qt = NT_OBJECT;
      qc = n_qc;
      assert(!tname);
      //tname = qc->getName();
   }
   DLLLOCAL void addAcceptsType(const QoreTypeInfo *typeInfo) {
      assert(typeInfo);
      assert(typeInfo != this);

      if (!accepts_mult)
         accepts_mult = true;

      at.push_back(typeInfo);
   }
   DLLLOCAL void setInt() {
      assert(!qc);
      is_int = true;
   }
   DLLLOCAL void setInexactReturn() {
      exact_return = false;
   }
   DLLLOCAL void setInputFilter() {
      input_filter = true;
   }
   DLLLOCAL void setIntMatch() {
      ambiguous_int_match = true;
   }
};

// returns type info for base types
DLLLOCAL const QoreTypeInfo *getTypeInfoForType(qore_type_t t);
// returns type info information for parse types (values)
DLLLOCAL const QoreTypeInfo *getTypeInfoForValue(const AbstractQoreNode *n);

#endif // _QORE_QORETYPEINFO_H
