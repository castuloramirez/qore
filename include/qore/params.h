/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  params.h

  Qore Programming Language

  Copyright 2003 - 2011 David Nichols

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

#ifndef _QORE_PARAMS_H

#define _QORE_PARAMS_H

#include <qore/AbstractQoreNode.h>

/** @file params.h
    Contains inline functions for accessing function and class method arguments.
 */

//! returns the number of arguments passed to the function
/** @param n a pointer to the argument list
    @return the number of arguments passed to the function
 */
static inline unsigned num_args(const QoreListNode *n) {
   return n ? (unsigned)n->size() : 0;
}

//! returns the number of arguments passed to the function
/** @param n a pointer to the argument list
    @return the number of arguments passed to the function
 */
static inline unsigned num_params(const QoreListNode *n) {
   return n ? (unsigned)n->size() : 0;
}

//! returns the argument in the position given or 0 if there is none
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return the argument in the position given or 0 if there is none
 */
static inline const AbstractQoreNode *get_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? 0 : p;
}

//! returns the argument type in the position given or 0 if there is none
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return the argument type in the position given or 0 if there is none
 */
static inline qore_type_t get_param_type(const QoreListNode *n, qore_size_t i) {
   if (!n) return NT_NOTHING;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return p ? p->getType() : NT_NOTHING;
}

//! returns an integer corresponding to the argument given or 0 if there is none
static inline int get_int_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? 0 : p->getAsInt();
}

//! returns a 64-bit integer corresponding to the argument given or 0 if there is none
static inline int64 get_bigint_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? 0 : p->getAsBigInt();
}

//! returns an integer corresponding to the argument given or a default value if there is none
static inline int get_int_param_with_default(const QoreListNode *n, qore_size_t i, int def) {
   if (!n) return def;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? def : p->getAsInt();
}

//! returns a 64-bit integer corresponding to the argument given or a default value if there is none
static inline int64 get_bigint_param_with_default(const QoreListNode *n, qore_size_t i, int64 def) {
   if (!n) return def;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? def : p->getAsBigInt();
}

//! returns a float corresponding to the argument given or 0 if there is none
static inline double get_float_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? 0 : p->getAsFloat();
}

//! returns a boolean value corresponding to the argument given or false if there is none
static inline bool get_bool_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   return is_nothing(p) ? false : p->getAsBool();
}

//! returns a const BinaryNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a BinaryNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a const BinaryNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a BinaryNode
 */
static inline const BinaryNode *test_binary_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_BINARY ? reinterpret_cast<const BinaryNode *>(p) : 0;
}

//! returns a const QoreStringNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreStringNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a const QoreStringNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreStringNode
 */
static inline const QoreStringNode *test_string_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_STRING ? reinterpret_cast<const QoreStringNode *>(p) : 0;
}

//! returns a QoreObject pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreObject
/** QoreObjects are returned not as "const" because they are unique and can always be manipulated
    @param n a pointer to the argument list
    @param i the offset in the list to test (first element is offset 0)
    @return a QoreObject pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreObject
 */
static inline QoreObject *test_object_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_OBJECT ? const_cast<QoreObject *>(reinterpret_cast<const QoreObject *>(p)) : 0;
}

//! returns a DateTimeNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a DateTimeNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a DateTimeNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a DateTimeNode
 */
static inline const DateTimeNode *test_date_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_DATE ? reinterpret_cast<const DateTimeNode *>(p) : 0;
}

//! returns a QoreHashNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreHashNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a QoreHashNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreHashNode
 */
static inline const QoreHashNode *test_hash_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_HASH ? reinterpret_cast<const QoreHashNode *>(p) : 0;
}

//! returns a QoreListNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreListNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a QoreListNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a QoreListNode
 */
static inline const QoreListNode *test_list_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_LIST ? reinterpret_cast<const QoreListNode *>(p) : 0;
}

//! returns a ResolvedCallReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ResolvedCallReferenceNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a ResolvedCallReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ResolvedCallReferenceNode
 */
static inline const ResolvedCallReferenceNode *test_callref_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && (p->getType() == NT_FUNCREF || p->getType() == NT_RUNTIME_CLOSURE) ? reinterpret_cast<const ResolvedCallReferenceNode *>(p) : 0;
}

//! returns a ResolvedCallReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ResolvedCallReferenceNode
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a ResolvedCallReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ResolvedCallReferenceNode
 */
static inline const ResolvedCallReferenceNode *test_funcref_param(const QoreListNode *n, qore_size_t i) {
   return test_callref_param(n, i);
}

