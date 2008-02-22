/*
  QoreString.h

  QoreString Class Definition

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

#ifndef _QORE_QORESTRINGNODE_H

#define _QORE_QORESTRINGNODE_H

#include <qore/AbstractQoreNode.h>
#include <qore/QoreString.h>

//! Qore's string value type, reference counted, dynamically-allocated only
/** for a version that can be used on the stack, use QoreString
    @see QoreString
    @see QoreEncoding
 */
class QoreStringNode : public SimpleQoreNode, public QoreString
{
   private:
      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreStringNode(QoreString *str);

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreStringNode& operator=(const QoreStringNode&);

      DLLLOCAL virtual bool getAsBoolImpl() const;
      DLLLOCAL virtual int getAsIntImpl() const;
      DLLLOCAL virtual int64 getAsBigIntImpl() const;
      DLLLOCAL virtual double getAsFloatImpl() const;

   protected:
      //! destructor only called when references = 0, use deref() instead
      DLLEXPORT virtual ~QoreStringNode();

   public:
      //! creates an empty string and assigns the default encoding QCS_DEFAULT
      DLLEXPORT QoreStringNode();

      //! creates a new object from a string and sets the character encoding
      /**
	 @param str the string data is copied to the new QoreStringNode object
	 @param enc the encoding for the string
       */
      DLLEXPORT QoreStringNode(const char *str, const class QoreEncoding *enc = QCS_DEFAULT);

      //! creates a new QoreStringNode from an existing QoreString reference
      /**
	 @param str copies the data into the new QoreStringNode
      */
      DLLEXPORT QoreStringNode(const QoreString &str);

      //! creates a new QoreStringNode from an existing QoreStringNode reference
      /**
	 @param str copies the data into the new QoreStringNode
      */
      DLLEXPORT QoreStringNode(const QoreStringNode &str);

      //! creates a new object from a std::string and sets the character encoding
      /**
	 @param str the string data is copied to the new QoreStringNode object
	 @param enc the encoding for the string
       */
      DLLEXPORT QoreStringNode(const std::string &str, const class QoreEncoding *enc = QCS_DEFAULT);

      // copies binary object and makes a base64-encoded string out of it
      DLLEXPORT QoreStringNode(const class BinaryNode *b);

      //! creates a new object; takes ownership of the char pointer passed, all parameters are mandatory
      /**
	 @param nbuf the pointer for the character data
	 @param nlen the length of the string in bytes (not including the trailing '\0')
	 @param nallocated the number of bytes allocated for this buffer (if unknown, set to nlen + 1)
	 @param enc the encoding for the string
      */
      DLLEXPORT QoreStringNode(char *nbuf, int nlen, int nallocated, const class QoreEncoding *enc);

      // copies str
      DLLEXPORT QoreStringNode(const char *str, int len, const class QoreEncoding *new_qorecharset = QCS_DEFAULT);

      // creates a string from a single character
      DLLEXPORT QoreStringNode(char c);

      //! concatenates the string data in double quotes to an existing QoreString
      /** used for %n and %N printf formatting.  An exception may be thrown if there is an encoding conversion error
	  @param str the string representation of the type will be concatenated to this QoreString reference
	  @param foff for multi-line formatting offset, -1 = no line breaks
	  @param xsink if an error occurs, the Qore-language exception information will be added here
	  @return -1 for exception raised, 0 = OK
      */
      DLLEXPORT int getAsString(QoreString &str, int foff, class ExceptionSink *xsink) const;

      //! returns a QoreString giving the string data in double quotes
      /** used for %n and %N printf formatting
	  @param del if this is true when the function returns, then the returned QoreString pointer should be deleted, if false, then it must not be
	  @param foff for multi-line formatting offset, -1 = no line breaks
	  @param xsink if an error occurs, the Qore-language exception information will be added here
	  NOTE: Use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using this function directly
	  @see QoreNodeAsStringHelper
      */
      DLLEXPORT QoreString *getAsString(bool &del, int foff, class ExceptionSink *xsink) const;

