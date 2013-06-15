/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
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

#ifndef _QORE_THREADPOOL_H
#define _QORE_THREADPOOL_H

#define QTP_DEFAULT_RELEASE_MS 5000

#include <deque>
#include <qore/qlist>

class ThreadTask;
class ThreadPoolThread;

typedef std::deque<ThreadTask*> taskq_t;
typedef qlist<ThreadPoolThread*> tplist_t;

class ThreadTask {
protected:
   ResolvedCallReferenceNode* code;
   ResolvedCallReferenceNode* cancelCode;
   
public:
   DLLLOCAL ThreadTask(ResolvedCallReferenceNode* c, ResolvedCallReferenceNode* cc) : code(c), cancelCode(cc) {
   }

   DLLLOCAL ~ThreadTask() {
      assert(!code);
      assert(!cancelCode);
   }

   DLLLOCAL void del(ExceptionSink* xsink) {
      code->deref(xsink);
      if (cancelCode)
         cancelCode->deref(xsink);
#ifdef DEBUG
      code = 0;
      cancelCode = 0;
#endif
   }

   DLLLOCAL AbstractQoreNode* run(ExceptionSink* xsink) {
      return code->exec(0, xsink);
   }

   DLLLOCAL void cancel(ExceptionSink* xsink) {
      if (cancelCode)
         discard(cancelCode->exec(0, xsink), xsink);
   }
};

class ThreadTaskHolder {
protected:
   ThreadTask* task;
   ExceptionSink* xsink;

public:
   DLLLOCAL ThreadTaskHolder(ThreadTask* t, ExceptionSink* xs) : task(t), xsink(xs) {
   }

   DLLLOCAL ~ThreadTaskHolder() {
      if (task) {
	 task->del(xsink);
	 delete task;
      }
   }

   DLLLOCAL ThreadTask* release() {
      ThreadTask* rv = task;
      task = 0;
      return rv;
   }
};

class ThreadPool;

class ThreadPoolThread {
protected:
   int id;
   ThreadPool& tp;
   ThreadTask* task;
   QoreCondition c,
      *stopCond;
   QoreThreadLock m;
   tplist_t::iterator pos;
   bool stopflag,
      stopped;

   DLLLOCAL void finalize(ExceptionSink* xsink);

public:
   DLLLOCAL ThreadPoolThread(ThreadPool& n_tp, ExceptionSink* xsink);

   DLLLOCAL ~ThreadPoolThread() {
      delete stopCond;
   }

   DLLLOCAL void setPos(tplist_t::iterator p) {
      pos = p;
   }

   DLLLOCAL bool valid() const {
      return id != -1;
   }

   DLLLOCAL void worker(ExceptionSink* xsink);

   DLLLOCAL void stop() {
      AutoLocker al(m);
      assert(!stopflag);
      stopflag = true;
      c.signal();
      //printd(5, "ThreadPoolThread::stop() signaling stop for id %d\n", id);
   }

   DLLLOCAL void stopWait() {
      //printd(5, "ThreadPoolThread::stopWait() stopping id %d\n", id);
      assert(!stopCond);
      stopCond = new QoreCondition;

      AutoLocker al(m);
      assert(!stopflag);
      stopflag = true;
      c.signal();
   }

   DLLLOCAL void stopConfirm(ExceptionSink* xsink) {
      {
	 AutoLocker al(m);
	 assert(stopflag);
	 assert(stopCond);
	 while (!stopped)
	    stopCond->wait(m);
      }

      //printd(5, "ThreadPoolThread::stopConfirm() stopped id %d\n", id);
      finalize(xsink);
   }

   DLLLOCAL void submit(ThreadTask* t) {
      AutoLocker al(m);
      assert(!stopflag);
      assert(!task);
      task = t;
      c.signal();
   }

   DLLLOCAL int getId() const {
      return id;
   }

   DLLLOCAL tplist_t::iterator getPos() const {
      return pos;
   }
};

class ThreadPool : public AbstractPrivateData {
protected:
   int max,        // maximum number of threads in pool (if <= 0 then unlimited)
      minidle,     // minimum number of idle threads
      maxidle,     // maximum number of idle threads
      release_ms;  // number of milliseconds before idle threads are released when > minidle

   // mutex for atomicity
   QoreThreadLock m;

   // worker thread condition variable
   QoreCondition cond;

   // stop condition variable
   QoreCondition stopCond;

   tplist_t ah,  // allocated thread list
      fh;        // free thread list

   // quit flag
   bool quit;

   // master task queue
   taskq_t q;

   // task waiting flag
   bool waiting;

   bool stopflag,   // stop flag
      stopped,      // stopped flag
      confirm;      // confirm member thread stop

