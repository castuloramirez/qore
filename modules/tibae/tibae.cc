/*
  TIBCO/tibae.cc

  TIBCO Active Enterprise integration to QORE

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

#include <qore/config.h>
#include <qore/support.h>
#include <qore/QoreNode.h>
#include <qore/QoreType.h>
#include <qore/Object.h>
#include <qore/Exception.h>
#include <qore/QoreString.h>
#include <qore/params.h>
#include <qore/QoreClass.h>

#include "tibae.h"
#include "QoreApp.h"
#include <memory>

int CID_TIBAE;

static inline class QoreNode *map_minstance_to_node(const MInstance *min, ExceptionSink *xsink)
{
   std::auto_ptr<MEnumerator<MString, MData *> >me(min->newEnumerator());   
   MString name;
   MData *val;

   Hash* h = new Hash;
   while (me->next(name, val) && !xsink->isEvent())
      h->setKeyValue((char *)name.c_str(), map_mdata_to_node(val, xsink), xsink);

   if (xsink->isEvent())
   {
      h->derefAndDelete(xsink);
      return NULL;
   }
   return new QoreNode(h);
}

// maps a TIBCO sequence to a QORE list
static inline class QoreNode *map_msequence_to_node(const MSequence *ms, ExceptionSink *xsink)
{
   class QoreNode *rv = new QoreNode(NT_LIST);
   rv->val.list = new List();

   for (unsigned i = 0; i < ms->size() && !xsink->isEvent(); i++)
      rv->val.list->push(map_mdata_to_node((MData *)(*ms)[i], xsink));

   if (xsink->isEvent())
   {
      rv->deref(xsink);
      return NULL;
   }
   return rv;
}

// maps a TIBCO associative list to a QORE hash
static inline class QoreNode *map_massoclist_to_node(const MAssocList *mal, ExceptionSink *xsink)
{
   Hash *h = new Hash();

   MEnumerator<MString, MData *> *me = mal->newEnumerator();
   MString name;
   MData *val;

   while (me->next(name, val) && !xsink->isEvent())
      h->setKeyValue((char *)name.c_str(), map_mdata_to_node(val, xsink), xsink);

   delete me;
   if (xsink->isEvent())
   {
      h->derefAndDelete(xsink);
      return NULL;
   }
   return new QoreNode(h);
}

// maps a TIBCO union to a QORE hash
static inline class QoreNode *map_munion_to_node(const MUnion *mu, ExceptionSink *xsink)
{
   Hash *h = new Hash();

   MString MSkey = mu->getMemberName();
   h->setKeyValue((char *)MSkey.c_str(), map_mdata_to_node((MData *)mu->get(MSkey), xsink), xsink);
   
   if (xsink->isEvent())
   {
      h->derefAndDelete(xsink);
      return NULL;
   }
   return new QoreNode(h);
}

// maps a TIBCO MTree to a QORE node
class QoreNode *map_mdata_to_node(MData *md, ExceptionSink *xsink)
{
   const MInstance *min;
   const MAssocList *mal;
   const MSequence *ms;
   const MUnion *mu;
   const MInteger *mi;
   const MStringData *msd;
   const MReal *mr;
   const MDateTime *mdt;
   const MBool *mb;
   const MDate *mdate;

   // is it an MInstance?
   if ((min = MInstance::downCast(md)))
      return map_minstance_to_node(min, xsink);
   // is it an associative list?
   else if ((mal = MAssocList::downCast(md)))
      return map_massoclist_to_node(mal, xsink);
   // or is it a sequence?
   else if ((ms = MSequence::downCast(md)))
      return map_msequence_to_node(ms, xsink);
   // or is it a union?
   else if ((mu = MUnion::downCast(md)))
      return map_munion_to_node(mu, xsink);
   else if ((mi = MInteger::downCast(md)))
   {
      QoreNode *rv = new QoreNode(NT_INT);
      rv->val.intval = mi->getAsLong();
      return rv;
   }
   else if ((msd = MStringData::downCast(md)))
   {
      MString ms = msd->getAsString();
      return new QoreNode((char *)ms.c_str());
   }
   else if ((mr = MReal::downCast(md)))
      return new QoreNode(mr->getAsDouble());
   else if ((mdt = MDateTime::downCast(md)))
      return new QoreNode(new DateTime(mdt->getYear(), mdt->getMonth(), mdt->getDay(), mdt->getHour(), mdt->getMinute(), mdt->getSecond(), mdt->getMicroSeconds() / 1000));
   else if ((mdate = MDate::downCast(md)))
      return new QoreNode(new DateTime(mdate->getYear(), mdate->getMonth(), mdate->getDay()));
   else if ((mb = MBool::downCast(md)))
   {
      QoreNode *rv = new QoreNode(NT_BOOLEAN);
      rv->val.boolval = mb->getAsBoolean();
      return rv;
   }
   xsink->raiseException("MAP-ERROR", "can't map MData element of class \"%s\" to QORE type",
	  md->getClassName());
   return NULL;
}

static inline void set_properties(MAppProperties *appProperties, Hash *h, ExceptionSink *xsink)
{
   tracein("set_properties()");

   HashIterator hi(h);
   while (hi.next())
   {
      char *key = hi.getKey();
      if (!hi.getValue())
      {
	 xsink->raiseException("TIBCO-INVALID-PROPERTIES-HASH", 
			"properties hash key \"%s\" has value = NOTHING",
			key);
	 return;
      }
      else if (hi.getValue()->type != NT_STRING)
      {
	 xsink->raiseException("TIBCO-INVALID-PROPERTIES-HASH",
			"properties hash has invalid type \"%s\" for key \"%s\" (must be string)",
			hi.getValue()->type->getName(), key);
	 return;
      }
      char *val = hi.getValue()->val.String->getBuffer();

      if (!strcmp(key, "AppVersion"))
	 appProperties->setAppVersion(val);
      else if (!strcmp(key, "AppInfo"))
	 appProperties->setAppInfo(val);
      else if (!strcmp(key, "AppName"))
	 appProperties->setAppName(val);
      else if (!strcmp(key, "RepoURL"))
	 appProperties->setRepoURL(val);
      else if (!strcmp(key, "ConfigURL"))
	 appProperties->setConfigURL(val);
#ifdef DEBUG
      else printe("ignoring properties member %s=%s\n", key, val);
#endif
   }

   appProperties->setMultiThreaded(); 
   //appProperties->setDefaultStringEncoding(MEncoding::M_ASCII);
   //appProperties->setDefaultStringEncoding(MEncoding::M_LATIN_1);

   traceout("set_properties()");
}

/*
const char *MStringData::data(char const *type, unsigned int *p) const
{
   printd(0, "MStringData::data(%s, %08p) returning %s\n", type ? type : "(NULL)", p, "hello");
   return "hello";
}
*/