      //! returns the current string and sets del to false
      /** NOTE: do not call this function directly, use QoreStringValueHelper instead
	  @param del output parameter: always sets del to false
	  @see QoreStringValueHelper
       */
      DLLEXPORT virtual QoreString *getStringRepresentation(bool &del) const;

      //! concatentates the value of the type to an existing QoreString reference
      /**
	 @param str a reference to a QoreString where the value of the type will be concatenated
       */
      DLLEXPORT virtual void getStringRepresentation(QoreString &str) const;

      //! returns the DateTime representation of this string
      /** NOTE: Use the DateTimeValueHelper class instead of using this function directly
	  @param del output parameter: if del is true, then the returned DateTime pointer belongs to the caller (and must be deleted manually), if false, then it must not be
	  @see DateTimeValueHelper
       */
      DLLEXPORT virtual class DateTime *getDateTimeRepresentation(bool &del) const;

      //! assigns the date representation of this string to the DateTime reference passed
      /** 
	  @param dt the DateTime reference to be assigned
       */
      DLLEXPORT virtual void getDateTimeRepresentation(DateTime &dt) const;

      //! returns a copy of the object, the caller owns the reference count
      DLLEXPORT virtual class AbstractQoreNode *realCopy() const;

      //! tests for equality ("deep compare" including all contained values for container types) with possible type and character encoding conversion (soft compare)
      /** An exception could be raised if character set encoding is required to do the compare the the conversion fails
	  @param v the value to compare
	  @param xsink if an error occurs, the Qore-language exception information will be added here
       */
      DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const;

      //! tests for equality ("deep compare" including all contained values for container types) without type or character encoding conversions (hard compare)
      /** if the character encodings of the two strings differ, the comparison fails immediately
	  this function does not throw any Qore-language exceptions as no character set encoding conversions are made
	  @param v the value to compare
	  @param xsink is not used in this implementation of the function
       */
      DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const;

      //! returns the data type
      DLLEXPORT virtual const QoreType *getType() const;

      //! returns the type name as a c string
      DLLEXPORT virtual const char *getTypeName() const;

      //! converts the encoding of the string to the specified encoding, returns 0 if an error occurs, the caller owns the pointer returned
      /** if the encoding is the same as the current encoding, a copy of the string is returned
	  @param nccs the encoding for the new string
	  @param xsink if an error occurs, the Qore-language exception information will be added here
	  @return the new string with the desired encoding or 0 if an error occured
       */
      DLLEXPORT QoreStringNode *convertEncoding(const class QoreEncoding *nccs, class ExceptionSink *xsink) const;

      //! returns a new string consisting of all the characters from the current string starting with character position "offset"
      /** offset is a character offset and not a byte offset
	  @param offset the offset in characters from the beginning of the string (starting with 0)
	  @return the new string
       */
      DLLEXPORT QoreStringNode *substr(int offset) const;

      //! returns a new string consisting of "length" characters from the current string starting with character position "offset"
      /** offset and length spoecify characters, not bytes
	  @param offset the offset in characters from the beginning of the string (starting with 0)
	  @return the new string
       */
      DLLEXPORT QoreStringNode *substr(int offset, int length) const;

      //! return a QoreStringNode with the characters reversed
      DLLEXPORT QoreStringNode *reverse() const;

      // copy function
      DLLEXPORT QoreStringNode *copy() const;

      //! creates a new QoreStringNode from a string and converts its encoding
      DLLEXPORT static QoreStringNode *createAndConvertEncoding(const char *str, const class QoreEncoding *from, const class QoreEncoding *to, ExceptionSink *xsink);

      //! parses the string as a base64-encoded binary and returns the decoded value as a QoreStringNode
      DLLEXPORT class QoreStringNode *parseBase64ToString(class ExceptionSink *xsink) const;

      //! constructor supporting createAndConvertEncoding(), not exported in the library
      DLLLOCAL QoreStringNode(const char *str, const class QoreEncoding *from, const class QoreEncoding *to, ExceptionSink *xsink);

