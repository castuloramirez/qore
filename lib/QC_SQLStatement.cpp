/*
 QC_SQLStatement.cpp
 
 Qore Programming Language
 
 Copyright 2003 - 2010 David Nichols
 
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
#include <qore/intern/QC_SQLStatement.h>
#include <qore/intern/QC_Datasource.h>
#include <qore/intern/QC_DatasourcePool.h>

qore_classid_t CID_SQLSTATEMENT;

// SQLStatement::constructor(Datasource *ds)
static void SQLSTATEMENT_constructor_ds(QoreObject *self, const QoreListNode *args, ExceptionSink *xsink) {
   HARD_QORE_OBJ_DATA(ds, ManagedDatasource, args, 0, CID_DATASOURCE, "Datasource", "SQLStatment::constructor", xsink);
   if (*xsink)
      return;

   ReferenceHolder<ManagedDatasource> ds_holder(ds, xsink);

   if (!ds->getDriver()->hasStatementAPI()) {
      xsink->raiseException("SQLSTATEMENT-ERROR", "DBI driver '%s' does not support the prepared statement API", ds->getDriver()->getName());
      return;
   }

   ReferenceHolder<QoreSQLStatement> ss(new QoreSQLStatement, xsink);
   // FIXME: reuse reference from this call
   ss->init(ds->getReferencedHelper(*ss), xsink);
   if (*xsink)
      return;

   self->setPrivate(CID_SQLSTATEMENT, ss.release());
}

static void SQLSTATEMENT_destructor(QoreObject *self, QoreSQLStatement *stmt, ExceptionSink *xsink) {
   stmt->deref(xsink);
}

static void SQLSTATEMENT_copy(QoreObject *self, QoreObject *old, QoreSQLStatement *stmt, ExceptionSink *xsink) {
   xsink->raiseException("SQLSTATEMENT-COPY-ERROR", "SQLStatement objects cannot be copied");
   //self->setPrivate(CID_SQLSTATEMENT, new SQLStatement(*old));
}

// SQLStatement::prepare(string $sql, ...) returns nothing 
static AbstractQoreNode *SQLSTATEMENT_prepare(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   ReferenceHolder<QoreListNode> l(xsink);

   if (num_args(args) > 1)
      l = args->copyListFrom(1);

   stmt->prepare(*HARD_QORE_STRING(args, 0), *l, xsink);
   return 0;
}

// SQLStatement::prepareRaw(string $sql) returns nothing 
static AbstractQoreNode *SQLSTATEMENT_prepareRaw(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   stmt->prepareRaw(*HARD_QORE_STRING(args, 0), xsink);
   return 0;
}

// SQLStatement::bind(...) returns nothing 
static AbstractQoreNode *SQLSTATEMENT_bind(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   stmt->bind(*args, xsink);
   return 0;
}

// SQLStatement::bindPlaceholders(...) returns nothing 
static AbstractQoreNode *SQLSTATEMENT_bindPlaceholders(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   stmt->bindPlaceholders(*args, xsink);
   return 0;
}

// SQLStatement::bindValues(...) returns nothing 
static AbstractQoreNode *SQLSTATEMENT_bindValues(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   stmt->bindValues(*args, xsink);
   return 0;
}

// SQLStatement::exec() returns nothing
static AbstractQoreNode *SQLSTATEMENT_exec(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   stmt->exec(xsink);
   return 0;
}

// SQLStatement::close() returns nothing
static AbstractQoreNode *SQLSTATEMENT_close(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   stmt->close(xsink);
   return 0;
}

// SQLStatement::next() returns bool
static AbstractQoreNode *SQLSTATEMENT_next(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   bool b = stmt->next(xsink);
   return *xsink ? get_bool_node(b) : 0;
}

// SQLStatement::fetchRow() returns hash
static AbstractQoreNode *SQLSTATEMENT_fetchRow(QoreObject *self, QoreSQLStatement *stmt, const QoreListNode *args, ExceptionSink *xsink) {
   return stmt->fetchRow(xsink);
}

QoreClass *initSQLStatementClass(QoreClass *QC_Datasource, QoreClass *QC_DatasourcePool) {
   QORE_TRACE("initSQLStatementClass()");

   QoreClass *QC_SQLSTATEMENT = new QoreClass("SQLStatement", QDOM_DATABASE);
   CID_SQLSTATEMENT = QC_SQLSTATEMENT->getID();

   QC_SQLSTATEMENT->setConstructorExtended(SQLSTATEMENT_constructor_ds, false, QC_NO_FLAGS, QDOM_DATABASE, 1, QC_Datasource->getTypeInfo(), QORE_PARAM_NO_ARG);
   //QC_SQLSTATEMENT->setConstructorExtended(SQLSTATEMENT_constructor_dsp, false, QC_NO_FLAGS, QDOM_DATABASE, 1, QC_DatasourcePool->getTypeInfo(), QORE_PARAM_NO_ARG);

   QC_SQLSTATEMENT->setDestructor((q_destructor_t)SQLSTATEMENT_destructor);
   QC_SQLSTATEMENT->setCopy((q_copy_t)SQLSTATEMENT_copy);

   // SQLStatement::prepare(string $sql, ...) returns nothing 
   QC_SQLSTATEMENT->addMethodExtended("prepare",     (q_method_t)SQLSTATEMENT_prepare, false, QC_USES_EXTRA_ARGS, QDOM_DEFAULT, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   // SQLStatement::prepareRaw(string $sql) returns nothing 
   QC_SQLSTATEMENT->addMethodExtended("prepareRaw",  (q_method_t)SQLSTATEMENT_prepareRaw, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   // SQLStatement::bind(...) returns nothing
   QC_SQLSTATEMENT->addMethodExtended("bind",        (q_method_t)SQLSTATEMENT_bind, false, QC_USES_EXTRA_ARGS, QDOM_DEFAULT, nothingTypeInfo);

   // SQLStatement::bindPlaceholders(...) returns nothing
   QC_SQLSTATEMENT->addMethodExtended("bindPlaceholders", (q_method_t)SQLSTATEMENT_bindPlaceholders, false, QC_USES_EXTRA_ARGS, QDOM_DEFAULT, nothingTypeInfo);

   // SQLStatement::bindValues(...) returns nothing
   QC_SQLSTATEMENT->addMethodExtended("bindValues",  (q_method_t)SQLSTATEMENT_bindValues, false, QC_USES_EXTRA_ARGS, QDOM_DEFAULT, nothingTypeInfo);

   // SQLStatement::exec() returns nothing
   QC_SQLSTATEMENT->addMethodExtended("exec",        (q_method_t)SQLSTATEMENT_exec, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // SQLStatement::close() returns nothing
   QC_SQLSTATEMENT->addMethodExtended("close",       (q_method_t)SQLSTATEMENT_close, false, QC_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

   // SQLStatement::next() returns bool
   QC_SQLSTATEMENT->addMethodExtended("next",        (q_method_t)SQLSTATEMENT_next, false, QC_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo);

   // SQLStatement::fetchRow() returns hash
   QC_SQLSTATEMENT->addMethodExtended("fetchRow",    (q_method_t)SQLSTATEMENT_fetchRow, false, QC_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo);

   return QC_SQLSTATEMENT;
}
