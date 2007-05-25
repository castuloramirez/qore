/*
  set_parameter.cc

  Sybase DB layer for QORE
  uses Sybase OpenClient C library

  Qore Programming language

  Copyright (C) 2007 Qore Technologies

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
#include <qore/Exception.h>
#include <qore/minitest.hpp>
#include <qore/QoreNode.h>
#include <qore/QoreType.h>
#include <qore/QoreString.h>
#include <qore/BinaryObject.h>

#include <assert.h>
#include <ctpublic.h>
#include <cstypes.h>

#include "set_parameter.h"
#include "command.h"
#include "conversions.h"
#include "common_constants.h"

#ifndef CS_MAX_CHAR
#  define CS_MAX_CHAR 255
#endif

// returns 0=OK, -1=error (exception raised)
int set_input_params(command& cmd, processed_language_command_t &query, class List *args, QoreEncoding *encoding, ExceptionSink *xsink)
{
   unsigned nparams = query.m_parameter_types.size();
   //printd(5, "query=%s\n", query.m_cmd.getBuffer());

   for (unsigned i = 0; i < nparams; ++i)
   {
      if (query.m_parameter_types[i] == 'd') 
	 continue;

      int dtype = 0;

      class QoreNode *val = args ? args->retrieve_entry(i) : NULL;
      
      if (!val || is_null(val) || is_nothing(val))
	 val = 0;
      else if (val->type == NT_STRING)
	 dtype = CS_CHAR_TYPE;
      else if (val->type == NT_DATE)
	 dtype = CS_DATETIME_TYPE;
      else if (val->type == NT_INT)
#ifdef CS_BIGINT_TYPE_1
	 dtype = CS_BIGINT_TYPE;
#else
         dtype = CS_INT_TYPE;
#endif
      else if (val->type == NT_BOOLEAN)
	 dtype = CS_BIT_TYPE;
      else if (val->type == NT_FLOAT)
	 dtype = CS_FLOAT_TYPE;
      else if (val->type == NT_BINARY)
	 dtype = CS_BINARY_TYPE;
      else
      {
	 xsink->raiseException("DBI:SYBASE:BIND-ERROR", "do not know how to bind values of type '%s'", val->type->getName());
	 return -1;
      }

      if (set_input_parameter(cmd, i, dtype, val, encoding, xsink))
	 return -1;     
   }
   return 0;
}

// returns 0=OK, -1 for error (exception raised)
int set_input_parameter(command& cmd, unsigned parameter_index, int type,
			 QoreNode* data, QoreEncoding* encoding, ExceptionSink* xsink)
{
  CS_DATAFMT datafmt;
  memset(&datafmt, 0, sizeof(datafmt));
  datafmt.status = CS_INPUTVALUE;
  datafmt.namelen = CS_NULLTERM;
  datafmt.maxlength = CS_UNUSED;
  datafmt.count = 1;

  CS_RETCODE err;
  if (!data || data->type == NT_NULL || data->type == NT_NOTHING) {

#ifdef FREETDS
     // it seems to be necessary to specify a type like 
     // this to get a null value to be bound with freetds
     datafmt.datatype = CS_CHAR_TYPE;
     datafmt.format = CS_FMT_NULLTERM;
     datafmt.maxlength = 1;
#endif

    // SQL NULL value
    err = ct_param(cmd(), &datafmt, 0, CS_UNUSED, -1);
    if (err != CS_SUCCEED) {
      xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase call ct_param(NULL) failed for parameter #%u with error %d", parameter_index, (int)err);
      return -1;
    }
    return 0;
  }

  switch (type) {
     case CS_LONGCHAR_TYPE: 
     case CS_CHAR_TYPE:
     case CS_TEXT_TYPE: // text could be used only with LIKE in WHERE statement, nowhere else
     case CS_VARCHAR_TYPE:
     {
	if (data->type != NT_STRING) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for string parameter #%u", parameter_index + 1);
	   return -1;
	}

	TempEncodingHelper s(data->val.String, (QoreEncoding*)encoding, xsink);
	if (xsink->isException()) {
	   return -1;
	}
	
	int slen = 0;
	const char* s2 = "";
	if (*s && s->strlen()) {
	   s2 = s->getBuffer();
	   slen = s->strlen();
	}

	datafmt.datatype = type;
	datafmt.format = CS_FMT_NULLTERM;
	// NOTE: setting large sizes here like 2GB works for sybase ctlib, not for freetds
	datafmt.maxlength = slen + 1;
	err = ct_param(cmd(), &datafmt, (CS_VOID*)s2, slen, 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for string parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;

     case CS_TINYINT_TYPE:
     {
	if (data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u", parameter_index);
	   return -1;
	}
	if (data->val.intval < -128 || data->val.intval >= 128) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Integer value (#%d parameter) is out of range for Sybase datatype", parameter_index);
	   return -1;
	}
	
	CS_TINYINT val = data->val.intval;
	datafmt.datatype = CS_TINYINT_TYPE;
	err = ct_param(cmd(), &datafmt, &val, sizeof(val), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_SMALLINT_TYPE:
     {
	if (data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u", parameter_index);
	   return -1;
	}
	if (data->val.intval < -32 * 1024 || data->val.intval >= 32 * 1024) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Integer value (#%d parameter) is out of range for Sybase datatype", parameter_index);
	   return -1;
	}
	
	CS_SMALLINT val = data->val.intval;
	datafmt.datatype = CS_SMALLINT_TYPE;
	err = ct_param(cmd(), &datafmt, &val, sizeof(val), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_INT_TYPE:
     {
	if (data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u", parameter_index);
	   return -1;
	}
	CS_INT val = data->val.intval;
	datafmt.datatype = CS_INT_TYPE;
	err = ct_param(cmd(), &datafmt, &val, sizeof(val), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;

#ifdef CS_BIGINT_TYPE     
     case CS_BIGINT_TYPE:
     {
	if (data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for integer parameter #%u", parameter_index);
	   return -1;
	}
	datafmt.datatype = CS_BIGINT_TYPE;
	err = ct_param(cmd(), &datafmt, &data->val.intval, sizeof(int64), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for integer parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
#endif     

     case CS_IMAGE_TYPE: // image could be used only with LIKE in WHERE statement, nowhere else
     case CS_LONGBINARY_TYPE:
     case CS_BINARY_TYPE:
     {
	if (data->type != NT_BINARY) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for binary parameter #%u", parameter_index);
	   return -1;
	}
	
	datafmt.datatype = type;
	datafmt.maxlength = data->val.bin->size();
	datafmt.count = 1;
	err = ct_param(cmd(), &datafmt, (void *)data->val.bin->getPtr(), data->val.bin->size(), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for binary parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_VARBINARY_TYPE:
     {
	// should never be actually returned as an input parameter
	if (data->type != NT_BINARY) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for varbinary parameter #%u", parameter_index);
	   return -1;
	}
	int size = data->val.bin->size();
	if (size > CS_MAX_CHAR) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Parameter #%u: varbinary supports up to %d B (attempt to insert %d B)", parameter_index, CS_MAX_CHAR, size);
	   return -1;
	}
	
	datafmt.datatype = type;
	datafmt.maxlength = size;
	CS_VARBINARY varbin; // required by ct_param()
	varbin.len = size;
	memcpy(varbin.array, data->val.bin->getPtr(), size);
	
	err = ct_param(cmd(), &datafmt, &varbin, size, 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for binary parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_REAL_TYPE:
     {
	if (data->type != NT_FLOAT && data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for float parameter #%u", parameter_index);
	   return -1;
	}
	CS_REAL val;
	if (data->type == NT_FLOAT) {
	   val = data->val.floatval;
	} else {
	   val = data->val.intval;
	}
	datafmt.datatype = CS_REAL_TYPE;
	err = ct_param(cmd(), &datafmt, &val, sizeof(val), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for float parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_FLOAT_TYPE:
     {
	if (data->type != NT_FLOAT && data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for float parameter #%u", parameter_index);
	   return -1;
	}
	CS_FLOAT val;
	if (data->type == NT_FLOAT) {
	   val = data->val.floatval;
	} else {
	   val = data->val.intval;
	}
	datafmt.datatype = CS_FLOAT_TYPE;
	err = ct_param(cmd(), &datafmt, &val, sizeof(val), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for float parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;

     case CS_BIT_TYPE:
     {
	CS_BIT val = data->getAsBool();
	datafmt.datatype = CS_BIT_TYPE;
	err = ct_param(cmd(), &datafmt, &val, sizeof(val), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for bool parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_DATETIME_TYPE:
     {
	if (data->type != NT_DATE) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for datetime parameter #%u", parameter_index);
	   return -1;
	}
	
	CS_DATETIME dt;
	DateTime_to_DATETIME(data->val.date_time, dt, xsink);
	if (xsink->isException()) {
	   return -1;
	}
	
	datafmt.datatype = CS_DATETIME_TYPE;
	err = ct_param(cmd(), &datafmt, &dt, sizeof(dt), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for datetime parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_DATETIME4_TYPE:
     {
	if (data->type != NT_DATE) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for datetime parameter #%u", parameter_index);
	   return -1;
	}
	
	CS_DATETIME4 dt;
	DateTime_to_DATETIME4(cmd.getConnection(), data->val.date_time, dt, xsink);
	if (xsink->isException()) {
	   return -1;
	}
	
	datafmt.datatype = CS_DATETIME4_TYPE;
	err = ct_param(cmd(), &datafmt, &dt, sizeof(dt), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for datetime parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_MONEY_TYPE:
     {
	if (data->type != NT_FLOAT && data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for money parameter #%u (integer or float expected)", parameter_index);
	   return -1;
	}
	double val;
	if (data->type == NT_FLOAT) {
	   val = data->val.floatval;
	} else {
	   val = data->val.intval;
	}
	CS_MONEY m;
	double_to_MONEY(cmd.getConnection(), val, m, xsink);
	if (xsink->isException()) {
	   return -1;
	}
	datafmt.datatype = CS_MONEY_TYPE;
	err = ct_param(cmd(), &datafmt, &m, sizeof(m), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for money parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_MONEY4_TYPE:
     {
	if (data->type != NT_FLOAT && data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for smallmoney parameter #%u (integer or float expected)", parameter_index);
	   return -1;
	}
	double val;
	if (data->type == NT_FLOAT) {
	   val = data->val.floatval;
	} else {
	   val = data->val.intval;
	}
	CS_MONEY4 m;
	double_to_MONEY4(cmd.getConnection(), val, m, xsink);
	if (xsink->isException()) {
	   return -1;
	}
	datafmt.datatype = CS_MONEY4_TYPE;
	err = ct_param(cmd(), &datafmt, &m, sizeof(m), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for smallmoney parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;
     
     case CS_DECIMAL_TYPE:
     case CS_NUMERIC_TYPE:
     {
	if (data->type != NT_FLOAT && data->type != NT_INT) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Incorrect type for decimal parameter #%u (integer or float expected)", parameter_index);
	   return -1;
	}
	double val;
	if (data->type == NT_FLOAT) {
	   val = data->val.floatval;
	} else {
	   val = data->val.intval;
	}
	CS_DECIMAL dc;
	double_to_DECIMAL(cmd.getConnection(), val, dc, xsink);
	if (xsink->isException()) {
	   return -1;
	}
	datafmt.datatype = CS_DECIMAL_TYPE;
	datafmt.precision = DefaultNumericPrecision;
	datafmt.scale = DefaultNumericScale;
	err = ct_param(cmd(), &datafmt, &dc, sizeof(dc), 0);
	if (err != CS_SUCCEED) {
	   xsink->raiseException("DBI-EXEC-EXCEPTION", "Sybase function ct_param() for decimal parameter #%u failed with error", parameter_index, (int)err);
	   return -1;
	}
     }
     return 0;

     default:
	xsink->raiseException("DBI-EXEC-EXCEPTION", "Unrecognized type %d of Sybase parameter # %u", type, parameter_index + 1);
	return -1;
  } // switch
  return 0;
}

//------------------------------------------------------------------------------
void set_output_parameter(command& cmd, unsigned parameter_index, const char* name, int type, ExceptionSink* xsink)
{
  CS_DATAFMT datafmt;
  memset(&datafmt, 0, sizeof(datafmt));
  datafmt.status = CS_RETURN;
  datafmt.namelen = CS_NULLTERM;
  datafmt.maxlength = CS_UNUSED;
  datafmt.count = 1;

  switch (type) {
  case CS_LONGCHAR_TYPE: 
  case CS_TEXT_TYPE:
  case CS_IMAGE_TYPE: 
  case CS_LONGBINARY_TYPE:
    datafmt.maxlength = 1024 * 1024 * 1024;
    break;
  default:
    datafmt.maxlength = CS_MAX_CHAR;
    break;
  }

/* Because of Sybase message: 
  When defining parameters, names must be supplied for either 
  all of the parameters or none of the parameters.

  char prepended_name[256];
  if (name) {
    sprintf(prepended_name, "@%s", name);
    strcpy(datafmt.name, prepended_name);
    datafmt.namelen = strlen(datafmt.name);
  }
*/
  datafmt.datatype = type;

  CS_RETCODE err = ct_param(cmd(), &datafmt, 0, 0, -1);
  if (err != CS_SUCCEED) {
    xsink->raiseException("DBI-EXEC-EXCEPTION", "ct_param failed for output parameter #%d, err = %d", parameter_index, (int)err);
  } 
}

// EOF


