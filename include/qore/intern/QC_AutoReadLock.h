/* -*- mode: c++; indent-tabs-mode nil -*- */
/*
 QC_AutoReadLock.h
 
 Qore Programming Language
 
 Copyright 2003 - 2010 David Nichols
 
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

#ifndef _QORE_QC_AUTOREADLOCK_H

#define _QORE_QC_AUTOREADLOCK_H

#include <qore/Qore.h>
#include <qore/intern/QC_RWLock.h>

DLLEXPORT extern qore_classid_t CID_AUTOREADLOCK;

DLLLOCAL class QoreClass *initAutoReadLockClass();

class QoreAutoReadLock : public AbstractPrivateData
{
   class RWLock *rwl;

public:
   DLLLOCAL QoreAutoReadLock(class RWLock *n_rwl, class ExceptionSink *xsink)
   {
      rwl = n_rwl;
      rwl->readLock(xsink);
   }

   DLLLOCAL virtual void deref(class ExceptionSink *xsink) 
   {
      if (ROdereference())
      {
	 rwl->deref(xsink);
	 delete this;
      }
   }

   DLLLOCAL virtual void destructor(class ExceptionSink *xsink) 
   {
      rwl->readUnlock(xsink);
   }
};


#endif
