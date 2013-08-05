/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  qore_program_private.h

  Qore Programming Language

  Copyright 2003 - 2013 David Nichols

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

#ifndef _QORE_QORE_PROGRAM_PRIVATE_H
#define _QORE_QORE_PROGRAM_PRIVATE_H

extern QoreListNode* ARGV, *QORE_ARGV;
extern QoreHashNode* ENV;

#include <qore/intern/ParserSupport.h>
#include <qore/intern/QoreNamespaceIntern.h>

#include <stdarg.h>
#include <errno.h>

class QoreParseLocationHelper {
public:
   DLLLOCAL QoreParseLocationHelper(const char* file, const char* src, int offset) {
      beginParsing(file, 0, src, offset);
   }

   DLLLOCAL ~QoreParseLocationHelper() {
      endParsing();
   }
};

class CharPtrList : public safe_dslist<const char* > {
public:
   // returns true for found, false for not found
   // FIXME: use STL find algorithm
   DLLLOCAL bool find(const char* str) const {
      const_iterator i = begin();
      while (i != end()) {
	 if (!strcmp(*i, str))
	    return true;
	 i++;
      }
   
      return false;
   }
};

// local variable container
typedef safe_dslist<LocalVar*> local_var_list_t;

class LocalVariableList : public local_var_list_t {
public:
   DLLLOCAL LocalVariableList() {
   }

   DLLLOCAL ~LocalVariableList() {
      for (local_var_list_t::iterator i = begin(), e = end(); i != e; ++i)
         delete *i;
   }
};

typedef QoreThreadLocalStorage<QoreHashNode> qpgm_thread_local_storage_t;

class ThreadLocalVariableData : public ThreadLocalData<LocalVarValue> {
public:
   // marks all variables as finalized on the stack
   DLLLOCAL void finalize(ExceptionSink* xsink) {
      ThreadLocalVariableData::iterator i(curr);
      while (i.next())
         i.get().finalize(xsink);
   }

   // deletes everything on the stack
   DLLLOCAL void del(ExceptionSink* xsink) {
      // then we uninstantiate
      while (curr->prev || curr->pos)
         uninstantiate(xsink);
   }

   DLLLOCAL LocalVarValue *instantiate() {
      if (curr->pos == QORE_THREAD_STACK_BLOCK) {
	 if (curr->next)
	    curr = curr->next;
	 else {
	    curr->next = new Block(curr);
	    //printf("this=%p: add curr=%p, curr->next=%p\n", this, curr, curr->next);
	    curr = curr->next;
	 }
      }
      return &curr->var[curr->pos++];
   }

   DLLLOCAL void uninstantiate(ExceptionSink* xsink) {
      if (!curr->pos) {
	 if (curr->next) {
	    //printf("this %p: del curr=%p, curr->next=%p\n", this, curr, curr->next);
	    delete curr->next;
	    curr->next = 0;
	 }
	 curr = curr->prev;
      }
      curr->var[--curr->pos].uninstantiate(xsink);
   }

   DLLLOCAL LocalVarValue *find(const char* id) {
      Block *w = curr;
      while (true) {
	 int p = w->pos;
	 while (p) {
	    if (w->var[--p].id == id && !w->var[p].skip)
	       return &w->var[p];
	 }
	 w = w->prev;
#ifdef DEBUG
	 if (!w) {
            printd(0, "ThreadLocalVariableData::find() this=%p no local variable '%s' (%p) on stack (pgm=%p) p=%d\n", this, id, id, getProgram(), p);
            p = curr->pos - 1;
            while (p >= 0) {
               printd(0, "var p=%d: %s (%p) (skip=%d)\n", p, curr->var[p].id, curr->var[p].id, curr->var[p].skip);
               --p;
            }
         }
#endif
	 assert(w);
      }
      // to avoid a warning on most compilers - note that this generates a warning on recent versions of aCC!
      return 0;
   }
};

class ThreadClosureVariableStack : public ThreadLocalData<ClosureVarValue*> {
private:
   DLLLOCAL void instantiate(ClosureVarValue *cvar) {
      //printd(5, "ThreadClosureVariableStack::instantiate(%p = '%s') this: %p pgm: %p\n", cvar->id, cvar->id, this, getProgram());

      if (curr->pos == QORE_THREAD_STACK_BLOCK) {
	 if (curr->next)
	    curr = curr->next;
	 else {
	    curr->next = new Block(curr);
	    //printf("this=%p: add curr=%p, curr->next=%p\n", this, curr, curr->next);
	    curr = curr->next;
	 }
      }
      curr->var[curr->pos++] = cvar;
   }

public:
   // marks all variables as finalized on the stack
   DLLLOCAL void finalize(ExceptionSink* xsink) {
      ThreadClosureVariableStack::iterator i(curr);
      while (i.next())
         i.get()->finalize(xsink);
   }

   // deletes everything on the stack
   DLLLOCAL void del(ExceptionSink* xsink) {
      while (curr->prev || curr->pos)
         uninstantiate(xsink);
   }

   DLLLOCAL ClosureVarValue* instantiate(const char* id, const QoreTypeInfo* typeInfo, QoreValue& nval) {
      ClosureVarValue *cvar = new ClosureVarValue(id, typeInfo, nval);
      instantiate(cvar);
      return cvar;
   }

   DLLLOCAL void uninstantiate(ExceptionSink* xsink) {
#if 0
      if (!curr->pos)
         printd(5, "ThreadClosureVariableStack::uninstantiate() this: %p pos: %d %p %s\n", this, curr->prev->pos - 1, curr->prev->var[curr->prev->pos - 1]->id, curr->prev->var[curr->prev->pos - 1]->id);
      else
         printd(5, "ThreadClosureVariableStack::uninstantiate() this: %p pos: %d %p %s\n", this, curr->pos - 1, curr->var[curr->pos - 1]->id, curr->var[curr->pos - 1]->id);
#endif
      if (!curr->pos) {
	 if (curr->next) {
	    //printf("this %p: del curr=%p, curr->next=%p\n", this, curr, curr->next);
	    delete curr->next;
	    curr->next = 0;
	 }
	 curr = curr->prev;
      }
      curr->var[--curr->pos]->deref(xsink);
   }

   DLLLOCAL ClosureVarValue *find(const char* id) {
      //printd(5, "ThreadClosureVariableStack::find() this: %p id: %p\n", this, id);
      Block *w = curr;
      while (true) {
	 int p = w->pos;
	 while (p) {
	    //printd(5, "ThreadClosureVariableStack::find(%p '%s') this: %p checking %p '%s' skip: %d\n", id, id, this, w->var[p - 1]->id, w->var[p - 1]->id, w->var[p - 1]->skip);
	    if (w->var[--p]->id == id && !w->var[p]->skip) {
	       //printd(5, "ThreadClosureVariableStack::find(%p '%s') this: %p returning: %p\n", id, id, this, w->var[p]);
	       return w->var[p];
	    }
	 }
	 w = w->prev;
#ifdef DEBUG
	 if (!w) {
	    //printd(5, "ThreadClosureVariableStack::find() this: %p no closure-bound local variable '%s' (%p) on stack (pgm: %p) p: %d curr->prev: %p\n", this, id, id, getProgram(), p, curr->prev);
            p = curr->pos - 1;
            while (p >= 0) {
               //printd(5, "var p=%d: %s (%p) (skip=%d)\n", p, curr->var[p]->id, curr->var[p]->id, curr->var[p]->skip);
               --p;
            }
         }
#endif
	 assert(w);
      }
      // to avoid a warning on most compilers - note that this generates a warning on recent versions of aCC!
      return 0;
   }
};

