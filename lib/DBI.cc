/*
  DBI.cc

  Database Independent SQL Layer

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
#include <qore/intern/QC_Datasource.h>
#include <qore/intern/QC_DatasourcePool.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define NUM_DBI_CAPS 4

typedef safe_dslist<class DBIDriver *> dbi_list_t;

// global qore library class for DBI driver management
DLLEXPORT DBIDriverList DBI;

struct dbi_cap_hash
{
   int cap;
   char *desc;
};

struct dbi_cap_hash dbi_cap_list[] =
{ { DBI_CAP_CHARSET_SUPPORT,        "CharsetSupport" },
  { DBI_CAP_TRANSACTION_MANAGEMENT, "TransactionManagement" },
  { DBI_CAP_STORED_PROCEDURES,      "StoredProcedures" },
  { DBI_CAP_LOB_SUPPORT,            "LargeObjectSupport" },
};

struct qore_dbi_mlist_private {
      dbi_method_list_t l;
};

qore_dbi_method_list::qore_dbi_method_list() : priv(new qore_dbi_mlist_private)
{
}

qore_dbi_method_list::~qore_dbi_method_list()
{
   delete priv;
}

// covers open, commit, rollback, and begin transaction
void qore_dbi_method_list::add(int code, q_dbi_open_t method)
{
   priv->l.push_back(std::make_pair(code, (void *)method));
}

// for close
void qore_dbi_method_list::add(int code, q_dbi_close_t method)
{
   priv->l.push_back(std::make_pair(code, (void *)method));
}

// covers select, select_rows. and exec
void qore_dbi_method_list::add(int code, q_dbi_select_t method)
{
   priv->l.push_back(std::make_pair(code, (void *)method));
}

// covers get_server_version
void qore_dbi_method_list::add(int code, q_dbi_get_server_version_t method)
{
   priv->l.push_back(std::make_pair(code, (void *)method));
}

// covers get_client_version
void qore_dbi_method_list::add(int code, q_dbi_get_client_version_t method)
{
   priv->l.push_back(std::make_pair(code, (void *)method));
}

dbi_method_list_t *qore_dbi_method_list::getMethods() const
{
   return &priv->l;
}

struct qore_dbi_private {
      DBIDriverFunctions f;
      int caps;
      const char *name;
      DLLLOCAL qore_dbi_private(const char *nme, const dbi_method_list_t &methods, int cps)
      {
	 // add methods to internal data structure
	 for (dbi_method_list_t::const_iterator i = methods.begin(), e = methods.end(); i != e; ++i)
	 {
	    assert((*i).first >= 0 && (*i).first < QDBI_VALID_CODES);
	    switch ((*i).first)
	    {
	       case QDBI_METHOD_OPEN:
		  assert(!f.open);
		  f.open = (q_dbi_open_t)(*i).second;
		  break;
	       case QDBI_METHOD_CLOSE:
		  assert(!f.close);
		  f.close = (q_dbi_close_t)(*i).second;
		  break;
	       case QDBI_METHOD_SELECT:
		  assert(!f.select);
		  f.select = (q_dbi_select_t)(*i).second;
		  break;
	       case QDBI_METHOD_SELECT_ROWS:
		  assert(!f.selectRows);
		  f.selectRows = (q_dbi_select_rows_t)(*i).second;
		  break;
	       case QDBI_METHOD_EXEC:
		  assert(!f.execSQL);
		  f.execSQL = (q_dbi_exec_t)(*i).second;
		  break;
	       case QDBI_METHOD_COMMIT:
		  assert(!f.commit);
		  f.commit = (q_dbi_commit_t)(*i).second;
		  break;
	       case QDBI_METHOD_ROLLBACK:
		  assert(!f.rollback);
		  f.rollback = (q_dbi_rollback_t)(*i).second;
		  break;
	       case QDBI_METHOD_BEGIN_TRANSACTION:
		  assert(!f.begin_transaction);
		  f.begin_transaction = (q_dbi_begin_transaction_t)(*i).second;
		  break;
	       case QDBI_METHOD_AUTO_COMMIT:
		  assert(!f.auto_commit);
		  f.auto_commit = (q_dbi_auto_commit_t)(*i).second;
		  break;
	       case QDBI_METHOD_ABORT_TRANSACTION_START:
		  assert(!f.abort_transaction_start);
		  f.abort_transaction_start = (q_dbi_abort_transaction_start_t)(*i).second;
		  break;
	       case QDBI_METHOD_GET_SERVER_VERSION:
		  assert(!f.get_server_version);
		  f.get_server_version = (q_dbi_get_server_version_t)(*i).second;
		  break;
	       case QDBI_METHOD_GET_CLIENT_VERSION:
		  assert(!f.get_client_version);
		  f.get_client_version = (q_dbi_get_client_version_t)(*i).second;
		  break;
#ifdef DEBUG
	       default:
		  assert(false);
#endif
	    }
	 }
	 // ensure minimum methods are defined
	 assert(f.open);
	 assert(f.close);
	 assert(f.select);
	 assert(f.selectRows);
	 assert(f.execSQL);
	 assert(f.commit);
	 assert(f.rollback);
   
	 name = nme;
	 caps = cps;
      }
};

DBIDriver::DBIDriver(const char *nme, const dbi_method_list_t &methods, int cps) : priv(new qore_dbi_private(nme, methods, cps))
{
}

DBIDriver::~DBIDriver()
{
   delete priv;
}

const char *DBIDriver::getName() const
{
   return priv->name;
}

int DBIDriver::getCaps() const
{
   return priv->caps;
}

QoreListNode *DBIDriver::getCapList() const
{
   QoreListNode *l = new QoreListNode();
   for (int i = 0; i < NUM_DBI_CAPS; i++)
      if (priv->caps & dbi_cap_list[i].cap)
	 l->push(new QoreStringNode(dbi_cap_list[i].desc));
   return l;
}

int DBIDriver::init(Datasource *ds, class ExceptionSink *xsink)
{
   return priv->f.open(ds, xsink);
}

int DBIDriver::close(Datasource *ds)
{
   return priv->f.close(ds);
}

class AbstractQoreNode *DBIDriver::select(Datasource *ds, const class QoreString *sql, const QoreListNode *args, class ExceptionSink *xsink)
{
   return priv->f.select(ds, sql, args, xsink);
}

class AbstractQoreNode *DBIDriver::selectRows(Datasource *ds, const class QoreString *sql, const QoreListNode *args, class ExceptionSink *xsink)
{
   return priv->f.selectRows(ds, sql, args, xsink);
}

class AbstractQoreNode *DBIDriver::execSQL(Datasource *ds, const class QoreString *sql, const QoreListNode *args, class ExceptionSink *xsink)
{
   return priv->f.execSQL(ds, sql, args, xsink);
}

int DBIDriver::commit(Datasource *ds, class ExceptionSink *xsink)
{
   return priv->f.commit(ds, xsink);
}

int DBIDriver::rollback(Datasource *ds, class ExceptionSink *xsink)
{
   return priv->f.rollback(ds, xsink);
}

int DBIDriver::beginTransaction(Datasource *ds, class ExceptionSink *xsink)
{
   if (priv->f.begin_transaction)
      return priv->f.begin_transaction(ds, xsink);
   return 0; // 0 = OK
}

int DBIDriver::autoCommit(Datasource *ds, class ExceptionSink *xsink)
{
   if (priv->f.auto_commit)
      return priv->f.auto_commit(ds, xsink);
   return 0; // 0 = OK
}

int DBIDriver::abortTransactionStart(Datasource *ds, class ExceptionSink *xsink)
{
   if (priv->f.abort_transaction_start)
      return priv->f.abort_transaction_start(ds, xsink);
   return 0; // 0 = OK
}

class AbstractQoreNode *DBIDriver::getServerVersion(Datasource *ds, class ExceptionSink *xsink)
{
   if (priv->f.get_server_version)
      return priv->f.get_server_version(ds, xsink);
   return 0;
}

class AbstractQoreNode *DBIDriver::getClientVersion(const Datasource *ds, class ExceptionSink *xsink)
{
   if (priv->f.get_client_version)
      return priv->f.get_client_version(ds, xsink);
   return 0;
}

// it's not necessary to lock this object because it will only be written to in one thread at a time
// (within the module lock)
// note that a safe_dslist is used because it can be safely read in multiple threads while
// being written to (in the lock).  The list should never be that long so the penalty for searching
// a linked list with strcmp() against using a hash with explicit locking around all searches
// should be acceptable...
struct qore_dbi_dlist_private {
      dbi_list_t l;

      DLLLOCAL ~qore_dbi_dlist_private()
      {
	 dbi_list_t::iterator i;
	 while ((i = l.begin()) != l.end())
	 {
	    class DBIDriver *driv = *i;
	    l.erase(i);
	    delete driv;
	 }
      }

      DLLLOCAL DBIDriver *find_intern(const char *name) const
      {
	 for (dbi_list_t::const_iterator i = l.begin(); i != l.end(); i++)
	    if (!strcmp(name, (*i)->getName()))
	       return *i;
	 
	 return 0;
      }

      DLLLOCAL class QoreListNode *getDriverList() const
      {
	 if (l.empty())
	    return 0;
	 
	 class QoreListNode *lst = new QoreListNode();
	 
	 for (dbi_list_t::const_iterator i = l.begin(); i != l.end(); i++)
	    lst->push(new QoreStringNode((*i)->getName()));
	 
	 return lst;
      }
};

DBIDriverList::DBIDriverList() : priv(new qore_dbi_dlist_private)
{
}

DBIDriverList::~DBIDriverList()
{
   delete priv;
}

DBIDriver *DBIDriverList::find_intern(const char *name) const
{
   return priv->find_intern(name);
}

DBIDriver *DBIDriverList::find(const char *name) const
{
   DBIDriver *d = priv->find_intern(name);
   if (d)
      return d;

   // to to load the driver if it doesn't exist
   // ignore any exceptions
   ExceptionSink xs;
   MM.runTimeLoadModule(name, &xs);
   xs.clear();

   return priv->find_intern(name);
}

class DBIDriver *DBIDriverList::registerDriver(const char *name, const struct qore_dbi_method_list &methods, int caps)
{
   assert(!priv->find_intern(name));
   
   DBIDriver *dd = new DBIDriver(name, *(methods.getMethods()), caps);
   priv->l.push_back(dd);
   return dd;
}

class QoreListNode *DBIDriverList::getDriverList() const
{
   return priv->getDriverList();
}

void DBI_concat_numeric(class QoreString *str, const AbstractQoreNode *v)
{
   if (is_nothing(v) || is_null(v))
   {
      str->concat("null");
      return;
   }

   if (v->type == NT_FLOAT || (v->type == NT_STRING && strchr((reinterpret_cast<const QoreStringNode *>(v))->getBuffer(), '.')))
   {
      str->sprintf("%g", v->getAsFloat());
      return;
   }
   str->sprintf("%lld", v->getAsBigInt());
}

int DBI_concat_string(class QoreString *str, const AbstractQoreNode *v, class ExceptionSink *xsink)
{
   if (is_nothing(v) || is_null(v))
      return 0;

   QoreStringValueHelper tstr(v, str->getEncoding(), xsink);
   if (*xsink)
      return -1;

   str->concat(*tstr, xsink);
   return *xsink;
}

/*
  parseDatasource()

  parses strings of the form: driver:user/pass@db(charset)%host

  driver, charset, and host are optional
*/