   DLLLOCAL int checkStopUnlocked(const char* m, ExceptionSink* xsink) {
      if (stopflag) {
	 xsink->raiseException("THREADPOOL-ERROR", "ThreadPool::%s() cannot be executed because the ThreadPool is being destroyed");
	 return -1;
      }
      return 0;
   }

   DLLLOCAL int addIdleWorker(ExceptionSink* xsink) {
      std::auto_ptr<ThreadPoolThread> tpth(new ThreadPoolThread(*this, xsink));
      if (!tpth->valid()) {
	 assert(*xsink);
	 return -1;
      }

      ThreadPoolThread* tpt = tpth.release();
      fh.push_back(tpt);
#ifdef DEBUG
      // set to an invalid iterator
      tpt->setPos(fh.end());
#endif
      return 0;
   }

   DLLLOCAL ThreadPoolThread* getThreadUnlocked(ExceptionSink* xsink) {
      while (!stopflag && fh.empty() && max && ah.size() == max) {
	 waiting = true;
	 cond.wait(m);
	 waiting = false;
      }

      if (stopflag)
	 return 0;

      ThreadPoolThread* tpt;

      if (!fh.empty()) {
	 tpt = fh.front();
	 fh.pop_front();
      }
      else {
	 std::auto_ptr<ThreadPoolThread> tpt_pt(new ThreadPoolThread(*this, xsink));
	 if (!tpt_pt->valid()) {
	    assert(*xsink);
	    return 0;
	 }
	 tpt = tpt_pt.release();
      }
      
      ah.push_back(tpt);
      tplist_t::iterator i = ah.end();
      --i;
      tpt->setPos(i);
      return tpt;
   }

public:
   DLLLOCAL ThreadPool(ExceptionSink* xsink, int n_max = 0, int n_minidle = 0, int m_maxidle = 0, int n_release_ms = QTP_DEFAULT_RELEASE_MS);

   DLLLOCAL ~ThreadPool() {
      assert(q.empty());
      assert(ah.empty());
      assert(fh.empty());
      assert(stopped);
   }

   DLLLOCAL void toString(QoreString& str) {
      AutoLocker al(m);
      
      str.sprintf("ThreadPool %p total: %d max: %d minidle: %d maxidle: %d release_ms: %d running: [", this, ah.size() + fh.size(), max, minidle, maxidle, release_ms);
      for (tplist_t::iterator i = ah.begin(), e = ah.end(); i != e; ++i) {
	 if (i != ah.begin())
	    str.concat(", ");
	 str.sprintf("%d", (*i)->getId());
      }

      str.concat("] idle: [");

      for (tplist_t::iterator i = fh.begin(), e = fh.end(); i != e; ++i) {
	 if (i != fh.begin())
	    str.concat(", ");
	 str.sprintf("%d", (*i)->getId());
      }

      str.concat(']');
   }

   // does not return until the thread pool has been stopped
   DLLLOCAL void stop() {
      AutoLocker al(m);
      if (!stopflag) {
	 stopflag = true;
	 cond.signal();
      }

      while (!stopped)
	 stopCond.wait(m);
   }

   DLLLOCAL int stopWait(ExceptionSink* xsink) {
      AutoLocker al(m);
      if (stopflag && !confirm) {
	 xsink->raiseException("THREADPOOL-ERROR", "cannot call ThreadPool::stopWait() after ()ThreadPool::stop() has been called since child threads have been detached and can no longer be traced");
	 return -1;
      }

      if (!stopflag) {
	 stopflag = true;
	 confirm = true;
	 cond.signal();
      }

      while (!stopped)
	 stopCond.wait(m);

      return 0;
   }

   DLLLOCAL int submit(ResolvedCallReferenceNode* c, ResolvedCallReferenceNode* cc, ExceptionSink* xsink) {
      // optimistically create the task object outside the lock
      ThreadTaskHolder task(new ThreadTask(c, cc), xsink);

      AutoLocker al(m);
      if (checkStopUnlocked("submit", xsink))
	  return -1;

      if (q.empty())
	 cond.signal();
      q.push_back(task.release());

      return 0;
   }

   DLLLOCAL void threadCounts(int& idle, int& running) {
      AutoLocker al(m);
      idle = fh.size();
      running = ah.size();
   }

   DLLLOCAL int done(ThreadPoolThread* tpt) {
      {
	 AutoLocker al(m);
	 if (!stopped && !confirm) {
	    tplist_t::iterator i = tpt->getPos();
	    ah.erase(i);
	    
	    if (!stopflag) {
	       // requeue thread if possible
	       if ((!maxidle && release_ms) || (fh.size() < maxidle) || q.size() > fh.size()) {
		  fh.push_back(tpt);
		  if (waiting || (release_ms && fh.size() > minidle))
		     cond.signal();
		  return 0;
	       }
	    }
	 }
      }

      return -1;
   }

   DLLLOCAL void worker(ExceptionSink* xsink);
};

#endif
