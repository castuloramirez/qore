/*
  Object.cc

  thread-safe object definition

  Qore Programming Language

  Copyright 2003 - 2008 David Nichols

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
#include <qore/intern/BuiltinMethod.h>
#include <qore/intern/QoreClassIntern.h>

#include <stdlib.h>
#include <assert.h>

#include <map>

#define OS_OK             0
#define OS_DELETED       -1

// if the second part of the pair is true, then the data is virtual
typedef std::pair<AbstractPrivateData *, bool> private_pair_t;

// mapping from qore class ID to the object data
typedef std::map<qore_classid_t, private_pair_t> keymap_t;

class KeyList;

struct qore_object_private {
      const QoreClass *theclass;
      int status;
      mutable QoreThreadLock m;
#ifdef QORE_CLASS_SYNCHRONOUS
      mutable VRMutex *sync_vrm;
#endif
      KeyList *privateData;
      QoreReferenceCounter tRefs;  // reference-references
      QoreHashNode *data;
      QoreProgram *pgm;
      bool system_object, delete_blocker_run, tderef_done;

      DLLLOCAL qore_object_private(const QoreClass *oc, QoreProgram *p, QoreHashNode *n_data) : 
	 theclass(oc), status(OS_OK), 
#ifdef QORE_CLASS_SYNCHRONOUS
	 sync_vrm(oc->has_synchronous_in_hierarchy() ? new VRMutex : 0),
#endif 
	 privateData(0), data(n_data), pgm(p), system_object(!p), delete_blocker_run(false), tderef_done(false)
      {
	 printd(5, "QoreObject::QoreObject() this=%08p, pgm=%08p, class=%s, refs 0->1\n", this, p, oc->getName());
	 /* instead of referencing the class, we reference the program, because the
	    program contains the namespace that contains the class, and the class'
	    methods may call functions in the program as well that could otherwise
	    disappear when the program is deleted
	 */
	 if (p) {
	    printd(5, "QoreObject::init() this=%08p (%s) calling QoreProgram::depRef() (%08p)\n", this, theclass->getName(), p);
	    p->depRef();
	 }
      }

      DLLLOCAL ~qore_object_private()
      {
	 assert(!pgm);
	 assert(!data);
	 assert(!privateData);
      }
};

// for objects with multiple classes, private data has to be keyed
class KeyList
{
   private:
      keymap_t keymap;

   public:
      DLLLOCAL AbstractPrivateData *getReferencedPrivateData(qore_classid_t key) const
      {
	 keymap_t::const_iterator i = keymap.find(key);
	 if (i == keymap.end())
	    return 0;

	 AbstractPrivateData *apd = i->second.first;
	 apd->ref();
	 return apd;
      }

      DLLLOCAL void addToString(QoreString *str) const
      {
	 for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i)
	    str->sprintf("%d=<0x%08p>, ", i->first, i->second.first);
      }

      DLLLOCAL void derefAll(ExceptionSink *xsink) const
      {
	 for (keymap_t::const_iterator i = keymap.begin(), e = keymap.end(); i != e; ++i)
	    if (!i->second.second)
	       i->second.first->deref(xsink);
      }

      DLLLOCAL AbstractPrivateData *getAndClearPtr(qore_classid_t key)
      {
	 keymap_t::iterator i = keymap.find(key);
	 if (i == keymap.end())
	    return 0;

	 assert(!i->second.second);
	 AbstractPrivateData *rv = i->second.first;
	 keymap.erase(i);
	 return rv;
      }

      DLLLOCAL void insert(qore_classid_t key, AbstractPrivateData *pd)
      {
	 keymap.insert(std::make_pair(key, std::make_pair(pd, false)));
      }

      DLLLOCAL void insertVirtual(qore_classid_t key, AbstractPrivateData *pd)
      {
	 if (keymap.find(key) == keymap.end())
	    keymap.insert(std::make_pair(key, std::make_pair(pd, true)));
      }
};

QoreObject::QoreObject(const QoreClass *oc, QoreProgram *p) : AbstractQoreNode(NT_OBJECT, false, false), priv(new qore_object_private(oc, p, new QoreHashNode()))
{
}

QoreObject::QoreObject(const QoreClass *oc, QoreProgram *p, QoreHashNode *h) : AbstractQoreNode(NT_OBJECT, false, false), priv(new qore_object_private(oc, p, h))
{
}

