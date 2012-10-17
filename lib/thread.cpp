/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  thread.cpp

  threading functionality for Qore

  Qore Programming Language

  Copyright 2003 - 2012 David Nichols

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
#include <qore/intern/ThreadResourceList.h>

#include <qore/intern/ConstantList.h>
#include <qore/intern/QoreSignal.h>
#include <qore/intern/qore_program_private.h>

// to register object types
#include <qore/intern/QC_Queue.h>
#include <qore/intern/QC_Mutex.h>
#include <qore/intern/QC_Condition.h>
#include <qore/intern/QC_RWLock.h>
#include <qore/intern/QC_Gate.h>
#include <qore/intern/QC_Sequence.h>
#include <qore/intern/QC_Counter.h>
#include <qore/intern/QC_AutoLock.h>
#include <qore/intern/QC_AutoGate.h>
#include <qore/intern/QC_AutoReadLock.h>
#include <qore/intern/QC_AutoWriteLock.h>
#include <qore/intern/QC_AbstractSmartLock.h>

#include <pthread.h>
#include <sys/time.h>
#include <assert.h>

#include <vector>
#include <map>
#include <set>
#include <string>

#if defined(DARWIN) && MAX_QORE_THREADS > 2560 && !defined(__MAC_10_7)
// testing has revealed that darwin's pthread_create will not return an error when more than 2560 threads
// are running, however the threads are not actually started, therefore we set MAX_QORE_THREADS to 2560 on 
// Darwin.  This should be much more than any program/script should need (famous last words? :-) )
// this bug is not present on 10.7.3 at least - in 10.7.3 pthread_create() returns an error after 2047
// threads have been created and therefore works reliably
#warning Darwin cannot support more than 2560 threads, MAX_QORE_THREADS set to 2560
#undef MAX_QORE_THREADS
#define MAX_QORE_THREADS 2560
#endif

#if defined(__ia64) && defined(__LP64__)
#define IA64_64
#endif

Operator *OP_BACKGROUND;

ThreadCleanupList tclist;

// TID 0 is reserved for the signal handler thread
static int current_tid = 1;

DLLLOCAL bool threads_initialized = false;

DLLLOCAL QoreThreadLock lThreadList;

// recursive mutex attribute
DLLLOCAL pthread_mutexattr_t ma_recursive;

#ifndef HAVE_GETHOSTBYNAME_R
DLLLOCAL QoreThreadLock lck_gethostbyname;
#endif

#ifndef HAVE_GETHOSTBYADDR_R
DLLLOCAL QoreThreadLock lck_gethostbyaddr;
#endif

#ifdef QORE_MANAGE_STACK
// default size and limit for qore threads; to be set in init_qore_threads()
size_t qore_thread_stack_size = 0;
size_t qore_thread_stack_limit = 0;
#endif

// total number of threads running
DLLLOCAL int num_threads = 0;

// default thread creation attribute
QorePThreadAttr ta_default;

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
pthread_t PTHREAD_NA, PTHREAD_AVAIL;
#else
#define PTHREAD_NA ((pthread_t)(-1L))
#define PTHREAD_AVAIL ((pthread_t)(0L))
#endif

class tid_node {
public:
   int tid;
   tid_node* next, *prev;
   
   DLLLOCAL tid_node(int ntid);
   DLLLOCAL ~tid_node();
};

static tid_node* tid_head = 0, *tid_tail = 0;
 
// this structure holds all thread data that can be addressed with the qore tid
class ThreadEntry {
public:
   pthread_t ptid;
   tid_node* tidnode;
#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
   CallStack *callStack;
#endif
   bool joined; // if set to true then pthread_detach should not be called on exit
   
   DLLLOCAL void cleanup() {
      // delete tidnode from tid_list
      delete tidnode;

#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
      // delete call stack
      delete callStack;
#endif

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__  
      if (ptid.p != PTHREAD_NA.p)
#else
      if (ptid != PTHREAD_NA)
#endif
      {
	 if (!joined)
	    pthread_detach(ptid);
	    
	 // set ptid to 0
	 ptid = PTHREAD_AVAIL;
      }
   }
   
   DLLLOCAL void allocate(tid_node* tn) {
      ptid = PTHREAD_NA;
      tidnode = tn;
#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
      callStack = 0;
#endif
      joined = false;
   }
};

DLLLOCAL ThreadEntry thread_list[MAX_QORE_THREADS];

const qore_class_private* ClassObj::getClass() const {
   if (!ptr)
      return 0;
   if (!(ptr & 1))
      return qore_class_private::get(*((QoreObject*)ptr)->getClass());

   return (const qore_class_private*)(ptr & ~1);
}

class ProgramLocation {
public:
   const char *file;
   void *parseState;
   ProgramLocation *next;
      
   DLLLOCAL ProgramLocation(const char *fname, void *ps = 0) { 
      file       = fname; 
      parseState = ps;
   }
};

class ArgvRefStack {
protected:
   typedef std::vector<int> rvec_t;
   rvec_t stack;
   // ignore numeric count
   int in;

public:
   DLLLOCAL ArgvRefStack() : in(0) {
      stack.push_back(0);
   }
   DLLLOCAL ~ArgvRefStack() {
   }
   DLLLOCAL void push() {
      stack.push_back(0);
   }
   DLLLOCAL int pop() {
      int rc = stack[stack.size() - 1];
      if (stack.size() > 1) 
	 stack.pop_back();
      else
	 stack[0] = 0;
      return rc;
   }
   DLLLOCAL int get() {
      assert(stack.size() == 1);
      int rc = stack[0];
      stack[0] = 0;
      return rc;
   }
   DLLLOCAL void push_numeric() {
      ++in;
   }
   DLLLOCAL void pop_numeric() {
      --in;
      assert(in >= 0);
   }
   DLLLOCAL void inc_numeric() {
      if (in)
	 return;
      inc();
   }
   DLLLOCAL void inc() {
      ++stack[stack.size() - 1];
   }
   DLLLOCAL void clear() {
      stack.clear();
      stack.push_back(0);
   }
};

struct ParseCountHelper {
   unsigned count;

   DLLLOCAL ParseCountHelper() : count (0) {
   }

   DLLLOCAL void inc() {
      ++count;
   }

   DLLLOCAL bool dec() {
      if (!count) {
         parse_error("unmatched %%endif");
         return false;
      }
      return !--count;
   }

   DLLLOCAL void purge() {
      if (count) {
         parse_error("%d %%catch-import-error block%s left open at end of file", count, count == 1 ? "" : "s");
         count = 0;
      }
   }
};

struct ParseConditionalStack {
   unsigned count;
   unsigned mark;

   DLLLOCAL ParseConditionalStack() : count(0), mark(0) {
   }

   DLLLOCAL void push(bool do_mark = false) {
      if (do_mark) {
         assert(!mark);
         mark = count;
      }
      ++count;
   }

   DLLLOCAL bool checkElse() {
      return count;
   }

   DLLLOCAL bool test() const {
      return mark == (count - 1);
   }

