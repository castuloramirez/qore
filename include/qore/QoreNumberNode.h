/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreNumberNode.h
  
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

#ifndef _QORE_QORENUMBERNODE_H

#define _QORE_QORENUMBERNODE_H

class LocalVar;
class QoreTypeInfo;

/** @defgroup number_format_flags Number Format Flags
    used with QoreNumberNode::toString()
 */
//@{
//! number format bitfield: default
#define QORE_NF_DEFAULT    0
//! number format bitfield: scientific
#define QORE_NF_SCIENTIFIC (1 << 0)
//! number format bitfield: raw (unrounded)
#define QORE_NF_RAW        (1 << 1)
//@}

//! Qore's arbitrary-precision number value type, dynamically-allocated only, reference counted
class QoreNumberNode : public SimpleValueQoreNode {
   friend struct qore_number_private;
private:
   //! returns the value as a bool
   DLLLOCAL virtual bool getAsBoolImpl() const;

   //! returns the value as an int
   DLLLOCAL virtual int getAsIntImpl() const;

   //! returns the value as a 64-bit int
   DLLLOCAL virtual int64 getAsBigIntImpl() const;

   //! returns the value as a double
   DLLLOCAL virtual double getAsFloatImpl() const;

protected:
   //! the private implementation of the type
   struct qore_number_private* priv;

   //! the destructor is protected because it should not be called directly
   /**
      @see SimpleValueQoreNode::deref()
   */
   DLLEXPORT virtual ~QoreNumberNode();

   // private
   DLLLOCAL QoreNumberNode(struct qore_number_private* p);

public:
   //! creates a new number value from the node, if not possible then the new number will be assigned 0
   DLLEXPORT QoreNumberNode(const AbstractQoreNode* n);

   //! creates a new number value and assigns the initial value to it
   /**
      @param f the value for the object
   */
   DLLEXPORT QoreNumberNode(double f);

   //! creates a new number value and assigns the initial value to it
   /**
      @param i the value for the object
   */
   DLLEXPORT QoreNumberNode(int64 i);

   //! creates a new number value and assigns the initial value to it
   /**
      @param str the value for the object
   */
   DLLEXPORT QoreNumberNode(const char* str);

   //! creates a new number value and assigns the initial value to it
   /**
      @param str the value for the object
      @param prec the initial precision for the number
   */
   DLLEXPORT QoreNumberNode(const char* str, unsigned prec);

   //! creates a new numbering-point value and assigns it to 0
   DLLEXPORT QoreNumberNode();

   //! creates a copy of the object
   DLLEXPORT QoreNumberNode(const QoreNumberNode& old);

   //! returns the number value converted to a string and sets del to true
   /** NOTE: do not use this function directly, use QoreStringValueHelper instead
       @param del output parameter: del is set to true, meaning that the resulting QoreString pointer belongs to the caller (and must be deleted manually)
       @return a QoreString pointer, use the del output parameter to determine ownership of the pointer
       @see QoreStringValueHelper
   */
   DLLEXPORT virtual QoreString* getStringRepresentation(bool& del) const;

   //! concatentates the number value to an existing QoreString reference, default implementation does nothing
   /**
      @param str a reference to a QoreString where the value of the type will be concatenated
   */
   DLLEXPORT virtual void getStringRepresentation(QoreString& str) const;

   //! returns the DateTime representation of this value and sets del to true
   /** The DateTime representation is calculated by converting the number value 
       to an integer interpreted as the number of seconds offset from January 1, 1970.
       @note Use the DateTimeValueHelper class instead of using this function directly
       @param del output parameter: if del is true, then the returned DateTime pointer belongs to the caller (and must be deleted manually), if false, then it must not be
       @see DateTimeValueHelper
   */
   DLLEXPORT virtual class DateTime *getDateTimeRepresentation(bool& del) const;

   //! assigns the date representation of the value to the DateTime reference passed
   /** The DateTime representation is calculated by converting the number value 
       to an integer interpreted as the number of seconds offset from January 1, 1970.
       @param dt the DateTime reference to be assigned
   */
   DLLEXPORT virtual void getDateTimeRepresentation(DateTime& dt) const;

