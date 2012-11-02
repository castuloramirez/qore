/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreNamespaceList.h

  Qore Programming Language

  Copyright 2003 - 2012 David Nichols

  namespaces are children of a program object.  there is a parse
  lock per program object to ensure that objects are added (or backed out)
  atomically per program object.  All the objects referenced here should 
  be safe to read & copied at all times.  They will only be deleted when the
  program object is deleted (except the pending structures, which will be
  deleted any time there is a parse error, together with all other
  pending structures)

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

#ifndef _QORE_NAMESPACELIST_H

#define _QORE_NAMESPACELIST_H

#include <map>

#ifdef HAVE_QORE_HASH_MAP
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include <qore/intern/xxhash.h>

typedef HASH_MAP<std::string, QoreNamespace*> nsmap_t;
#else
typedef std::map<std::string, QoreNamespace*> nsmap_t;
#endif

class qore_ns_private;
class qore_root_ns_private;

class QoreNamespaceList {
private:
   DLLLOCAL void deleteAll();

   // not implemented
   DLLLOCAL QoreNamespaceList(const QoreNamespaceList& old);
   // not implemented
   DLLLOCAL QoreNamespaceList& operator=(const QoreNamespaceList& nsl);

public:
   nsmap_t nsmap;

   DLLLOCAL QoreNamespaceList() {
   }

   DLLLOCAL QoreNamespaceList(const QoreNamespaceList& old, int64 po, const qore_ns_private& parent);

   DLLLOCAL ~QoreNamespaceList() {
      deleteAll();
   }

   DLLLOCAL QoreNamespace *find(const char *name) {
      nsmap_t::iterator i = nsmap.find(name);
      return i == nsmap.end() ? 0 : i->second;
   }
   DLLLOCAL QoreNamespace *find(const std::string &name) {
      nsmap_t::iterator i = nsmap.find(name);
      return i == nsmap.end() ? 0 : i->second;
   }
   DLLLOCAL const QoreNamespace* find(const std::string &name) const {
      nsmap_t::const_iterator i = nsmap.find(name);
      return i == nsmap.end() ? 0 : i->second;
   }

   // do not delete the pointer returned from this function
   DLLLOCAL qore_ns_private* parseAdd(QoreNamespace *ot, qore_ns_private* parent);

   DLLLOCAL qore_ns_private* runtimeAdd(QoreNamespace *ot, qore_ns_private* parent);

   DLLLOCAL void resolveCopy();
   DLLLOCAL void parseInitConstants();

   DLLLOCAL void parseInitGlobalVars();
   DLLLOCAL void clearConstants(ExceptionSink* sink);
   DLLLOCAL void clearData(ExceptionSink* sink);
   DLLLOCAL void deleteGlobalVars(ExceptionSink* sink);

   DLLLOCAL void parseInit();
   DLLLOCAL void parseCommit(QoreNamespaceList& n);
   DLLLOCAL void parseRollback();
   DLLLOCAL void deleteAllConstants(ExceptionSink *xsink);
   DLLLOCAL void reset();

   DLLLOCAL void parseAssimilate(QoreNamespaceList& n, qore_ns_private* parent);
   DLLLOCAL void runtimeAssimilate(QoreNamespaceList& n, qore_ns_private* parent);

   DLLLOCAL void deleteData(ExceptionSink *xsink);

   DLLLOCAL bool empty() const {
      return nsmap.empty();
   }

   DLLLOCAL qore_size_t size() const {
      return nsmap.size();
   }
};

#endif