struct ThreadLocalProgramData {
private:
   // not implemented
   DLLLOCAL ThreadLocalProgramData(const ThreadLocalProgramData &old);

public:
   // local variable data slots
   ThreadLocalVariableData lvstack;
   // closure variable stack
   ThreadClosureVariableStack cvstack;
   // current thread's time zone locale (if any)
   const AbstractQoreZoneInfo* tz;
   // the "time zone set" flag
   bool tz_set : 1;

   // top-level vars instantiated
   bool inst : 1;

   DLLLOCAL ThreadLocalProgramData() : tz(0), tz_set(false), inst(false) {
      //printd(5, "ThreadLocalProgramData::ThreadLocalProgramData() this=%p\n", this);
   }

   DLLLOCAL ~ThreadLocalProgramData() {
      assert(lvstack.empty());
      assert(cvstack.empty());
   }

   DLLLOCAL void finalize(ExceptionSink* xsink) {
      lvstack.finalize(xsink);
      cvstack.finalize(xsink);
   }

   DLLLOCAL void del(ExceptionSink* xsink) {
      lvstack.del(xsink);
      cvstack.del(xsink);
      delete this;
   }

   DLLLOCAL void setTZ(const AbstractQoreZoneInfo* n_tz) {
      tz_set = true;
      tz = n_tz;
   }

   DLLLOCAL void clearTZ() {
      tz_set = false;
      tz = 0;
   }
};

// maps from thread handles to thread-local data
typedef std::map<ThreadProgramData*, ThreadLocalProgramData*> pgm_data_map_t;

// map for "defines" in programs
typedef std::map<std::string, AbstractQoreNode* > dmap_t;

// map for pushed parse options
typedef std::map<const char*, int64, ltstr> ppo_t;

class AbstractQoreZoneInfo;

class qore_program_private_base {
   friend class QoreProgramAccessHelper;

public:
   LocalVariableList local_var_list;

   // for the thread counter, used only with plock
   QoreCondition pcond;
   unsigned thread_count;   // number of threads currently running in this Program
   unsigned thread_waiting; // number of threads waiting on all threads to terminate

   // to save file names for later deleting
   cstr_vector_t fileList;
   // features present in this Program object
   CharPtrList featureList;
   // user modules present in this Program object
   CharPtrList userFeatureList;

   // parse lock, making parsing actions atomic and thread-safe
   mutable QoreThreadLock plock;

   // set of signals being handled by code in this Program (to be deleted on exit)
   int_set_t sigset;

   // dependency counter, when this hits zero, the object is deleted
   QoreReferenceCounter dc;
   ExceptionSink* parseSink, *warnSink, *pendingParseSink;
   RootQoreNamespace* RootNS;
   QoreNamespace* QoreNS;

   // top level statements
   TopLevelStatementBlock sb;
      
   // bit field flags
   bool only_first_except : 1,
      po_locked : 1,
      po_allow_restrict : 1,
      exec_class : 1,
      base_object : 1,
      requires_exception : 1;

   int tclear;   // clearing thread-local variables in progress? if so, this is the TID

   int exceptions_raised,
      ptid;

   ParseWarnOptions pwo;

   int64 dom,    // a mask of functional domains used in this Program
      pend_dom;  // a mask of pending function domains used in this Program

   std::string exec_class_name, script_dir, script_path, script_name, include_path;

   // thread-local data (could be inherited from another program)
   qpgm_thread_local_storage_t *thread_local_storage;

   mutable QoreThreadLock tlock;  // thread variable data lock, for accessing the thread variable data map and the thr_init variable
   mutable QoreCondition tcond;   // cond variable for tclear to become false, used only with tlock
   mutable unsigned twaiting;     // threads waiting on tclear to become false

   // thread-local variable storage - map from thread ID to thread-local storage
   pgm_data_map_t pgm_data_map;

   // time zone setting for the program
   const AbstractQoreZoneInfo* TZ;

   // define map
   dmap_t dmap;

   // pushed parse option map
   ppo_t ppo;

   // thread initialization user code
   ResolvedCallReferenceNode* thr_init;

   // public object that owns this private implementation
   QoreProgram* pgm;

   DLLLOCAL qore_program_private_base(QoreProgram *n_pgm, int64 n_parse_options, QoreProgram *p_pgm = 0) 
      : thread_count(0), thread_waiting(0), plock(&ma_recursive), parseSink(0), warnSink(0), pendingParseSink(0), RootNS(0), QoreNS(0),
        only_first_except(false), po_locked(false), po_allow_restrict(true), exec_class(false), base_object(false),
        requires_exception(false), tclear(0),
        exceptions_raised(0), ptid(0), pwo(n_parse_options), dom(0), pend_dom(0), thread_local_storage(0), twaiting(0),
        thr_init(0), pgm(n_pgm) {
      //printd(5, "qore_program_private_base::qore_program_private_base() this: %p pgm: %p\n", this, pgm);

      if (p_pgm)
	 setParent(p_pgm, n_parse_options);
      else {
	 TZ = QTZM.getLocalZoneInfo();
	 newProgram();
      }

      // initialize global vars
      Var *var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "ARGV", listTypeInfo);
      if (var && ARGV)
	 var->setInitial(ARGV->copy());
	 
      var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "QORE_ARGV", listTypeInfo);
      if (var && QORE_ARGV)
	 var->setInitial(QORE_ARGV->copy());
	 
      var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "ENV", hashTypeInfo);
      if (var)
         var->setInitial(ENV->copy());
   }

#ifdef DEBUG
   DLLLOCAL ~qore_program_private_base() {
      //printd(5, "qore_program_private_base::~qore_program_private_base() this: %p pgm: %p\n", this, pgm);
   }
#endif

   DLLLOCAL void startThread(ExceptionSink& xsink);

protected:
   DLLLOCAL void setParent(QoreProgram *p_pgm, int64 n_parse_options);

   // for independent programs (not inherited from another QoreProgram object)
   DLLLOCAL void newProgram();
};

class PreParseHelper {
protected:
   qore_program_private_base *p;
   bool swapped;

public:
   DLLLOCAL PreParseHelper(qore_program_private_base *n_p) : p(n_p), swapped(false) {
      if (!p->parseSink) {
         if (!p->pendingParseSink)
            p->pendingParseSink = new ExceptionSink;
         p->parseSink = p->pendingParseSink;
         swapped = true;
      }
   }

   DLLLOCAL ~PreParseHelper() {
      if (swapped)
         p->parseSink = 0;
   }
};

class qore_program_private : public qore_program_private_base {
private:
   DLLLOCAL void init(QoreProgram *n_pgm, int64 n_parse_options, const AbstractQoreZoneInfo *n_TZ = QTZM.getLocalZoneInfo()) {
   }

public:
   DLLLOCAL qore_program_private(QoreProgram *n_pgm, int64 n_parse_options) : qore_program_private_base(n_pgm, n_parse_options) {
      printd(5, "qore_program_private::qore_program_private() this: %p pgm: %p\n", this, pgm);
   }