QoreObject::~QoreObject()
{
   //tracein("QoreObject::~QoreObject()");
   //printd(5, "QoreObject::~QoreObject() this=%08p, pgm=%08p, class=%s\n", this, priv->pgm, priv->theclass->getName());
   delete priv;
}

const QoreClass *QoreObject::getClass() const 
{ 
   return priv->theclass; 
}

const char *QoreObject::getClassName() const 
{ 
   return priv->theclass->getName(); 
}

int QoreObject::getStatus() const 
{ 
   return priv->status; 
}

bool QoreObject::isValid() const 
{ 
   return priv->status == OS_OK; 
}

QoreProgram *QoreObject::getProgram() const
{
   return priv->pgm;
}

bool QoreObject::isSystemObject() const
{
   return priv->system_object;
}

void QoreObject::tRef() const
{
   printd(5, "QoreObject::tRef(this=%08p) class=%s, tref %d->%d\n", this, priv->theclass->getName(), priv->tRefs.reference_count(), priv->tRefs.reference_count() + 1);
   priv->tRefs.ROreference();
}

void QoreObject::tDeref()
{
   printd(5, "QoreObject::tDeref(this=%08p) class=%s, tref %d->%d\n", this, priv->theclass->getName(), priv->tRefs.reference_count(), priv->tRefs.reference_count() - 1);
   if (priv->tRefs.ROdereference())
      delete this;
}

AbstractQoreNode *QoreObject::evalBuiltinMethodWithPrivateData(BuiltinMethod *meth, const QoreListNode *args, ExceptionSink *xsink)
{
   // get referenced object
   AbstractPrivateData *pd = getReferencedPrivateData(meth->myclass->getIDForMethod(), xsink);

   if (pd) {
      AbstractQoreNode *rv = meth->evalMethod(this, pd, args, xsink);
      pd->deref(xsink);
      return rv;
   }

   //printd(5, "QoreObject::evalBuiltingMethodWithPrivateData() this=%08p, call=%s::%s(), class ID=%d, method class ID=%d\n", this, meth->myclass->getName(), meth->getName(), meth->myclass->getID(), meth->myclass->getIDForMethod());
   if (xsink->isException())
      return 0;
   if (priv->theclass == meth->myclass)
      xsink->raiseException("OBJECT-ALREADY-DELETED", "the method %s::%s() cannot be executed because the object has already been deleted", priv->theclass->getName(), meth->getName());
   else
      xsink->raiseException("OBJECT-ALREADY-DELETED", "the method %s::%s() (base class of '%s') cannot be executed because the object has already been deleted", meth->myclass->getName(), meth->getName(), priv->theclass->getName());
   return 0;
}

void QoreObject::evalCopyMethodWithPrivateData(BuiltinMethod *meth, QoreObject *self, const char *class_name, ExceptionSink *xsink)
{
   // get referenced object
   AbstractPrivateData *pd = getReferencedPrivateData(meth->myclass->getID(), xsink);

   if (pd) {
      meth->evalCopy(self, this, pd, class_name, xsink);
      pd->deref(xsink);
      return;
   }

   if (xsink->isException())
      return;
   if (priv->theclass == meth->myclass)
      xsink->raiseException("OBJECT-ALREADY-DELETED", "the method %s::copy() cannot be executed because the object has already been deleted", priv->theclass->getName());
   else
      xsink->raiseException("OBJECT-ALREADY-DELETED", "the method %s::copy() (base class of '%s') cannot be executed because the object has already been deleted", meth->myclass->getName(), priv->theclass->getName());
}

// note that the lock is already held when this method is called
bool QoreObject::evalDeleteBlocker(BuiltinMethod *meth)
{
   // FIXME: eliminate reference counts for private data, private data should be destroyed after the destructor terminates

   // get referenced object
   ExceptionSink xsink;
   ReferenceHolder<AbstractPrivateData> pd(priv->privateData->getReferencedPrivateData(meth->myclass->getIDForMethod()), &xsink);

   if (pd)
      return meth->evalDeleteBlocker(this, *pd);

   //printd(5, "QoreObject::evalBuiltingMethodWithPrivateData() this=%08p, call=%s::%s(), class ID=%d, method class ID=%d\n", this, meth->myclass->getName(), meth->getName(), meth->myclass->getID(), meth->myclass->getIDForMethod());
   return false;
}

