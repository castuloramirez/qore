/*
  QoreNumberNode.cpp
  
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
#include <qore/intern/qore_number_private.h>

void qore_number_private::getAsString(QoreString& str, bool round) const {
   // first check for zero
   if (zero()) {
      str.concat("0");
      return;
   }

   mpfr_exp_t exp;

   char* buf = mpfr_get_str(0, &exp, 10, 0, num, QORE_MPFR_RND);
   if (!buf) {
      numError(str);
      return;
   }
   ON_BLOCK_EXIT(mpfr_free_str, buf);

   //printd(5, "qore_number_private::getAsString(round: %d) this: %p buf: '%s'\n", round, this, buf);

   // if it's a regular number, then format accordingly
   if (number()) {
      int sgn = sign();
      qore_size_t len = str.size() + (sgn < 0 ? 1 : 0);
      //printd(5, "qore_number_private::getAsString() this: %p '%s' exp "QLLD" len: "QLLD"\n", this, buf, exp, len);

      qore_size_t dp = 0;

      str.concat(buf);
      // trim the trailing zeros off the end
      str.trim_trailing('0');
      if (exp <= 0) {
	 exp = -exp;
	 str.insert("0.", len);
	 dp = len + 2;
	 if (exp)
	    str.insertch('0', len + 2, exp);
      }
      else {
	 // get remaining length of string (how many characters were added)
	 qore_size_t rlen = str.size() - len;

	 //printd(0, "qore_number_private::getAsString() this: %p str: '%s' rlen: "QLLD"\n", this, str.getBuffer(), rlen);

	 // assert that we have added at least 1 character
	 assert(rlen > 0);
	 if ((qore_size_t)exp > rlen)
	    str.insertch('0', str.size(), exp - rlen);
	 else if ((qore_size_t)exp < rlen) {
	    str.insertch('.', len + exp, 1);
	    dp = len + exp;
	 }
      }
      // try to do some rounding (noise reduction with binary->decimal conversions)
      if (dp && round)
         applyRoundingHeuristic(str, dp, str.size());
   }
   else
      str.concat(buf);

   //printd(5, "qore_number_private::getAsString() this: %p returning '%s'\n", this, str.getBuffer());
}

void qore_number_private::applyRoundingHeuristic(QoreString& str, qore_size_t dp, qore_size_t last) {
   // if there are some significant digits after the decimal point (signal)
   bool signal = false;
   // the position of the last significant digit
   qore_offset_t pos = (qore_offset_t)dp;
   qore_size_t i = dp;
   // the last digit found in the sequence
   char lc = 0;
   // 0 or 9 count
   unsigned cnt = 0;
   // don't check the last character
   --last;
   // check all except the last digit
   while (i < last) {
      char c = str[i++];

      if (c == '0' || c == '9') {
         // continue the sequence
         if (c == lc) {
            ++cnt;
            continue;
         }
         // check for 2nd threshold
         if ((i == last) && cnt > QORE_MPFR_ROUND_THRESHOLD_2) {
            ++cnt;
            break;
         }

         // set last digit to digit found
         lc = c;
         // if first digit, then do not set signal flag
         if (i == (dp + 1))
            continue;
      }
      else {
         // check for 2nd threshold
         if ((i == last) && cnt > QORE_MPFR_ROUND_THRESHOLD_2) {
            ++cnt;
            break;
         }
         // no 0 or 9 digit found
         lc = 0;
      }

      // mark position of the last significant digit
      pos = i - 2;
      //printd(5, "qore_number_private::applyRoundingHeuristic('%s') set pos: %lld ('%c') dp: %lld\n", str.getBuffer(), pos, str[pos], dp);

      // found a non-noise digit
      if (!signal)
         signal = true;
      // reset count
      cnt = 0;
   }

   // round the number for display
   if (signal && cnt > QORE_MPFR_ROUND_THRESHOLD) {
      //printd(5, "ROUND BEFORE: (cnt: %d) %s\n", cnt, str.getBuffer());
      // if rounding right after the decimal point, then remove the decimal point
      if (pos == (qore_offset_t)dp)
         --pos;
      // remove the excess digits
      str.replace(pos + 1, cnt + 3, (const char*)0);

      // rounding down is easy; the truncation is enough
      if (lc == '9') // round up
         roundUp(str, pos);
      //printd(5, "ROUND AFTER: %s\n", str.getBuffer());
   }
}

int qore_number_private::roundUp(QoreString& str, qore_offset_t pos) {
   for (; pos >= 0; --pos) {
      char c = str[pos];
      if (c == '.')
         continue;
      if (!pos && c == '-')
         break;
      if (c < '9') {
         str.replaceChar(pos, c + 1);
         break;
      }
      str.replaceChar(pos, '0');
   }
   if (pos == -1 || (!pos && str[0] == '-')) {
      str.insertch('1', pos + 1, 1);
      return 1;
   }
   return 0;
}

int qore_number_private::formatNumberString(QoreString& num, const QoreString& fmt, ExceptionSink* xsink) {
   assert(!num.empty());
   assert(num.getEncoding() == fmt.getEncoding());
   // get the length of the format string in characters (not bytes)
   qore_size_t fl = fmt.length();
   if (fmt.empty() || fl == 2) {
      printd(5, "qore_number_private::formatNumberString() invalid format string: '%s' for number: '%s'\n", fmt.getBuffer(), num.getBuffer());
      return 0;
   }

   // get thousands separator character
   QoreString tsep;
   if (tsep.concat(fmt, 0, 1, xsink))
      return -1;

   // decimal separator
   QoreString dsep;
   // number of digits after the decimal separator
   unsigned prec = 0;
   if (fl > 1) {
      if (dsep.concat(fmt, 1, 1, xsink))
         return -1;
      // get byte offset of start of decimal precision number
      qore_offset_t i = fmt.getByteOffset(2, xsink);
      if (*xsink)
         return -1;
      assert(i >= 2);
      prec = atoi(fmt.getBuffer() + i);
      if (!prec)
         dsep.clear();
   }

   //printd(5, "qore_number_private::formatNumberString() tsep: '%s' dsep: '%s' prec: %d '%s'\n", tsep.getBuffer(), dsep.getBuffer(), prec, num.getBuffer());

   // find decimal point
   qore_offset_t dp = num.find('.');
   if (dp != -1) {
      // how many digits do we have now after the decimal point
      qore_size_t d = num.strlen() - dp - 1;
      assert(d);
      if (d < prec)
         num.addch('0', prec - d);
      else if (d > prec) {
         if ((num[dp + prec + 1] > '4') && (roundUp(num, dp + prec)))
            ++dp;
         num.terminate(dp + prec + 1);
      }
      // now substitute decimal point if necessary
      if (dsep.strlen() != 1 || dsep[0] != '.')
         num.replace(dp, 1, dsep.getBuffer());
   }
   else {
      dp = num.size();
      if (prec) {
         // add decimal point
         num.concat(&dsep, xsink);
         assert(!*xsink);
         // add zeros for significant digits
         num.addch('0', prec);
      }
   }

   // now insert thousands separator
   // start of digits before the decimal point
   qore_offset_t ds = num[0] == '-' ? 1 : 0;

   // work backwards from the decimal point
   qore_offset_t i = dp - 3;
   while (i > ds) {
      num.replace(i, 0, tsep.getBuffer());
      i -= 3;
   }

   //printd(0, "qore_number_private::formatNumberString() ok '%s'\n", num.getBuffer());

   //assert(false); xxx
   return 0;
}

QoreNumberNode::QoreNumberNode(struct qore_number_private* p) : SimpleValueQoreNode(NT_NUMBER), priv(p) {
}

QoreNumberNode::QoreNumberNode(const AbstractQoreNode* n) : SimpleValueQoreNode(NT_NUMBER), priv(0) {
   qore_type_t t = get_node_type(n);
   if (t == NT_NUMBER) {
      priv = new qore_number_private(*reinterpret_cast<const QoreNumberNode*>(n)->priv);
      return;
   }

   if (t == NT_FLOAT) {
      priv = new qore_number_private(reinterpret_cast<const QoreFloatNode*>(n)->f);
      return;
   }

   if (t == NT_STRING) {
      priv = new qore_number_private(reinterpret_cast<const QoreStringNode*>(n)->getBuffer());
      return;
   }

   if (t == NT_INT || (t > QORE_NUM_TYPES && dynamic_cast<const QoreBigIntNode*>(n))) {
      priv = new qore_number_private(reinterpret_cast<const QoreBigIntNode*>(n)->val);
      return;
   }

   if (t != NT_BOOLEAN
       && t != NT_DATE
       && t != NT_NULL) {
      priv = new qore_number_private(0ll);
      return;
   }

   priv = new qore_number_private(n->getAsFloat());
}

QoreNumberNode::QoreNumberNode(double f) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(f)) {
}

QoreNumberNode::QoreNumberNode(int64 i) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(i)) {
}

QoreNumberNode::QoreNumberNode(const char* str) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(str)) {
}

QoreNumberNode::QoreNumberNode(const char* str, unsigned prec) : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(str, prec)) {
}

QoreNumberNode::QoreNumberNode() : SimpleValueQoreNode(NT_NUMBER), priv(new qore_number_private(0ll)) {
}

QoreNumberNode::QoreNumberNode(const QoreNumberNode& old) : SimpleValueQoreNode(old), priv(new qore_number_private(*old.priv)) {
}

QoreNumberNode::~QoreNumberNode() {
   delete priv;
}

// get the value of the type in a string context (default implementation = del = false and returns NullString)
// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
// use the QoreStringValueHelper class (defined in QoreStringNode.h) instead of using this function directly
QoreString* QoreNumberNode::getStringRepresentation(bool& del) const {
   del = true;
   QoreString* str = new QoreString;
   priv->getAsString(*str);
   return str;
}

// concatenate string representation to a QoreString (no action for complex types = default implementation)
void QoreNumberNode::getStringRepresentation(QoreString& str) const {
   priv->getAsString(str);
}

// if del is true, then the returned DateTime * should be deleted, if false, then it should not
DateTime *QoreNumberNode::getDateTimeRepresentation(bool& del) const {
   del = true;
   double f = priv->getAsFloat();
   return DateTime::makeAbsoluteLocal(currentTZ(), (int64)f, (int)((f - (float)((int)f)) * 1000000));
}

// assign date representation to a DateTime (no action for complex types = default implementation)
void QoreNumberNode::getDateTimeRepresentation(DateTime& dt) const {
   double f = priv->getAsFloat();
   dt.setLocalDate(currentTZ(), (int64)f, (int)((f - (float)((int)f)) * 1000000));
}

bool QoreNumberNode::getAsBoolImpl() const {
   return priv->getAsBool();
}

int QoreNumberNode::getAsIntImpl() const {
   return priv->getAsBigInt();
}

int64 QoreNumberNode::getAsBigIntImpl() const {
   return priv->getAsBigInt();
}

double QoreNumberNode::getAsFloatImpl() const {
   return priv->getAsFloat();
}

int QoreNumberNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
   getStringRepresentation(str);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreNumberNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
   return getStringRepresentation(del);
}

AbstractQoreNode* QoreNumberNode::realCopy() const {
   return new QoreNumberNode(*this);
}

// the type passed must always be equal to the current type
bool QoreNumberNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
   if (v->getType() == NT_NUMBER)
      return !priv->compare(*reinterpret_cast<const QoreNumberNode*>(v)->priv);
   if (v->getType() == NT_INT || dynamic_cast<const QoreBigIntNode*>(v))
      return !priv->compare(reinterpret_cast<const QoreBigIntNode*>(v)->val);

   return !priv->compare(v->getAsFloat());
}

bool QoreNumberNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
   if (v->getType() != NT_NUMBER)
      return false;
   const QoreNumberNode* n = reinterpret_cast<const QoreNumberNode*>(v);
   return !priv->compare(*n->priv);
}

// returns the type name as a c string
const char* QoreNumberNode::getTypeName() const {
   return getStaticTypeName();
}

AbstractQoreNode* QoreNumberNode::parseInit(LocalVar* oflag, int pflag, int& lvids, const QoreTypeInfo*& typeInfo) {
   typeInfo = numberTypeInfo;
   return this;
}

bool QoreNumberNode::zero() const {
   return priv->zero();
}

int QoreNumberNode::sign() const {
   return priv->sign();
}

QoreNumberNode* QoreNumberNode::doPlus(const QoreNumberNode& right) const {
   return new QoreNumberNode(priv->doPlus(*right.priv));
}

//! add the argument to this value and return the result
QoreNumberNode* QoreNumberNode::doMinus(const QoreNumberNode& n) const {
   return new QoreNumberNode(priv->doMinus(*n.priv));
}

//! add the argument to this value and return the result
QoreNumberNode* QoreNumberNode::doMultiply(const QoreNumberNode& n) const {
   return new QoreNumberNode(priv->doMultiply(*n.priv));
}

//! add the argument to this value and return the result
QoreNumberNode* QoreNumberNode::doDivideBy(const QoreNumberNode& n, ExceptionSink* xsink) const {
   qore_number_private* p = priv->doDivideBy(*n.priv, xsink);
   return p ? new QoreNumberNode(p) : 0;
}

QoreNumberNode* QoreNumberNode::negate() const {
   return new QoreNumberNode(priv->negate());
}

int QoreNumberNode::compare(const QoreNumberNode& n) const {
   return priv->compare(*n.priv);
}

QoreNumberNode* QoreNumberNode::numberRefSelf() const {
   ref();
   return const_cast<QoreNumberNode*>(this);
}

void QoreNumberNode::toString(QoreString& str, int fmt) const {
   priv->toString(str, fmt);
}

unsigned QoreNumberNode::getPrec() const {
   return priv->getPrec();
}