   DLLLOCAL qore_program_private(QoreProgram *n_pgm, int64 n_parse_options, QoreProgram *p_pgm) : qore_program_private_base(n_pgm, n_parse_options, p_pgm) {
      printd(5, "qore_program_private::qore_program_private() this: %p pgm: %p\n", this, pgm);
   }
   
   DLLLOCAL ~qore_program_private() {
      printd(5, "qore_program_private::~qore_program_private() this: %p pgm: %p\n", this, pgm);
      assert(!parseSink);
      assert(!warnSink);
      assert(!pendingParseSink);
      assert(pgm_data_map.empty());
   }

   DLLLOCAL void depDeref(ExceptionSink* xsink) {
      printd(5, "qore_program_private::depDeref() this=%p pgm=%p %d->%d\n", this, pgm, dc.reference_count(), dc.reference_count() - 1);
      if (dc.ROdereference()) {
         del(xsink);
         delete pgm;
      }
   }

   DLLLOCAL void clearProgramThreadData(ExceptionSink* xsink) {
      // delete local variables for all threads that have used this program
      pgm_data_map_t pdm_copy;
      // copy the map and run on the copy to avoid deadlocks
      {
         AutoLocker al(tlock);
         // twaiting must be 0 here, as it can only be incremented while clearProgramThreadData() is in progress, which can only be executed once
         assert(!twaiting);
         assert(!tclear);
         tclear = gettid();
         pdm_copy = pgm_data_map;
      }

      for (pgm_data_map_t::iterator i = pdm_copy.begin(), e = pdm_copy.end(); i != e; ++i) {
         i->second->finalize(xsink);
      }

      for (pgm_data_map_t::iterator i = pdm_copy.begin(), e = pdm_copy.end(); i != e; ++i) {
         i->second->del(xsink);
         i->first->delProgram(pgm);
      }
   }

   DLLLOCAL void waitForTerminationAndClear(ExceptionSink* xsink);

   // called when the program's ref count = 0 (but the dc count may not go to 0 yet)
   DLLLOCAL void clear(ExceptionSink* xsink);

   // returns 0 for OK, -1 for error
   DLLLOCAL int incThreadCount(ExceptionSink* xsink) {
      // grab program-level lock
      AutoLocker al(plock);
      if (ptid && ptid != gettid()) {
         xsink->raiseException("PROGRAM-ERROR", "the program accessed has already been deleted");
         return -1;
      }
      ++thread_count;
      return 0;
   }

   DLLLOCAL void decThreadCount() {
      // grab program-level lock
      AutoLocker al(plock);
      assert(thread_count > 0);
      if (!--thread_count && thread_waiting)
	 pcond.broadcast();
   }

   DLLLOCAL void waitForAllThreadsToTerminateIntern() {
      while (thread_count) {
         ++thread_waiting;
         pcond.wait(plock);
         --thread_waiting;
      }
   }

   DLLLOCAL void waitForAllThreadsToTerminate() {
      // grab program-level lock
      AutoLocker al(&plock);
      waitForAllThreadsToTerminateIntern();
   }

   DLLLOCAL const char* parseGetScriptPath() const {
      return script_path.empty() ? 0 : script_path.c_str();
   }

   DLLLOCAL const char* parseGetScriptDir() const {
      return script_dir.empty() ? 0 : script_dir.c_str();
   }

   DLLLOCAL const char* parseGetScriptName() const {
      return script_name.empty() ? 0 : script_name.c_str();
   }

   DLLLOCAL QoreStringNode* getScriptPath() const {
      // grab program-level parse lock
      AutoLocker al(&plock);
      return script_path.empty() ? 0 : new QoreStringNode(script_path);
   }

   DLLLOCAL QoreStringNode* getScriptDir() const {
      // grab program-level parse lock
      AutoLocker al(&plock);
      return script_dir.empty() ? 0 : new QoreStringNode(script_dir);
   }

   DLLLOCAL QoreStringNode* getScriptName() const {
      // grab program-level parse lock
      AutoLocker al(&plock);
      return script_name.empty() ? 0 : new QoreStringNode(script_name);
   }

   DLLLOCAL void setScriptPathExtern(const char* path) {
      // grab program-level parse lock
      AutoLocker al(&plock);
      setScriptPath(path);
   }

   DLLLOCAL void setScriptPath(const char* path) {
      if (!path) {
	 script_dir.clear();
	 script_path.clear();
	 script_name.clear();
      }
      else {
	 // find file name
	 const char* p = q_basenameptr(path);
	 if (p == path) {
	    script_name = path;
	    script_dir = "." QORE_DIR_SEP_STR;
	    script_path = script_dir + script_name;
	 }
	 else {
	    script_path = path;
	    script_name = p;
	    script_dir.assign(path, p - path);
	 }
      }
   }

   DLLLOCAL QoreListNode* getVarList() {
      AutoLocker al(&plock);
      // FIXME: implement
      return new QoreListNode;
      //return global_var_list.getVarList();
   }

   DLLLOCAL QoreListNode* getFeatureList() const {
      QoreListNode* l = new QoreListNode();
	 
      for (CharPtrList::const_iterator i = featureList.begin(), e = featureList.end(); i != e; ++i)
	 l->push(new QoreStringNode(*i));
	 
      for (CharPtrList::const_iterator i = userFeatureList.begin(), e = userFeatureList.end(); i != e; ++i)
	 l->push(new QoreStringNode(*i));
	 
      return l;
   }

   DLLLOCAL void internParseRollback();

   // call must push the current program on the stack and pop it afterwards
   DLLLOCAL int internParsePending(const char* code, const char* label, const char* orig_src = 0, int offset = 0) {
      //printd(5, "qore_program_private::internParsePending() code: %d bytes label: '%s' src: '%s' offset: %d\n", strlen(code), label, orig_src ? orig_src : "(null)", offset);
 
      assert(code && code[0]);

      // save this file name for storage in the parse tree and deletion
      // when the QoreProgram object is deleted
      char* sname = strdup(label);
      addFile(sname);
      char* src = orig_src ? strdup(orig_src) : 0;
      if (src)
         addFile(src);

      QoreParseLocationHelper qplh(sname, src, offset);

      beginParsing(sname, 0, src, offset);

      // no need to save buffer, because it's deleted automatically in lexer
      //printd(5, "qore_program_private::internParsePending() parsing tag=%s (%p): '%s'\n", label, label, code);

      yyscan_t lexer;
      yylex_init(&lexer);
      yy_scan_string(code, lexer);
      yyset_lineno(1, lexer);
      // yyparse() will call endParsing() and restore old pgm position
      yyparse(lexer);

      printd(5, "qore_program_private::internParsePending() returned from yyparse()\n");
      int rc = 0;
      if (parseSink->isException()) {
	 rc = -1;
	 printd(5, "qore_program_private::internParsePending() parse exception: calling parseRollback()\n");
	 internParseRollback();
	 requires_exception = false;
      }

      printd(5, "qore_program_private::internParsePending() about to call yylex_destroy()\n");
      yylex_destroy(lexer);
      printd(5, "qore_program_private::internParsePending() returned from yylex_destroy()\n");
      return rc;
   }