      //! constructor using the private implementation of QoreString; not exported in the library
      DLLLOCAL QoreStringNode(struct qore_string_private *p);

      DLLLOCAL static const char *getStaticTypeName()
      {
	 return "string";
      }
};

extern QoreStringNode *NullString;

//! this class is used to safely manage calls to AbstractQoreNode::getStringRepresentation() when a simple QoreString value is needed, stack only, may not be dynamically allocated
/** the QoreString value returned by this function is managed safely in an exception-safe way with this class
    \Example
    \code
    QoreStringValueHelper str(n);
    printf("str='%s'\n", str->getBuffer());
    \endcode
 */
class QoreStringValueHelper {
   private:
      QoreString *str;
      bool del;

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreStringValueHelper(const QoreStringValueHelper&); // not implemented

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreStringValueHelper& operator=(const QoreStringValueHelper&); // not implemented

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL void* operator new(size_t); // not implemented, make sure it is not new'ed

   public:
      //! creates the object and acquires a pointer to the QoreString representation of the AbstractQoreNode passed
      DLLLOCAL QoreStringValueHelper(const AbstractQoreNode *n)
      {
	 if (n) {
	    //optimization to remove the need for a virtual function call in the most common case
	    if (n->getType() == NT_STRING) {
	       del = false;
	       str = const_cast<QoreStringNode *>(reinterpret_cast<const QoreStringNode *>(n));
	    }
	    else
	       str = n->getStringRepresentation(del);
	 }
	 else {
	    str = NullString;
	    del = false;
	 }
      }

      //! gets the QoreString representation and ensures that it's in the desired encoding
      /** a Qore-language exception may be thrown if an encoding error occurs
       */
      DLLLOCAL QoreStringValueHelper(const AbstractQoreNode *n, const QoreEncoding *enc, ExceptionSink *xsink)
      {
	 if (n) {
	    //optimization to remove the need for a virtual function call in the most common case
	    if (n->getType() == NT_STRING) {
	       del = false;
	       str = const_cast<QoreStringNode *>(reinterpret_cast<const QoreStringNode *>(n));
	    }
	    else
	       str = n->getStringRepresentation(del);
	    if (str->getEncoding() != enc) {
	       QoreString *t = str->convertEncoding(enc, xsink);
	       if (!t)
		  return;
	       if (del)
		  delete str;
	       str = t;
	       del = true;
	    }
	 }
	 else {
	    str = NullString;
	    del = false;
	 }
      }

      //! destroys the object and deletes the QoreString pointer being managed if it was a temporary pointer
      DLLLOCAL ~QoreStringValueHelper()
      {
	 if (del)
	    delete str;
      }

      //! returns the object being managed
      /**
	 @return the object being managed
       */
      DLLLOCAL const QoreString *operator->() { return str; }

      //! returns the object being managed
      /**
	 @return the object being managed
       */
      DLLLOCAL const QoreString *operator*() { return str; }

      //! returns a copy of the QoreString that the caller owns
      /** the object may be left empty after this call
	  @return a QoreString pointer owned by the caller
       */
      DLLLOCAL QoreString *giveString()
      {
	 if (!str)
	    return 0;
	 if (!del)
	    return str->copy();

	 QoreString *rv = str;
	 del = false;
	 str = 0;
	 return rv;
      }

      //! returns true if the pointer being managed is temporary
      DLLLOCAL bool is_temp() const
      {
	 return del;
      }
};

//! this class is used to safely manage calls to AbstractQoreNode::getStringRepresentation() when a QoreStringNode value is needed, stack only, may not be dynamically allocated
/** the QoreStringNode value returned by this function is managed safely in an exception-safe way with this class
    \Example
    \code
    QoreStringNodeValueHelper str(n);
    printf("str='%s'\n", str->getBuffer());
    return str.getReferencedValue();
    \endcode
 */
