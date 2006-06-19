/*
  main.cc

  Qore Programming Language

  Copyright (C) 2003, 2004, 2005, 2006 David Nichols

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
#include <qore/common.h>
#include <qore/Statement.h>
#include <qore/support.h>
#include <qore/BuiltinFunctionList.h>
#include <qore/mysignal.h>
#include <qore/QoreType.h>
#include <qore/ModuleManager.h>
#include <qore/Variable.h>
#include <qore/Operator.h>
#include <qore/thread.h>
#include <qore/Namespace.h>
#include <qore/charset.h>
#include <qore/QoreProgram.h>

#include <qore/DBI.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <libxml/parser.h>

#include <openssl/ssl.h>
#include <openssl/err.h>


extern char **environ;

void qore_init(char *def_charset, bool show_module_errors)
{
   // initialize libxml2 library
   LIBXML_TEST_VERSION

   // initialize openssl library
   SSL_load_error_strings();
   SSL_library_init();
   // seed PRNG

   // init threading infrastructure
   init_qore_threads();

   initProgramGlobalVars(environ);

   // initialize charset encoding support
   QEM.init(def_charset);

   // initialize signal handling
   init_signals();

   // set up core operators
   operatorsInit();

   // init object subsystem
   initObjects();

   builtinFunctions.init();

   // init module subsystem
   MM.init(show_module_errors);

   struct sigaction sa;
   sa.sa_handler = SIG_IGN;
   sigemptyset (&sa.sa_mask);
   sa.sa_flags = 0;
   // ignore SIGPIPE signals
   sigaction(SIGPIPE, &sa, NULL);
}

// NOTE: we do not cleanup in reverse initialization order
// the threading subsystem is deleted before the modules are
// unloaded in case there are any module-specific thread
// cleanup functions to be run...
void qore_cleanup()
{
   // now free memory
   delete_global_variables();

   // delete threading infrastructure
   delete_qore_threads();

   // delete all loadable modules
   MM.cleanup();

   // delete object structures
   deleteObjects();

   // cleanup openssl library
   ERR_free_strings();

   // cleanup libxml2 library
   xmlCleanupParser();
}
