/*
 Datasource.cpp
 
 Qore Programming Language
 
 Copyright 2003 - 2013 David Nichols
 
 NOTE that 2 copies of connection values are kept in case
 the values are changed while a connection is in use
 
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
#include <qore/intern/qore_dbi_private.h>
#include <qore/intern/qore_ds_private.h>

#include <stdlib.h>
#include <string.h>

bool qore_ds_private::statementExecuted(int rc, ExceptionSink *xsink) {
   if (!in_transaction) {
      if (!rc) {
         assert(!active_transaction);
         in_transaction = true;
         active_transaction = true;
         return true;
      }
      else
         qore_dbi_private::get(*dsl)->abortTransactionStart(ds, xsink);
   }
   else if (!rc && !active_transaction) {
      active_transaction = true;
   }
   return false;
}

Datasource::Datasource(DBIDriver* ndsl) : priv(new qore_ds_private(this, ndsl)) {
}

Datasource::Datasource(const Datasource& old) : priv(new qore_ds_private(*old.priv, this)) {
}

Datasource::~Datasource() {
   if (priv->isopen)
      close();

   delete priv;
}

void Datasource::setPendingConnectionValues(const Datasource *other) {
   priv->setPendingConnectionValues(other->priv);
}

void Datasource::setTransactionStatus(bool t) {
   //printd(5, "Datasource::setTS(%d) this=%08p\n", t, this);
   priv->in_transaction = t;
}

QoreListNode *Datasource::getCapabilityList() const {
   return qore_dbi_private::get(*priv->dsl)->getCapList();
}

int Datasource::getCapabilities() const {
   return qore_dbi_private::get(*priv->dsl)->getCaps();
}

bool Datasource::isInTransaction() const { 
   return priv->in_transaction; 
}

bool Datasource::activeTransaction() const { 
   return priv->active_transaction; 
}

bool Datasource::getAutoCommit() const { 
   return priv->autocommit;
}

bool Datasource::isOpen() const { 
   return priv->isopen; 
}

Datasource *Datasource::copy() const {
   return new Datasource(*this);
}

void Datasource::setConnectionValues() {
   priv->setConnectionValues();
}

void Datasource::setAutoCommit(bool ac) {
   priv->autocommit = ac;
}

AbstractQoreNode *Datasource::select(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
   AbstractQoreNode *rv = qore_dbi_private::get(*priv->dsl)->select(this, query_str, args, xsink);
   autoCommit(xsink);

   // set active_transaction flag if in a transaction and the active_transaction flag
   // has not yet been set and no exception was raised
   if (priv->in_transaction && !priv->active_transaction && !*xsink)
      priv->active_transaction = true;

   return rv;
}

AbstractQoreNode *Datasource::selectRows(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
   AbstractQoreNode *rv = qore_dbi_private::get(*priv->dsl)->selectRows(this, query_str, args, xsink);
   autoCommit(xsink);

   // set active_transaction flag if in a transaction and the active_transaction flag
   // has not yet been set and no exception was raised
   if (priv->in_transaction && !priv->active_transaction && !*xsink)
      priv->active_transaction = true;

   return rv;
}

QoreHashNode *Datasource::selectRow(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
   QoreHashNode *rv = qore_dbi_private::get(*priv->dsl)->selectRow(this, query_str, args, xsink);
   autoCommit(xsink);

   // set active_transaction flag if in a transaction and the active_transaction flag
   // has not yet been set and no exception was raised
   if (priv->in_transaction && !priv->active_transaction && !*xsink)
      priv->active_transaction = true;

   return rv;
}

AbstractQoreNode *Datasource::exec_internal(bool doBind, const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
   if (!priv->autocommit && !priv->in_transaction && beginImplicitTransaction(xsink))
      return 0;

   assert(priv->isopen && priv->private_data);

   AbstractQoreNode *rv = doBind ? qore_dbi_private::get(*priv->dsl)->execSQL(this, query_str, args, xsink)
      : qore_dbi_private::get(*priv->dsl)->execRawSQL(this, query_str, xsink);;
   //printd(5, "Datasource::exec_internal() this=%08p, autocommit=%d, in_transaction=%d, xsink=%d\n", this, priv->autocommit, priv->in_transaction, xsink->isException());

   if (priv->connection_aborted) {
      assert(*xsink);
      assert(!rv);
      return 0;
   }

   if (priv->autocommit)
      qore_dbi_private::get(*priv->dsl)->autoCommit(this, xsink);
   else 
      priv->statementExecuted(*xsink, xsink);

   return rv;
}

int Datasource::autoCommit(ExceptionSink *xsink) {
   if (priv->autocommit && !priv->connection_aborted)
      return qore_dbi_private::get(*priv->dsl)->autoCommit(this, xsink);
   return 0;
}

AbstractQoreNode *Datasource::exec(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
   return exec_internal(true, query_str, args, xsink);
}

// deprecated: remove due to extraneous ignored "args" argument
AbstractQoreNode *Datasource::execRaw(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
   assert(!args);
   return exec_internal(false, query_str, 0, xsink);
}

AbstractQoreNode *Datasource::execRaw(const QoreString *query_str, ExceptionSink *xsink) {
   return exec_internal(false, query_str, 0, xsink);
}

int Datasource::beginImplicitTransaction(ExceptionSink *xsink) {
   //printd(5, "Datasource::beginImplicitTransaction() autocommit=%s\n", autocommit ? "true" : "false");
   if (priv->autocommit) {
      xsink->raiseException("AUTOCOMMIT-ERROR", "%s:%s@%s: transaction management is not available because autocommit is enabled for this Datasource", getDriverName(), priv->username.c_str(), priv->dbname.c_str());
      return -1;
   }
   return qore_dbi_private::get(*priv->dsl)->beginTransaction(this, xsink);
}

int Datasource::beginTransaction(ExceptionSink *xsink) {
   int rc = beginImplicitTransaction(xsink);
   if (!rc && !priv->in_transaction) {
      priv->in_transaction = true;
      assert(!priv->active_transaction);
   }
   return rc;
}

int Datasource::commit(ExceptionSink *xsink) {
   if (!priv->in_transaction && beginImplicitTransaction(xsink))
      return -1;

   int rc = qore_dbi_private::get(*priv->dsl)->commit(this, xsink);
   priv->in_transaction = false;
   priv->active_transaction = false;

   return rc;
}

int Datasource::rollback(ExceptionSink *xsink) {
   if (!priv->in_transaction && beginImplicitTransaction(xsink))
      return -1;

   //printd(5, "Datasource::rollback() this: %p in_transaction: %d active_transaction: %d\n", this, priv->in_transaction, priv->active_transaction);

   int rc = qore_dbi_private::get(*priv->dsl)->rollback(this, xsink);
   priv->in_transaction = false;
   priv->active_transaction = false;
   return rc;
}

int Datasource::open(ExceptionSink *xsink) {
   int rc;
   
   if (!priv->isopen) {
      // copy pending connection values to connection values
      setConnectionValues();
      
      priv->connection_aborted = false;

      rc = qore_dbi_private::get(*priv->dsl)->init(this, xsink);
      if (!*xsink) {
	 assert(priv->qorecharset);
	 priv->isopen = true;
      }
   }
   else
      rc = 0;
   
   return rc;
}

int Datasource::close() {
   if (priv->isopen) {
      qore_dbi_private::get(*priv->dsl)->close(this);
      priv->isopen = false;
      priv->in_transaction = false;
      priv->active_transaction = false;
      return 0;
   }
   return -1;
}

void Datasource::connectionAborted() {
   assert(priv->isopen);
   priv->connection_aborted = true;
   close();
}

bool Datasource::wasConnectionAborted() const {
   return priv->connection_aborted;
}

// forces a close and open to reset a database connection
void Datasource::reset(ExceptionSink *xsink) {
   if (priv->isopen) {
      // close the Datasource
      qore_dbi_private::get(*priv->dsl)->close(this);
      priv->isopen = false;
      
      // open the connection
      open(xsink);
      
      // close any open transaction(s)
      priv->in_transaction = false;
      priv->active_transaction = false;
   }
}

void *Datasource::getPrivateData() const {
   return priv->private_data;
}

void Datasource::setPrivateData(void *data) {
   priv->private_data = data;
}

void Datasource::setPendingUsername(const char *u) {
   priv->p_username = u;
}

void Datasource::setPendingPassword(const char *p) {
   priv->p_password = p;
}

void Datasource::setPendingDBName(const char *d) {
   priv->p_dbname = d;
}

void Datasource::setPendingDBEncoding(const char *c) {
   priv->p_db_encoding = c;
}

void Datasource::setPendingHostName(const char *h) {
   priv->p_hostname = h;
}

void Datasource::setPendingPort(int port) {
   priv->p_port = port;
}

const std::string &Datasource::getUsernameStr() const {
   return priv->username;
}

const std::string &Datasource::getPasswordStr() const {
   return priv->password;
}

const std::string &Datasource::getDBNameStr() const {
   return priv->dbname;
}

const std::string &Datasource::getDBEncodingStr() const {
   return priv->db_encoding;
}

const std::string &Datasource::getHostNameStr() const {
   return priv->hostname;
}

const char *Datasource::getUsername() const {
   return priv->username.empty() ? 0 : priv->username.c_str();
}

const char *Datasource::getPassword() const {
   return priv->password.empty() ? 0 : priv->password.c_str();
}

const char *Datasource::getDBName() const {
   return priv->dbname.empty() ? 0 : priv->dbname.c_str();
}

const char *Datasource::getDBEncoding() const {
   return priv->db_encoding.empty() ? 0 : priv->db_encoding.c_str();
}

const char *Datasource::getOSEncoding() const {
   return priv->qorecharset ? priv->qorecharset->getCode() : 0;
}

const char *Datasource::getHostName() const {
   return priv->hostname.empty() ? 0 : priv->hostname.c_str();
}

int Datasource::getPort() const {
   return priv->port;
}

const QoreEncoding *Datasource::getQoreEncoding() const {
   return priv->qorecharset;
}

void Datasource::setDBEncoding(const char *name) {
   priv->db_encoding = name;
}

void Datasource::setQoreEncoding(const char *name) {
   priv->qorecharset = QEM.findCreate(name);
}

void Datasource::setQoreEncoding(const QoreEncoding *enc) {
   priv->qorecharset = enc;
}

QoreStringNode *Datasource::getPendingUsername() const {
   return priv->p_username.empty() ? 0 : new QoreStringNode(priv->p_username.c_str());
}

QoreStringNode *Datasource::getPendingPassword() const {
   return priv->p_password.empty() ? 0 : new QoreStringNode(priv->p_password.c_str());
}

QoreStringNode *Datasource::getPendingDBName() const {
   return priv->p_dbname.empty() ? 0 : new QoreStringNode(priv->p_dbname.c_str());
}

QoreStringNode *Datasource::getPendingDBEncoding() const {
   return priv->p_db_encoding.empty() ? 0 : new QoreStringNode(priv->p_db_encoding.c_str());
}

QoreStringNode *Datasource::getPendingHostName() const {
   return priv->p_hostname.empty() ? 0 : new QoreStringNode(priv->p_hostname.c_str());
}

int Datasource::getPendingPort() const {
   return priv->p_port;
}

const char *Datasource::getDriverName() const {
   return priv->dsl->getName();
}

const DBIDriver *Datasource::getDriver() const {
   return priv->dsl;
}

AbstractQoreNode *Datasource::getServerVersion(ExceptionSink *xsink) {
   return qore_dbi_private::get(*priv->dsl)->getServerVersion(this, xsink);
}

AbstractQoreNode *Datasource::getClientVersion(ExceptionSink *xsink) const {
   return qore_dbi_private::get(*priv->dsl)->getClientVersion(this, xsink);
}

QoreHashNode* Datasource::getOptionHash() const {
   return priv->private_data ? qore_dbi_private::get(*priv->dsl)->getOptionHash(this) : priv->opt->hashRefSelf();
}

int Datasource::setOption(const char* opt, const AbstractQoreNode* val, ExceptionSink* xsink) {
   // maintain a copy of the option internally
   priv->setOption(opt, val, xsink);
   // only set options in private data if private data is already set
   return priv->private_data ? qore_dbi_private::get(*priv->dsl)->opt_set(this, opt, val, xsink) : 0;
}

const QoreHashNode* Datasource::getConnectOptions() const {
   return priv->opt;
}

AbstractQoreNode* Datasource::getOption(const char* opt, ExceptionSink* xsink) {
   return qore_dbi_private::get(*priv->dsl)->opt_get(this, opt, xsink);
}

QoreHashNode* Datasource::getConfigHash() const {
   QoreHashNode* h = new QoreHashNode;

   h->setKeyValue("type", new QoreStringNode(priv->dsl->getName()), 0);
   if (!priv->username.empty())
      h->setKeyValue("user", new QoreStringNode(priv->username), 0);
   if (!priv->password.empty())
      h->setKeyValue("pass", new QoreStringNode(priv->password), 0);
   if (!priv->dbname.empty())
      h->setKeyValue("db", new QoreStringNode(priv->dbname), 0);
   if (!priv->db_encoding.empty())
      h->setKeyValue("charset", new QoreStringNode(priv->db_encoding), 0);
   if (!priv->hostname.empty())
      h->setKeyValue("host", new QoreStringNode(priv->hostname), 0);
   if (priv->port)
      h->setKeyValue("port", new QoreBigIntNode(priv->port), 0);

   QoreHashNode* options = 0;

   ReferenceHolder<QoreHashNode> opts(qore_dbi_private::get(*priv->dsl)->getOptionHash(this), 0);
   ConstHashIterator hi(*opts);
   while (hi.next()) {
      const QoreHashNode* ov = reinterpret_cast<const QoreHashNode*>(hi.getValue());
      const AbstractQoreNode* v = ov->getKeyValue("value");
      if (!v || v == &False)
	 continue;

      if (!options)
	 options = new QoreHashNode;

      options->setKeyValue(hi.getKey(), v->refSelf(), 0);
   }
   if (options)
      h->setKeyValue("options", options, 0);

   return h;
}

QoreStringNode* Datasource::getConfigString() const {
   QoreStringNode* str = new QoreStringNode(priv->dsl->getName());
   str->concat(':');

   if (!priv->username.empty())
      str->concat(priv->username);
   if (!priv->password.empty())
      str->sprintf("/%s", priv->password.c_str());
   if (!priv->dbname.empty())
      str->sprintf("@%s", priv->dbname.c_str());
   if (!priv->db_encoding.empty())
      str->sprintf("(%s)", priv->db_encoding.c_str());
   if (!priv->hostname.empty())
      str->sprintf("%%%s", priv->hostname.c_str());
   if (priv->port)
      str->sprintf(":%d", priv->port);

   bool first = false;
   ReferenceHolder<QoreHashNode> opts(qore_dbi_private::get(*priv->dsl)->getOptionHash(this), 0);
   ConstHashIterator hi(*opts);
   while (hi.next()) {
      const QoreHashNode* ov = reinterpret_cast<const QoreHashNode*>(hi.getValue());
      const AbstractQoreNode* v = ov->getKeyValue("value");
      if (!v || v == &False)
	 continue;

      if (first)
	 str->concat(',');
      else {
	 str->concat('{');
	 first = true;
      }
      str->concat(hi.getKey());
      if (v == &True)
	 continue;

      QoreStringValueHelper sv(v);
      str->sprintf("=%s", sv->getBuffer());
   }
   if (first)
      str->concat('}');

   return str;
}