   //! concatenate the string representation of the number value to an existing QoreString
   /** used for %n and %N printf formatting
       @param str the string representation of the type will be concatenated to this QoreString reference
       @param foff for multi-line formatting offset, ignored by this implementation of the function
       @param xsink ignored by this implementation of the function
       @return -1 for exception raised, 0 = OK
   */
   DLLEXPORT virtual int getAsString(QoreString& str, int foff, class ExceptionSink* xsink) const;

   //! returns a QoreString giving the string representation of the number value, sets del to true
   /** Used for %n and %N printf formatting.  Do not call this function directly; use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead
       @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
       @param foff for multi-line formatting offset, ignored by this implementation of the function
       @param xsink ignored by this implementation of the function
       @see QoreNodeAsStringHelper
   */
   DLLEXPORT virtual QoreString *getAsString(bool& del, int foff, class ExceptionSink* xsink) const;

   //! returns a copy of the object; the caller owns the reference count
   /**
      @return a copy of the object; the caller owns the reference count
   */
   DLLEXPORT virtual AbstractQoreNode* realCopy() const;

   //! tests for equality with possible type conversion (soft compare)
   /**
      @param v the value to compare
      @param xsink ignored by this implementation of the function
      @return true if the objects are equal, false if not
   */
   DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

   //! tests for equality without type conversions (hard compare)
   /**
      @param v the value to compare
      @param xsink ignored by this implementation of the function
      @return true if the objects are equal, false if not
   */
   DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

   //! returns the type name as a c string
   /**
      @return the type name as a c string
   */
   DLLEXPORT virtual const char* getTypeName() const;

   //! add the argument to this value and return the result
   DLLEXPORT QoreNumberNode* doPlus(const QoreNumberNode& n) const;

   //! add the argument to this value and return the result
   DLLEXPORT QoreNumberNode* doMinus(const QoreNumberNode& n) const;

   //! add the argument to this value and return the result
   DLLEXPORT QoreNumberNode* doMultiply(const QoreNumberNode& n) const;

   //! add the argument to this value and return the result
   DLLEXPORT QoreNumberNode* doDivideBy(const QoreNumberNode& n, ExceptionSink* xsink) const;

   //! returns the negative of the current object (this)
   DLLEXPORT QoreNumberNode* negate() const;

   //! returns true if the number is zero
   DLLEXPORT bool zero() const;

   //! returns -1 if the number is negative, 0 if zero, or 1 if the number is positive
   DLLEXPORT int sign() const;

   //! compares the argument to the current object, returns -1 if the current object is less than the argument, 0 if equals, and 1 if greater than the argument
   DLLEXPORT int compare(const QoreNumberNode& n) const;

   //! compares the argument to the current object, returns -1 if the current object is less than the argument, 0 if equals, and 1 if greater than the argument
   DLLEXPORT int compare(int64 n) const;

   //! compares the argument to the current object, returns -1 if the current object is less than the argument, 0 if equals, and 1 if greater than the argument
   DLLEXPORT int compare(double n) const;

   //! returns a pointer to this with the reference count incremented
   DLLEXPORT QoreNumberNode* numberRefSelf() const;

   //! concatenates the string value corresponding to the number to the string given
   /** @param str the string to append to
       @param fmt a bitfield of @ref number_format_flags "number format flags"
    */
   DLLEXPORT void toString(QoreString& str, int fmt = QORE_NF_DEFAULT) const;

   //! returns the precision of the number
   DLLEXPORT unsigned getPrec() const;

   //! returns the type information
   DLLLOCAL virtual AbstractQoreNode* parseInit(LocalVar* oflag, int pflag, int& lvids, const QoreTypeInfo*& typeInfo);

   //! returns the representation of the value as a number if possible (otherwise returns 0), caller owns the reference returned
   DLLEXPORT static QoreNumberNode* toNumber(const AbstractQoreNode* v);

   //! returns the type name (useful in templates)
   /**
      @return the type name as a c string
   */
   DLLLOCAL static const char* getStaticTypeName() {
      return "number";
   }

   //! returns the type code (useful in templates)
   DLLLOCAL static qore_type_t getStaticTypeCode() {
      return NT_NUMBER;
   }
};

#endif