class QoreHashNode *parseDatasource(const char *ds, class ExceptionSink *xsink)
{
   if (!ds || !ds[0])
      return NULL;

   // use a QoreString to create a temporary buffer
   QoreString tmp(ds);
   char *str = const_cast<char *>(tmp.getBuffer());

   ReferenceHolder<QoreHashNode> h(new QoreHashNode(), xsink);
   char *p = strchr(str, ':');
   if (p)
   {
      *p = '\0';
      h->setKeyValue("type", new QoreStringNode(str), NULL);
      str = p + 1;
   }

   bool has_pass = false;
   p = strchr(str, '/');
   if (p)
   {
      *p = '\0';
      h->setKeyValue("user", new QoreStringNode(str), NULL);
      str = p + 1;
      has_pass = true;
   }

   p = strrchr(str, '@');
   if (!p)
   {
      xsink->raiseException("DATASOURCE-PARSE-ERROR", "missing database name delimited by '@' in '%s'", ds);
      return 0;
   }

   *p = '\0';
   if (p != str)
   {
      if (has_pass)
	 h->setKeyValue("pass", new QoreStringNode(str), NULL);
      else
	 h->setKeyValue("user", new QoreStringNode(str), NULL);
   }
   str = p + 1;

   char *db = str;
   p = strchr(str, '(');
   if (p)
   {
      char *end = strchr(p, ')');
      if (!end) 
      {
	 xsink->raiseException("DATASOURCE-PARSE-ERROR", "missing closing parenthesis in charset specification in '%s'", ds);
	 return 0;
      }
      *p = '\0';  // terminate for db
      *end = '\0';  // terminate charset
      p++;
      h->setKeyValue("charset", new QoreStringNode(p), NULL);
      str = end + 1;
   }
   