// usage: new TibcoAdapter(session-name, properties, classlist, [, service, network, daemon])
void TIBAE_constructor(class Object *self, class QoreNode *params, class ExceptionSink *xsink)
{
   tracein("TIBAE_constructor");

   char *session_name, *service = NULL, *network = NULL, *daemon = NULL;
   QoreNode *p0, *p1, *p2, *p;

   if (!(p0 = test_param(params, NT_STRING, 0)) ||
       !(p1 = test_param(params, NT_HASH, 1)))
   {
      xsink->raiseException("TIBCO-PARAMETER-ERROR", "invalid parameters passed to Tibco() constructor, expecting session name (string), properties (object), [classlist (object), service (string), network (string), daemon (string)]");
      traceout("TIBAE_constructor");
      return;
   }

   session_name = p0->val.String->getBuffer();
   Hash *classlist;
   if ((p2 = test_param(params, NT_HASH, 2)))
   {
      // FIXME: check that classlist hash has only String values!
      classlist = p2->val.hash->copy();
   }
   else
      classlist = NULL;

   if ((p = test_param(params, NT_STRING, 3)))
      service = p->val.String->getBuffer();

   if ((p = test_param(params, NT_STRING, 4)))
      network = p->val.String->getBuffer();

   if ((p = test_param(params, NT_STRING, 5)))
      daemon = p->val.String->getBuffer();

   // create adapter instance
   printd(1, "TIBAE_constructor() session=%s service=%s network=%s daemon=%s\n",
	  session_name, 
	  service ? service : "(null)", 
	  network ? network : "(null)", 
	  daemon  ? daemon  : "(null)");
   class QoreApp *myQoreApp = NULL;

   MAppProperties *appProps = new MAppProperties();
   set_properties(appProps, p1->val.hash, xsink); 

   if (xsink->isEvent())
   {
      if (classlist)
	 classlist->derefAndDelete(xsink);
      return;
   }
   try 
   {
      printd(4, "before QoreApp constructor\n");
      myQoreApp =
         new QoreApp(appProps, session_name, classlist, service, network, daemon);
      printd(4, "after QoreApp constructor (%08p)\n", myQoreApp);

      printd(4, "before start()\n");
      myQoreApp->start(Mfalse);
      printd(4, "after start()\n");
   }
   catch (MException &te)
   {
      xsink->raiseException("TIBCO-EXCEPTION", "Exception thrown in Tibco() constructor %s: %s",
			 te.getType().c_str(), te.getDescription().c_str());
      if (myQoreApp)
	 myQoreApp->deref(xsink);
      traceout("TIBAE_constructor");
      return;
   }
   self->setPrivate(CID_TIBAE, myQoreApp);
   printd(5, "TIBAE_constructor() this=%08p myQoreApp=%08p\n", self, myQoreApp);
   traceout("TIBAE_constructor");
}