   DLLLOCAL void startParsing(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
      warnSink = wS;
      pwo.warn_mask = wm;
      parseSink = xsink;

      if (pendingParseSink) {
         parseSink->assimilate(pendingParseSink);
         pendingParseSink = 0;
      }
   }

   DLLLOCAL int parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm, const char* orig_src = 0, int offset = 0) {
      //printd(5, "qore_program_private::parsePending() wm=0x%x UV=0x%x on=%d\n", wm, QP_WARN_UNREFERENCED_VARIABLE, wm & QP_WARN_UNREFERENCED_VARIABLE);

      ProgramThreadCountContextHelper pch(xsink, pgm, false);
      if (*xsink)
         return -1;

      // grab program-level parse lock
      AutoLocker al(plock);

      startParsing(xsink, wS, wm);

      int rc = internParsePending(code, label, orig_src, offset);
      warnSink = 0;
#ifdef DEBUG
      parseSink = 0;
#endif
      return rc;
   }

   // caller must have grabbed the lock and put the current program on the program stack
   DLLLOCAL int internParseCommit();

   // checks to see if parseCommit() can be called - updating existing runtime data structures
   DLLLOCAL int checkParseCommitUnlocked(ExceptionSink* xsink) {
      // if no threads are running, return 0
      if (!thread_count)
	 return 0;

      // if one thread is running, and it's the current thread, return 0
      if (thread_count == 1 && getProgram() == pgm)
	 return 0;

      xsink->raiseException("PROGRAM-PARSE-CONFLICT", "cannot execute any operation on a program object that modifies run-time data structures when another thread is currently executing in that program object (thread count: %d)", thread_count);
      return -1;
   }

   DLLLOCAL int parseCommit(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
      // grab program-level parse lock
      AutoLocker al(plock);
      if (checkParseCommitUnlocked(xsink))
	 return -1;

      startParsing(xsink, wS, wm);

      // finalize parsing, back out or commit all changes
      int rc = internParseCommit();

#ifdef DEBUG
      parseSink = 0;
#endif   
      warnSink = 0;
      // release program-level parse lock
      return rc;
   }

   DLLLOCAL void parseRollback() {
      // grab program-level parse lock and release on exit
      AutoLocker al(&plock);
   
      // back out all pending changes
      internParseRollback();
   }

   DLLLOCAL void parse(FILE *fp, const char* name, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
      printd(5, "QoreProgram::parse(fp=%p, name=%s, xsink=%p, wS=%p, wm=%d)\n", fp, name, xsink, wS, wm);

      // if already at the end of file, then return
      // try to get one character from file
      int c = fgetc(fp);
      if (feof(fp)) {
	 printd(5, "QoreProgram::parse(fp=%p, name=%s) EOF\n", fp, name);
	 return;
      }
      // push back read character
      ungetc(c, fp);

      yyscan_t lexer;
	 
      // grab program-level parse lock
      {
         ProgramThreadCountContextHelper pch(xsink, pgm, false);
         if (*xsink)
            return;

	 AutoLocker al(plock);
	 if (checkParseCommitUnlocked(xsink))
	    return;

         startParsing(xsink, wS, wm);
	 
	 // save this file name for storage in the parse tree and deletion
	 // when the QoreProgram object is deleted
	 char* sname = strdup(name);
	 addFile(sname);

	 QoreParseLocationHelper qplh(sname, 0, 0);

	 beginParsing(sname);
	 
	 //printd(5, "QoreProgram::parse(): about to call yyparse()\n");
	 yylex_init(&lexer);
	 yyset_in(fp, lexer);
	 // yyparse() will call endParsing() and restore old pgm position
	 yyparse(lexer);
	 
	 // finalize parsing, back out or commit all changes
	 internParseCommit();
	 
#ifdef DEBUG
	 parseSink = 0;
#endif
	 warnSink = 0;
	 // release program-level parse lock
      }
	 
      yylex_destroy(lexer);
      if (only_first_except && exceptions_raised > 1)
         fprintf(stderr, "\n%d exception(s) skipped\n\n", exceptions_raised);
   }

   DLLLOCAL void parse(const QoreString *str, const QoreString *lstr, ExceptionSink* xsink, ExceptionSink* wS, int wm, const QoreString* source = 0, int offset = 0) {
      if (!str->strlen())
	 return;

      // ensure code string has correct character set encoding
      TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
      if (*xsink)
	 return;

      // ensure label string has correct character set encoding
      TempEncodingHelper tlstr(lstr, QCS_DEFAULT, xsink);
      if (*xsink)
	 return;

      TempEncodingHelper src;
      if (source && !source->empty() && !src.set(source, QCS_DEFAULT, xsink))
         return;

      parse(tstr->getBuffer(), tlstr->getBuffer(), xsink, wS, wm, source ? src->getBuffer() : 0, offset);
   }

   DLLLOCAL void parse(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm, const char* orig_src = 0, int offset = 0) {
      //printd(5, "qore_program_private::parse(%s) pgm: %p po: %lld\n", label, pgm, pwo.parse_options);

      assert(code && code[0]);

      ProgramThreadCountContextHelper pch(xsink, pgm, false);
      if (*xsink)
         return;

      // grab program-level parse lock
      AutoLocker al(plock);
      if (checkParseCommitUnlocked(xsink))
	 return;

      startParsing(xsink, wS, wm);

      // parse text given
      if (!internParsePending(code, label, orig_src, offset))
	 internParseCommit();   // finalize parsing, back out or commit all changes

#ifdef DEBUG
      parseSink = 0;
#endif
      warnSink = 0;
   }

   DLLLOCAL void parseFile(const char* filename, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
      QORE_TRACE("QoreProgram::parseFile()");

      printd(5, "QoreProgram::parseFile(%s)\n", filename);

      FILE *fp;
      if (!(fp = fopen(filename, "r"))) {
	 if ((only_first_except && !exceptions_raised) || !only_first_except)
	    xsink->raiseErrnoException("PARSE-EXCEPTION", errno, "cannot open qore script '%s'", filename);
	 exceptions_raised++;
	 return;
      }
      ON_BLOCK_EXIT(fclose, fp);

      setScriptPath(filename);
      
      ProgramThreadCountContextHelper pch(xsink, pgm, false);
      if (*xsink)
         return;

      parse(fp, filename, xsink, wS, wm);
   }
   
   DLLLOCAL void parsePending(const QoreString *str, const QoreString *lstr, ExceptionSink* xsink, ExceptionSink* wS, int wm, const QoreString* source = 0, int offset = 0) {
      assert(!str->empty());

      // ensure code string has correct character set encoding
      TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
      if (*xsink)
	 return;

      // ensure label string has correct character set encoding
      TempEncodingHelper tlstr(lstr, QCS_DEFAULT, xsink);
      if (*xsink)
	 return;

      TempEncodingHelper src;
      if (source && !source->empty() && !src.set(source, QCS_DEFAULT, xsink))
         return;

      parsePending(tstr->getBuffer(), tlstr->getBuffer(), xsink, wS, wm, source ? src->getBuffer() : 0, offset);
   }

   // called during run time (not during parsing)
   DLLLOCAL void importFunction(ExceptionSink* xsink, QoreFunction *u, const qore_ns_private& oldns, const char* new_name = 0);

   DLLLOCAL void del(ExceptionSink* xsink);

   DLLLOCAL QoreHashNode* clearThreadData(ExceptionSink* xsink) {
      QoreHashNode* h = thread_local_storage->get();
      printd(5, "QoreProgram::clearThreadData() this=%p h=%p (size=%d)\n", this, h, h ? h->size() : 0);
      if (h) 
         h->clear(xsink);
      return h;
   }

   DLLLOCAL void deleteThreadData(ExceptionSink* xsink) {
      QoreHashNode* h = clearThreadData(xsink);
      if (h) {
         h->deref(xsink);
         thread_local_storage->set(0);
      }
   }

   DLLLOCAL void endThread(ThreadProgramData *td, ExceptionSink* xsink) {
      //printd(5, "qore_program_private::endThread() this=%p pgm=%p\n", this, pgm);
      // delete thread local storage data
      deleteThreadData(xsink);

      // delete all local variables for this thread
      SafeLocker sl(tlock);
      while (tclear) {
         ++twaiting;
         tcond.wait(tlock);
         --twaiting;
      }

      pgm_data_map_t::iterator i = pgm_data_map.find(td);
      if (i != pgm_data_map.end()) {
         // make sure the Program doesn't disappear while we are clearing thread-local variables
         pgm->ref();
         ReferenceHolder<QoreProgram> pgm_holder(pgm, xsink);
         // remove from map and delete outside of lock to avoid deadlocks (or the need for recursive locking)
         ThreadLocalProgramData *tlpd = i->second;
         sl.unlock();
         tlpd->finalize(xsink);
         sl.lock();
         pgm_data_map.erase(i);
         sl.unlock();
         tlpd->del(xsink);
      }
   }

   DLLLOCAL void doTopLevelInstantiation(ThreadLocalProgramData &tlpd) {
      // instantiate top-level vars for this thread
      const LVList *lvl = sb.getLVList();
      if (lvl)
         for (unsigned i = 0; i < lvl->size(); ++i)
            lvl->lv[i]->instantiate();

      //printd(5, "qore_program_private::doTopLevelInstantiation() lvl=%p setup %ld local vars pgm=%p\n", lvl, lvl ? lvl->size() : 0, getProgram());

      tlpd.inst = true;
   }

   // returns true if setting for the first time, false if not
   DLLLOCAL bool setThreadVarData(ThreadProgramData* td, ThreadLocalProgramData*& new_tlpd, bool run) {
      SafeLocker sl(tlock);
      // wait for data to finished being cleared if applicable
      while (tclear) {
         if (tclear == gettid()) {
            // can be called recursively when destructors are run in local variable finalization
            assert(pgm_data_map.find(td) != pgm_data_map.end());
            assert(pgm_data_map[td]->inst);
            break;
         }
         ++twaiting;
         tcond.wait(tlock);
         --twaiting;
      }

      pgm_data_map_t::iterator i = pgm_data_map.find(td);
      if (i == pgm_data_map.end()) {
         ThreadLocalProgramData *tlpd = new ThreadLocalProgramData;

         //printd(5, "qore_program_private::setThreadVarData() (first) this=%p pgm=%p td=%p run=%s inst=%s\n", this, pgm, td, run ? "true" : "false", tlpd->inst ? "true" : "false");

         new_tlpd = tlpd;

         pgm_data_map.insert(pgm_data_map_t::value_type(td, tlpd));

         sl.unlock();

         if (run) {
            //printd(5, "qore_program_private::setThreadVarData() (first) this=%p pgm=%p td=%p\n", this, pgm, td);
            doTopLevelInstantiation(*tlpd);
         }

         return true;
      }
      
      ThreadLocalProgramData *tlpd = pgm_data_map[td];
      new_tlpd = tlpd;
      
      sl.unlock();

      //printd(5, "qore_program_private::setThreadVarData() (not first) this=%p pgm=%p td=%p run=%s inst=%s\n", this, pgm, td, run ? "true" : "false", tlpd->inst ? "true" : "false");

      if (run && !tlpd->inst) {
         doTopLevelInstantiation(*tlpd);
      }

      return false;
   }

   DLLLOCAL const AbstractQoreZoneInfo* currentTZ(ThreadProgramData* tpd = get_thread_program_data()) const {
      AutoLocker al(tlock);
      pgm_data_map_t::const_iterator i = pgm_data_map.find(tpd);
      if (i != pgm_data_map.end() && i->second->tz_set)
         return i->second->tz;
      return TZ;
   }

   DLLLOCAL void setTZ(const AbstractQoreZoneInfo* n_TZ) {
      TZ = n_TZ;
   }

   DLLLOCAL void exportFunction(ExceptionSink* xsink, qore_program_private* p, const char* name, const char* new_name = 0) {
      if (this == p) {
	 xsink->raiseException("PROGRAM-IMPORTFUNCTION-PARAMETER-ERROR", "cannot import a function from the same Program object");
	 return;
      }

      const QoreFunction* u;
      const qore_ns_private* ns = 0;

      {
	 AutoLocker al(plock);
	 u = qore_root_ns_private::runtimeFindFunction(*RootNS, name, ns);
      }

      if (!u)
	 xsink->raiseException("PROGRAM-IMPORTFUNCTION-NO-FUNCTION", "function '%s' does not exist in the current program scope", name);
      else {
         assert(ns);
	 p->importFunction(xsink, const_cast<QoreFunction*>(u), *ns, new_name);
      }
   }

   DLLLOCAL bool parseExceptionRaised() const {
      assert(parseSink);
      return *parseSink;
   }

   DLLLOCAL void setParseOptions(int64 po, ExceptionSink* xsink = 0) {
      // only raise the exception if parse options are locked and the option is not a "free option"
      // also check if options may be made more restrictive and the option also does so
      if (!((po & PO_FREE_OPTIONS) == po) && po_locked && (!po_allow_restrict || (po & PO_POSITIVE_OPTIONS))) {
         if (xsink)
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
         else
            parse_error("parse options have been locked on this program object");
         return;
      }

      pwo.parse_options |= po;
   }

   DLLLOCAL void disableParseOptions(int64 po, ExceptionSink* xsink = 0) {
      // only raise the exception if parse options are locked and the option is not a "free option"
      // also check if options may be made more restrictive and the option also does so
      if (!((po & PO_FREE_OPTIONS) == po) && po_locked && (!po_allow_restrict || (po & PO_POSITIVE_OPTIONS))) {
         if (xsink)
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
         else
            parse_error("parse options have been locked on this program object");
         return;
      }

      pwo.parse_options &= ~po;
   }

   DLLLOCAL void replaceParseOptions(int64 po, ExceptionSink* xsink) {
      if (!(getProgram()->priv->pwo.parse_options & PO_NO_CHILD_PO_RESTRICTIONS)) {
         xsink->raiseException("OPTION-ERROR", "the calling Program does not have the PO_NO_CHILD_PO_RESTRICTIONS option set, and therefore cannot call Program::replaceParseOptions()");
         return;
      }

      //printd(5, "qore_program_private::replaceParseOptions() this: %p pgm: %p replacing po: %lld with po: %lld\n", this, pgm, pwo.parse_options, po);
      pwo.parse_options = po;
   }

   DLLLOCAL void mergeParseException(ExceptionSink &xsink) {
      if (parseSink)
         parseSink->assimilate(xsink);
      else {
         // grab program-level parse lock if we are not already parsing; just in case
         AutoLocker al(&plock);
         
         if (!pendingParseSink)
            pendingParseSink = new ExceptionSink;
         pendingParseSink->assimilate(xsink);
      }
   }

   DLLLOCAL void parseSetTimeZone(const char* zone) {
      // check PO_NO_LOCALE_CONTROL
      ExceptionSink xsink;
      if (pwo.parse_options & PO_NO_LOCALE_CONTROL) {
         mergeParseException(xsink);
         return;
      }

      const AbstractQoreZoneInfo *tz = (*zone == '-' || *zone == '+') ? QTZM.findCreateOffsetZone(zone, &xsink) : QTZM.findLoadRegion(zone, &xsink);
      if (xsink) {
         assert(!tz);
         mergeParseException(xsink);
         return;
      }
      // note that tz may be NULL in case the offset is UTC (ie 0)
      assert(tz || ((*zone == '-' || *zone == '+')));
      
      TZ = tz;
   }

   DLLLOCAL void makeParseException(const char* err, QoreStringNode* desc) {
      QoreStringNodeHolder d(desc);
      if (!requires_exception) {
         assert(parseSink);
         QoreException *ne = new ParseException(err, d.release());
         parseSink->raiseException(ne);
      }
   }

   DLLLOCAL void makeParseException(QoreStringNode* desc) {
      QoreStringNodeHolder d(desc);
      if (!requires_exception) {
         QoreException *ne = new ParseException("PARSE-EXCEPTION", d.release());
         if ((only_first_except && !exceptions_raised) || !only_first_except)
            parseSink->raiseException(ne);
         exceptions_raised++;
      }
   }

   DLLLOCAL void makeParseException(const QoreProgramLocation &loc, const char* err, QoreStringNode* desc) {
      QORE_TRACE("QoreProgram::makeParseException()");

      QoreStringNodeHolder d(desc);
      if (!requires_exception) {
         QoreException *ne = new ParseException(loc, err, d.release());
         if ((only_first_except && !exceptions_raised) || !only_first_except)
            parseSink->raiseException(ne);
         exceptions_raised++;
      }
   }

   DLLLOCAL void parseException(QoreProgramLocation &loc, const char* fmt, ...) {
      //printd(5, "qore_program_private::parseException(\"%s\", ...) called\n", fmt);

      // ignore if a "requires" exception has been raised
      if (requires_exception)
         return;

      QoreStringNode* desc = new QoreStringNode;
      while (true) {
         va_list args;
         va_start(args, fmt);
         int rc = desc->vsprintf(fmt, args);
         va_end(args);
         if (!rc)
            break;
      }
      makeParseException(loc, "PARSE-EXCEPTION", desc);
   }

   // internal method - does not bother with the parse lock
   // returns true if the define already existed
   DLLLOCAL bool setDefine(const char* name, AbstractQoreNode* v, ExceptionSink* xsink) {
      dmap_t::iterator i = dmap.find(name);
      if (i != dmap.end()) {
         if (i->second)
            i->second->deref(xsink);
         i->second = v;
         return true;
      }

      dmap[name] = v;
      return false;
   }

   // internal method - does not bother with the parse lock
   DLLLOCAL const AbstractQoreNode* getDefine(const char* name, bool &is_defined) {
      dmap_t::iterator i = dmap.find(name);
      if (i != dmap.end()) {
         is_defined = true;
         return i->second;
      }
      is_defined = false;
      return 0;
   }

   DLLLOCAL AbstractQoreNode* runTimeGetDefine(const char* name, bool &is_defined) {
      AutoLocker al(plock);
      const AbstractQoreNode* rv = getDefine(name, is_defined);
      return rv ? rv->refSelf() : 0;
   }

   // internal method - does not bother with the parse lock
   // returns true if the define existed
   DLLLOCAL bool unDefine(const char* name, ExceptionSink* xsink) {
      dmap_t::iterator i = dmap.find(name);
      if (i != dmap.end()) {
         if (i->second)
            i->second->deref(xsink);
         dmap.erase(i);
         return true;
      }
      return false;
   }

   DLLLOCAL bool parseUnDefine(const char* name) {
      PreParseHelper pph(this);
      return unDefine(name, parseSink);
   }

   DLLLOCAL bool runTimeUnDefine(const char* name, ExceptionSink* xsink) {
      AutoLocker al(plock);
      return unDefine(name, xsink);
   }

   // internal method - does not bother with the parse lock
   DLLLOCAL bool isDefined(const char* name) {
      dmap_t::iterator i = dmap.find(name);
      return i == dmap.end() ? false : true;
   }

   DLLLOCAL bool runTimeIsDefined(const char* name) {
      AutoLocker al(plock);
      return isDefined(name);
   }

   DLLLOCAL int checkDefine(const QoreProgramLocation &loc, const char* str, ExceptionSink* xsink) {
      const char* p = str;
      if (!isalpha(*p)) {
         xsink->raiseException(loc, "PARSE-EXCEPTION", 0, "illegal define variable '%s'; does not begin with an alphabetic character", p);
         return -1;
      }

      while (*(++p)) {
         if (!isalnum(*p) && *p != '_') {
            
            xsink->raiseException(loc, "PARSE-EXCEPTION", 0, "illegal character '%c' in define variable '%s'", *p, str);
            return -1;
         }
      }

      return 0;
   }

   DLLLOCAL void parseDefine(const QoreProgramLocation &loc, const char* str, AbstractQoreNode* val) {
      PreParseHelper pph(this);

      if (checkDefine(loc, str, parseSink))
         return;

      setDefine(str, val, parseSink);
   }

   DLLLOCAL void runTimeDefine(const char* str, AbstractQoreNode* val, ExceptionSink* xsink) {
      QoreProgramLocation loc(RunTimeLocation);

      if (checkDefine(loc, str, xsink))
         return;

      AutoLocker al(plock);
      setDefine(str, val, xsink);
   }

   DLLLOCAL ResolvedCallReferenceNode* runtimeGetCallReference(const char* name, ExceptionSink* xsink) {
      AutoLocker al(&plock);
      return qore_root_ns_private::runtimeGetCallReference(*RootNS, name, xsink);
   }

   DLLLOCAL void pushParseOptions(const char* pf) {
      assert(ppo.find(pf) == ppo.end());
      ppo[pf] = pwo.parse_options;
      //printd(5, "qore_program_private::pushParseOptions() this: %p %p '%s' saving %lld\n", this, pf, pf, pwo.parse_options);
   }

   DLLLOCAL void restoreParseOptions(const char* pf) {
      ppo_t::iterator i = ppo.find(pf);
      if (i != ppo.end()) {
         //printd(5, "qore_program_private::restoreParseOptions() this: %p %p '%s' restoring %lld\n", this, pf, pf, pwo.parse_options);
         pwo.parse_options = i->second;
         ppo.erase(i);
      }
#if 0
      else {
         printd(5, "qore_program_private::restoreParseOptions() this: %p %p '%s' parse options not found\n", this, pf, pf, pwo.parse_options);
         for (ppo_t::iterator i = ppo.begin(), e = ppo.end(); i != e; ++i) {
            printd(5, " + ppo: %p '%s' = %lld\n", i->first, i->first, i->second);
         }
      }
#endif
   }

   DLLLOCAL void addParseException(ExceptionSink& xsink, QoreProgramLocation* loc = 0) {
      if (requires_exception) {
         xsink.clear();
         return;
      }

      // ensure that all exceptions reflect the current parse location
      if (!loc) {
         QoreProgramLocation pl(ParseLocation);
         xsink.overrideLocation(pl);
      }
      else
         xsink.overrideLocation(*loc);

      parseSink->assimilate(xsink);
   }

   DLLLOCAL int parseAddDomain(int64 n_dom) {
      if (n_dom & pwo.parse_options)
         return -1;
      pend_dom |= n_dom;
      return 0;
   }

   DLLLOCAL void exportGlobalVariable(const char* name, bool readonly, qore_program_private& tpgm, ExceptionSink* xsink);

   // returns true if there was already a thread init closure set, false if not
   DLLLOCAL bool setThreadInit(const ResolvedCallReferenceNode* n_thr_init, ExceptionSink* xsink) {
      ReferenceHolder<> old(xsink);
      {
         AutoLocker al(tlock);
         old = thr_init;
         thr_init = n_thr_init ? n_thr_init->refRefSelf() : 0;
      }
      return (bool)old;
   }

   DLLLOCAL void setThreadTZ(ThreadProgramData* tpd, const AbstractQoreZoneInfo* tz) {
      AutoLocker al(tlock);
      pgm_data_map_t::iterator i = pgm_data_map.find(tpd);
      assert(i != pgm_data_map.end());
      i->second->setTZ(tz);
   }

   DLLLOCAL const AbstractQoreZoneInfo* getThreadTZ(ThreadProgramData* tpd, bool& set) const {
      AutoLocker al(tlock);
      pgm_data_map_t::const_iterator i = pgm_data_map.find(tpd);
      assert(i != pgm_data_map.end());
      set = i->second->tz_set;
      return i->second->tz;
   }

   DLLLOCAL void clearThreadTZ(ThreadProgramData* tpd) {
      AutoLocker al(tlock);
      pgm_data_map_t::iterator i = pgm_data_map.find(tpd);
      assert(i != pgm_data_map.end());
      i->second->clearTZ();
   }

   DLLLOCAL void addStatement(AbstractStatement* s) {
      sb.addStatement(s);

      // see if top level statements are allowed
      if (pwo.parse_options & PO_NO_TOP_LEVEL_STATEMENTS && !s->isDeclaration())
         parse_error("illegal top-level statement (conflicts with parse option NO_TOP_LEVEL_STATEMENTS)");
   }

   DLLLOCAL void importClass(qore_program_private& from_pgm, const char* path, ExceptionSink* xsink);

   DLLLOCAL void addFile(char* f) {
      fileList.push_back(f);
   }

   DLLLOCAL void addUserFeature(const char* f) {
      userFeatureList.push_back(f);
   }

   DLLLOCAL static void addUserFeature(QoreProgram& pgm, const char* f) {
      pgm.priv->addUserFeature(f);
   }

   DLLLOCAL static void addFile(QoreProgram& pgm, char* f) {
      pgm.priv->addFile(f);
   }

   DLLLOCAL static void importClass(QoreProgram& pgm, QoreProgram& from_pgm, const char* path, ExceptionSink* xsink) {
      pgm.priv->importClass(*(from_pgm.priv), path, xsink);
   }

   DLLLOCAL static void addStatement(QoreProgram& pgm, AbstractStatement* s) {
      pgm.priv->addStatement(s);
   }

   DLLLOCAL static const AbstractQoreZoneInfo* currentTZIntern(QoreProgram& pgm) {
      return pgm.priv->TZ;
   }

   DLLLOCAL static int incThreadCount(QoreProgram& pgm, ExceptionSink* xsink) {
      return pgm.priv->incThreadCount(xsink);
   }

   DLLLOCAL static void decThreadCount(QoreProgram& pgm) {
      pgm.priv->decThreadCount();
   }

   // locking is done by the signal manager
   DLLLOCAL static void addSignal(QoreProgram& pgm, int sig) {
      assert(pgm.priv->sigset.find(sig) == pgm.priv->sigset.end());
      pgm.priv->sigset.insert(sig);
   }

   // locking is done by the signal manager
   DLLLOCAL static void delSignal(QoreProgram& pgm, int sig) {
      assert(pgm.priv->sigset.find(sig) != pgm.priv->sigset.end());
      pgm.priv->sigset.erase(sig);
   }

   DLLLOCAL static void startThread(QoreProgram& pgm, ExceptionSink& xsink) {
      pgm.priv->qore_program_private_base::startThread(xsink);
   }

   DLLLOCAL static bool setThreadInit(QoreProgram& pgm, const ResolvedCallReferenceNode* n_thr_init, ExceptionSink* xsink) {
      return pgm.priv->setThreadInit(n_thr_init, xsink);
   }

   DLLLOCAL static ResolvedCallReferenceNode* runtimeGetCallReference(QoreProgram* pgm, const char* name, ExceptionSink* xsink) {
      return pgm->priv->runtimeGetCallReference(name, xsink);
   }

   DLLLOCAL static const ParseWarnOptions &getParseWarnOptions(const QoreProgram *pgm) {
      return pgm->priv->pwo;
   }

   DLLLOCAL static bool setSaveParseWarnOptions(const QoreProgram *pgm, const ParseWarnOptions &new_opts, ParseWarnOptions &old_opts) {
      if (new_opts == pgm->priv->pwo)
         return false;
      old_opts = pgm->priv->pwo;
      pgm->priv->pwo = new_opts;
      return true;
   }

   DLLLOCAL static void setParseWarnOptions(const QoreProgram *pgm, const ParseWarnOptions &new_opts) {
      pgm->priv->pwo = new_opts;
   }

   DLLLOCAL static bool setThreadVarData(QoreProgram *pgm, ThreadProgramData *td, ThreadLocalProgramData *&tlpd, bool run) {
      return pgm->priv->setThreadVarData(td, tlpd, run);
   }

   DLLLOCAL static void endThread(QoreProgram *pgm, ThreadProgramData *td, ExceptionSink* xsink) {
      pgm->priv->endThread(td, xsink);
   }

   DLLLOCAL static void makeParseException(QoreProgram *pgm, const char* err, QoreStringNode* desc) {
      pgm->priv->makeParseException(err, desc);
   }

   DLLLOCAL static void makeParseException(QoreProgram *pgm, QoreStringNode* desc) {
      pgm->priv->makeParseException(desc);
   }

   DLLLOCAL static void makeParseException(QoreProgram *pgm, const QoreProgramLocation &loc, QoreStringNode* desc) {
      pgm->priv->makeParseException(loc, "PARSE-EXCEPTION", desc);
   }

   DLLLOCAL static void makeParseException(QoreProgram *pgm, const QoreProgramLocation &loc, const char* err, QoreStringNode* desc) {
      pgm->priv->makeParseException(loc, err, desc);
   }

   DLLLOCAL static const AbstractQoreNode* parseGetDefine(QoreProgram *pgm, const char* name) {
      bool is_defined;
      return pgm->priv->getDefine(name, is_defined);
   }

   DLLLOCAL static const AbstractQoreNode* parseGetDefine(QoreProgram *pgm, const char* name, bool &is_defined) {
      return pgm->priv->getDefine(name, is_defined);
   }

   DLLLOCAL static AbstractQoreNode* runTimeGetDefine(QoreProgram *pgm, const char* name) {
      bool is_defined;
      return pgm->priv->runTimeGetDefine(name, is_defined);
   }

   DLLLOCAL static AbstractQoreNode* runTimeGetDefine(QoreProgram *pgm, const char* name, bool &is_defined) {
      return pgm->priv->runTimeGetDefine(name, is_defined);
   }

   DLLLOCAL static bool parseUnDefine(QoreProgram *pgm, const char* name) {
      return pgm->priv->parseUnDefine(name);
   }

   DLLLOCAL static bool runTimeUnDefine(QoreProgram *pgm, const char* name, ExceptionSink* xsink) {
      return pgm->priv->runTimeUnDefine(name, xsink);
   }

   DLLLOCAL static bool parseIsDefined(QoreProgram *pgm, const char* name) {
      return pgm->priv->isDefined(name);
   }

   DLLLOCAL static bool runTimeIsDefined(QoreProgram *pgm, const char* name) {
      return pgm->priv->runTimeIsDefined(name);
   }

   DLLLOCAL static void parseDefine(QoreProgram *pgm, QoreProgramLocation loc, const char* str, AbstractQoreNode* val) {
      pgm->priv->parseDefine(loc, str, val);
   }

   DLLLOCAL static void runTimeDefine(QoreProgram *pgm, const char* str, AbstractQoreNode* val, ExceptionSink* xsink) {
      pgm->priv->runTimeDefine(str, val, xsink);
   }

   DLLLOCAL static void pushParseOptions(QoreProgram* pgm, const char* pf) {
      pgm->priv->pushParseOptions(pf);
   }

   DLLLOCAL static void restoreParseOptions(QoreProgram* pgm, const char* pf) {
      pgm->priv->restoreParseOptions(pf);
   }
   
   DLLLOCAL static void addParseException(QoreProgram* pgm, ExceptionSink* xsink, QoreProgramLocation* loc = 0) {
      pgm->priv->addParseException(*xsink, loc);
      delete xsink;
   }

   DLLLOCAL static void addParseException(QoreProgram* pgm, ExceptionSink& xsink, QoreProgramLocation* loc = 0) {
      pgm->priv->addParseException(xsink, loc);
   }

   DLLLOCAL static void exportFunction(QoreProgram* srcpgm, ExceptionSink* xsink, QoreProgram *trgpgm, const char* name, const char* new_name = 0) {
      srcpgm->priv->exportFunction(xsink, trgpgm->priv, name, new_name);
   }

   DLLLOCAL static int parseAddDomain(QoreProgram* pgm, int64 n_dom) {
      return pgm->priv->parseAddDomain(n_dom);
   }

   DLLLOCAL static int64 getDomain(const QoreProgram& pgm) {
      return pgm.priv->dom;
   }

   DLLLOCAL static void runtimeAddDomain(QoreProgram& pgm, int64 n_dom) {
      pgm.priv->dom |= n_dom;
   }

   /*
   DLLLOCAL static int64 forceReplaceParseOptions(QoreProgram& pgm, int64 po) {
      int64 rv = pgm.priv->pwo.parse_options;
      pgm.priv->pwo.parse_options = po;
      return rv;
   }
   */

   DLLLOCAL static void makeParseWarning(QoreProgram* pgm, int code, const char* warn, const char* fmt, ...) {
      //printd(5, "QP::mPW(code=%d, warn='%s', fmt='%s') priv->pwo.warn_mask=%d priv->warnSink=%p %s\n", code, warn, fmt, priv->pwo.warn_mask, priv->warnSink, priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
      if (!pgm->priv->warnSink || !(code & pgm->priv->pwo.warn_mask))
         return;

      QoreStringNode* desc = new QoreStringNode;
      while (true) {
         va_list args;
         va_start(args, fmt);
         int rc = desc->vsprintf(fmt, args);
         va_end(args);
         if (!rc)
            break;
      }
      QoreException *ne = new ParseException(warn, desc);
      pgm->priv->warnSink->raiseException(ne);
   }

   DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation &loc, int code, const char* warn, const char* fmt, ...) {
      //printd(5, "QP::mPW(code=%d, warn='%s', fmt='%s') priv->pwo.warn_mask=%d priv->warnSink=%p %s\n", code, warn, fmt, priv->pwo.warn_mask, priv->warnSink, priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
      if (!pgm->priv->warnSink || !(code & pgm->priv->pwo.warn_mask))
         return;
      
      QoreStringNode* desc = new QoreStringNode;
      while (true) {
         va_list args;
         va_start(args, fmt);
         int rc = desc->vsprintf(fmt, args);
         va_end(args);
         if (!rc)
            break;
      }
      QoreException *ne = new ParseException(loc, warn, desc);
      pgm->priv->warnSink->raiseException(ne);
   }

   DLLLOCAL static void makeParseWarning(QoreProgram* pgm, int code, const char* warn, QoreStringNode* desc) {
      //printd(5, "QoreProgram::makeParseWarning(code=%d, warn='%s', desc='%s') priv->pwo.warn_mask=%d priv->warnSink=%p %s\n", code, warn, desc->getBuffer(), priv->pwo.warn_mask, priv->warnSink, priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
      if (!pgm->priv->warnSink || !(code & pgm->priv->pwo.warn_mask)) {
         desc->deref();
         return;
      }
      
      QoreException *ne = new ParseException(warn, desc);
      pgm->priv->warnSink->raiseException(ne);
   }

   DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation &loc, int code, const char* warn, QoreStringNode* desc) {
      //printd(5, "QoreProgram::makeParseWarning(code=%d, warn='%s', desc='%s') priv->pwo.warn_mask=%d priv->warnSink=%p %s\n", code, warn, desc->getBuffer(), priv->pwo.warn_mask, priv->warnSink, priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
      if (!pgm->priv->warnSink || !(code & pgm->priv->pwo.warn_mask)) {
         desc->deref();
         return;
      }
      
      QoreException *ne = new ParseException(loc, warn, desc);
      pgm->priv->warnSink->raiseException(ne);
   }

   DLLLOCAL static void exportGlobalVariable(QoreProgram* pgm, const char* name, bool readonly, QoreProgram* tpgm, ExceptionSink* xsink) {
      pgm->priv->exportGlobalVariable(name, readonly, *(tpgm->priv), xsink);
   }
};

class ParseWarnHelper : public ParseWarnOptions {
protected:
   bool restore;
public:
   DLLLOCAL ParseWarnHelper(const ParseWarnOptions &new_opts) {
      QoreProgram *pgm = getProgram();
      restore = pgm ? qore_program_private::setSaveParseWarnOptions(pgm, new_opts, *this) : false;
   }
   DLLLOCAL ~ParseWarnHelper() {
      if (restore)
	 qore_program_private::setParseWarnOptions(getProgram(), *this);
   }
};

#endif