bool QoreObject::validInstanceOf(qore_classid_t cid) const
{
   if (priv->status == OS_DELETED)
      return 0;

   return priv->theclass->getClass(cid);
}

AbstractQoreNode *QoreObject::evalMethod(const QoreString *name, const QoreListNode *args, ExceptionSink *xsink)
{
   TempEncodingHelper tmp(name, QCS_DEFAULT, xsink);
   if (!tmp)
      return 0;

   return evalMethod(tmp->getBuffer(), args, xsink);
}

AbstractQoreNode *QoreObject::evalMethod(const char *name, const QoreListNode *args, ExceptionSink *xsink)
{
   return priv->theclass->evalMethod(this, name, args, xsink);
}

AbstractQoreNode *QoreObject::evalMethod(const QoreMethod &method, const QoreListNode *args, ExceptionSink *xsink)
{
   return method.eval(this, args, xsink);
}

const QoreClass *QoreObject::getClass(qore_classid_t cid) const
{
   if (cid == priv->theclass->getID())
      return priv->theclass;
   return priv->theclass->getClass(cid);
}

AbstractQoreNode *QoreObject::evalMember(const QoreString *member, ExceptionSink *xsink)
{
   // make sure to convert string encoding if necessary to default character set
   TempEncodingHelper tstr(member, QCS_DEFAULT, xsink);
   if (!tstr)
      return 0;

   const char *mem = tstr->getBuffer();

   //printd(5, "QoreObject::evalMember() find_key(%s)=%08p theclass=%s\n", mem, find_key(mem), theclass ? theclass->getName() : "NONE");
   // if accessed outside the class and the member is a private member 
   QoreObject *obj = getStackObject();
   printd(5, "QoreObject::evalMember(%s) obj=%08p class=%s ID=%d stack obj=%08p class=%s ID=%d isPrivateMember=%s\n", mem, this, priv->theclass->getName(), priv->theclass->getID(), obj, obj ? obj->priv->theclass->getName() : "(null)", obj ? obj->priv->theclass->getID() : -1, priv->theclass->isPrivateMember(mem) ? "true" : "false");

   // if accessed outside the class and the member is a private member 
   if ((!obj || (obj && obj->priv->theclass->getID() != priv->theclass->getID())) && priv->theclass->isPrivateMember(mem)) {
      if (priv->theclass->hasMemberGate()) {
	 return priv->theclass->evalMemberGate(this, *tstr, xsink);
      }
      xsink->raiseException("PRIVATE-MEMBER", "'%s' is a private member of class '%s'", mem, priv->theclass->getName());
      return 0;
   }

   AbstractQoreNode *rv;
   bool exists;
   {
      AutoLocker al(priv->m);

      if (priv->status == OS_DELETED)
	 return 0;

      rv = priv->data->getReferencedKeyValue(mem, exists);
   }

   // execute memberGate method for objects where no member exists
   if (!exists && priv->theclass->hasMemberGate()) {
      return priv->theclass->evalMemberGate(this, *tstr, xsink);
   }

   return rv;
}

// 0 = equal, 1 = not equal
bool QoreObject::compareSoft(const QoreObject *obj, ExceptionSink *xsink) const
{
   // currently objects are only equal if they are the same object
   return !(this == obj);
}

// 0 = equal, 1 = not equal
bool QoreObject::compareHard(const QoreObject *obj, ExceptionSink *xsink) const
{
   // currently objects are only equal if they are the same object
   return !(this == obj);
}

// lock already held
void QoreObject::doDeleteIntern(ExceptionSink *xsink)
{
   printd(5, "QoreObject::doDeleteIntern(this=%08p) execing destructor()\n", this);   
   priv->theclass->execDestructor(this, xsink);

   QoreHashNode *td;
   {
      AutoLocker al(priv->m);
      priv->status = OS_DELETED;
      td = priv->data;
      priv->data = 0;
   }
   cleanup(xsink, td);
}