void TIBAE_copy(class Object *self, class Object *old, class QoreApp *myQoreApp, class ExceptionSink *xsink)
{
   xsink->raiseException("TIBCO-ADAPTER-COPY-ERROR", "copying TibcoAdapter objects is curently not supported");
}

// usage: TIBAE_sendSubject(subject, function_name, message)
class QoreNode *TIBAE_sendSubject(class Object *self, class QoreApp *myQoreApp, class QoreNode *params, class ExceptionSink *xsink)
{
   class QoreNode *p0, *p1, *p2;

   // check input parameters
   if (!(p0 = test_param(params, NT_STRING, 0)) ||
       !(p1 = test_param(params, NT_STRING, 1)) ||
       !(p2 = test_param(params, NT_HASH, 2)))
   {
      xsink->raiseException("TIBCO-SENDSUBJECT-PARAMETER-ERROR", "invalid parameters passed to TibcoAdapter::sendSubject(), expecting subject (string), function name (string), message (hash)");
      return NULL;
   }

   char *subject = p0->val.String->getBuffer();
   char *fname = p1->val.String->getBuffer();

   // try to send message
   try
   {
      myQoreApp->send(subject, fname, p2, xsink);
   }
   catch(MException &te)
   {
      xsink->raiseException("TIBCO-EXCEPTION", 
			    "Exception caught while sending \"%s\" with subject \"%s\": %s: %s", 
			    fname, subject, te.getType().c_str(), te.getDescription().c_str());      
   }
   return NULL;
}

// usage: Tibco::sendSubjectWithSyncReply(subject, function_name, message[, timeout])
class QoreNode *TIBAE_sendSubjectWithSyncReply(class Object *self, class QoreApp *myQoreApp, class QoreNode *params, class ExceptionSink *xsink)
{
   class QoreNode *p0, *p1, *p2, *p3;
   char *fname;

   // check input parameters
   if (!(p0 = test_param(params, NT_STRING, 0)) ||
       !(p1 = test_param(params, NT_STRING, 1)) ||
       !((p2 = test_param(params, NT_OBJECT, 2)) || (p2 = test_param(params, NT_HASH, 2))))
   {
      xsink->raiseException("TIBCO-SEND-WITH-SYNC-REPLY-PARAMETER-ERROR", "invalid parameters passed to tibco_send_with_sync_reply(), expecting subject (string), function name (string), message (object), [timeout (date/time or integer milliseconds)]");
      return NULL;
   }

   fname = p1->val.String->getBuffer();

   // set timeout parameter if present
   int timeout = 0;
   if ((p3 = get_param(params, 3))) {
      QoreNode* n = test_param(params, NT_INT, 3);
      if (n) {
        timeout = p3->getAsInt();
      } else {
        n = test_param(params, NT_DATE, 3);
        if (!n) {
          xsink->raiseException("TIBCO-SEND-WITH-SYNC-REPLY-PARAMETER-ERROR", "The timeout parameter needs to be integer or string.");
          return 0;
        }
        timeout = n->val.date_time->getRelativeMilliseconds();
      }
   }

   // try to send message
   try
   {
      printd(2, "TIBAE_sendSubjectWithSyncReply() calling sendWithSyncReply()\n");
      return myQoreApp->sendWithSyncReply(p0->val.String->getBuffer(), fname, p2, timeout, xsink);
   }
   catch(MException &te)
   {
      xsink->raiseException("TIBCO-EXCEPTION", 
			    "Exception caught while sending \"%s\" with subject \"%s\": %s: %s", 
			    fname, p0->val.String->getBuffer(), te.getType().c_str(), te.getDescription().c_str());      
   }

   return NULL;
}

// Tibco::receive(subject, [timeout])
class QoreNode *TIBAE_receive(class Object *self, class QoreApp *myQoreApp, class QoreNode *params, class ExceptionSink *xsink)
{
   QoreNode *p0, *p1;