   DLLLOCAL bool pop() {
      if (!count) {
         parse_error("unmatched %%endif");
         return false;
      }
      --count;
      if (count == mark) {
         mark = 0;
         return true;
      }
      return false;
   }

   DLLLOCAL void purge() {
      if (count) {
         parse_error("%d conditional block%s left open at end of file", count, count == 1 ? "" : "s");
         count = 0;
         mark = 0;
      }
   }
};

// for detecting circular references at runtime
typedef std::set<const lvalue_ref*> ref_set_t;

// this structure holds all thread-specific data
class ThreadData {
public:
   int tid;
   VLock vlock;     // for deadlock detection
   Context *context_stack;
   ProgramLocation *plStack;
   int parse_line_start, parse_line_end;
   const char *parse_file;
   int pgm_counter_start, pgm_counter_end;
   const char *pgm_file;
   const char *parse_code; // the current function, method, or closure being parsed
   void *parseState;
   VNode* vstack;  // used during parsing (local variable stack)
   CVNode* cvarstack;
   QoreClass *parseClass; // current class being parsed
   QoreException *catchException;
   std::list<block_list_t::iterator> on_block_exit_list;
   ThreadResourceList* trlist;

   // for detecting circular references at runtime
   ref_set_t ref_set;

   // current function/method name
   const char *current_code;

   // current object context
   ClassObj current_classobj;

   // current program context
   QoreProgram *current_pgm;

   // current namespace context for parsing
   qore_ns_private* current_ns;

   // current implicit argument
   QoreListNode* current_implicit_arg;

   // this data structure is stored in the current Program object on a per-thread basis
   ThreadLocalProgramData* tlpd;

   // this data structure contains the set of Program objects that this thread has data in
   ThreadProgramData* tpd;

   // current parsing closure environment
   ClosureParseEnvironment *closure_parse_env;

   // current runtime closure environment
   ThreadSafeLocalVarRuntimeEnvironment *closure_rt_env;

   ArgvRefStack argv_refs;

#ifdef QORE_MANAGE_STACK
   size_t stack_limit;
#ifdef IA64_64
   size_t rse_limit;
#endif
#endif

   // used to detect output of recursive data structures at runtime
   const_node_set_t node_set;

   // currently-executing/parsing block's return type
   const QoreTypeInfo *returnTypeInfo;

   // current implicit element offset
   int element;

   // start of global thread-local variables for the current thread and program being parsed
   VNode* global_vnode;

   // Maintains the conditional parse block count for each file parsed
   ParseConditionalStack pcs;

   // Maintains the %catch-import-error block count for each file
   ParseCountHelper cie;

   // for capturing namespace and class names while parsing
   typedef std::vector<std::string> npvec_t;
   npvec_t npvec;

   // used for error handling when merging module code into a Program object
   QoreModuleContext* qmc;

   // used to capture the module defnition in user modules
   QoreModuleDefContext* qmd;

   DLLLOCAL ThreadData(int ptid, QoreProgram *p) : 
      tid(ptid), vlock(ptid), context_stack(0), plStack(0), 
      parse_line_start(0), parse_line_end(0), parse_file(0), 
      pgm_counter_start(0), pgm_counter_end(0), pgm_file(0), 
      parse_code(0), parseState(0), vstack(0), cvarstack(0),
      parseClass(0), catchException(0), trlist(new ThreadResourceList), current_code(0),
      current_pgm(p), current_ns(0), current_implicit_arg(0), tlpd(0), tpd(new ThreadProgramData(this)),
      closure_parse_env(0), closure_rt_env(0), 
      returnTypeInfo(0), element(0), global_vnode(0),
      qmc(0), qmd(0) {
 
#ifdef QORE_MANAGE_STACK

#ifdef STACK_DIRECTION_DOWN
      stack_limit = get_stack_pos() - qore_thread_stack_limit;
#else
      stack_limit = get_stack_pos() + qore_thread_stack_limit;
#endif // #ifdef STACK_DIRECTION_DOWN

#ifdef IA64_64
      // RSE stack grows up
      rse_limit = get_rse_bsp() + qore_thread_stack_limit;
#endif // #ifdef IA64_64

#endif // #ifdef QORE_MANAGE_STACK
   }

   DLLLOCAL ~ThreadData() {
      assert(on_block_exit_list.empty());
      assert(!tpd);
      assert(!trlist->prev);
      delete trlist;
   }

   DLLLOCAL int getElement() {
      return element;
   }

   DLLLOCAL int saveElement(int n_element) {
      int rc = element;
      element = n_element;
      return rc;
   }

   DLLLOCAL void del(ExceptionSink *xsink) {
      tpd->del(xsink);
      tpd->deref();
#ifdef DEBUG
      tpd = 0;
#endif
   }

   DLLLOCAL void pushName(const char* name) {
      npvec.push_back(name);
   }

   DLLLOCAL std::string popName() {
      assert(!npvec.empty());
      std::string rv = npvec.back();
      npvec.pop_back();
      return rv;
   }

   DLLLOCAL void parseRollback() {
      npvec.clear();
   }

   DLLLOCAL qore_ns_private* set_ns(qore_ns_private* ns) {
      if (ns == current_ns)
         return ns;

      qore_ns_private* rv = current_ns;
      current_ns = ns;
      return rv;
   }
};

void ThreadProgramData::delProgram(QoreProgram *pgm) {
   {
      AutoLocker al(pslock);
      pgm_set_t::iterator i = pgm_set.find(pgm);
      if (i == pgm_set.end())
         return;
      pgm_set.erase(i);
   }
   // this can never cause the program to go out of scope because it's always called
   // when the reference count > 1, therefore *xsink = 0 is OK
   pgm->depDeref(0);
   deref();
}

void ThreadProgramData::saveProgram(bool runtime) {
   if (!qore_program_private::setThreadVarData(td->current_pgm, this, td->tlpd, runtime))
      return;
   ref();
   td->current_pgm->depRef();
   AutoLocker al(pslock);
   assert(pgm_set.find(td->current_pgm) == pgm_set.end());
   pgm_set.insert(td->current_pgm);
}

void ThreadProgramData::del(ExceptionSink *xsink) {
   while (true) {
      QoreProgram *pgm;
      {
         AutoLocker al(pslock);
         pgm_set_t::iterator i = pgm_set.begin();
         if (i == pgm_set.end())
            break;
         
         pgm = (*i);
         pgm_set.erase(i);
      }
      pgm->depDeref(xsink);
      qore_program_private::endThread(pgm, this, xsink);
      deref();
   }
}

static QoreThreadLocalStorage<ThreadData> thread_data;

class ThreadCleanupNode {
public:
   qtdest_t func;
   void *arg;
   ThreadCleanupNode* next;
};

DLLLOCAL ThreadCleanupNode* ThreadCleanupList::head = 0;

class ThreadParams {
public:
   AbstractQoreNode* fc;
   int tid;
   QoreProgram *pgm;
   
   DLLLOCAL ThreadParams(AbstractQoreNode* f, int t) {
      fc = f; 
      tid = t;
      pgm = getProgram();
   } 
};