//! returns a ReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ReferenceNode
/**
   Note that this will also return a value for a closure; this is a synonym for test_funcref_param
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return a ReferenceNode pointer for the argument position given or 0 if there is no argument there or if the argument is not a ReferenceNode
 */
static inline const ReferenceNode *test_reference_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return 0;
   const AbstractQoreNode *p = n->retrieve_entry(i);
   // the following is faster than a dynamic_cast
   return p && p->getType() == NT_REFERENCE ? reinterpret_cast<const ReferenceNode *>(p) : 0;
}

//! returns true if the arugment position given is NOTHING
/**
   @param n a pointer to the argument list
   @param i the offset in the list to test (first element is offset 0)
   @return true if the arugment position given is NOTHING
 */
static inline bool test_nothing_param(const QoreListNode *n, qore_size_t i) {
   if (!n) return true;
   return is_nothing(n->retrieve_entry(i));
}

//! returns the QoreEncoding corresponding to the string passed or a default encoding
static inline const QoreEncoding *get_encoding_param(const QoreListNode *n, qore_size_t i, const QoreEncoding *def = QCS_DEFAULT) {
   const QoreStringNode *str = test_string_param(n, i);
   return str ? QEM.findCreate(str) : def;
}

//! returns the given type for hard typed parameters
template <typename T>
static inline T *get_hard_param(const QoreListNode *n, qore_size_t i) {
   assert(n);
   assert(dynamic_cast<T *>(n->retrieve_entry(i)));
   return reinterpret_cast<T *>(n->retrieve_entry(i));
}

static inline void HARD_QORE_DATA(const QoreListNode *n, qore_size_t i, const void *&ptr, qore_size_t &len) {
   const AbstractQoreNode *p = get_hard_param<const AbstractQoreNode>(n, i);
   if (p->getType() == NT_STRING) {
      const QoreStringNode *str = reinterpret_cast<const QoreStringNode *>(p);
      ptr = (const void *)str->getBuffer();
      len = str->size();
      return;
   }
   const BinaryNode *b = reinterpret_cast<const BinaryNode *>(p);
   ptr = b->getPtr();
   len = b->size();
}

//! returns a hard typed parameter
#define HARD_QORE_PARAM(name, Type, list, i) Type *name = get_hard_param<Type>(list, i)

//! returns an int64 from a hard typed int param
#define HARD_QORE_INT(list, i) get_hard_param<const QoreBigIntNode>(list, i)->val

//! returns a double from a hard typed float param
#define HARD_QORE_FLOAT(list, i) get_hard_param<const QoreFloatNode>(list, i)->f

//! returns a bool from a hard typed bool param
#define HARD_QORE_BOOL(list, i) get_hard_param<const QoreBoolNode>(list, i)->getValue()

//! returns a const QoreStringNode* from a hard typed string param
#define HARD_QORE_STRING(list, i) get_hard_param<const QoreStringNode>(list, i)

//! returns a const DateTimeNode* from a hard typed date param
#define HARD_QORE_DATE(list, i) get_hard_param<const DateTimeNode>(list, i)

//! returns a const BinaryNode* from a hard typed binary param
#define HARD_QORE_BINARY(list, i) get_hard_param<const BinaryNode>(list, i)

//! returns a const QoreListNode* from a hard typed list param
#define HARD_QORE_LIST(list, i) get_hard_param<const QoreListNode>(list, i)

//! returns a const QoreHashNode* from a hard typed hash param
#define HARD_QORE_HASH(list, i) get_hard_param<const QoreHashNode>(list, i)

//! returns a const QoreHashNode* from a hard typed hash param
#define HARD_QORE_REF(list, i) get_hard_param<const ReferenceNode>(list, i)

//! returns a QoreObject* from a hard typed object param
#define HARD_QORE_OBJECT(list, i) const_cast<QoreObject *>(get_hard_param<const QoreObject>(list, i))

// sets up an object pointer
#define HARD_QORE_OBJ_DATA(vname, Type, list, i, cid, dname, cname, xsink) HARD_QORE_PARAM(obj_##vname, const QoreObject, list, i); Type *vname = reinterpret_cast<Type *>(obj_##vname->getReferencedPrivateData(cid, xsink)); if (!vname && !*xsink) xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot complete call setup to %s() because parameter %d (<class %s>) has already been deleted", cname, i + 1, dname)

//! returns the QoreEncoding corresponding to the string passed or a default encoding
static inline const QoreEncoding *get_hard_qore_encoding_param(const QoreListNode *n, qore_size_t i) {
   HARD_QORE_PARAM(str, const QoreStringNode, n, i);
   return QEM.findCreate(str);
}

#endif
