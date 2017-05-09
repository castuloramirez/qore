/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindMatchingSymbolsQuery.h

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

#ifndef _QLS_QUERIES_FINDMATCHINGSYMBOLSQUERY_H
#define _QLS_QUERIES_FINDMATCHINGSYMBOLSQUERY_H

#include <string>
#include <vector>

#include "ast/ASTName.h"
#include "ast/ASTSymbolInfo.h"

class ASTDeclaration;
class ASTExpression;
class ASTStatement;
class ASTTree;

class FindMatchingSymbolsQuery {
public:
    FindMatchingSymbolsQuery() = delete;
    FindMatchingSymbolsQuery(const FindMatchingSymbolsQuery& other) = delete;

    static std::vector<ASTSymbolInfo>* find(ASTTree* tree, const std::string& query);

private:
    static void inDeclaration(std::vector<ASTSymbolInfo>* vec, ASTDeclaration* decl, const std::string& query);
    static void inExpression(std::vector<ASTSymbolInfo>* vec, ASTExpression* expr, const std::string& query);
    static void inName(std::vector<ASTSymbolInfo>* vec, ASTName& name, const std::string& query);
    static void inName(std::vector<ASTSymbolInfo>* vec, ASTName* name, const std::string& query);
    static void inStatement(std::vector<ASTSymbolInfo>* vec, ASTStatement* stmt, const std::string& query);
};

#endif // _QLS_QUERIES_FINDMATCHINGSYMBOLSQUERY_H