// this constructor must only be called with the lThreadList lock held
tid_node::tid_node(int ntid) {
   tid = ntid;
   next = 0;
   prev = tid_tail;
   if (!tid_head)
      tid_head = this;
   else
      tid_tail->next = this;
   tid_tail = this;
}

// this destructor must only be called with the lThreadList lock held
tid_node::~tid_node() {
   if (prev)
      prev->next = next;
   else
      tid_head = next;
   if (next)
      next->prev = prev;
   else
      tid_tail = prev;
}

class BGThreadParams {
private:
   //QoreObject *callobj;
   ClassObj callobj;

public:
   QoreObject *obj;
   AbstractQoreNode* fc;
   QoreProgram *pgm;
   int tid;
   QoreProgramLocation loc;
   bool deref_count;
   
   DLLLOCAL BGThreadParams(AbstractQoreNode* f, int t, ExceptionSink *xsink) : callobj((thread_data.get())->current_classobj), obj(0),
         fc(f), pgm(getProgram()), tid(t), loc(RunTimeLocation), deref_count(false) {
      // callobj: get and reference the current stack object, if any, for the new call stack

      qore_type_t fctype = fc->getType();
      //printd(5, "BGThreadParams::BGThreadParams(f=%p (%s %d), t=%d) this=%p callobj=%p\n", f, f->getTypeName(), f->getType(), t, this, callobj);
      if (fctype == NT_SELF_CALL) {
	 // must have a current object if an in-object method call is being executed
	 // (i.e. $.method())
	 // we reference the object so it won't go out of scope while the thread is running
	 obj = callobj.getObj();
         assert(obj);
	 obj->ref();
	 callobj.clear();
      }
      else if (fctype == NT_OPERATOR) {
         QoreDotEvalOperatorNode* deon = dynamic_cast<QoreDotEvalOperatorNode*>(fc);
         if (deon) {
	    // evaluate object
	    QoreNodeEvalOptionalRefHolder n(deon->getExpression(), xsink);
	    if (*xsink || is_nothing(*n))
	       return;

	    // if we have actually evaluated something, then we save the result in the tree
	    if (n.isTemp()) {
               deon->replaceExpression(n.getReferencedValue());
	    } else if (n->getType() == NT_OBJECT) {
	       // we reference the object so it won't go out of scope while the thread is running
	       obj = reinterpret_cast<QoreObject *>(n.getReferencedValue());
	       callobj.clear();
	    }
	 }
      }

      QoreObject* o = callobj.getObj();
      if (o)
	 o->tRef();

      // increment the program's thread counter
      if (!qore_program_private::incThreadCount(*pgm, xsink))
         deref_count = true;
   }

   DLLLOCAL ~BGThreadParams() {
      // decrement program's thread count
      if (deref_count)
         qore_program_private::decThreadCount(*pgm);
   }

   DLLLOCAL QoreObject *getCallObject() {
      return obj ? obj : callobj.getObj();
   }
   
   DLLLOCAL void cleanup(ExceptionSink *xsink) {
      if (fc) fc->deref(xsink);
      derefObj(xsink);
      derefCallObj();
   }
   
   DLLLOCAL void derefCallObj() {
      // dereference call object if present
      QoreObject* o = callobj.getObj();
      if (o)
	 o->tDeref();
      callobj.clear();
   }

   DLLLOCAL void derefObj(ExceptionSink *xsink) {
      if (obj) {
	 obj->deref(xsink);
	 obj = 0;
      }
   }
   
   DLLLOCAL AbstractQoreNode* exec(ExceptionSink *xsink) {
      //printd(5, "BGThreadParams::exec() this=%p fc=%p (%s %d)\n", this, fc, fc->getTypeName(), fc->getType());
      AbstractQoreNode* rv = fc->eval(xsink);
      fc->deref(xsink);
      fc = 0;
      return rv;
   }
};

ThreadCleanupList::ThreadCleanupList() {
   //printf("ThreadCleanupList::ThreadCleanupList() head=NULL\n");
   head = 0;
}

ThreadCleanupList::~ThreadCleanupList() {
   //printf("ThreadCleanupList::~ThreadCleanupList() head=%p\n", head);

   while (head) {
      ThreadCleanupNode* w = head->next;
      delete head;
      head = w;
   }
}

void ThreadCleanupList::exec() {
   class ThreadCleanupNode* w = head;
   while (w) {
      w->func(w->arg);
      w = w->next;
   }
}

void ThreadCleanupList::push(qtdest_t func, void *arg) {
   class ThreadCleanupNode* w = new ThreadCleanupNode;
   w->next = head;
   w->func = func;
   w->arg = arg;
   head = w;
   //printf("TCL::push() this=%p, &head=%p, head=%p, head->next=%p\n", this, &head, head, head->next);
}

void ThreadCleanupList::pop(bool exec) {
   //printf("TCL::pop() this=%p, &head=%p, head=%p\n", this, &head, head);
   // NOTE: if exit() is called, then somehow head = 0 !!!
   // I can't explain it, but that's why the if statement is there... :-(
   if (head) {
      if (exec)
	 head->func(head->arg);
      ThreadCleanupNode* w = head->next;
      delete head;
      head = w;
   }
}

#ifdef QORE_MANAGE_STACK
int check_stack(ExceptionSink *xsink) {
   ThreadData *td = thread_data.get();
   printd(5, "check_stack() current=%p limit=%p\n", get_stack_pos(), td->stack_limit);
#ifdef IA64_64
   //printd(5, "check_stack() bsp current=%p limit=%p\n", get_rse_bsp(), td->rse_limit);
   if (td->rse_limit < get_rse_bsp()) {
      xsink->raiseException("STACK-LIMIT-EXCEEDED", "this thread's stack has exceeded the IA-64 RSE (Register Stack Engine) stack size limit (%ld bytes)", qore_thread_stack_limit);
      return -1;
   }

#endif

   if (td->stack_limit
#ifdef STACK_DIRECTION_DOWN
   >
#else
   <
#endif
   get_stack_pos()) {
      xsink->raiseException("STACK-LIMIT-EXCEEDED", "this thread's stack has exceeded the stack size limit (%ld bytes)", qore_thread_stack_limit);
      return -1;
   }
   
   return 0;
}
#endif

// returns 1 if data structure is already on stack, 0 if not (=OK)
int thread_push_container(const AbstractQoreNode* n) {
   std::pair<const_node_set_t::iterator, bool> rv = thread_data.get()->node_set.insert(n);
   return !rv.second;
}

void thread_pop_container(const AbstractQoreNode* n) {
   ThreadData* td = thread_data.get();
   
   const_node_set_t::iterator i = td->node_set.find(n);
   assert(i != td->node_set.end());
   td->node_set.erase(i);
}

int thread_ref_set(const lvalue_ref* r) {
   ThreadData* td = thread_data.get();
   return !td->ref_set.insert(r).second ? -1 : 0;
}