class QoreStringNodeValueHelper {
   private:
      QoreStringNode *str;
      bool temp;

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreStringNodeValueHelper(const QoreStringNodeValueHelper&);

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreStringNodeValueHelper& operator=(const QoreStringNodeValueHelper&);

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL void* operator new(size_t);

   public:
      DLLLOCAL QoreStringNodeValueHelper(const AbstractQoreNode *n)
      {
	 if (!n) {
	    str = NullString;
	    temp = false;
	    return;
	 }

	 const QoreType *ntype = n->getType();
	 if (ntype == NT_STRING) {
	    str = const_cast<QoreStringNode *>(reinterpret_cast<const QoreStringNode *>(n));
	    temp = false;
	 }
	 else {
	    str = new QoreStringNode();
	    n->getStringRepresentation(*(static_cast<QoreString *>(str)));
	    temp = true;
	 }
      }

      //! destroys the object and dereferences the QoreStringNode if it is a temporary pointer
      DLLLOCAL ~QoreStringNodeValueHelper()
      {
	 if (temp)
	    str->deref();
      }

      //! returns the object being managed
      /**
	 @return the object being managed
       */
      DLLLOCAL const QoreStringNode *operator->() { return str; }

      //! returns the object being managed
      /**
	 @return the object being managed
       */
      DLLLOCAL const QoreStringNode *operator*() { return str; }

      //! returns a referenced value - the caller will own the reference
      /**
	 The string is referenced if necessary (if it was a temporary value)
	 @return the string value, where the caller will own the reference count
      */
      DLLLOCAL QoreStringNode *getReferencedValue()
      {
	 if (temp)
	    temp = false;
	 else if (str)
	    str->ref();
	 return str;
      }
};

#include <qore/ReferenceHolder.h>

//! For use on the stack only: manages a QoreStringNode reference count
/**
   @see SimpleRefHolder
 */
typedef SimpleRefHolder<QoreStringNode> QoreStringNodeHolder;

extern QoreString NothingTypeString;

//! safely manages the return values to AbstractQoreNode::getAsString(), stack only, cannot be dynamically allocated
/**
   \Example
   \code
   // FMT_NONE gives all information on a single line
   QoreNodeAsStringHelper str(n, FMT_NONE, xsink);
   if (*xsink)
      return 0;
    printf("str='%s'\n", str->getBuffer());
   \endcode
 */
class QoreNodeAsStringHelper {
   private:
      QoreString *str;
      bool del;

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreNodeAsStringHelper(const QoreNodeAsStringHelper&); // not implemented

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL QoreNodeAsStringHelper& operator=(const QoreNodeAsStringHelper&); // not implemented

      //! this function is not implemented; it is here as a private function in order to prohibit it from being used
      DLLLOCAL void* operator new(size_t); // not implemented, make sure it is not new'ed

   public:
      //! makes the call to AbstractQoreNode::getAsString() and manages the return values
      DLLLOCAL QoreNodeAsStringHelper(const AbstractQoreNode *n, int offset, ExceptionSink *xsink)
      {
	 if (n)
	    str = n->getAsString(del, offset, xsink);
	 else {
	    str = &NothingTypeString;
	    del = false;
	 }
      }

      //! destroys the object and deletes the QoreString pointer being managed if it was a temporary pointer
      DLLLOCAL ~QoreNodeAsStringHelper()
      {
	 if (del)
	    delete str;
      }

      //! returns the object being managed
      /**
	 @return the object being managed
       */
      DLLLOCAL const QoreString *operator->() { return str; }

      //! returns the object being managed
      /**
	 @return the object being managed
       */
      DLLLOCAL const QoreString *operator*() { return str; }

      //! returns a copy of the QoreString that the caller owns
      /** the object may be left empty after this call
	  @return a QoreString pointer owned by the caller
       */
      DLLLOCAL QoreString *giveString()
      {
	 if (!str)
	    return 0;
	 if (!del)
	    return str->copy();

	 QoreString *rv = str;
	 del = false;
	 str = 0;
	 return rv;
      }
};

#endif