void QoreObject::doDelete(ExceptionSink *xsink)
{
   {
      AutoLocker al(priv->m);

      if (priv->status == OS_DELETED)
	 return;

      if (priv->status > 0) {
	 xsink->raiseException("DOUBLE-DELETE-EXCEPTION", "destructor called from within destructor");
	 return;
      }

      // mark status as in destructor
      priv->status = gettid();
   }
   doDeleteIntern(xsink);
}

// does a deep dereference and executes the destructor if necessary
bool QoreObject::derefImpl(ExceptionSink *xsink)
{
   printd(5, "QoreObject::derefImpl() this=%08p, class=%s references=0 status=%d has_delete_blocker=%d delete_blocker_run=%d\n", this, getClassName(), priv->status, priv->theclass->has_delete_blocker(), priv->delete_blocker_run);
   {
      SafeLocker sl(priv->m);
      if (priv->status != OS_OK) {
	 // only execute tDeref() once per object
	 bool need_tderef = !priv->tderef_done;
	 if (need_tderef)
	    priv->tderef_done = true;
	 sl.unlock();
	 if (need_tderef)
	    tDeref();
	 return false;
      }

      // if the scope deletion is blocked, then do not run the destructor
      if (!priv->delete_blocker_run && priv->theclass->has_delete_blocker()) {
	 if (priv->theclass->execDeleteBlocker(this, xsink)) {
	    //printd(5, "QoreObject::derefImpl() this=%08p class=%s blocking delete\n", this, getClassName());
	    priv->delete_blocker_run = true;
	    //printd(0, "Object lock %08p unlocked (safe)\n", &priv->m);
	    return false;
	 }
      }

      //printd(5, "QoreObject::derefImpl() class=%s this=%08p going out of scope\n", getClassName(), this);

      // reference for destructor
      ROreference();

      // mark status as in destructor
      priv->status = gettid();

      //printd(0, "Object lock %08p unlocked (safe)\n", &priv->m);
   }

   doDeleteIntern(xsink);
   if (ROdereference()) {
      // check if we can execute tDeref()
      bool need_tderef;
      {
	 AutoLocker al(priv->m);
	 need_tderef = !priv->tderef_done;
	 if (need_tderef)
	    priv->tderef_done = true;
      }
      if (need_tderef)
	 tDeref();
   }

   return false;
}

// this method is called when there is an exception in a constructor and the object should be deleted
void QoreObject::obliterate(ExceptionSink *xsink)
{
   printd(5, "QoreObject::obliterate(this=%08p) class=%s %d->%d\n", this, priv->theclass->getName(), references, references - 1);

   if (ROdereference()) {
      {
	 SafeLocker sl(priv->m);
	 //printd(0, "Object lock %08p locked   (safe)\n", &priv->m);
	 printd(5, "QoreObject::obliterate() class=%s deleting this=%08p\n", priv->theclass->getName(), this);

	 if (priv->status == OS_OK) {
	    priv->status = OS_DELETED;
	    QoreHashNode *td = priv->data;
	    priv->data = 0;
	    //printd(0, "Object lock %08p unlocked (safe)\n", &priv->m);
	    sl.unlock();

	    if (priv->privateData)
	       priv->privateData->derefAll(xsink);

	    cleanup(xsink, td);
	 }
	 else {
	    printd(5, "QoreObject::obliterate() %08p data=%08p status=%d\n", this, priv->data, priv->status);
	    //printd(0, "Object lock %08p unlocked (safe)\n", &priv->m);
	 }
      }
      tDeref();
   }
}

class qore_object_lock_handoff_manager {
   private:
      QoreObject *self;
      AutoVLock *vl;

   public:
      DLLLOCAL qore_object_lock_handoff_manager(QoreObject *n_self, AutoVLock *n_vl) : self(n_self), vl(n_vl)
      {
	 // reference current object
	 self->tRef();

	 // unlock previous lock and release from AutoVLock structure
	 vl->del();

	 //printd(0, "Object lock %08p locked   (handoff by %s - vlock by %s)\n", &self->priv->m, who, vl->getWho());
	 // lock current object
	 self->priv->m.lock();
      }

      DLLLOCAL ~qore_object_lock_handoff_manager() 
      {
	 // unlock if lock not saved in AutoVLock structure
	 if (self) {
	    //printd(0, "Object lock %08p unlocked (handoff)\n", &self->priv->m);
	    self->priv->m.unlock();
	    self->tDeref();
	 }
      }

