/*
  QC_Gate.cc

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

#include <qore/Qore.h>
#include <qore/intern/QC_Gate.h>

// rmutex class is depcreated and will be removed in the next major release
int CID_GATE, CID_RMUTEX;

static void GATE_constructor(class QoreObject *self, const QoreListNode *params, ExceptionSink *xsink)
{
   self->setPrivate(CID_GATE, new QoreGate());
}

static void GATE_destructor(class QoreObject *self, class QoreGate *g, ExceptionSink *xsink)
{
   g->destructor(xsink);
   g->deref(xsink);
}

static void GATE_copy(class QoreObject *self, class QoreObject *old, class QoreGate *g, ExceptionSink *xsink)
{
   self->setPrivate(CID_GATE, new QoreGate());
}

static class AbstractQoreNode *GATE_enter(class QoreObject *self, class QoreGate *g, const QoreListNode *params, ExceptionSink *xsink)
{
   AbstractQoreNode *p = get_param(params, 0);

   if (!is_nothing(p))
   {
      int timeout_ms = getMsZeroInt(p);
      return new QoreBigIntNode(g->grab(xsink, timeout_ms));
   }

   return new QoreBigIntNode(g->grab(xsink));
}

static class AbstractQoreNode *GATE_exit(class QoreObject *self, class QoreGate *g, const QoreListNode *params, ExceptionSink *xsink)
{
   return new QoreBigIntNode(g->release(xsink));
}

static class AbstractQoreNode *GATE_tryEnter(class QoreObject *self, class QoreGate *g, const QoreListNode *params, ExceptionSink *xsink)
{
   return new QoreBigIntNode(g->tryGrab());
}

static class AbstractQoreNode *GATE_numInside(class QoreObject *self, class QoreGate *g, const QoreListNode *params, ExceptionSink *xsink)
{
   return new QoreBigIntNode(g->get_count());
}

static class AbstractQoreNode *GATE_numWaiting(class QoreObject *self, class QoreGate *g, const QoreListNode *params, ExceptionSink *xsink)
{
   return new QoreBigIntNode(g->get_waiting());
}

class QoreClass *initGateClass()
{
   tracein("initGateClass()");

   class QoreClass *QC_GATE = new QoreClass("Gate", QDOM_THREAD_CLASS);
   CID_GATE = QC_GATE->getID();
   QC_GATE->setConstructor(GATE_constructor);
   QC_GATE->setDestructor((q_destructor_t)GATE_destructor);
   QC_GATE->setCopy((q_copy_t)GATE_copy);
   QC_GATE->addMethod("enter",         (q_method_t)GATE_enter);
   QC_GATE->addMethod("exit",          (q_method_t)GATE_exit);
   QC_GATE->addMethod("tryEnter",      (q_method_t)GATE_tryEnter);
   QC_GATE->addMethod("numInside",     (q_method_t)GATE_numInside);
   QC_GATE->addMethod("numWaiting",    (q_method_t)GATE_numWaiting);

   traceout("initGateClass()");
   return QC_GATE;
}

class QoreClass *initRMutexClass()
{
   class QoreClass *QC_RMUTEX = new QoreClass("RMutex", QDOM_THREAD_CLASS); 
   CID_RMUTEX = QC_RMUTEX->getID();
   QC_RMUTEX->setConstructor(GATE_constructor);
   QC_RMUTEX->setDestructor((q_destructor_t)GATE_destructor);
   QC_RMUTEX->setCopy((q_copy_t)GATE_copy);
   QC_RMUTEX->addMethod("enter",         (q_method_t)GATE_enter);
   QC_RMUTEX->addMethod("exit",          (q_method_t)GATE_exit);
   QC_RMUTEX->addMethod("tryEnter",      (q_method_t)GATE_tryEnter);
   QC_RMUTEX->addMethod("numInside",     (q_method_t)GATE_numInside);
   QC_RMUTEX->addMethod("numWaiting",    (q_method_t)GATE_numWaiting);

   return QC_RMUTEX;
}
