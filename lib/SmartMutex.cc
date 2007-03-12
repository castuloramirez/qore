/* 
  SmartMutex.cc

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
#include <qore/SmartMutex.h>

#include <assert.h>

#ifdef DEBUG
SmartMutex::~SmartMutex()
{
   assert(cmap.empty());
}
#endif

int SmartMutex::releaseImpl()
{
   if (tid < 0)
      return -1;
   return 0;
}

int SmartMutex::grabImpl(int mtid, class VLock *nvl, class ExceptionSink *xsink, int timeout_ms)
{
   if (tid == mtid)
   {
      // getName() for possible inheritance
      xsink->raiseException("LOCK-ERROR", "TID %d called %s::lock() twice without an intervening %s::unlock()", tid, getName(), getName());
      return -1;
   }
   while (tid >= 0)
   {
      waiting++;
      int rc =  nvl->waitOn((AbstractSmartLock *)this, vl, xsink, timeout_ms);
      waiting--;
      if (rc)
	 return -1;
   }
   if (tid == Lock_Deleted)
   {
      // getName() for possible inheritance
      xsink->raiseException("LOCK-ERROR", "%s has been deleted in another thread", getName());
      return -1;
   }
   return 0;
}

int SmartMutex::releaseImpl(class ExceptionSink *xsink)
{
   int mtid = gettid();
   if (tid < 0)
   {
      // getName() for possible inheritance
      xsink->raiseException("LOCK-ERROR", "TID %d called %s::unlock() while the lock was already unlocked", mtid, getName());
      return -1;
   }
   if (tid != mtid)
   {
      // getName() for possible inheritance
      xsink->raiseException("LOCK-ERROR", "TID %d called %s::unlock() while the lock is held by tid %d", mtid, tid, getName());
      return -1;
   }
   return 0;
}

int SmartMutex::tryGrabImpl(int mtid, class VLock *nvl)
{
   if (tid != Lock_Unlocked)
      return -1;
   return 0;
}

int SmartMutex::externWaitImpl(int mtid, class QoreCondition *cond, class ExceptionSink *xsink, int timeout_ms)
{
   // make sure this TID owns the lock
   if (verify_wait_unlocked(mtid, xsink))
      return -1;

   // insert into cond map
   cond_map_t::iterator i = cmap.find(cond);
   if (i == cmap.end())
      i = cmap.insert(std::make_pair(cond, 1)).first;
   else
      ++(i->second);

   // save vlock
   class VLock *nvl = vl;

   // release lock
   release_intern();

   // wait for condition
   int rc = timeout_ms ? cond->wait(&asl_lock, timeout_ms) : cond->wait(&asl_lock);

   // decrement cond count and delete from map if 0
   if (!--(i->second))
      cmap.erase(i);

   // reacquire the lock
   if (grabImpl(mtid, nvl, xsink))
      return -1;

   grab_intern(mtid, nvl);
   return rc;
}

void SmartMutex::destructorImpl(class ExceptionSink *xsink)
{
   // wake up all condition variables waiting on this mutex
   for (cond_map_t::iterator i = cmap.begin(), e = cmap.end(); i != e; i++)
      i->first->broadcast();
}

bool SmartMutex::owns_lock()
{
   AutoLocker al(&asl_lock);
   return tid == gettid() ? true : false;
}

// returns how many condition variables are waiting on this mutex
int SmartMutex::cond_count(class QoreCondition *cond)
{
   AutoLocker al(&asl_lock);

   cond_map_t::iterator i = cmap.find(cond);
   if (i != cmap.end())
      return i->second;
   return 0;
}