   if (!(p0 = test_param(params, NT_STRING, 0)))
   {
      xsink->raiseException("TIBCO-RECEIVE-PARAMETER-ERROR", "invalid parameters passed to tibco_receive(), expecting subject (string), [timeout (date/time or integer milliseconds)]");
      return NULL;
   }

   char *subject = p0->val.String->getBuffer();
   unsigned long timeout = 0;
   if ((p1 = get_param(params, 1))) {
     QoreNode* n = test_param(params, NT_INT, 1);
     if (n) {
       timeout = (unsigned long)n->val.intval;
     } else {
       n = test_param(params, NT_DATE, 3);
       if (n) {
        timeout = (unsigned long)n->val.date_time->getRelativeMilliseconds();
       } else {
        xsink->raiseException("TIBCO-RECEIVE-PARAMETER-ERROR", "timeout parameter needs to be either integer or date/time.");
       }
     }
   }

   return myQoreApp->receive(subject, timeout, xsink);
}


//------------------------------------------------------------------------------
// Parameters:
// * class name (string), in Tibco docs called "name of the class repository object in which this operation is defined"
//   Example: "greetings".
// * method name (string), in Tibco docs "name of the operation repository that defined this operation"
//   Example: "setGreetings"
// * data (hash), keys are string parameter names + appropriate type (according to repository) of values
// * optional: timeout (date/time or integer, millseconds), default is 60 seconds, 0 means infinite
// * optional: client name (string), in Tibco docs "this name must refer to a client repository object defined 
//   in endpoint.clients directory
//   Default value is ""
//
// Returns hash with data send as a reply to this call.
//
static QoreNode* TIBAE_operationsCallWithSyncResult(Object* self, QoreApp* myQoreApp, QoreNode* params, ExceptionSink *xsink)
{
  char* err = "Invalid parameters. Expected: class name (string), method name (string), data (hash), "
    "[timeout (integer in millis or date/time), ] [client name (string)]";
  char* func = "TIBCO-OPERATIONS-CALL-WITH-SYNC_RESULT";
  QoreNode* class_name = test_param(params, NT_STRING, 0);
  if (!class_name) {
    return xsink->raiseException(func, err);
  }
  char* class_name_extracted = class_name->val.String->getBuffer(); 
  QoreNode* method_name = test_param(params, NT_STRING, 1);
  if (!method_name) {
    return xsink->raiseException(func, err);
  } 
  char* method_name_extracted = method_name->val.String->getBuffer();
  QoreNode* data = test_param(params, NT_HASH, 2);
  if (!data) {
    return xsink->raiseException(func, err);
  }
  Hash* data_extracted = data->val.hash;
  unsigned timeout = 60 * 1000;
  char* client_name = "";

  int next_item = 3;
  QoreNode* n = test_param(params, NT_INT, 3);
  if (n) {
    timeout = (unsigned)n->val.intval;
    ++next_item;
  } else {
    n = test_param(params, NT_DATE, 3);
    if (n) {
      timeout = (unsigned)n->val.date_time->getRelativeMilliseconds();
      ++next_item;
    }
  }
  n = test_param(params, NT_STRING, next_item);
  if (n) {
    client_name = n->val.String->getBuffer();
  }

  return myQoreApp->operationsCallWithSyncResult(class_name_extracted, method_name_extracted, data_extracted,
    timeout, client_name, xsink);
}

//------------------------------------------------------------------------------
// The same parameters as for TIBAE_operationsCallWithSyncResult, 
// except for the timeout (not needed), always returns 0.
//
static QoreNode* TIBAE_operationsOneWayCall(Object* self, QoreApp* myQoreApp, QoreNode* params, ExceptionSink *xsink)
{
  char* err = "Invalid parameters. Expected: class name (string), method name (string), data (hash), "
    "[client name (string)]";
  char* func = "TIBCO-OPERATIONS-ONE-WAY-CALL";
  QoreNode* class_name = test_param(params, NT_STRING, 0);
  if (!class_name) {
    return xsink->raiseException(func, err);
  }
  char* class_name_extracted = class_name->val.String->getBuffer();
  QoreNode* method_name = test_param(params, NT_STRING, 1);
  if (!method_name) {
    return xsink->raiseException(func, err);
  }
  char* method_name_extracted = method_name->val.String->getBuffer();
  QoreNode* data = test_param(params, NT_HASH, 2);
  if (!data) {
    return xsink->raiseException(func, err);
  }
  Hash* data_extracted = data->val.hash;
  char* client_name = "";

  QoreNode* n = test_param(params, NT_STRING, 3);
  if (n) {
    client_name = n->val.String->getBuffer();
  }

  myQoreApp->operationsOneWayCall(class_name_extracted, method_name_extracted, data_extracted, client_name, xsink);
  return 0;
}

