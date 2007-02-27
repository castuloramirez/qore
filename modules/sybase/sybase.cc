/*
  sybase.cc

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2003, 2004, 2005, 2006

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
#include <qore/DBI.h>
#include <qore/QoreNode.h>
#include <qore/Exception.h>
#include <qore/Qore.h>
#include <qore/minitest.hpp>

#include <ctpublic.h>
#include <assert.h>
#include <ctype.h>
#include <memory>
#include <string>
#include <vector>

#include "sybase.h"
#include <qore/ScopeGuard.h>

#ifdef DEBUG
#  define private public
#endif

#ifndef QORE_MONOLITHIC
DLLEXPORT char qore_module_name[] = "sybase";
DLLEXPORT char qore_module_version[] = "1.0";
DLLEXPORT char qore_module_description[] = "Sybase database driver";
DLLEXPORT char qore_module_author[] = "Qore Technologies";
DLLEXPORT char qore_module_url[] = "http://qore.sourceforge.net";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = sybase_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = sybase_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = sybase_module_delete;
#endif

static DBIDriver* DBID_SYBASE;

// capabilities of this driver (todo - review this, copied from Oracle module)
#define DBI_SYBASE_CAPS (DBI_CAP_TRANSACTION_MANAGEMENT | DBI_CAP_CHARSET_SUPPORT | DBI_CAP_LOB_SUPPORT) 
// DBI_CAP_STORED_PROCEDURES is commented out  - this would require to recognise @ as parameter name prefix 
// and also not to execute the statement immediatelly 

//------------------------------------------------------------------------------
#ifdef DEBUG
// exported
QoreNode* runSybaseTests(QoreNode* params, ExceptionSink* xsink)
{
  minitest::result res = minitest::execute_all_tests();
  if (res.all_tests_succeeded) {
    printf("Sybase module: %d tests succeeded\n", res.sucessful_tests_count);
    return 0;
  }

  xsink->raiseException("SYBASE-TEST-FAILURE", "Sybase test in file %s, line %d threw an exception.",
    res.failed_test_file, res.failed_test_line);
  return 0;
}
#endif

static int sybase_commit(Datasource *ds, ExceptionSink *xsink);

namespace {

//------------------------------------------------------------------------------
class sybase_connection
{
private:
  CS_CONTEXT* m_context;
  CS_CONNECTION* m_connection;

  // Sybase requires callbacks
  static CS_RETCODE clientmsg_callback();
  static CS_RETCODE servermsg_callback();
  static CS_RETCODE message_callback();

public:
  sybase_connection();
  ~sybase_connection();
  void init(char* username, char* password, char* dbname, ExceptionSink* xsink);
  CS_CONNECTION* getConnection() const { return m_connection; }
};

//------------------------------------------------------------------------------
sybase_connection::sybase_connection()
: m_context(0), m_connection(0)
{
}

//------------------------------------------------------------------------------
sybase_connection::~sybase_connection()
{
  CS_RETCODE ret = CS_SUCCEED;
  if (m_connection) {
    ret = ct_close(m_connection, CS_UNUSED);
    if (ret != CS_SUCCEED) {
// commented out since it returns CS_BUSY. No idea at the time of writing what to do with it.
//      assert(false); // not much can be done here
    }
  }
  if (m_context) {
   CS_INT exit_type = ret == CS_SUCCEED ? CS_UNUSED : CS_FORCE_EXIT;
    ret = ct_exit(m_context, exit_type);
    if (ret != CS_SUCCEED) {
      assert(false); // not much can be done here
    } 
    ret = cs_ctx_drop(m_context);
    if (ret != CS_SUCCEED) {
      assert(false); // not much can be done here
    }
  }
}

//------------------------------------------------------------------------------
// Post-constructor initialization used as it fits (unfortunately) better
// with current programming model.
void sybase_connection::init(char* username, char* password, char* dbname, ExceptionSink* xsink)
{
  assert(!m_connection);
  assert(!m_context);

  CS_RETCODE ret = cs_ctx_alloc(CS_VERSION_100, &m_context);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-CANNOT-ALLOCATE-ERROR", "cs_ctx_alloc() failed with error %d", ret);
    return;
  }

  ret = ct_init(m_context, CS_VERSION_100);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-INIT-FAILED", "ct_init() failed with error %d", ret);
    return;  
  }

  // add callbacks
  ret = cs_config(m_context, CS_SET, CS_MESSAGE_CB, (CS_VOID*)message_callback, CS_UNUSED, NULL);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-SET-CALLBACK", "ct_config(CS_MESSAGE_CB) failed with error %d", ret);
    return;
  }

  ret = ct_callback(m_context, 0, CS_SET, CS_CLIENTMSG_CB, (CS_VOID*)&clientmsg_callback);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-SET-CALLBACK", "ct_callback(CS_SERVERMSG_CB) failed with error %d", ret);
    return;
  }

  ret = ct_callback(m_context, 0, CS_SET, CS_SERVERMSG_CB, (CS_VOID*)&servermsg_callback);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-SET-CALLBACK", "ct_callback(CS_SERVERMSG_CB) failed with error %d", ret);
    return;
  }

  ret = ct_con_alloc(m_context, &m_connection);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-CREATE-CONNECTION", "ct_con_alloc() failed with error %d", ret);
    return;
  }

  ret = ct_con_props(m_connection, CS_SET, CS_USERNAME, username, CS_NULLTERM, 0);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-SET-USERNAME", "ct_con_props(CS_USERNAME) failed with error %d", ret);
    return;
  }
  if (password && password[0]) {
    ret = ct_con_props(m_connection, CS_SET, CS_PASSWORD, password, CS_NULLTERM, 0);
    if (ret != CS_SUCCEED) {
      xsink->raiseException("DBI:SYBASE:CT-LIB-SET-PASSWORD", "ct_con_props(CS_PASSWORD) failed with error %d", ret);
      return;
    }
  }

  ret = ct_connect(m_connection, dbname,  strlen(dbname));
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-CONNECT", "ct_connect() failed with error %d", ret);
    return;
  }

  // transaction management is done by the driver (docs says it is by default)
  CS_BOOL chained_transactions = CS_FALSE;
  ret = ct_options(m_connection, CS_SET, CS_OPT_CHAINXACTS, &chained_transactions, CS_UNUSED, NULL);
  if (ret != CS_SUCCEED) {
    xsink->raiseException("DBI:SYBASE:CT-LIB-SET-TRANSACTION-CHAINING", "ct_options(CS_OPT_CHAINXACTS) failed with error %d", ret);
    return;
  }
}

//------------------------------------------------------------------------------
CS_RETCODE sybase_connection::message_callback()
{
  return CS_SUCCEED;
}

//------------------------------------------------------------------------------
CS_RETCODE sybase_connection::clientmsg_callback()
{
  return CS_SUCCEED;
}

//------------------------------------------------------------------------------
CS_RETCODE sybase_connection::servermsg_callback()
{
  return CS_SUCCEED;
}

//------------------------------------------------------------------------------
// from exutils.h in samples
typedef struct _ex_column_data
{
  CS_SMALLINT     indicator;
  CS_CHAR*        value;
  CS_INT          valuelen;
} EX_COLUMN_DATA;

//------------------------------------------------------------------------------
class SybaseBindGroup
{
private:
  QoreString* m_cmd; // as passed by the user
  Datasource* m_ds; // passed from upper layer
  CS_CONNECTION* m_connection; // Sybase specific
  std::vector<QoreNode*> m_input_parameters; // as provided by the user
  std::string m_command_id; // unique name for the command, generated here

  // Mostly copied from Oracle module. Also changes passed SQL command to Sybase format
  // (replacing %v with ?). Does not work with procedures. May be better rewritten.
  void parseQuery(List *args, ExceptionSink *xsink);

  // helpers for execute_command() ---------------------------------------------

  // wraps ct_dynamic(CS_PREPARE)
  CS_COMMAND* prepare_command(ExceptionSink* xsink);
  // opposite to prepare_command(). Always fails for unknown reason
  // and commented out. Even than the app does not leak (tested).
  static void deallocate_prepared_statement(CS_COMMAND* cmd, char* id);

  // description of both input parameters and output data
  typedef struct column_info_t {
    column_info_t(const std::string& n, unsigned t, unsigned s) : m_column_name(n), m_column_type(t), m_max_size(s) {}
    std::string m_column_name;
    unsigned m_column_type; // CS_..._TYPE constants
    unsigned m_max_size;
  };

  // wraps ct_dynamic(CS_DESCRIBE_INPUT)
  std::vector<column_info_t> extract_input_parameters_info(CS_COMMAND* cmd, ExceptionSink* xsink);
  bool does_command_return_data() const; // equivalent to: is it SQL select?
  // wraps ct_dynamic(CS_DESCRIBE_OUTPUT). The function may be redundant - needs investigation.  
  std::vector<column_info_t> extract_output_parameters_info(CS_COMMAND* cmd, ExceptionSink* xsink);
  // wraps ct_param(). Uses m_input_parameters to get data from input 'QoreNode's 
  void bind_input_parameters(CS_COMMAND* cmd, const std::vector<column_info_t>& params, ExceptionSink* xsink);
  // wraps ct_results(), returns Hash (1 row) or List (more rows) 
  QoreNode* read_output(CS_COMMAND* cmd, const std::vector<column_info_t>& out_info, ExceptionSink* xsink);

  // helper for read_output(), adds just read single row into 'out'
  void read_row(CS_COMMAND* cmd, const std::vector<column_info_t>& out_info, QoreNode*& out, ExceptionSink* xsink);
  // converts Sybase data to QoreNode
  void extract_row_data_to_Hash(Hash* out, CS_INT col_index, CS_DATAFMT* datafmt, EX_COLUMN_DATA* coldata, 
    const column_info_t& out_info, ExceptionSink* xsink);

  // the main functionality of this class is here: executes m_cmd using m_input_parameters
  // and returns data converted into Qore nodes
  QoreNode* execute_command(ExceptionSink* xsink);

#ifdef DEBUG
  // needed to be able to create independent tests. The whole architecture woul be better changed
  // to support testing.
  SybaseBindGroup(QoreString* ostr);
#endif
public:
  SybaseBindGroup(Datasource* ds, QoreString* ostr, List *args, ExceptionSink *xsink); 
  ~SybaseBindGroup();

  QoreNode* exec(class ExceptionSink *xsink);
  QoreNode* select(class ExceptionSink *xsink);
  QoreNode* selectRows(class ExceptionSink *xsink);
};

//------------------------------------------------------------------------------
#ifdef DEBUG
SybaseBindGroup::SybaseBindGroup(QoreString* ostr)
: m_cmd(0),
  m_ds(0),
  m_connection(0)
{
  m_cmd = new QoreString(ostr->getBuffer());
}
#endif

//------------------------------------------------------------------------------
SybaseBindGroup::SybaseBindGroup(Datasource* ds, QoreString* ostr, List *args, ExceptionSink *xsink) 
: m_cmd(0),
  m_ds(ds),
  m_connection(0)
{
  m_cmd = ostr->convertEncoding(ds->getQoreEncoding(), xsink);
  if (xsink->isEvent()) {
    return;
  } 
  sybase_connection* sc = (sybase_connection*)ds->getPrivateData();
  m_connection = sc->getConnection();

  // process query string and setup bind value list
  parseQuery(args, xsink);
  if (xsink->isEvent()) {
    return;
  }
}

//------------------------------------------------------------------------------
SybaseBindGroup::~SybaseBindGroup()
{
  delete m_cmd;
}

//------------------------------------------------------------------------------
void SybaseBindGroup::parseQuery(List* args, ExceptionSink* xsink)
{
  // code copied from Oracle module, not knowing better way
  if (args) {
    m_input_parameters.reserve(args->size());
  }
  char quote = 0;
  char *p = m_cmd->getBuffer();
  while (*p)
  {
    if (!quote && (*p) == '%') // found value marker
    {
      p++;
      if ((*p) != 'v')
      {
        xsink->raiseException("DBI-EXEC-PARSE-EXCEPTION", "invalid value specification (expecting '%v', got %%%c)", *p);
        break;
      }
      p++;
      if (isalpha(*p))
      {
        xsink->raiseException("DBI-EXEC-PARSE-EXCEPTION", "invalid value specification (expecting '%v', got %%v%c*)", *p);
        break;
      }
      if (!args || args->size() <= (int)m_input_parameters.size())
      {
        xsink->raiseException("DBI-EXEC-PARSE-EXCEPTION", "too few arguments passed (%d) for value expression (%d)",
          args ? args->size() : 0, m_input_parameters.size() + 1);
        break;
      }
      QoreNode *v = args->retrieve_entry(m_input_parameters.size());

      // replace value marker with ? expected by ct_dynamic()
      QoreString tn(" ?");
      int offset = p - m_cmd->getBuffer() - 2;
      m_cmd->replace(offset, 2, &tn);
      p = m_cmd->getBuffer() + offset + tn.strlen();

      m_input_parameters.push_back(v);
    }
    else if (((*p) == '\'') || ((*p) == '\"'))
    {
      if (!quote)
        quote = *p;
      else if (quote == (*p))
        quote = 0;
      p++;
    }
    else
      p++;
   }
}

//------------------------------------------------------------------------------
CS_COMMAND* SybaseBindGroup::prepare_command(ExceptionSink* xsink)
{
  CS_COMMAND* result = 0;
  CS_RETCODE err = ct_cmd_alloc(m_connection, &result);
  if (err != CS_SUCCEED) {
printf("#### err1 %p\n", m_connection);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_cmd_alloc() failed with error %d", (int)err);
    return 0;
  }
  ScopeGuard g = MakeGuard(ct_cmd_drop, result);

  // try to make unique name, as much unique as possible
  static unsigned counter = 0;
  ++counter;
  char aux[30];
  sprintf(aux, "my_cmd_%u_%u", (unsigned)pthread_self(), counter);
  m_command_id = aux;

  err = ct_dynamic(result, CS_PREPARE, (CS_CHAR*)m_command_id.c_str(), CS_NULLTERM, m_cmd->getBuffer(), CS_NULLTERM);
  if (err != CS_SUCCEED) {
printf("### err2\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_dynamic(CS_PREPARE, \"%s\") failed with error %d", m_cmd->getBuffer(), (int)err);
    return 0;
  }
  
  err = ct_send(result);
  if (err != CS_SUCCEED) {
printf("### err3\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_send() for \"%s\" failed with error %d", m_cmd->getBuffer(), (int)err);
    return 0;
  }

  // no results expected
  CS_INT result_type;
  err = ct_results(result, &result_type);
  if (err != CS_SUCCEED) {
printf("### err 4\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_result() failed with error %d", (int)err);
    return 0;
  }
  if (result_type != CS_CMD_SUCCEED) {
printf("### err 5 (errcode = %d, result type = %d, expected %d): cmd = %s\n", (int)err, (int)result_type, (int)CS_CMD_FAIL, m_cmd->getBuffer());
    assert(result_type == CS_CMD_FAIL);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() ct_dynamic(CS_PREPARE) failed with error %d", (int)err);
    return 0;
  }
  while((err = ct_results(result, &result_type)) == CS_SUCCEED);

  g.Dismiss();
  return result;
}

//------------------------------------------------------------------------------
void SybaseBindGroup::deallocate_prepared_statement(CS_COMMAND* cmd, char* id)
{
  CS_RETCODE err = ct_dynamic(cmd, CS_DEALLOC, id, CS_NULLTERM, 0, CS_UNUSED);
//###  assert(err == CS_SUCCEED);
  err = ct_send(cmd);
//###  assert(err == CS_SUCCEED);
  CS_INT result_type;
  while ((err = ct_results(cmd, &result_type)) == CS_SUCCEED);
}

//------------------------------------------------------------------------------
std::vector<SybaseBindGroup::column_info_t> SybaseBindGroup::extract_input_parameters_info(CS_COMMAND* cmd, ExceptionSink* xsink)
{
  std::vector<column_info_t> empty;
  assert(cmd);
  CS_RETCODE err = ct_dynamic(cmd, CS_DESCRIBE_INPUT, (CS_CHAR*)m_command_id.c_str(), CS_NULLTERM, 0, CS_UNUSED);
  if (err != CS_SUCCEED) {
printf("### err10\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_dynamic(CS_DESCRIBE_INPUT) failed with error %d", (int)err);
    return empty;
  }

  err = ct_send(cmd);
  if (err != CS_SUCCEED) {
printf("### err20\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_send() failed with error %d", (int)err);
    return empty;
  }

  std::vector<column_info_t> result;

  CS_INT result_type;
  while ((err = ct_results(cmd, &result_type)) == CS_SUCCEED) {
    if (result_type != CS_DESCRIBE_RESULT) continue;
    CS_INT numparam = 0;
    CS_INT len;
    err = ct_res_info(cmd, CS_NUMDATA, &numparam, CS_UNUSED, &len);
    if (err != CS_SUCCEED) {
printf("### err33\n");
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call to ct_res_info(CS_DESCRIBE_RESULT) failed with error %d", (int)err);
      return empty;
    }
    result.reserve(numparam);

    CS_DATAFMT datafmt;
    for (CS_INT i = 1; i <= numparam; ++i) { 
      err = ct_describe(cmd, i, &datafmt);
      if (err != CS_SUCCEED) {
printf("### err 44\n");
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call to ct_describe(%d) failed with error %d", i, (int)err);
        return empty;
      }
      std::string name;
      if (datafmt.name[0]) {
        name = datafmt.name;
      } else {
        name = "<unnamed>";
      }
      result.push_back(column_info_t(name, datafmt.datatype, datafmt.maxlength));
    }
  }
  return result;
}

//------------------------------------------------------------------------------
// Checks if it is select command. May should handle stored procedures?
bool SybaseBindGroup::does_command_return_data() const
{
  char* p = m_cmd->getBuffer();
  if (!p) return false;
  while (*p && isspace(*p)) ++p;
  if (!*p) return false;
  if (strncasecmp(p, "select", 6) == 0) return true;
  return false;
}

//------------------------------------------------------------------------------
std::vector<SybaseBindGroup::column_info_t> SybaseBindGroup::extract_output_parameters_info(CS_COMMAND* cmd, ExceptionSink* xsink)
{
  std::vector<column_info_t> empty;
  assert(cmd);
  CS_RETCODE err = ct_dynamic(cmd, CS_DESCRIBE_OUTPUT, (CS_CHAR*)m_command_id.c_str(), CS_NULLTERM, 0, CS_UNUSED);
  if (err != CS_SUCCEED) {
printf("### err10\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_dynamic(CS_DESCRIBE_OUTPUT) failed with error %d", (int)err);
    return empty;
  }

  err = ct_send(cmd);
  if (err != CS_SUCCEED) {
printf("### err20\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_send() failed with error %d", (int)err);
    return empty;
  }

  std::vector<column_info_t> result;

  CS_INT result_type;
  while ((err = ct_results(cmd, &result_type)) == CS_SUCCEED) {
    if (result_type != CS_DESCRIBE_RESULT) continue;
    CS_INT numparam = 0;
    CS_INT len;
    err = ct_res_info(cmd, CS_NUMDATA, &numparam, CS_UNUSED, &len);
    if (err != CS_SUCCEED) {
printf("### err33\n");
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call to ct_res_info(CS_DESCRIBE_RESULT) failed with error %d", (int)err);
      return empty;
    }
    result.reserve(numparam);

    CS_DATAFMT datafmt;
    for (CS_INT i = 1; i <= numparam; ++i) {
      err = ct_describe(cmd, i, &datafmt);
      if (err != CS_SUCCEED) {
printf("### err 44\n");
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call to ct_describe(%d) failed with error %d", i, (int)err);
        return empty;
      }
      std::string name;
      if (datafmt.name[0]) {
        name = datafmt.name;
      } else {
        name = "<unnamed>";
      }
      result.push_back(column_info_t(name, datafmt.datatype, datafmt.maxlength));
    }
  }
  return result;
}

//------------------------------------------------------------------------------
void SybaseBindGroup::bind_input_parameters(CS_COMMAND* cmd, const std::vector<column_info_t>& param_info, ExceptionSink* xsink)
{
  if (param_info.size() != m_input_parameters.size()) {
printf("### err x4\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Number of expected parameters: %u, available parameters %u", 
      param_info.size(), m_input_parameters.size());
    return;
  }

  CS_RETCODE err = ct_dynamic(cmd, CS_EXECUTE, (CS_CHAR*)m_command_id.c_str(), CS_NULLTERM, 0, CS_UNUSED);
  if (err != CS_SUCCEED) {
printf("### failed to execute\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_dynamic(CS_EXECUTE, \"%s\") failed with error %d", m_cmd->getBuffer(), (int)err);
    return;
  }

  for (unsigned i = 0, n = param_info.size(); i != n; ++i) {
    QoreNode* n = m_input_parameters[i];
    const char* param_name = param_info[i].m_column_name.c_str();

    CS_DATAFMT datafmt;
    memset(&datafmt, 0, sizeof(datafmt));
    datafmt.status = CS_INPUTVALUE;
    datafmt.namelen = CS_NULLTERM;
    datafmt.maxlength = CS_UNUSED;
    datafmt.count = 1;   

    if (is_null(n)) {
      err = ct_param(cmd, &datafmt, 0, CS_UNUSED, -1);
      if (err != CS_SUCCEED) {
printf("### err x5\n");
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_param(NULL) failed for parameter #%u (%s) with error", i + 1, param_name, (int)err);
        return;
      }
      continue;
    }

    switch (param_info[i].m_column_type) {
    case CS_VARCHAR_TYPE:
    case CS_LONGCHAR_TYPE:
    case CS_CHAR_TYPE: // all types are almost equivalent
    {
      if (n->type != NT_STRING) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for string parameter #%u (%s)", i + 1, param_name);
        return;
      }
      QoreString* aux = n->val.String;
      // TBD - convert
      char* s = "";
      if (aux && aux->getBuffer()) {
        s = aux->getBuffer();
printf("#### VALUE = %s\n", s);
      }
printf("########### strlen = %d\n", strlen(s));
if (s == 0) printf("### IS NULL\n"); else printf("#### NOT NULL\n");
      datafmt.datatype = param_info[i].m_column_type;
      datafmt.format = CS_FMT_NULLTERM;
      datafmt.maxlength = CS_MAX_CHAR;

      err = ct_param(cmd, &datafmt, s, strlen(s), 0);
printf("#### after ct_param()\n");
      if (err != CS_SUCCEED) {
printf("####### ?????\n");
printf("### err9373 adding [%s]\n", s);
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for string parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }

    case CS_BINARY_TYPE:
      // TBD
      assert(false);
    case CS_LONGBINARY_TYPE:
      // TBD
      assert(false);
    case CS_TEXT_TYPE:
      // TBD
      assert(false);
    case CS_IMAGE_TYPE:
      // TBD
      assert(false);
    case CS_TINYINT_TYPE:
    {
      if (n->type != NT_INT) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u (%s)", i + 1, param_name);
        return;
      }
      if (n->val.intval < 128 || n->val.intval >= 128) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Integer value (%d parameter) is out of range for Sybase datatype", i + 1
);
        return;
      }

      CS_TINYINT val = n->val.intval;
      datafmt.datatype = CS_TINYINT_TYPE;
      err = ct_param(cmd, &datafmt, &val, sizeof(val), 0);
      if (err != CS_SUCCEED) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }

    case CS_SMALLINT_TYPE:
    {
      if (n->type != NT_INT) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u (%s)", i + 1, param_name);
        return;
      }
      if (n->val.intval < 32 * 1024 || n->val.intval >= 32 * 1024) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Integer value (%d parameter) is out of range for Sybase datatype", i + 1);
        return;
      }

      CS_SMALLINT val = n->val.intval;
      datafmt.datatype = CS_SMALLINT_TYPE;
      err = ct_param(cmd, &datafmt, &val, sizeof(val), 0);
      if (err != CS_SUCCEED) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }

    case CS_INT_TYPE:
    {
      if (n->type != NT_INT) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u (%s)", i + 1, param_name);
        return;
      }
      CS_INT val = n->val.intval;
      datafmt.datatype = CS_INT_TYPE;      
      err = ct_param(cmd, &datafmt, &val, sizeof(val), 0);
      if (err != CS_SUCCEED) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }
    case CS_REAL_TYPE:
    {
      if (n->type != NT_FLOAT) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for float parameter #%u (%s)", i + 1, param_name);
        return;
      }
      CS_REAL val = n->val.floatval;
      datafmt.datatype = CS_REAL_TYPE;
      err = ct_param(cmd, &datafmt, &val, sizeof(val), 0);
      if (err != CS_SUCCEED) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for float parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }

    case CS_FLOAT_TYPE:
    {
      if (n->type != NT_FLOAT) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for float parameter #%u (%s)", i + 1, param_name);
        return;
      }
      CS_FLOAT val = n->val.floatval;
      datafmt.datatype = CS_FLOAT_TYPE;
      err = ct_param(cmd, &datafmt, &val, sizeof(val), 0);
      if (err != CS_SUCCEED) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for float parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }

    case CS_BIT_TYPE:
    {
      if (n->type != NT_BOOLEAN) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for boolean parameter #%u (%s)", i + 1, param_name);
        return;
      }
      CS_BIT val = n->val.boolval ? 1 : 0;
      datafmt.datatype = CS_BIT_TYPE;
      err = ct_param(cmd, &datafmt, &val, sizeof(val), 0);
      if (err != CS_SUCCEED) {
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for bool parameter #%u (%s) failed with error", i + 1, param_name, (int)err);
        return;
      }
      break;
    }

    case CS_DATETIME_TYPE:
      // TBD
      assert(false);
    case CS_DATETIME4_TYPE:
      // TBD
      assert(false);
    case CS_MONEY_TYPE:
    case CS_MONEY4_TYPE:
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Setting Money Sybase datatype is currently not supported");
      return;

    case CS_NUMERIC_TYPE:
      // TBD
      assert(false);
    case CS_DECIMAL_TYPE:
      // TBD
      assert(false);

    case CS_VARBINARY_TYPE:
      assert(false);
      // TBD - deal with all the types
      break;
    default:
printf("### err x1\n");
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Unrecognized type %d of Sybase parameter # %u", 
        (int)param_info[i].m_column_type, i + 1);
      return;
    }
  }
printf("#### before send\n");

  err = ct_send(cmd);
  if (err != CS_SUCCEED) {
printf("### err 876\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_send() failed with error %d", (int)err);
    return;
  }
printf("### after send\n");
}

//------------------------------------------------------------------------------
QoreNode* SybaseBindGroup::read_output(CS_COMMAND* cmd, const std::vector<column_info_t>& out_info, ExceptionSink* xsink)
{
  QoreNode* result = 0;
  CS_RETCODE err;
  CS_INT result_type = 0; 

  while ((err = ct_results(cmd, &result_type)) == CS_SUCCEED) {    
    switch (result_type) {
    case CS_COMPUTE_RESULT:
      // single row
      // TBD
      assert(false);
      break;
    case CS_CURSOR_RESULT:
      // Sybase bug??? This code does not use cursors.
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() failed with result code CS_CURSOR_RESULT");
      return result;
    case CS_PARAM_RESULT:
      // single row
      // TBD
      assert(false);
      break;
    case CS_ROW_RESULT:
      // 0 or more rows
printf("#### A ROW RETURNED\n");
      read_row(cmd, out_info, result, xsink);
      if (xsink->isException()) {
        return result;
      }
      break;
    case CS_STATUS_RESULT:
      // single value
      if (result) { // cannot happen?
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() already read CS_STATUC_RESULT");
        return result;
      }
      if (out_info.size() != 1) { // Sybase bug??? Description should be for this single value.
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() returned unexpected single value");
        return result;
      }
      read_row(cmd, out_info, result, xsink);
      if (xsink->isException()) {
        return result;
      }
      break;
    case CS_COMPUTEFMT_RESULT:
      // Sybase bug???
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() failed with result code CS_COMPUTE_FMT_RESULT");
      return result;
    case CS_MSG_RESULT:
      // Sybase bug???
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() failed with result code CS_MSG_RESULT");
      return result;
    case CS_ROWFMT_RESULT:
      // Sybase bug???
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() failed with result code CS_ROW_FMT_RESULT");
      return result;      
    case CS_DESCRIBE_RESULT:
      // Sybase bug?
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() failed with result code CS_DESCRIBE_RESULTS");
      return result;
    case CS_CMD_DONE:
      // e.g. update, ct_res_info() could be used to get # of affected rows
      return result;
    case CS_CMD_FAIL:
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() failed with result code CS_CMD_FAIL");
      return result;
    case CS_CMD_SUCCEED: // no data returned
      if (!out_info.empty()) {
        // Sybase bug???
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() returns no data where expected");
      }
      return result;
    default:
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() gave unknown result type %d", (int)result_type);
      return result;
    }
  }

  if (err != CS_END_RESULTS) {
printf("### err in CS_END_RESULTS, result\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() finished with unexpected result code %d", (int)err);
  }

  return  result;
}

//------------------------------------------------------------------------------
// helper to free allocated data
static void free_coldata(EX_COLUMN_DATA* coldata, CS_INT cnt)
{
  for (CS_INT i = 0; i < cnt; ++i) {
    if (coldata[i].value) {
      free(coldata[i].value);
    }
  }
}

//------------------------------------------------------------------------------
void SybaseBindGroup::read_row(CS_COMMAND* cmd, const std::vector<column_info_t>& out_info, QoreNode*& out, ExceptionSink* xsink)
{
  CS_INT num_cols;
  CS_RETCODE err = ct_res_info(cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
  if (err != CS_SUCCEED) {
printf("### err line %d\n", __LINE__);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_res_info() failed to get number of rows, error %d", (int)err);
    return;
  }
printf("#### READ ROW HAS %d columns\n", num_cols);
  if (num_cols <= 0) {
    assert(false); // cannot happen
printf("### err line %d\n", __LINE__);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Internal error: no columns returned");
    return;
  }

  if (num_cols != (CS_INT)out_info.size()) {
    assert(false); // cannot happen
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Internal error: different sizes of output row received");
    return;
  }

  EX_COLUMN_DATA* coldata = (EX_COLUMN_DATA *)malloc(num_cols * sizeof (EX_COLUMN_DATA));
  if (!coldata) {
    xsink->outOfMemory();
    return;
  }
  ON_BLOCK_EXIT(free, coldata);
  memset(coldata, 0, num_cols * sizeof(EX_COLUMN_DATA));
  ON_BLOCK_EXIT(free_coldata, coldata, num_cols);

  CS_DATAFMT* datafmt = (CS_DATAFMT *)malloc(num_cols * sizeof (CS_DATAFMT));
  if (!datafmt) {
printf("### erro 111\n");
    xsink->outOfMemory();
    return;
  }
  ON_BLOCK_EXIT(free, datafmt);
  memset(datafmt, 0, num_cols * sizeof(CS_DATAFMT));

  for (CS_INT i = 0; i < num_cols; ++i) {
printf("### reading coilumn #%d\n", i + 1);
    err = ct_describe(cmd, i + 1, &datafmt[i]);
    if (err != CS_SUCCEED) {
printf("### err line %d\n", __LINE__);
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_describe() failed with error %d", (int)err);
      return;
    }
    datafmt[i].count = 1; // fetch just single item
    assert(datafmt[i].maxlength < 100000); // guess, if invalid then app semnatic is wrong (I assume the value is actual data length)

printf("### column length = %d\n", datafmt[i].maxlength);
    coldata[i].value = (CS_CHAR*)malloc(datafmt[i].maxlength + 4); // some padding for zero terminator, 4 is safe bet
    datafmt[i].maxlength += 4;
    if (!coldata[i].value) {
printf("### err line %d\n", __LINE__);
      xsink->outOfMemory();
      return;    
    }
    // TBD - handle text, image, varchar

    err = ct_bind(cmd, i + 1, &datafmt[i], coldata[i].value, &coldata[i].valuelen, &coldata[i].indicator);
    if (err != CS_SUCCEED) {
printf("### err line %d\n", __LINE__);
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_bind() failed with error %d", (int)err);
      return;
    }
  }

printf("### before fetching\n");
  CS_INT rows_read = 0;
  while ((err = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &rows_read)) == CS_SUCCEED) {
printf("### a value fetched\n");
    // process the row
    Hash* h = new Hash;

    for (CS_INT j = 0; j < num_cols; ++j) {
      extract_row_data_to_Hash(h, j, &datafmt[j], &coldata[j], out_info[j], xsink);
      if (xsink->isException()) {
printf("#### extract_row_data_to_Hash failed\n");
        QoreNode* aux = new QoreNode(h);
        aux->deref(xsink);
        return;
      }
    }

    if (out == 0) {
printf("### new note allocated\n");
      out = new QoreNode(h);
    } else
    if (out->type == NT_HASH) {
printf("### returning list\n");
      // convert to list
      List* l = new List;
      l->push(out);
      l->push(new QoreNode(h));
      out = new QoreNode(l);
    } else {
printf("### retunring hash\n");
      assert(out->type == NT_LIST);
      out->val.list->push(new QoreNode(h));
    }
  }
printf("### fetching done\n");
  if (err != CS_END_DATA) {
    assert(false);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_fetch() returned erro %d", (int)err);
    return;
  }
}

//------------------------------------------------------------------------------
void SybaseBindGroup::extract_row_data_to_Hash(Hash* out, CS_INT col_index, CS_DATAFMT* datafmt, EX_COLUMN_DATA* coldata,
    const column_info_t& out_info, ExceptionSink* xsink)
{
  std::string column_name;
  if (datafmt->name && datafmt->name[0]) {
    column_name = datafmt->name;
  } else
  if (out_info.m_column_name[0] != '<') { // <unnamed>
    column_name = out_info.m_column_name;
  } else {
    char buffer[20];
    sprintf(buffer, "column%d", (int)(col_index + 1));
    column_name = buffer;
  }
  // TBD - convert column name by encoding?
  std::auto_ptr<QoreString> key(new QoreString((char*)column_name.c_str()));
  QoreNode* v = 0;

  if (coldata->indicator == -1) { // NULL
    out->setKeyValue(key.get(), new QoreNode(NT_NULL), xsink);
    return;
  }
 
  assert(datafmt->datatype == (CS_INT)out_info.m_column_type);
  switch (datafmt->datatype) {
  case CS_CHAR_TYPE: // varchar
  {
    CS_CHAR* value = (CS_CHAR*)(coldata->value);
    // TBD - conversion
    QoreString* s = new QoreString(value);
    v = new QoreNode(s);
    break;
  }
  case CS_BINARY_TYPE:
  case CS_LONGCHAR_TYPE:
  case CS_LONGBINARY_TYPE:
  case CS_TEXT_TYPE:
  case CS_IMAGE_TYPE:
    // TBD
    assert(false);
  case CS_TINYINT_TYPE:
  {
    CS_TINYINT* value = (CS_TINYINT*)(coldata->value);
    v = new QoreNode((int64)*value);
    break;
  }
  case CS_SMALLINT_TYPE:
  {
    CS_SMALLINT* value = (CS_SMALLINT*)(coldata->value);
    v = new QoreNode((int64)*value);
    break;
  }
  case CS_INT_TYPE:
  {
printf("#### extractyed INT\n");
    CS_INT* value = (CS_INT*)(coldata->value);
    v = new QoreNode((int64)*value);
    break;
  }
  case CS_REAL_TYPE:
  {
    CS_REAL* value = (CS_REAL*)(coldata->value);
    v = new QoreNode((double)*value);
    break;
  }
  case CS_FLOAT_TYPE:
  {
    CS_FLOAT* value = (CS_FLOAT*)(coldata->value);
    v = new QoreNode((double)*value);
    break;
  }
  case CS_BIT_TYPE:
  {
    CS_BIT* value = (CS_BIT*)(coldata->value);
    v = new QoreNode(*value != 0);
    break;
  }
  case CS_DATETIME_TYPE:
  case CS_DATETIME4_TYPE:
  case CS_MONEY_TYPE:
  case CS_MONEY4_TYPE:
  case CS_NUMERIC_TYPE:
  case CS_DECIMAL_TYPE:
  case CS_VARCHAR_TYPE:
  case CS_VARBINARY_TYPE:
    assert(false);
    // TBD - deal with all the types
    break;
  default:
    assert(false);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Unknown data type %d", (int)datafmt->datatype);
    return;
  } 
  assert(out);
  out->setKeyValue(key.get(), v, xsink);
}

//------------------------------------------------------------------------------
QoreNode* SybaseBindGroup::execute_command(ExceptionSink* xsink)
{
  // TBD - for not select use the immediate execute (drop returns 1 value! though)

  CS_COMMAND* cmd = prepare_command(xsink);
  if (xsink->isException()) {
printf("### err A\n");
    return 0;
  }  
  ON_BLOCK_EXIT(ct_cmd_drop, cmd);
  ScopeGuard cancel_guard = MakeGuard(ct_cancel, (CS_CONNECTION*)0, cmd, CS_CANCEL_ALL);
  ON_BLOCK_EXIT(&SybaseBindGroup::deallocate_prepared_statement, cmd, (char*)m_command_id.c_str());

  std::vector<column_info_t> inputs = extract_input_parameters_info(cmd, xsink);
  if (xsink->isException()) {
printf("### err B\n");
    return 0;
  }
  std::vector<column_info_t> outputs;
  if (does_command_return_data()) {
printf("### command has ouput data\n");
    outputs = extract_output_parameters_info(cmd, xsink);
    if (xsink->isException()) {
      return 0;
    }
  }
  bind_input_parameters(cmd, inputs, xsink);
  if (xsink->isException()) {
printf("#### AAA\n");
    return 0;
  }  
printf("### before read output\n");

  QoreNode* res = read_output(cmd, outputs, xsink);
printf("### after read output\n");
  if (!xsink->isException()) {
printf("#### cancelling\n");
    cancel_guard.Dismiss();
  } else {
printf("### derefing\n");
    if (res) {
      res->deref(xsink);
      res = 0;
    }
  }
printf("### execute_command() returned\n");
  return res;
}

//------------------------------------------------------------------------------
QoreNode* SybaseBindGroup::exec(class ExceptionSink *xsink)
{
  QoreNode* n = execute_command(xsink);
  if (n) n->deref(xsink); // not needed
  if (xsink->isException()) {
    return 0;
  }
  if (m_ds->getAutoCommit()) {
    sybase_commit(m_ds, xsink);
  }
  return 0;
}

//------------------------------------------------------------------------------
QoreNode* SybaseBindGroup::select(class ExceptionSink *xsink)
{
  QoreNode* n = execute_command(xsink);
  if (xsink->isException()) {
    if (n) n->deref(xsink);
    return 0;
  }
  if (n) {
    if (n->type == NT_LIST) {
      n->deref(xsink);
      xsink->raiseException("DBI-EXEC-EXCEPTION", "SQL command returned more than one row");
      return 0;
    }
    if (n->type != NT_HASH) {
      n->deref(xsink);
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Internal error - unexpected type returned");
      return 0;
    }
  }

  return n;
}

//------------------------------------------------------------------------------
QoreNode* SybaseBindGroup::selectRows(class ExceptionSink *xsink)
{
  QoreNode* n = execute_command(xsink);
  if (xsink->isException()) {
    if (n) n->deref(xsink);
    return 0;
  }
  if (n) {
    if (n->type != NT_LIST) {
      if (n->type != NT_HASH) {
        n->deref(xsink);
        xsink->raiseException("DBI-EXEC-EXCEPTION", "Internal error - unexpected type returned");
        return 0;
      }
    }
  }
  return n;
}

} // anonymous namespace

//------------------------------------------------------------------------------
static int sybase_open(Datasource *ds, ExceptionSink *xsink)
{
  tracein("sybase_open()");

  if (!ds->getUsername()) {
    xsink->raiseException("DATASOURCE-MISSING-USERNAME", "Datasource has an empty username parameter");
    traceout("oracle_open()");
    return -1;
  }
  if (!ds->getPassword()) {
    xsink->raiseException("DATASOURCE-MISSING-PASSWORD", "Datasource has an empty password parameter");
    traceout("oracle_open()");
    return -1;
  }
  if (!ds->getDBName()) {
    xsink->raiseException("DATASOURCE-MISSING-DBNAME", "Datasource has an empty dbname parameter");
    traceout("oracle_open()");
    return -1;
  }

  std::auto_ptr<sybase_connection> sc(new sybase_connection);
  sc->init(ds->getUsername(), ds->getPassword(), ds->getDBName(), xsink);
  if (xsink->isException()) {
    return -1;  
  }
  ds->setPrivateData(sc.release());

  // TBD - something about string encoding should be here

  traceout("sybase_open()");
  return 0;
}

//------------------------------------------------------------------------------
static int sybase_close(Datasource *ds)
{
  tracein("sybase_close()");
  sybase_connection* sc = (sybase_connection*)ds->getPrivateData();
  ds->setPrivateData(0);
  delete sc;
  traceout("sybase_close()");
  return 0;
}

//------------------------------------------------------------------------------
static QoreNode* sybase_select(Datasource *ds, QoreString *qstr, List *args, ExceptionSink *xsink)
{
  SybaseBindGroup grp(ds, qstr, args, xsink);
  if (xsink->isException()) {
    return 0;
  }
  return grp.select(xsink);
}

//------------------------------------------------------------------------------
static QoreNode* sybase_select_rows(Datasource *ds, QoreString *qstr, List *args, ExceptionSink *xsink)
{
  SybaseBindGroup grp(ds, qstr, args, xsink);
  if (xsink->isException()) {
    return 0;
  }
  return grp.selectRows(xsink);
}

//------------------------------------------------------------------------------
static QoreNode* sybase_exec(Datasource *ds, QoreString *qstr, List *args, ExceptionSink *xsink)
{
  SybaseBindGroup grp(ds, qstr, args, xsink);
  if (xsink->isException()) {
    return 0;
  }
  return grp.exec(xsink);
}

//------------------------------------------------------------------------------
// Locally debugable function
static int sybase_commit_impl(sybase_connection* sc, ExceptionSink* xsink)
{
  CS_COMMAND* cmd = 0;
  CS_RETCODE err = ct_cmd_alloc(sc->getConnection(), &cmd);
  if (err != CS_SUCCEED) {
printf("### commit failure1\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_cmd_alloc() failed with error %d", (int)err);
    return 0;
  }
  ON_BLOCK_EXIT(ct_cmd_drop, cmd);

  err = ct_dynamic(cmd, CS_EXEC_IMMEDIATE, 0, CS_UNUSED, "commit transaction", CS_NULLTERM);
  if (err != CS_SUCCEED) {
printf("### commit failure2\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_dynamic(\"commit transaction\") failed with error %d", (int)err);
    return 0;
  }
  err = ct_send(cmd);
  if (err != CS_SUCCEED) {
printf("### commit failure3\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_send() failed with error %d", (int)err);
    return 0;
  }

  // no results expected
  CS_INT result_type;
  err = ct_results(cmd, &result_type);
  if (err != CS_SUCCEED) {
printf("### commit failure4\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_result() failed with error %d", (int)err);
    return 0;
  }
  if (result_type != CS_CMD_SUCCEED) {
    assert(result_type == CS_CMD_FAIL);
printf("### commit failure5\n");
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() for \"commit transaction\" failed with error %d",
 (int)err);
    return 0;
  }
  while((err = ct_results(cmd, &result_type)) == CS_SUCCEED);

  return 1;
}

//------------------------------------------------------------------------------
static int sybase_commit(Datasource *ds, ExceptionSink *xsink)
{
  sybase_connection* sc = (sybase_connection*)ds->getPrivateData();
  return sybase_commit_impl(sc, xsink);
}

//------------------------------------------------------------------------------
// Locally debugeable function
static int sybase_rollback_impl(sybase_connection* sc, ExceptionSink* xsink)
{
  CS_COMMAND* cmd = 0;
  CS_RETCODE err = ct_cmd_alloc(sc->getConnection(), &cmd);
  if (err != CS_SUCCEED) {
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_cmd_alloc() failed with error %d", (int)err);
    return 0;
  }
  ON_BLOCK_EXIT(ct_cmd_drop, cmd);

  err = ct_dynamic(cmd, CS_EXEC_IMMEDIATE, 0, CS_UNUSED, "rollback transaction", CS_NULLTERM);
  if (err != CS_SUCCEED) {
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_dynamic(\"rollback transaction\") failed with error %d", (int)err);
    return 0;
  }
  err = ct_send(cmd);
  if (err != CS_SUCCEED) {
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_send() failed with error %d", (int)err);
    return 0;
  }

  // no results expected
  CS_INT result_type;
  err = ct_results(cmd, &result_type);
  if (err != CS_SUCCEED) {
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_result() failed with error %d", (int)err);
    return 0;
  }
  if (result_type != CS_CMD_SUCCEED) {
    assert(result_type == CS_CMD_FAIL);
    xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_results() for \"rollback transaction\" failed with error %d", (int)err);
    return 0;
  }
  while((err = ct_results(cmd, &result_type)) == CS_SUCCEED);

  return 1;
}

//------------------------------------------------------------------------------
static int sybase_rollback(Datasource *ds, ExceptionSink *xsink)
{
  sybase_connection* sc = (sybase_connection*)ds->getPrivateData();
  return sybase_rollback_impl(sc, xsink);
}

//------------------------------------------------------------------------------
QoreString* sybase_module_init()
{
   tracein("sybase_module_init()");

#ifdef DEBUG
  builtinFunctions.add("runSybaseTests", runSybaseTests, QDOM_DATABASE);
#endif

/* old registration method replaced on 2007/02/22
   // register driver with DBI subsystem
   DBIDriverFunctions *ddf =
      new DBIDriverFunctions(sybase_open,
                             sybase_close,
                             sybase_select,
                             sybase_select_rows,
                             sybase_exec,
                             sybase_commit,
                             sybase_rollback);
   DBID_SYBASE = DBI.registerDriver("sybase", ddf, DBI_SYBASE_CAPS);
*/
   // register driver with DBI subsystem
   class qore_dbi_method_list methods;
   methods.add(QDBI_METHOD_OPEN, sybase_open);
   methods.add(QDBI_METHOD_CLOSE, sybase_close);
   methods.add(QDBI_METHOD_SELECT, sybase_select);
   methods.add(QDBI_METHOD_SELECT_ROWS, sybase_select_rows);
   methods.add(QDBI_METHOD_EXEC, sybase_exec);
   methods.add(QDBI_METHOD_COMMIT, sybase_commit);
   methods.add(QDBI_METHOD_ROLLBACK, sybase_rollback);

   
   DBID_SYBASE = DBI.registerDriver("sybase", methods, DBI_SYBASE_CAPS);

   traceout("sybase_module_init()");
   return NULL;
}

//------------------------------------------------------------------------------
void sybase_module_ns_init(Namespace *rns, Namespace *qns)
{
   tracein("sybase_module_ns_init()");
   // nothing to do at the moment
   traceout("sybase_module_ns_init()");
}

//------------------------------------------------------------------------------
void sybase_module_delete()
{
   tracein("sybase_module_delete()");
   //DBI_deregisterDriver(DBID_SYBASE); - commented out because it is so in oracle module
   traceout("sybase_module_delete()");
}

#ifdef DEBUG
#  include "tests/sybase_tests.cc"
#endif

// EOF