void thread_ref_remove(const lvalue_ref* r) {
   ThreadData* td = thread_data.get();
   assert(td->ref_set.find(r) != td->ref_set.end());
   td->ref_set.erase(r);
}

LocalVarValue *thread_instantiate_lvar() {
   return thread_data.get()->tlpd->lvstack.instantiate();
}

void thread_uninstantiate_lvar(ExceptionSink *xsink) {
   ThreadData *td = thread_data.get();
   td->tlpd->lvstack.uninstantiate(xsink);
}

LocalVarValue *thread_find_lvar(const char *id) {
   ThreadData *td = thread_data.get();
   return td->tlpd->lvstack.find(id);
}

ClosureVarValue *thread_instantiate_closure_var(const char *n_id, const QoreTypeInfo* typeInfo, QoreValue& nval) {
   return thread_data.get()->tlpd->cvstack.instantiate(n_id, typeInfo, nval);
}

void thread_uninstantiate_closure_var(ExceptionSink *xsink) {
   thread_data.get()->tlpd->cvstack.uninstantiate(xsink);
}

ClosureVarValue* thread_find_closure_var(const char *id) {
   return thread_data.get()->tlpd->cvstack.find(id);
}

ThreadSafeLocalVarRuntimeEnvironment *thread_get_runtime_closure_env() {
   return thread_data.get()->closure_rt_env;
}

void thread_set_runtime_closure_env(ThreadSafeLocalVarRuntimeEnvironment *cenv) {
   thread_data.get()->closure_rt_env = cenv;
}

void thread_set_closure_parse_env(ClosureParseEnvironment *cenv) {
   thread_data.get()->closure_parse_env = cenv;
}

ClosureVarValue *thread_get_runtime_closure_var(const LocalVar *id) {
   return thread_data.get()->closure_rt_env->find(id);
}

ClosureParseEnvironment *thread_get_closure_parse_env() {
   return thread_data.get()->closure_parse_env;
}

void parse_push_name(const char* name) {
   ThreadData *td = thread_data.get();
   td->pushName(name);
}

std::string parse_pop_name() {
   ThreadData *td = thread_data.get();
   return td->popName();
}

void set_thread_resource(AbstractThreadResource *atr) {
   ThreadData *td = thread_data.get();
   td->trlist->set(atr);
}

int remove_thread_resource(AbstractThreadResource *atr) {
   ThreadData *td = thread_data.get();
   return td->trlist->remove(atr);
}

void set_thread_resource_id(q_trid_t trid, AbstractThreadResource *atr) {
   ThreadData *td = thread_data.get();
   td->trlist->set(trid, atr);   
}

int remove_thread_resource_id(q_trid_t trid) {
   ThreadData *td = thread_data.get();
   return td->trlist->remove_id(trid);
}

bool check_thread_resource_id(q_trid_t trid) {
   return thread_data.get()->trlist->check(trid);
}

q_trid_t qore_get_trid() {
   return ThreadResourceList::get_resource_id();
}

void mark_thread_resources() {
   ThreadData *td = thread_data.get();
   ThreadResourceList* trl = new ThreadResourceList(td->trlist);
   td->trlist = trl;
}

// returns 0 if the last mark has been cleared, -1 if there are more marks to check
static int purge_thread_resources_to_mark(ThreadData* td, ExceptionSink *xsink) {
   td->trlist->purge(xsink);
   if (td->trlist->prev) {
      ThreadResourceList* tr = td->trlist;
      td->trlist = tr->prev;
      delete tr;
      return -1;
   }
   return 0;
}

// returns 0 if the last mark has been cleared, -1 if there are more marks to check
int purge_thread_resources_to_mark(ExceptionSink *xsink) {
   ThreadData *td = thread_data.get();
   return purge_thread_resources_to_mark(td, xsink);
}

void purge_thread_resources(ExceptionSink *xsink) {
   ThreadData *td = thread_data.get();
   while (purge_thread_resources_to_mark(td, xsink));
}

void parse_try_module_inc() {
   ThreadData *td = thread_data.get();
   td->cie.inc();
}

bool parse_try_module_dec() {
   ThreadData *td = thread_data.get();
   return td->cie.dec();
}

void parse_cond_push(bool mark) {
   ThreadData *td = thread_data.get();
   td->pcs.push(mark);
}

bool parse_cond_else() {
   ThreadData *td = thread_data.get();
   return td->pcs.checkElse();
}

bool parse_cond_pop() {
   ThreadData *td = thread_data.get();
   return td->pcs.pop();
}

bool parse_cond_test() {
   ThreadData *td = thread_data.get();
   return td->pcs.test();
}

void push_parse_options() {
   ThreadData *td = thread_data.get();
   qore_program_private::pushParseOptions(td->current_pgm, td->parse_file);
}

// called when a StatementBlock has "on_exit" blocks
void pushBlock(block_list_t::iterator i) {
   ThreadData *td = thread_data.get();
   td->on_block_exit_list.push_back(i);
}

// called when a StatementBlock has "on_exit" blocks
block_list_t::iterator popBlock() {
   ThreadData *td = thread_data.get();
   block_list_t::iterator i = td->on_block_exit_list.back();
   td->on_block_exit_list.pop_back();
   return i;
}

// called by each "on_exit" statement to activate its code for the block exit
void advanceOnBlockExit() {
   ThreadData *td = thread_data.get();
   --td->on_block_exit_list.back();
}

// new file name, current parse state
void beginParsing(char *file, void *ps) {
   ThreadData *td = thread_data.get();
   //printd(5, "beginParsing() td: %p of %p (%s), (stack=%s)\n", td, file, file ? file : "(null)", (td->plStack ? td->plStack->file : "n/a"));
   
   // store current position
   ProgramLocation *pl = new ProgramLocation(td->parse_file, td->parseState);
   pl->next = td->plStack;
   td->plStack = pl;

   // set new position
   td->parse_file = file;
   td->parseState = ps;
}

void* endParsing() {
   ThreadData* td = thread_data.get();
   //printd(5, "endParsing() td: %p restoreParseOptions pgm: %p parse_file: %p '%s'\n", td, td->current_pgm, td->parse_file, td->parse_file);
   qore_program_private::restoreParseOptions(td->current_pgm, td->parse_file);

   void* rv = td->parseState;

   // ensure there are no conditional blocks left open at EOF
   td->pcs.purge();
   td->cie.purge();
   
   assert(td->plStack);

   ProgramLocation* pl = td->plStack->next;
   //printd(5, "endParsing() td: %p ending parsing of '%s', returning %p, setting file: %p '%s'\n", td, td->parse_file, rv, td->plStack->file, td->plStack->file);

   td->parse_file  = td->plStack->file;
   td->parseState  = td->plStack->parseState;
   delete td->plStack;
   td->plStack = pl;

   return rv;
}

// thread-local functions
bool is_valid_qore_thread() {
   return (bool)thread_data.get();
}

int gettid() {
   return (thread_data.get())->tid;
}

VLock *getVLock() {
   ThreadData *td = thread_data.get();
   return &td->vlock;
}

