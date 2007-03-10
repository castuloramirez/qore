/*
 QoreProgram.h
 
 Program Object Definition
 
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

#ifndef _QORE_GLOBALVARIABLELIST_H

#define _QORE_GLOBALVARIABLELIST_H

#include <qore/hash_map.h>

// this is a "grow-only" container
// all reading and writing is done withing the parse lock on the contining program object
class GlobalVariableList
{
private:
   hm_var_t vmap; // iterators are not invalidated on inserts
   
public:
   DLLLOCAL GlobalVariableList();
   DLLLOCAL ~GlobalVariableList();
   DLLLOCAL void delete_all(class ExceptionSink *xsink);
   DLLLOCAL void clear_all(class ExceptionSink *xsink);
   DLLLOCAL void import(class Var *var, class ExceptionSink *xsink, bool readonly = false);
   DLLLOCAL class Var *newVar(const char *name);
   DLLLOCAL class Var *newVar(class Var *v, bool readonly);
   DLLLOCAL class Var *findVar(const char *name);
   DLLLOCAL class Var *checkVar(const char *name, int *new_vars);
   DLLLOCAL class List *getVarList() const;
};

#endif
