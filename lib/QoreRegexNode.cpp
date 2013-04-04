/*
 QoreRegexNode.cpp
 
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
#include <qore/intern/qore_program_private.h>

QoreRegexNode::QoreRegexNode() : ParseNoEvalNode(NT_REGEX) {
   init();
   str = new QoreString();
}

QoreRegexNode::QoreRegexNode(QoreString *s) : ParseNoEvalNode(NT_REGEX) {
   init();
   str = s;
   parse();
}

QoreRegexNode::QoreRegexNode(const QoreString &s, int64 opts, ExceptionSink *xsink) : ParseNoEvalNode(NT_REGEX) {
   init(opts);
   str = 0;

   if (check_re_options(options)) {
      xsink->raiseException("REGEX-OPTION-ERROR", "%d contains invalid option bits", opts);
      options = 0;
   }

   parseRT(&s, xsink);
}

QoreRegexNode::~QoreRegexNode() {
   if (p)
      pcre_free(p);
   if (str)
      delete str;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int QoreRegexNode::getAsString(QoreString &mstr, int foff, ExceptionSink *xsink) const {
   mstr.sprintf("regular expression (0x%08p)", this);
   return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreRegexNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = true;
   QoreString *rv = new QoreString();
   getAsString(*rv, foff, xsink);
   return rv;
}

// returns the data type
qore_type_t QoreRegexNode::getType() const {
   return NT_REGEX;
}

// returns the type name as a c string
const char *QoreRegexNode::getTypeName() const {
   return "regular expression";
}

void QoreRegexNode::concat(char c) {
   str->concat(c);
}

void QoreRegexNode::parseRT(const QoreString *pattern, ExceptionSink *xsink) {
   const char *err;
   int eo;
   
   // convert to UTF-8 if necessary
   TempEncodingHelper t(pattern, QCS_UTF8, xsink);
   if (*xsink)
      return;

   //printd(5, "QoreRegexNode::parseRT(%s) this=%p\n", t->getBuffer(), this);
   
   p = pcre_compile(t->getBuffer(), options, &err, &eo, 0);
   if (err) {
      //printd(5, "QoreRegexNode::parse() error parsing '%s': %s", t->getBuffer(), (char *)err);
      xsink->raiseException("REGEX-COMPILATION-ERROR", (char *)err);
   }
}

void QoreRegexNode::parse() {
   ExceptionSink xsink;
   parseRT(str, &xsink);
   delete str;
   str = 0;
   if (xsink.isEvent())
      qore_program_private::addParseException(getProgram(), xsink);
}

#define OVECCOUNT 30
bool QoreRegexNode::exec(const QoreString *target, ExceptionSink *xsink) const {
   TempEncodingHelper t(target, QCS_UTF8, xsink);
   if (!t)
      return false;

   return exec(t->getBuffer(), t->strlen());
}

bool QoreRegexNode::exec(const char *str, size_t len) const {   
   // the PCRE docs say that if we don't send an ovector here the library may have to malloc
   // memory, so, even though we don't need the results, we include the vector to avoid 
   // extraneous malloc()s
   int ovector[OVECCOUNT];
   int rc = pcre_exec(p, 0, str, len, 0, 0, ovector, OVECCOUNT);
   //printd(5, "QoreRegexNode::exec(%s) this=%p pre_exec() rc=%d\n", str, this, rc);   
   return rc >= 0;
}

#define OVEC_LATELEM 20
QoreListNode *QoreRegexNode::extractSubstrings(const QoreString* target, ExceptionSink* xsink) const {
   TempEncodingHelper t(target, QCS_UTF8, xsink);
   if (!t)
      return 0;
   
   ReferenceHolder<QoreListNode> l(xsink);

   int offset = 0;
   while (true) {
      int ovector[OVECCOUNT];
      if (offset >= t->strlen())
         break;
      int rc = pcre_exec(p, 0, t->getBuffer(), t->strlen(), offset, 0, ovector, OVECCOUNT);
      //printd(5, "QoreRegexNode::exec(%s) =~ /xxx/ = %d (global: %d)\n", t->getBuffer() + offset, rc, global);

      // FIXME: rc = 0 means that not enough space was available in ovector!
      if (rc < 1)
         break;

      if (rc > 1) {
         int x = 0;
         while (++x < rc) {
            int pos = x * 2;
            if (ovector[pos] == -1) {
               if (!l) l = new QoreListNode;
               l->push(nothing());
               continue;
            }
            QoreStringNode *tstr = new QoreStringNode;
            //printd(5, "substring %d: %d - %d (len %d)\n", x, ovector[pos], ovector[pos + 1], ovector[pos + 1] - ovector[pos]);
            tstr->concat(t->getBuffer() + ovector[pos], ovector[pos + 1] - ovector[pos]);
            if (!l) l = new QoreListNode;
            l->push(tstr);
         }

         offset = ovector[(x - 1) * 2 + 1];
         //printd(5, "QoreRegexNode::exec() offset: %d size: %d ovector[%d]: %d\n", offset, t->strlen(), (x - 1) * 2 + 1, ovector[(x - 1) * 2 + 1]);
      }
      else
         break;

      if (!global)
         break;
   }
   
   return l.release();
}

void QoreRegexNode::init(int64 opt) {
   p = 0;
   options = (int)opt;
   global = opt & QRE_GLOBAL ? true : false;
}

QoreString *QoreRegexNode::getString() {
   QoreString *rs = str;
   str = 0;
   return rs;
}