Context *get_context_stack() {
   return (thread_data.get())->context_stack;
}

void update_context_stack(Context *cstack) {
   ThreadData *td    = thread_data.get();
   td->context_stack = cstack;
}

const char *get_pgm_counter(int &start_line, int &end_line) {
   ThreadData *td = thread_data.get();
   start_line = td->pgm_counter_start;
   end_line = td->pgm_counter_end;
   return td->pgm_file;
}

void update_pgm_counter_pgm_file(int start_line, int end_line, const char *f) {
   ThreadData *td  = thread_data.get();
   td->pgm_counter_start = start_line;
   td->pgm_counter_end   = end_line;
   td->pgm_file          = f;
}

void update_runtime_location(const QoreProgramLocation& loc) {
   ThreadData *td  = thread_data.get();
   td->pgm_counter_start = loc.start_line;
   td->pgm_counter_end   = loc.end_line;
   td->pgm_file          = loc.file;
}

void update_pgm_counter(int start_line, int end_line) {
   ThreadData *td  = thread_data.get();
   td->pgm_counter_start = start_line;
   td->pgm_counter_end   = end_line;
}

const char *get_pgm_file() {
   return (thread_data.get())->pgm_file;
}

QoreProgramLocation get_parse_location() {
   ThreadData *td = thread_data.get();
   return QoreProgramLocation(td->parse_line_end, td->parse_line_end, td->parse_file);
}

void update_parse_location(const QoreProgramLocation& loc) {
   ThreadData *td = thread_data.get();
   td->parse_line_start = loc.start_line;
   td->parse_line_end   = loc.end_line;
   td->parse_file       = loc.file;
}

void get_parse_location(int &start_line, int &end_line) {
   ThreadData *td = thread_data.get();
   start_line = td->parse_line_start;
   end_line = td->parse_line_end;
}

void update_parse_location(int start_line, int end_line, const char *f) {
   ThreadData *td  = thread_data.get();
   td->parse_line_start = start_line;
   td->parse_line_end   = end_line;
   td->parse_file       = f;
   //printd(5, "update_parse_location(start: %d, end: %d, f: %p '%s'\n", start_line, end_line, f, f);
}

void update_parse_location(int start_line, int end_line) {
   ThreadData *td  = thread_data.get();
   td->parse_line_start = start_line;
   td->parse_line_end   = end_line;
}

const char *get_parse_file() {
   return (thread_data.get())->parse_file;
}

const char *get_parse_code() {
   return (thread_data.get())->parse_code;
}

void parseSetCodeInfo(const char *parse_code, const QoreTypeInfo *returnTypeInfo, const char *&old_code, const QoreTypeInfo *&old_returnTypeInfo) {
   ThreadData *td = thread_data.get();
   old_code = td->parse_code;
   old_returnTypeInfo = td->returnTypeInfo;
   td->parse_code = parse_code;
   td->returnTypeInfo = returnTypeInfo;
}

void parseRestoreCodeInfo(const char *parse_code, const QoreTypeInfo *returnTypeInfo) {
   ThreadData *td = thread_data.get();
   td->parse_code = parse_code;
   td->returnTypeInfo = returnTypeInfo;
}

const QoreTypeInfo *getReturnTypeInfo() {
   return (thread_data.get())->returnTypeInfo;
}

const QoreTypeInfo *saveReturnTypeInfo(const QoreTypeInfo *returnTypeInfo) {
   ThreadData *td = thread_data.get();
   const QoreTypeInfo *rv = td->returnTypeInfo;
   td->returnTypeInfo = returnTypeInfo;
   return rv;
}

const AbstractQoreZoneInfo* currentTZ() {
   ThreadData* td = thread_data.get();
   if (td->tlpd && td->tlpd->tz_set)
      return td->tlpd->tz;
   return td->current_pgm ? qore_program_private::currentTZIntern(*(td->current_pgm)) : QTZM.getLocalZoneInfo();
}

void set_thread_tz(const AbstractQoreZoneInfo* tz) {
   ThreadData* td = thread_data.get();
   if (!td->tlpd) {
      printd(0, "set_thread_tz(%p '%s') ignored - no current thread-local program data\n", tz, tz ? tz->getRegionName() : "(null)");
      return;
   }
   td->tlpd->setTZ(tz);
}

const AbstractQoreZoneInfo* get_thread_tz(bool& set) {
   ThreadData* td = thread_data.get();
   if (!td->tlpd) {
      printd(0, "get_thread_tz() ignored - no current thread-local program data\n");
      set = false;
      return 0;
   }
   set = td->tlpd->tz_set;
   return td->tlpd->tz;
}

void clear_thread_tz() {
   ThreadData* td = thread_data.get();
   if (!td->tlpd) {
      printd(0, "clear_thread_tz() ignored - no current thread-local program data\n");
      return;
   }
   td->tlpd->clearTZ();
}

ThreadProgramData* get_thread_program_data() {
   ThreadData* td = thread_data.get();
   assert(td);
   return td->tpd;
}

// pushes a new argv reference counter
void new_argv_ref() {
   thread_data.get()->argv_refs.push();
}

// increments the parse argv reference counter
void inc_argv_ref() {
   thread_data.get()->argv_refs.inc();
}

// pushes an "ignore numeric reference" context
void push_ignore_numeric_argv_ref() {
   thread_data.get()->argv_refs.push_numeric();
}

// pops an "ignore numeric reference" context
void pop_ignore_numeric_argv_ref() {
   thread_data.get()->argv_refs.pop_numeric();
}

// increments the parse argv numeric reference counter
void inc_numeric_argv_ref() {
   thread_data.get()->argv_refs.inc_numeric();
}

// gets the parse argv reference counter and pops the context
int get_pop_argv_ref() {
   return thread_data.get()->argv_refs.pop();
}

// clears the argv reference stack
void clear_argv_ref() {
   thread_data.get()->argv_refs.clear();
}

int get_implicit_element() {
   return thread_data.get()->getElement();
}

int save_implicit_element(int n_element) {
   return thread_data.get()->saveElement(n_element);
}

void end_signal_thread(ExceptionSink *xsink) {
   thread_data.get()->tpd->del(xsink);
}

qore_ns_private* parse_set_ns(qore_ns_private* ns) {
   return thread_data.get()->set_ns(ns);
}

qore_ns_private* parse_get_ns() {
   return thread_data.get()->current_ns;
}

void set_module_context(QoreModuleContext* qmc) {
   thread_data.get()->qmc = qmc;
}

QoreModuleContext* get_module_context() {
   return thread_data.get()->qmc;
}

QoreModuleDefContext* set_module_def_context(QoreModuleDefContext* qmd) {
   ThreadData* td = thread_data.get();
   QoreModuleDefContext* rv = td->qmd;
   td->qmd = qmd;
   return rv;
}

QoreModuleDefContext* get_module_def_context() {
   QoreModuleDefContext* qmd = thread_data.get()->qmd;
   if (qmd)
      qmd->checkName();
   return qmd;
}

