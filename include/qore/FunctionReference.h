/*
 FunctionReference.h
 
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

#ifndef _QORE_FUNCTIONREFERENCE_H

#define _QORE_FUNCTIONREFERENCE_H

class FunctionReferenceCall {
private:
   class QoreNode *exp;    // must evaluate to an AbstractFunctionReference
   class QoreNode *args;
public:
   DLLLOCAL FunctionReferenceCall(class QoreNode *n_exp, class QoreNode *n_args);
   DLLLOCAL ~FunctionReferenceCall();
   DLLLOCAL class QoreNode *eval(class ExceptionSink *xsink) const;
   DLLLOCAL int parseInit(lvh_t oflag, int pflag);
};

class AbstractFunctionReference {
   public:
      DLLLOCAL virtual ~AbstractFunctionReference() {}
      DLLLOCAL virtual void del(class ExceptionSink *xsink)
      {
	 delete this;
      }
      DLLLOCAL virtual class QoreNode *exec(class QoreNode *args, class ExceptionSink *xsink) const = 0;
      DLLLOCAL virtual void resolve() {}
      DLLLOCAL virtual AbstractFunctionReference *copy() = 0;
      DLLLOCAL virtual class QoreNode *eval(class QoreNode *n);
};

class FunctionReference : public AbstractFunctionReference
{
   public:
      union {
	    struct {
		  class UserFunction *uf;
		  class QoreProgram *pgm;
	    } user;
	    class BuiltinFunction *bf;
	    class ImportedFunctionCall *ifunc;
	    char *str;
      } f;
      int type;
      
      DLLLOCAL FunctionReference(char *n_str);
      DLLLOCAL FunctionReference(class UserFunction *n_uf);
      DLLLOCAL FunctionReference(class UserFunction *n_uf, class QoreProgram *pgm);
      DLLLOCAL virtual ~FunctionReference();
      DLLLOCAL virtual void del(class ExceptionSink *xsink);
      DLLLOCAL virtual class QoreNode *exec(class QoreNode *args, class ExceptionSink *xsink) const;
      DLLLOCAL virtual void resolve();
      DLLLOCAL virtual AbstractFunctionReference *copy();
      DLLLOCAL void set_static(class UserFunction *n_uf, class QoreProgram *n_pgm)
      {
	 type = FC_STATICUSERREF;
	 f.user.uf = n_uf;
	 f.user.pgm = n_pgm;
      }
      DLLLOCAL virtual class QoreNode *eval(class QoreNode *n);
};

class RunTimeObjectMethodReference : public AbstractFunctionReference
{
private:
   class Object *obj;
   char *method;
   
public:
   DLLLOCAL RunTimeObjectMethodReference(class Object *n_obj, char *n_method);
   DLLLOCAL virtual ~RunTimeObjectMethodReference();
   DLLLOCAL virtual class QoreNode *exec(class QoreNode *args, class ExceptionSink *xsink) const;
   DLLLOCAL virtual AbstractFunctionReference *copy();
};

class RunTimeObjectScopedMethodReference : public AbstractFunctionReference
{
private:
   class Object *obj;
   class Method *method;

public:
   DLLLOCAL RunTimeObjectScopedMethodReference(class Object *n_obj, class Method *n_method);
   DLLLOCAL virtual ~RunTimeObjectScopedMethodReference();
   DLLLOCAL virtual class QoreNode *exec(class QoreNode *args, class ExceptionSink *xsink) const;
   DLLLOCAL virtual AbstractFunctionReference *copy();
};

#endif