   p = strchr(str, '%');
   if (p)
   {
      *p = '\0';
      p++;
      if (!*p)
      {
	 xsink->raiseException("DATASOURCE-PARSE-ERROR", "missing hostname string after '%' delimeter in '%s'", ds);
	 return 0;
      }
      h->setKeyValue("host", new QoreStringNode(p), NULL);
   }

   h->setKeyValue("db", new QoreStringNode(db), NULL);
   return h.release();
}

class AbstractQoreNode *f_parseDatasource(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0;

   if (!(p0 = test_string_param(params, 0)))
      return NULL;

   return parseDatasource(p0->getBuffer(), xsink);
}

class AbstractQoreNode *f_getDBIDriverList(const QoreListNode *params, ExceptionSink *xsink)
{
   return DBI.getDriverList();
}

class AbstractQoreNode *f_getDBIDriverCapabilityList(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0;

   if (!(p0 = test_string_param(params, 0)))
      return NULL;

   DBIDriver *dd = DBI.find(p0->getBuffer());
   if (!dd)
      return NULL;

   return dd->getCapList();
}

class AbstractQoreNode *f_getDBIDriverCapabilities(const QoreListNode *params, ExceptionSink *xsink)
{
   QoreStringNode *p0;

   if (!(p0 = test_string_param(params, 0)))
      return NULL;