void ModuleContextNamespaceList::clear() {
   for (mcnl_t::iterator i = begin(), e = end(); i != e; ++i)
      delete (*i).nns;
   mcnl_t::clear();
}

void ModuleContextFunctionList::clear() {
   for (mcfl_t::iterator i = begin(), e = end(); i != e; ++i)
      (*i).v->deref();
   mcfl_t::clear();
}

ObjectSubstitutionHelper::ObjectSubstitutionHelper(QoreObject *obj) {
   ThreadData *td  = thread_data.get();
   old = td->current_classobj;
   td->current_classobj = obj;
}

ObjectSubstitutionHelper::~ObjectSubstitutionHelper() {
   ThreadData *td  = thread_data.get();
   td->current_classobj = old;
}

CodeContextHelper::CodeContextHelper(const char *code, ClassObj obj, ExceptionSink *xs) {
   ThreadData *td  = thread_data.get();
   old_code = td->current_code;
   old = td->current_classobj;
   xsink = xs;

   QoreObject* o = obj.getObj();
   if (o)
      o->ref();
   td->current_code = code;
   td->current_classobj = obj;
   //printd(5, "CodeContextHelper::CodeContextHelper(code: '%s', {cls: %p, obj: %p}) this: %p td: %p, old_code: %s, old {cls: %p, obj: %p}\n", code ? code : "null", obj.getClass(), obj.getObj(), this, td, old_code ? old_code : "null", old.getClass(), old.getObj());
}

CodeContextHelper::~CodeContextHelper() {
   ThreadData *td  = thread_data.get();

   QoreObject* o = td->current_classobj.getObj();
   if (o)
      o->deref(xsink);

   //printd(5, "CodeContextHelper::~CodeContextHelper() this: %p td: %p current=(code: %s, {cls: %p, obj: %p}) restoring code=%s, {cls: %p, obj: %p}\n", this, td, td->current_code ? td->current_code : "null", td->current_classobj.getClass(), o, old_code ? old_code : "null", old.getClass(), old.getObj());
   td->current_code = old_code;
   td->current_classobj = old;
}

ArgvContextHelper::ArgvContextHelper(QoreListNode* argv, ExceptionSink *n_xsink) : xsink(n_xsink) {
   ThreadData *td  = thread_data.get();
   old_argv = td->current_implicit_arg;
   td->current_implicit_arg = argv;
   //printd(5, "ArgvContextHelper::ArgvContextHelper() setting argv=%p\n", argv);
}

ArgvContextHelper::~ArgvContextHelper() {
   ThreadData *td  = thread_data.get();
   if (td->current_implicit_arg)
      td->current_implicit_arg->deref(xsink);
   td->current_implicit_arg = old_argv;
   //printd(5, "ArgvContextHelper::~ArgvContextHelper() setting argv=%p\n", old_argv);
}

SingleArgvContextHelper::SingleArgvContextHelper(const AbstractQoreNode* val, ExceptionSink *n_xsink) : xsink(n_xsink) {
   //printd(5, "SingleArgvContextHelper::SingleArgvContextHelper() this=%p arg=%p (%s)\n", this, val, val ? val->getTypeName() : 0);
   ThreadData *td  = thread_data.get();
   old_argv = td->current_implicit_arg;
   QoreListNode* argv;
   if (!is_nothing(val)) {
      argv = new QoreListNode();
      argv->push(val->refSelf());
   }
   else
      argv = 0;
   td->current_implicit_arg = argv;
}

SingleArgvContextHelper::~SingleArgvContextHelper() {
   ThreadData *td  = thread_data.get();
   if (td->current_implicit_arg)
      td->current_implicit_arg->deref(xsink);
   td->current_implicit_arg = old_argv;
}

const QoreListNode* thread_get_implicit_args() {
   //printd(5, "thread_get_implicit_args() returning %p\n", thread_data.get()->current_implicit_arg);
   return thread_data.get()->current_implicit_arg;
}

#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
void pushCall(CallNode* cn) {
   thread_list[gettid()].callStack->push(cn);
}

void popCall(ExceptionSink *xsink) {
   thread_list[gettid()].callStack->pop(xsink);
}

QoreListNode* getCallStackList() {
   return thread_list[gettid()].callStack->getCallStack();
}

CallStack *getCallStack() {
   return thread_list[gettid()].callStack;
}
#endif

bool runtime_in_object_method(const char *name, const QoreObject *o) {
   ThreadData *td = thread_data.get();
   if (td->current_classobj.getObj() == o && td->current_code == name)
      return true;
   return false;
}

QoreObject *runtime_get_stack_object() {
   return (thread_data.get())->current_classobj.getObj();
}

const qore_class_private* runtime_get_class() {
   return (thread_data.get())->current_classobj.getClass();
}

ProgramThreadCountContextHelper::ProgramThreadCountContextHelper(ExceptionSink* xsink, QoreProgram* pgm, bool runtime) :
            old_pgm(0), old_tlpd(0), restore(false) {
   if (!pgm)
      return;

   ThreadData *td = thread_data.get();
   printd(5, "ProgramThreadCountContextHelper::ProgramThreadCountContextHelper() current_pgm=%p new_pgm=%p\n", td->current_pgm, pgm);
   if (pgm != td->current_pgm) {
      // try to increment thread count
      if (qore_program_private::incThreadCount(*pgm, xsink))
         return;

      // set up thread stacks
      restore = true;
      old_pgm = td->current_pgm;
      old_tlpd = td->tlpd;
      td->current_pgm = pgm;
      td->tpd->saveProgram(runtime);
   }
}

ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() {
   if (!restore)
      return;

   // restore thread stacks
   ThreadData *td = thread_data.get();

   QoreProgram* pgm = td->current_pgm;
   //printd(5, "ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() current_pgm: %p restoring old pgm: %p old tlpd: %p\n", td->current_pgm, old_pgm, old_tlpd);
   td->current_pgm = old_pgm;
   td->tlpd        = old_tlpd;

   qore_program_private::decThreadCount(*pgm);
}

QoreProgram *getProgram() {
   return (thread_data.get())->current_pgm;
   //return (thread_data.get())->pgmStack->getProgram();
}

RootQoreNamespace *getRootNS() {
   return (thread_data.get())->current_pgm->getRootNS();
   //return (thread_data.get())->pgmStack->getProgram()->getRootNS();
}

int64 getParseOptions() {
   return (thread_data.get())->current_pgm->getParseOptions64();
   //return (thread_data.get())->pgmStack->getProgram()->getParseOptions();
}

bool checkParseOption(int64 o) {
   return (getParseOptions() & o) == o;
}

void updateCVarStack(CVNode* ncvs) {
   ThreadData *td = thread_data.get();
   td->cvarstack = ncvs;
}

CVNode* getCVarStack() {
   return (thread_data.get())->cvarstack;
}

void updateVStack(VNode* nvs) {
   ThreadData *td = thread_data.get();
   td->vstack = nvs;
}

VNode* getVStack() {
   return (thread_data.get())->vstack;
}