//------------------------------------------------------------------------------
// The same parameters as for TIBAE_operationsCallWithSyncResult (including timeout).
// Always return 0. To get the reply use combination of class name + method name passed to this call.
//
static QoreNode* TIBAE_operationsAsyncCall(Object* self, QoreApp* myQoreApp, QoreNode* params, ExceptionSink *xsink)
{
 char* err = "Invalid parameters. Expected: class name (string), method name (string), data (hash), "
    "[timeout (integer in millis or date/time), ] [client name (string)]";
  char* func = "TIBCO-OPERATIONS-ASYNC-CALL";
  QoreNode* class_name = test_param(params, NT_STRING, 0);
  if (!class_name) {
    return xsink->raiseException(func, err);
  }
  char* class_name_extracted = class_name->val.String->getBuffer();
  QoreNode* method_name = test_param(params, NT_STRING, 1);
  if (!method_name) {
    return xsink->raiseException(func, err);
  }
  char* method_name_extracted = method_name->val.String->getBuffer();
  QoreNode* data = test_param(params, NT_HASH, 2);
  if (!data) {
    return xsink->raiseException(func, err);
  }
  Hash* data_extracted = data->val.hash;
  unsigned timeout = 60 * 1000;
  char* client_name = "";

  int next_item = 3;
  QoreNode* n = test_param(params, NT_INT, 3);
  if (n) {
    timeout = (unsigned)n->val.intval;
    ++next_item;
  } else {
    n = test_param(params, NT_DATE, 3);
    if (n) {
      timeout = (unsigned)n->val.date_time->getRelativeMilliseconds();
      ++next_item;
    }
  }
  n = test_param(params, NT_STRING, next_item);
  if (n) {
    client_name = n->val.String->getBuffer();
  }

  myQoreApp->operationsAsyncCall(class_name_extracted, method_name_extracted, data_extracted, timeout, client_name, xsink);
  return 0;
}

//------------------------------------------------------------------------------
// Parameters:
// * class name (string)
// * method name (string)
// The class/method need to be invoked prior with operationsAsyncCall().
//
// Returns hash with retrieved values.
//
static QoreNode* TIBAE_operationsGetAsyncCallResult(Object* self, QoreApp* myQoreApp, QoreNode* params, ExceptionSink *xsink)
{
  char* err = "Invalid parameters. Expected: class name (string), method name (string)";
  char* func = "TIBCO-OPERATIONS-GET-ASYNC-RESULT";
  QoreNode* class_name = test_param(params, NT_STRING, 0);
  if (!class_name) {
    return xsink->raiseException(func, err);
  }
  char* class_name_extracted = class_name->val.String->getBuffer();
  QoreNode* method_name = test_param(params, NT_STRING, 1);
  if (!method_name) {
    return xsink->raiseException(func, err);
  }
  char* method_name_extracted = method_name->val.String->getBuffer();

  return myQoreApp->operationsGetAsyncCallResult(class_name_extracted, method_name_extracted, xsink);
}

//------------------------------------------------------------------------------
class QoreClass *initTibcoAdapterClass()
{
   tracein("initTibcoAdapterClass()");

   class QoreClass *QC_TIBAE = new QoreClass(QDOM_NETWORK, strdup("TibcoAdapter"));
   CID_TIBAE = QC_TIBAE->getID();
   QC_TIBAE->setConstructor(TIBAE_constructor);
   QC_TIBAE->setCopy((q_copy_t)TIBAE_copy);
   QC_TIBAE->addMethod("receive",                  (q_method_t)TIBAE_receive);
   QC_TIBAE->addMethod("sendSubject",              (q_method_t)TIBAE_sendSubject);
   QC_TIBAE->addMethod("sendSubjectWithSyncReply", (q_method_t)TIBAE_sendSubjectWithSyncReply);

   // operations
   QC_TIBAE->addMethod("callOperationWithSyncReply",  (q_method_t)TIBAE_operationsCallWithSyncResult);
   QC_TIBAE->addMethod("callOperationOneWay",         (q_method_t)TIBAE_operationsOneWayCall);
/* commented out for now as the retrieving async call method still fails
   QC_TIBAE->addMethod("operationsAsyncCall",          (q_method_t)TIBAE_operationsAsyncCall);
   QC_TIBAE->addMethod("operationsGetAsyncCallResult", (q_method_t)TIBAE_operationsGetAsyncCallResult);
*/
   traceout("initTibcoAdapterClass()");
   return QC_TIBAE;
}