   DBIDriver *dd = DBI.find(p0->getBuffer());
   if (!dd)
      return NULL;

   return new QoreBigIntNode(dd->getCaps());
}

void init_dbi_functions()
{
   builtinFunctions.add("getDBIDriverList", f_getDBIDriverList);
   builtinFunctions.add("getDBIDriverCapabilityList", f_getDBIDriverCapabilityList);
   builtinFunctions.add("getDBIDriverCapabilities", f_getDBIDriverCapabilities);
   builtinFunctions.add("parseDatasource", f_parseDatasource);
}

class QoreNamespace *getSQLNamespace()
{
   // create Qore::SQL namespace
   class QoreNamespace *SQLNS = new QoreNamespace("SQL");

   SQLNS->addSystemClass(initDatasourceClass());
   SQLNS->addSystemClass(initDatasourcePoolClass());

   // datasource type constants
   SQLNS->addConstant("DSOracle",   new QoreStringNode("oracle"));
   SQLNS->addConstant("DSMySQL",    new QoreStringNode("mysql"));
   SQLNS->addConstant("DSSybase",   new QoreStringNode("sybase"));
   SQLNS->addConstant("DSPGSQL",    new QoreStringNode("pgsql"));
   SQLNS->addConstant("DSMSSQL",    new QoreStringNode("mssql"));
   // the following have no drivers yet
   SQLNS->addConstant("DSDB2",      new QoreStringNode("db2"));
   SQLNS->addConstant("DSInformix", new QoreStringNode("informix"));

   // for DBI driver capabilities
   SQLNS->addConstant("DBI_CAP_CHARSET_SUPPORT",        new QoreBigIntNode(DBI_CAP_CHARSET_SUPPORT));
   SQLNS->addConstant("DBI_CAP_TRANSACTION_MANAGEMENT", new QoreBigIntNode(DBI_CAP_TRANSACTION_MANAGEMENT));
   SQLNS->addConstant("DBI_CAP_STORED_PROCEDURES",      new QoreBigIntNode(DBI_CAP_STORED_PROCEDURES));
   SQLNS->addConstant("DBI_CAP_LOB_SUPPORT",            new QoreBigIntNode(DBI_CAP_LOB_SUPPORT));
   SQLNS->addConstant("DBI_CAP_BIND_BY_VALUE",          new QoreBigIntNode(DBI_CAP_BIND_BY_VALUE));
   SQLNS->addConstant("DBI_CAP_BIND_BY_PLACEHOLDER",    new QoreBigIntNode(DBI_CAP_BIND_BY_PLACEHOLDER));

   // for column types for binding
   SQLNS->addConstant("VARCHAR", new QoreStringNode("string"));
   SQLNS->addConstant("NUMBER",  new QoreStringNode("string"));
   SQLNS->addConstant("CLOB",    new QoreStringNode("clob"));
   SQLNS->addConstant("BLOB",    new QoreStringNode("blob"));
   SQLNS->addConstant("DATE",    new QoreStringNode("date"));

   return SQLNS;
}