void save_global_vnode(VNode* vn) {
   ThreadData *td = thread_data.get();
   td->global_vnode = vn;
}

VNode* get_global_vnode() {
   return (thread_data.get())->global_vnode;
}

void setParseClass(QoreClass *c) {
   ThreadData *td = thread_data.get();
   td->parseClass = c;
}

QoreClass *getParseClass() {
   return (thread_data.get())->parseClass;
}

// to save the exception for "rethrow"
QoreException* catchSwapException(QoreException *e) {
   ThreadData *td = thread_data.get();
   QoreException* old = td->catchException;
   td->catchException = e;
   return old;
}

// for "rethrow"
QoreException *catchGetException() {
   ThreadData *td = thread_data.get();
   //printd(5, "cGE() td=%p e=%p\n", td, td->catchException);
   assert(td->catchException);
   return td->catchException;
}

void qore_exit_process(int rc) {
   int tid = gettid();
   SafeLocker sl(lThreadList);

   // call pthread_cancel on all threads so the call to exit() will not cause a core dump
   for (int i = 1; i < MAX_QORE_THREADS; ++i) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
      if (i != tid && thread_list[i].ptid.p != PTHREAD_AVAIL.p && thread_list[i].ptid.p != PTHREAD_NA.p) {
#else
      if (i != tid && thread_list[i].ptid && thread_list[i].ptid != PTHREAD_NA) {
#endif
         //printf("qore_exit_process() canceling TID %d ptid: %p (this TID: %d)\n", i, thread_list[i].ptid, tid);
	 int trc = pthread_cancel(thread_list[i].ptid);
	 if (!trc) {
#if 1
//#ifdef DARWIN
	    // for some reason we cannot call pthread_join() on the main thread in Darwin or we get a segfault
	    // at least in 10.7.4 - but only when debugging is disabled for some reason
	    if (i == 1)
	       continue;
#endif
	    thread_list[i].joined = true;

	    //printf("qore_exit_process() ptid: %p\n", thread_list[i].ptid);
	    sl.unlock();
	    pthread_join(thread_list[i].ptid, 0);
	    //printd("ptid: %p p: %p\n", thread_list[i].ptid, p);
	    sl.lock();
	 }
#ifdef DEBUG
	 else
	    printd(0, "pthread_cancel() returned %d (%s) on tid %d (%p)\n", rc, strerror(rc), tid, thread_list[i].ptid);
#endif
      }
   }
   sl.unlock();

#ifdef HAVE_SIGNAL_HANDLING
   // stop signal handling thread
   QSM.del();
#endif

   threads_initialized = false;
   exit(rc);
}

// sets up the signal thread entry in the thread list
int get_signal_thread_entry() {
   AutoLocker al(lThreadList);
   thread_list[0].allocate(0);
   return 0;
}

// returns tid allocated for thread
int get_thread_entry() {
   int tid = -1;
   AutoLocker al(lThreadList);

   if (current_tid == MAX_QORE_THREADS) {
      int i;
      // scan thread_list for free entry
      for (i = 1; i < MAX_QORE_THREADS; i++) {
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
	 if (thread_list[i].ptid.p == PTHREAD_AVAIL.p) {
#else
	 if (thread_list[i].ptid == PTHREAD_AVAIL) {
#endif
	    tid = i;
	    goto finish;
	 }
      }
      if (i == MAX_QORE_THREADS)
	 return -1;
   }
   else
      tid = current_tid++;

  finish:
   thread_list[tid].allocate(new tid_node(tid));
   num_threads++;
   //printf("t%d cs=0\n", tid);

   return tid;
}

static void delete_thread_data() {
   delete thread_data.get();
}

void deregister_thread(int tid) {
   //printf("DEBUG: deregister_thread() TID %d terminated\n", tid);
   // NOTE: cannot safely call printd here, because normally the thread_data has been deleted
   AutoLocker al(lThreadList);
   thread_list[tid].cleanup();
   --num_threads;
}

void deregister_signal_thread() {
   // NOTE: cannot safely call printd here, because normally the thread_data has been deleted
   AutoLocker al(lThreadList);
   thread_list[0].cleanup();
}

void delete_signal_thread() {
   thread_data.get()->del(0);
   delete_thread_data();
   deregister_signal_thread();
}

// should only be called from the new thread
void register_thread(int tid, pthread_t ptid, QoreProgram *p) {
   thread_list[tid].ptid = ptid;
#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
   thread_list[tid].callStack = new CallStack();
#endif
   ThreadData *td = new ThreadData(tid, p);
   thread_data.set(td);
   // set lvstack if QoreProgram set
   if (p)
      td->tpd->saveProgram(true);
}

static void qore_thread_cleanup(void* n) {
   mpfr_free_cache();
}

// put "op_background_thread" in an unnamed namespace to make it 'static extern "C"'
namespace {
   extern "C" void* op_background_thread(void* x) {
      BGThreadParams *btp = (BGThreadParams *)x;
      // register thread
      register_thread(btp->tid, pthread_self(), btp->pgm);
      printd(5, "op_background_thread() btp=%p TID %d started\n", btp, btp->tid);
      //printf("op_background_thread() btp=%p TID %d started\n", btp, btp->tid);

      pthread_cleanup_push(qore_thread_cleanup, (void*)0);

      {
         ExceptionSink xsink;

         // create thread-local data for this thread in the program object
         qore_program_private::startThread(*btp->pgm, xsink);
         // set program counter for new thread
         update_runtime_location(btp->loc);

         {
            AbstractQoreNode* rv;
            {
               CodeContextHelper cch(0, btp->getCallObject(), &xsink);
#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
               // push this call on the thread stack
               CallStackHelper csh("background operator", CT_NEWTHREAD, btp->getCallObject(), &xsink);
#endif

               // dereference call object if present
               btp->derefCallObj();

               // run thread expression
               rv = btp->exec(&xsink);

               // if there is an object, we dereference the extra reference here
               btp->derefObj(&xsink);
            }
            
            // dereference any return value from the background expression
            if (rv)
               rv->deref(&xsink);

            // delete any thread data
            thread_data.get()->del(&xsink);

            // cleanup thread resources
            purge_thread_resources(&xsink);

            xsink.handleExceptions();

            printd(4, "thread terminating");

            // delete internal thread data structure
            delete_thread_data();
            
            // deregister_thread
            deregister_thread(btp->tid);
            
            // run any cleanup functions
            tclist.exec();

            //printd(5, "deleting thread params %p\n", btp);
            delete btp;
         }
      }

      pthread_cleanup_pop(1);

      pthread_exit(0);
      return 0;
   }
}

static AbstractQoreNode* op_background(const AbstractQoreNode* left, const AbstractQoreNode* ignored, bool ref_rv, ExceptionSink *xsink) {
   if (!left)
      return 0;

   //printd(2, "op_background() before crlr left = %p\n", left);
   ReferenceHolder<AbstractQoreNode> nl(copy_and_resolve_lvar_refs(left, xsink), xsink);
   //printd(2, "op_background() after crlr nl = %p\n", nl);
   if (*xsink || !nl)
      return 0;

   // now we are ready to create the new thread

   // get thread entry
   //printd(2, "calling get_thread_entry()\n");
   int tid = get_thread_entry();
   //printd(2, "got %d()\n", tid);

   // if can't start thread, then throw exception
   if (tid == -1) {
      xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
      return 0;
   }

   BGThreadParams *tp = new BGThreadParams(nl.release(), tid, xsink);
   //printd(5, "created BGThreadParams(%p, %d) = %p\n", *nl, tid, tp);
   if (*xsink) {
      deregister_thread(tid);
      return 0;
   }
   //printd(5, "tp = %p\n", tp);
   // create thread
   int rc;
   pthread_t ptid;

   //printd(5, "calling pthread_create(%p, %p, %p, %p)\n", &ptid, &ta_default, op_background_thread, tp);

   if ((rc = pthread_create(&ptid, ta_default.get_ptr(), op_background_thread, tp))) {
      tp->cleanup(xsink);
      delete tp;

      deregister_thread(tid);
      xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc, "could not create thread");
      return 0;
   }
   //printd(5, "pthread_create() new thread TID %d, pthread_create() returned %d\n", tid, rc);
   return ref_rv ? new QoreBigIntNode(tid) : 0;
}