      DLLLOCAL void stay_locked()
      {
	 vl->set(self, &self->priv->m);
	 self = 0;
      }
};

// unlocking the lock is managed with the AutoVLock object
AbstractQoreNode **QoreObject::getMemberValuePtr(const char *key, AutoVLock *vl, ExceptionSink *xsink) const
{
   // do lock handoff
   qore_object_lock_handoff_manager qolhm(const_cast<QoreObject *>(this), vl);

   if (priv->status == OS_DELETED)
      return 0;

   qolhm.stay_locked();
   return priv->data->getKeyValuePtr(key);
}

// unlocking the lock is managed with the AutoVLock object
AbstractQoreNode **QoreObject::getMemberValuePtr(const QoreString *key, AutoVLock *vl, ExceptionSink *xsink) const
{
   TempEncodingHelper enc(key, QCS_DEFAULT, xsink);
   if (!enc)
      return 0;

   return getMemberValuePtr(enc->getBuffer(), vl, xsink);
}

// unlocking the lock is managed with the AutoVLock object
AbstractQoreNode *QoreObject::getMemberValueNoMethod(const QoreString *key, AutoVLock *vl, ExceptionSink *xsink) const
{
   TempEncodingHelper enc(key, QCS_DEFAULT, xsink);
   if (!enc)
      return 0;

   return getMemberValueNoMethod(enc->getBuffer(), vl, xsink);
}

// unlocking the lock is managed with the AutoVLock object
AbstractQoreNode *QoreObject::getMemberValueNoMethod(const char *key, AutoVLock *vl, ExceptionSink *xsink) const
{
   // do lock handoff
   qore_object_lock_handoff_manager qolhm(const_cast<QoreObject *>(this), vl);

   if (priv->status == OS_DELETED) {
      makeAccessDeletedObjectException(xsink, key, priv->theclass->getName());
      return 0;
   }

   AbstractQoreNode *rv = priv->data->getKeyValue(key);
   if (rv && rv->isReferenceCounted()) {
      qolhm.stay_locked();
   }
   return rv;
}

void QoreObject::deleteMemberValue(const QoreString *key, ExceptionSink *xsink)
{
   TempEncodingHelper enc(key, QCS_DEFAULT, xsink);
   if (!enc)
      return;

   deleteMemberValue(enc->getBuffer(), xsink);
}

void QoreObject::deleteMemberValue(const char *key, ExceptionSink *xsink)
{
   AbstractQoreNode *v;
   {
      AutoLocker al(priv->m);

      if (priv->status == OS_DELETED) {
	 makeAccessDeletedObjectException(xsink, key, priv->theclass->getName());
	 return;
      }
      
      v = priv->data->takeKeyValue(key);
   }

   if (!v)
      return;

   if (v->getType() == NT_OBJECT)
      reinterpret_cast<QoreObject *>(v)->doDelete(xsink);
   v->deref(xsink);
}

QoreListNode *QoreObject::getMemberList(ExceptionSink *xsink) const
{
   AutoLocker al(priv->m);

   if (priv->status == OS_DELETED) {
      makeAccessDeletedObjectException(xsink, priv->theclass->getName());
      return 0;
   }

   return priv->data->getKeys();
}

void QoreObject::setValue(const char *key, AbstractQoreNode *value, ExceptionSink *xsink)
{
   AbstractQoreNode *old_value;

   {
      AutoLocker al(priv->m);

      if (priv->status == OS_DELETED) {
	 makeAccessDeletedObjectException(xsink, key, priv->theclass->getName());
	 return;
      }

      old_value = priv->data->takeKeyValue(key);

      priv->data->setKeyValue(key, value, xsink);
   }

   if (old_value)
      old_value->deref(xsink);
}

int QoreObject::size(ExceptionSink *xsink) const
{
   AutoLocker al(priv->m);

   if (priv->status == OS_DELETED)
      return 0;

   return priv->data->size();
}

