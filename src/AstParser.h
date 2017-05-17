/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstParser.h

  Qore AST Parser

  Copyright (C) 2017 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#ifndef _QLS_ASTPARSER_H
#define _QLS_ASTPARSER_H

#include <ostream>
#include <string>

#include "AstParseErrorLog.h"

class ASTTree;

class AstParser : public AstParseErrorLog {
public:
    AstParser() {}
    ~AstParser() {}

    ASTTree* parseFile(const char* filename);
    ASTTree* parseFile(std::string& filename);

    ASTTree* parseString(const char* str);
    ASTTree* parseString(std::string& str);

    void printTree(std::ostream& os);
};

#endif // _QLS_ASTPARSER_H
