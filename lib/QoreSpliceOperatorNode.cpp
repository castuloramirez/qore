/*
 QoreSpliceOperatorNode.cpp
 
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
#include <qore/intern/qore_program_private.h>

QoreString QoreSpliceOperatorNode::splice_str("splice operator expression");

// if del is true, then the returned QoreString * should be spliced, if false, then it must not be
QoreString *QoreSpliceOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = false;
   return &splice_str;
}

int QoreSpliceOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
   str.concat(&splice_str);
   return 0;
}

AbstractQoreNode *QoreSpliceOperatorNode::evalImpl(ExceptionSink *xsink) const {
   return splice(xsink);
}

AbstractQoreNode *QoreSpliceOperatorNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
   needs_deref = ref_rv;
   return splice(xsink);
}

AbstractQoreNode *QoreSpliceOperatorNode::parseInitImpl(LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&typeInfo) {
   const QoreTypeInfo *expTypeInfo = 0;
   
   pflag &= ~PF_RETURN_VALUE_IGNORED;

   // check lvalue expression
   lvalue_exp = lvalue_exp->parseInit(oflag, pflag | PF_FOR_ASSIGNMENT, lvids, expTypeInfo);
   if (lvalue_exp && check_lvalue(lvalue_exp))
      parse_error("the splice operator expects an lvalue as the first expression, got '%s' instead", lvalue_exp->getTypeName());

   if (expTypeInfo->hasType()) {
      if (!expTypeInfo->parseAcceptsReturns(NT_LIST)
          && !expTypeInfo->parseAcceptsReturns(NT_STRING)) {
	 QoreStringNode *desc = new QoreStringNode("the lvalue expression (1st position) with the 'splice' operator is ");
	 expTypeInfo->getThisType(*desc);
	 desc->sprintf(", therefore this operation is invalid and would throw an exception at run-time; the 'splice' operator only operates on lists and strings");
	 qore_program_private::makeParseException(getProgram(), "PARSE-TYPE-ERROR", desc);
      }
      else
	 returnTypeInfo = typeInfo = expTypeInfo;
   }

   // check offset expression
   expTypeInfo = 0;
   offset_exp = offset_exp->parseInit(oflag, pflag, lvids, expTypeInfo);
   if (expTypeInfo->nonNumericValue())
      expTypeInfo->doNonNumericWarning("the offset expression (2nd position) with the 'splice' operator is ");

   // check length expression, if any
   if (length_exp) {
      expTypeInfo = 0;
      length_exp = length_exp->parseInit(oflag, pflag, lvids, expTypeInfo);
      if (expTypeInfo->nonNumericValue())
	 expTypeInfo->doNonNumericWarning("the length expression (3nd position) with the 'splice' operator is ");
   }

   // check new value expression, if any
   if (new_exp)
      new_exp = new_exp->parseInit(oflag, pflag, lvids, expTypeInfo);

   return this;
}

AbstractQoreNode *QoreSpliceOperatorNode::splice(ExceptionSink *xsink) const {
   printd(5, "QoreSpliceOperatorNode::splice() lvalue_exp = %p, offset_exp=%p, length_exp=%p, new_exp=%p, isEvent=%d\n", lvalue_exp, offset_exp, length_exp, new_exp, xsink->isEvent());

   // evaluate arguments
   QoreNodeEvalOptionalRefHolder eoffset(offset_exp, xsink);
   if (*xsink)
      return 0;

   QoreNodeEvalOptionalRefHolder elength(length_exp, xsink);
   if (*xsink)
      return 0;

   QoreNodeEvalOptionalRefHolder exp(new_exp, xsink);
   if (*xsink)
      return 0;

   // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
   LValueHelper val(lvalue_exp, xsink);
   if (!val)
      return 0;

   // if value is not a list or string, throw exception
   qore_type_t vt = val.getType();

   if (vt == NT_NOTHING) {
      // see if the lvalue has a default type
      const QoreTypeInfo *typeInfo = val.getTypeInfo();
      if (typeInfo == softListTypeInfo || typeInfo == listTypeInfo || typeInfo == stringTypeInfo || typeInfo == softStringTypeInfo) {
	 if (val.assign(typeInfo->getDefaultValue()))
	    return 0;
	 vt = val.getType();
      }
   }

   if (vt != NT_LIST && vt != NT_STRING) {
      xsink->raiseException("SPLICE-ERROR", "first (lvalue) argument to the splice operator is not a list or a string");
      return 0;
   }
   
   // no exception can occur here
   val.ensureUnique();

   qore_size_t offset = eoffset ? (qore_size_t)eoffset->getAsBigInt() : 0;

#ifdef DEBUG
   if (vt == NT_LIST) {
      QoreListNode *vl = reinterpret_cast<QoreListNode *>(val.getValue());
      printd(5, "op_splice() val: %p (size: "QSD") offset: "QSD"\n", vl, vl->size(), offset);
   }
   else {
      QoreStringNode *vs = reinterpret_cast<QoreStringNode *>(val.getValue());
      printd(5, "op_splice() val: %p (strlen: "QSD") offset: "QSD"\n", vs, vs->strlen(), offset);
   }
#endif

   if (vt == NT_LIST) {
      QoreListNode *vl = reinterpret_cast<QoreListNode *>(val.getValue());
      if (!length_exp && !new_exp)
	 vl->splice(offset, xsink);
      else {
	 qore_size_t length = elength ? (qore_size_t)elength->getAsBigInt() : 0;
	 if (!new_exp)
	    vl->splice(offset, length, xsink);
	 else	    
	    vl->splice(offset, length, *exp, xsink);
      }
   }
   else { // must be a string
      QoreStringNode *vs = reinterpret_cast<QoreStringNode *>(val.getValue());
      if (!length_exp && !new_exp)
	 vs->splice(offset, xsink);
      else {
	 qore_size_t length = elength ? (qore_size_t)elength->getAsBigInt() : 0;
	 if (!new_exp)
	    vs->splice(offset, length, xsink);
	 else
	    vs->splice(offset, length, *exp, xsink);
      }
   }

   // reference for return value
   return ref_rv ? val.getReferencedValue() : 0;
}