// adds all elements (and references them) from the hash passed, leaves the
// hash passed alone
void QoreObject::merge(const QoreHashNode *h, ExceptionSink *xsink)
{
   // list for saving all overwritten values to be dereferenced outside the object lock
   ReferenceHolder<QoreListNode> holder(xsink);

   {
      AutoLocker al(priv->m);

      if (priv->status == OS_DELETED) {
	 makeAccessDeletedObjectException(xsink, priv->theclass->getName());
	 return;
      }

      ConstHashIterator hi(h);
      while (hi.next()) {
	 AbstractQoreNode *n = priv->data->swapKeyValue(hi.getKey(), hi.getReferencedValue());
	 // if we are overwriting a value, then save it in the list for deleting after the lock is released
	 if (n) {
	    if (!holder)
	       holder = new QoreListNode();
	    holder->push(n);
	 }
      }
   }
}

AbstractQoreNode *QoreObject::getReferencedMemberNoMethod(const char *mem, ExceptionSink *xsink) const
{
   AutoLocker al(priv->m);

   printd(5, "QoreObject::getReferencedMemberNoMethod(this=%08p, mem=%08p (%s), xsink=%08p, data->size()=%d)\n",
	  this, mem, mem, xsink, priv->data ? priv->data->size() : -1);

   if (priv->status == OS_DELETED) {
      makeAccessDeletedObjectException(xsink, mem, priv->theclass->getName());
      return 0;
   }

   return priv->data->getReferencedKeyValue(mem);
}

QoreHashNode *QoreObject::copyData(ExceptionSink *xsink) const
{
   AutoLocker al(priv->m);

   if (priv->status == OS_DELETED)
      return 0;
   
   return priv->data->copy();
}

void QoreObject::mergeDataToHash(QoreHashNode *hash, ExceptionSink *xsink)
{
   AutoLocker al(priv->m);

   if (priv->status == OS_DELETED) {
      makeAccessDeletedObjectException(xsink, priv->theclass->getName());
      return;
   }

   hash->merge(priv->data, xsink);
}

// unlocking the lock is managed with the AutoVLock object
// we check if the object is already locked
AbstractQoreNode **QoreObject::getExistingValuePtr(const QoreString *mem, AutoVLock *vl, ExceptionSink *xsink) const
{
   TempEncodingHelper enc(mem, QCS_DEFAULT, xsink);
   if (!enc)
      return 0;

   return getExistingValuePtr(enc->getBuffer(), vl, xsink);
}

// unlocking the lock is managed with the AutoVLock object
// we check if the object is already locked
AbstractQoreNode **QoreObject::getExistingValuePtr(const char *mem, AutoVLock *vl, ExceptionSink *xsink) const
{
   // do lock handoff
   qore_object_lock_handoff_manager qolhm(const_cast<QoreObject *>(this), vl);

   if (priv->status == OS_DELETED) {
      makeAccessDeletedObjectException(xsink, mem, priv->theclass->getName());
      return 0;
   }

   AbstractQoreNode **rv = priv->data->getExistingValuePtr(mem);
   if (rv) {
      qolhm.stay_locked();
   }

   return rv;
}

AbstractPrivateData *QoreObject::getReferencedPrivateData(qore_classid_t key, ExceptionSink *xsink) const
{ 
   AutoLocker al(priv->m);

   if (priv->status == OS_DELETED || !priv->privateData)
      return 0;

   return priv->privateData->getReferencedPrivateData(key);
}

AbstractPrivateData *QoreObject::getAndClearPrivateData(qore_classid_t key, ExceptionSink *xsink)
{
   AutoLocker al(priv->m);

   if (priv->privateData)
      return priv->privateData->getAndClearPtr(key);

   return 0;
}

// add virtual IDs for private data to class list
void QoreObject::addVirtualPrivateData(AbstractPrivateData *apd)
{
   class BCSMList *sml = priv->theclass->getBCSMList();
   if (!sml)
      return;

   for (class_list_t::const_iterator i = sml->begin(), e = sml->end(); i != e; ++i)
      if ((*i).second)
	 priv->privateData->insertVirtual((*i).first->getID(), apd);
}

// called only during constructor execution, therefore no need for locking
void QoreObject::setPrivate(qore_classid_t key, AbstractPrivateData *pd)
{ 
   if (!priv->privateData)
      priv->privateData = new KeyList();
   priv->privateData->insert(key, pd);
   addVirtualPrivateData(pd);
}

void QoreObject::addPrivateDataToString(QoreString *str, ExceptionSink *xsink) const
{
   str->concat('(');
   AutoLocker al(priv->m);

   if (priv->status == OS_OK && priv->privateData) {
      priv->privateData->addToString(str);
      str->terminate(str->strlen() - 2);
   }
   else
      str->concat("<NO PRIVATE DATA>");

   str->concat(')');
}

