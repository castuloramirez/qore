/*
 QoreCondition.cpp
 
 Qore Programming Language
 
 Copyright (C) David Nichols 2005 - 2010
 
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
#include <qore/QoreCondition.h>

#include <qore/intern/ErrnoConstants.h>

#include <string.h>

QoreCondition::QoreCondition() {
   pthread_cond_init(&c, 0);
}

QoreCondition::~QoreCondition() {
   pthread_cond_destroy(&c);
}

int QoreCondition::signal() {
   return pthread_cond_signal(&c);
}

int QoreCondition::broadcast() {
   return pthread_cond_broadcast(&c);
}

int QoreCondition::wait(pthread_mutex_t *m) {   
#ifdef DEBUG
   int rc = pthread_cond_wait(&c, m);
   if (rc) {
      printd(0, "QoreCondition::wait(%p) pthread_cond_wait() returned %d %s\n", m, rc, strerror(rc));
      // print out a backtrace if possible
      qore_machine_backtrace();
   }
   assert(!rc);
   return rc;
#else
   return pthread_cond_wait(&c, m);
#endif
}

// timeout is in milliseconds
int QoreCondition::wait(pthread_mutex_t *m, int timeout_ms) {
#ifdef DARWIN
   // use more efficient pthread_cond_timedwait_relative_np() on Darwin
   struct timespec tmout;
   tmout.tv_sec = timeout_ms / 1000;
   tmout.tv_nsec = (timeout_ms - tmout.tv_sec * 1000) * 1000000;

#ifndef DEBUG
   return pthread_cond_timedwait_relative_np(&c, m, &tmout);
#else // !DEBUG
   //printd(5, "QoreCondition::wait(%p, %d) this=%p +trigger=%d.%09d\n", m, timeout_ms, this, tmout.tv_sec, tmout.tv_nsec);
   int rc = pthread_cond_timedwait_relative_np(&c, m, &tmout);
   if (rc && rc != ETIMEDOUT) {
      printd(0, "QoreCondition::wait(m=%p, timeout_ms=%d) this=%p pthread_cond_timedwait_relative_np() returned %d %s (errno=%d %s)\n", m, timeout_ms, this, rc, strerror(rc), errno, strerror(errno));
      // print out a backtrace if possible
      qore_machine_backtrace();
   }
   assert(!rc || rc == ETIMEDOUT);
   return rc;
#endif // DEBUG
#else // !DARWIN
   struct timeval now;
   struct timespec tmout;

#ifdef DEBUG
   int timeout_ms_orig = timeout_ms;
#endif // DEBUG
   
   gettimeofday(&now, 0);
   int secs = timeout_ms / 1000;
   timeout_ms -= secs * 1000;
   int nsecs = now.tv_usec * 1000 + timeout_ms * 1000000;
   int dsecs = nsecs / 1000000000;
   nsecs -= dsecs * 1000000000;
   tmout.tv_sec = now.tv_sec + secs + dsecs;
   tmout.tv_nsec = nsecs;

   // make sure mutex is locked
   assert(pthread_mutex_trylock(m) == EBUSY);

#ifndef DEBUG
   return pthread_cond_timedwait(&c, m, &tmout);
#else // !DEBUG
   //printd(5, "QoreCondition::wait(%p, %d) this=%p now=%d.%09d trigger=%d.%09d\n", m, timeout_ms, this, now.tv_sec, now.tv_usec * 1000, tmout.tv_sec, tmout.tv_nsec);
   int rc = pthread_cond_timedwait(&c, m, &tmout);
   if (rc && rc != ETIMEDOUT) {
      printd(0, "QoreCondition::wait(m=%p, timeout_ms=%d) pthread_cond_timedwait() returned %d %s\n", m, timeout_ms_orig, rc, strerror(rc));
      // print out a backtrace if possible
      qore_machine_backtrace();
   }
   assert(!rc || rc == ETIMEDOUT);
   return rc;
#endif // DEBUG
#endif // DARWIN
}
