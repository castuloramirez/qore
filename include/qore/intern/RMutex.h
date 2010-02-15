/* -*- mode: c++; indent-tabs-mode nil -*- */
/*
  RMutex.h

  recursive lock object

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

#ifndef _QORE_RMUTEX_H

#define _QORE_RMUTEX_H

#include <pthread.h>

class RMutex {
private:
   pthread_mutex_t m;

public:
   DLLLOCAL RMutex();
   DLLLOCAL ~RMutex();
   DLLLOCAL int enter();
   DLLLOCAL int tryEnter();
   DLLLOCAL int exit();
};

#endif