void QoreObject::cleanup(ExceptionSink *xsink, QoreHashNode *td)
{
   if (priv->privateData) {
      delete priv->privateData;
#ifdef DEBUG
      priv->privateData = 0;
#endif
   }
   
   if (priv->pgm) {
      printd(5, "QoreObject::cleanup() this=%08p (%s) calling QoreProgram::depDeref() (%08p)\n", this, priv->theclass->getName(), priv->pgm);
      priv->pgm->depDeref(xsink);
#ifdef DEBUG
      priv->pgm = 0;
#endif
   }

   td->deref(xsink);
}

void QoreObject::defaultSystemDestructor(qore_classid_t classID, ExceptionSink *xsink)
{
   AbstractPrivateData *pd = getAndClearPrivateData(classID, xsink);
   printd(5, "QoreObject::defaultSystemDestructor() this=%08p class=%s private_data=%08p\n", this, priv->theclass->getName(), pd); 
   if (pd)
      pd->deref(xsink);
}

QoreString *QoreObject::getAsString(bool &del, int foff, ExceptionSink *xsink) const
{
   del = false;

   TempString rv(new QoreString());
   if (getAsString(*(*rv), foff, xsink))
      return 0;

   del = true;
   return rv.release();
}

int QoreObject::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const
{
   QoreHashNodeHolder h(copyData(xsink), xsink);
   if (*xsink)
      return -1;

   str.sprintf("class %s: ", priv->theclass->getName());

   if (foff != FMT_NONE) {
      addPrivateDataToString(&str, xsink);
      if (*xsink)
         return -1;

      str.concat(' ');
   }
   if (!h->size())
      str.concat("<NO MEMBERS>");
   else {
      if (foff != FMT_NONE)
         str.sprintf("(%d member%s)\n", h->size(), h->size() == 1 ? "" : "s");
      else
         str.concat('(');

      class HashIterator hi(*h);

      bool first = false;
      while (hi.next()) {
         if (first)
            if (foff != FMT_NONE)
               str.concat('\n');
            else
               str.concat(", ");
         else
            first = true;

         if (foff != FMT_NONE)
            str.addch(' ', foff + 2);

         str.sprintf("%s : ", hi.getKey());

	 AbstractQoreNode *n = hi.getValue();
	 if (!n) n = &Nothing;
	 if (n->getAsString(str, foff != FMT_NONE ? foff + 2 : foff, xsink))
	    return -1;
      }
      if (foff == FMT_NONE)
         str.concat(')');
   }

   return 0;
}

AbstractQoreNode *QoreObject::realCopy() const
{
   return refSelf();
}

// performs a lexical compare, return -1, 0, or 1 if the "this" value is less than, equal, or greater than
// the "val" passed
//DLLLOCAL virtual int compare(const AbstractQoreNode *val) const;
// the type passed must always be equal to the current type
bool QoreObject::is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const
{
   const QoreObject *o = dynamic_cast<const QoreObject *>(v);
   if (!o)
      return false;
   return !compareSoft(o, xsink);
}

bool QoreObject::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const
{
   const QoreObject *o = dynamic_cast<const QoreObject *>(v);
   if (!o)
      return false;
   return !compareHard(o, xsink);
}

// returns the type name as a c string
const char *QoreObject::getTypeName() const
{
   return getStaticTypeName();
}

AbstractQoreNode *QoreObject::evalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

AbstractQoreNode *QoreObject::evalImpl(bool &needs_deref, ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

int64 QoreObject::bigIntEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

int QoreObject::integerEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0;
}

bool QoreObject::boolEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return false;
}

double QoreObject::floatEvalImpl(ExceptionSink *xsink) const
{
   assert(false);
   return 0.0;
}

bool QoreObject::hasMemberNotification() const
{
   return priv->theclass->hasMemberNotification();
}

void QoreObject::execMemberNotification(const char *member, ExceptionSink *xsink)
{
   priv->theclass->execMemberNotification(this, member, xsink);
}

#ifdef QORE_CLASS_SYNCHRONOUS
VRMutex *QoreObject::getClassSyncLock()
{
   return priv->sync_vrm;
}
#endif
