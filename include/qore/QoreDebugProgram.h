/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreDebugProgram.h

  Program Debug Object Definition

  Qore Programming Language

  Copyright (C) 2003 - 2016 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/


#ifndef INCLUDE_QORE_QOREDEBUGPROGRAM_H_
#define INCLUDE_QORE_QOREDEBUGPROGRAM_H_

#include <qore/AbstractPrivateData.h>
#include <qore/Restrictions.h>

class QoreProgram;
class ExceptionSink;
class AbstractStatement;
class StatementBlock;
class qore_program_private;
class qore_debug_program_private;

//! supports parsing and executing Qore-language code, reference counted, dynamically-allocated only
/** This class implements a transaction and thread-safe container for qore-language code
    This class implements two-layered reference counting to address problems with circular references.
    When a program has a global variable that contains an object that references the program...
    objects now reference the dependency counter, so when the object's counter reaches zero and
    the global variable list is deleted, then the variables will in turn dereference the program
    so it can be deleted.
*/
class QoreDebugProgram : public AbstractPrivateData {
   friend class qore_program_private;
   friend class qore_debug_program_private;
private:
   //! private implementation
   qore_debug_program_private* priv;

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreDebugProgram(const QoreDebugProgram&);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL QoreDebugProgram& operator=(const QoreDebugProgram&);

protected:
   //! the destructor is private in order to prohibit the object from being allocated on the stack
   /** the destructor is run when the reference count reaches 0
    */
   DLLLOCAL virtual ~QoreDebugProgram();

public:
   //! creates the object
   DLLEXPORT QoreDebugProgram();//: priv(new qore_debug_program_private(this)) {};


   DLLEXPORT void addProgram(QoreProgram *pgm);/* {
      priv->addProgram(pgm);
   }*/

   DLLEXPORT void removeProgram(QoreProgram *pgm);/* {
      priv->removeProgram(pgm);
   }*/

   DLLEXPORT virtual ThreadDebugEnum onAttach(QoreProgram *pgm, ExceptionSink* xsink) = 0;
   DLLEXPORT virtual ThreadDebugEnum onDetach(QoreProgram *pgm, ExceptionSink* xsink) = 0;
   /**
    * Executed on every step of StatementBlock.
    * @param blockStatement
    * @param statement current AbstractStatement of blockStatement being processed. Executed also when blockStatement is entered with value of NULL
    * @param retCode
    */
   DLLEXPORT virtual ThreadDebugEnum onStep(QoreProgram *pgm, const StatementBlock *blockStatement, const AbstractStatement *statement, int &retCode, ExceptionSink* xsink) = 0;
   DLLEXPORT virtual ThreadDebugEnum dbgFunctionEnter(QoreProgram *pgm, const AbstractStatement *statement, ExceptionSink* xsink) = 0;
   /**
    * Executed when a function is exited.
    */
   DLLEXPORT virtual ThreadDebugEnum dbgFunctionExit(QoreProgram *pgm, const AbstractStatement *statement, QoreValue& returnValue, ExceptionSink* xsink) = 0;
   /**
    * Executed when an exception is raised.
    */
   DLLEXPORT virtual ThreadDebugEnum onException(QoreProgram *pgm, const AbstractStatement *statement, const ExceptionSink* xsink) = 0;

};


#endif /* INCLUDE_QORE_QOREDEBUGPROGRAM_H_ */


