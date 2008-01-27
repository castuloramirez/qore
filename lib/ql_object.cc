/*
 ql_object.cc
 
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
#include <qore/intern/ql_object.h>

// returns a list of method names for the object passed as a parameter
static QoreNode *f_getMethodList(const QoreList *params, ExceptionSink *xsink)
{
   QoreObject *p0 = test_object_param(params, 0);
   if (!p0)
      return NULL;
   
   return p0->getClass()->getMethodList();
}

static QoreNode *f_callObjectMethod(const QoreList *params, ExceptionSink *xsink)
{
   // get object
   QoreObject *p0 = test_object_param(params, 0);
   if (!p0)
      return NULL;
   
   // get method name
   QoreStringNode *p1 = test_string_param(params, 1);
   if (!p1)
      return NULL;
   
   ReferenceHolder<QoreList> args(xsink);
   
   // if there are arguments to pass
   if (get_param(params, 2))
   {
      // create argument list by copying current list
      ReferenceHolder<QoreList> l(params->copyListFrom(2), xsink);
      if (*xsink)
	 return 0;
      args = l.release();
   }

   // make sure method call is internal (allows access to private methods) if this function was called internally
   CodeContextHelper cch(NULL, p0, xsink);
   return p0->evalMethod(p1, *args, xsink);
}

static QoreNode *f_callObjectMethodArgs(const QoreList *params, ExceptionSink *xsink)
{
   // get object
   QoreObject *p0 = test_object_param(params, 0);
   if (!p0)
      return NULL;
   
   // get method name
   QoreStringNode *p1 = test_string_param(params, 1);
   if (!p1)
      return NULL;

   ReferenceHolder<QoreList> args(xsink);
   QoreNode *p2;

   bool new_args = false;
   // if there are arguments to pass
   if ((p2 = get_param(params, 2)))
   {
      args = dynamic_cast<QoreList *>(p2);
      if (!args)
      {
	 args = new QoreList();
	 args->push(p2);
	 new_args = true;
      }
   }
   
   // make sure method call is internal (allows access to private methods) if this function was called internally
   QoreNode *rv;
   {
      CodeContextHelper cch(NULL, p0, xsink);
      rv = p0->evalMethod(p1, *args, xsink);

      // remove value (and borrowed reference) from list if necessary
      if (new_args)
	 args->shift();
   }
   
   return rv;
}

void init_object_functions()
{
   tracein("init_object_functions()");
   
   builtinFunctions.add("getMethodList", f_getMethodList);
   builtinFunctions.add("callObjectMethod", f_callObjectMethod);
   builtinFunctions.add("callObjectMethodArgs", f_callObjectMethodArgs);
   traceout("init_object_functions()");
}