static AbstractQoreNode* check_op_background(QoreTreeNode* tree, LocalVar *oflag, int pflag, int &lvids, const QoreTypeInfo *&returnTypeInfo, const char *name, const char *descr) {
   returnTypeInfo = bigIntTypeInfo;

   if (pflag & PF_CONST_EXPRESSION)
      parseException("ILLEGAL-OPERATION", "the background operator may not be used in an expression initializing a constant value executed at parse time");

   return tree->defaultParseInit(oflag, pflag, lvids, returnTypeInfo);
}

#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
#include <qore/QoreRWLock.h>

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
extern QoreRWLock *thread_stack_lock;
#else
extern QoreRWLock thread_stack_lock;
#endif
#endif

void init_qore_threads() {
   QORE_TRACE("qore_init_threads()");

#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
   thread_stack_lock = new QoreRWLock;
#endif
#endif

#ifdef QORE_MANAGE_STACK
   // get default stack size
#ifdef SOLARIS
#if TARGET_BITS == 32
   // pthread_attr_getstacksize() on default attributes returns 0 on Solaris
   // so we set according to defaults - 1MB on 32-bit builds
   qore_thread_stack_size = 1024*1024;
#else
   // 2MB on 64-bit builds
   qore_thread_stack_size = 1024*1024*2;
#endif // #if TARGET_BITS == 32
#else
#ifdef WIN32
   // FIXME: get real thread stack size via win API call
   qore_thread_stack_size = 1024*1024;
#else // !WIN32 && !SOLARIS
   qore_thread_stack_size = ta_default.getstacksize();
   assert(qore_thread_stack_size);
   //printd(5, "getstacksize() returned: %ld\n", qore_thread_stack_size);
#endif // #ifdef WIN32
#endif // #ifdef SOLARIS

#ifdef IA64_64
   // the top half of the stack is for the normal stack, the bottom half is for the register stack
   qore_thread_stack_size /= 2;
#endif // #ifdef IA64_64
   qore_thread_stack_limit = qore_thread_stack_size - QORE_STACK_GUARD;
   //printd(5, "default stack size %ld, limit %ld\n", qore_thread_stack_size, qore_thread_stack_limit);
#endif // #ifdef QORE_MANAGE_STACK

   // setup parent thread data
   register_thread(get_thread_entry(), pthread_self(), 0);

   // register "background" Operator.handler
   OP_BACKGROUND = oplist.add(new Operator(1, "background", "run in background thread", 0, true, false, check_op_background));
   OP_BACKGROUND->addFunction(NT_ALL, NT_NONE, op_background);

   // initialize recursive mutex attribute
   pthread_mutexattr_init(&ma_recursive);
   pthread_mutexattr_settype(&ma_recursive, PTHREAD_MUTEX_RECURSIVE);

   // mark threading as active
   threads_initialized = true;
}

QoreNamespace *get_thread_ns(QoreNamespace &qorens) {
   // create Qore::Thread namespace
   QoreNamespace *Thread = new QoreNamespace("Thread");

   Thread->addSystemClass(initQueueClass(*Thread));
   Thread->addSystemClass(initAbstractSmartLockClass(*Thread));
   Thread->addSystemClass(initMutexClass(*Thread));
   //Thread->addSystemClass(initRMutexClass());
   Thread->addSystemClass(initConditionClass(*Thread));
   Thread->addSystemClass(initRWLockClass(*Thread));
   Thread->addSystemClass(initGateClass(*Thread));
   Thread->addSystemClass(initSequenceClass(*Thread));
   Thread->addSystemClass(initCounterClass(*Thread));

   Thread->addSystemClass(initAutoLockClass(*Thread));
   Thread->addSystemClass(initAutoGateClass(*Thread));
   Thread->addSystemClass(initAutoReadLockClass(*Thread));
   Thread->addSystemClass(initAutoWriteLockClass(*Thread));

   return Thread;
}

void delete_thread_local_data() {
   // set no program location
   update_pgm_counter_pgm_file(0, 0, 0);

   ExceptionSink xsink;
   // delete any thread data
   thread_data.get()->del(&xsink);
   
   purge_thread_resources(&xsink);
   xsink.handleExceptions();
}

void delete_qore_threads() {
   QORE_TRACE("delete_qore_threads()");

   // mark threading as inactive
   threads_initialized = false;

   pthread_mutexattr_destroy(&ma_recursive);

   delete_thread_data();

   thread_list[1].cleanup();

#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
   delete thread_stack_lock;
#endif
#endif
}

QoreListNode* get_thread_list() {
   QoreListNode* l = new QoreListNode();

   AutoLocker al(lThreadList);
   tid_node* w = tid_head;
   while (w) {
      l->push(new QoreBigIntNode(w->tid));
      w = w->next;
   }
   return l;
}

#ifdef QORE_RUNTIME_THREAD_STACK_TRACE
#include <qore/QoreRWLock.h>

#if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
extern QoreRWLock *thread_stack_lock;
#else
extern QoreRWLock thread_stack_lock;
#endif

QoreHashNode* getAllCallStacks() {
   QoreHashNode* h = new QoreHashNode();
   QoreString str;

   // grab the call stack write lock
   QoreAutoRWWriteLocker wl(thread_stack_lock);

   // grab thread list lock
   AutoLocker al(lThreadList);

   tid_node* w = tid_head;
   while (w) {
      // get call stack
      if (thread_list[w->tid].callStack) {
	 QoreListNode* l = thread_list[w->tid].callStack->getCallStack();
	 if (l->size()) {
	    // make hash entry
	    str.clear();
	    str.sprintf("%d", w->tid);
	    h->setKeyValue(str.getBuffer(), l, 0);
	 }
	 else
	    l->deref(0);
      }
      w = w->next;
   }   
   return h;
}
#endif
