//*************************************************************************
// DESCRIPTION: Verilator: Produce a graph netlist from the AST.
//
// Code available from: http://www.veripool.org/verilator
//
//*************************************************************************
//
// Copyright 2003-2018 by Wilson Snyder.  This program is free software;
// you can redistribute it and/or modify it under the terms of either the
// GNU Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************

#ifndef _V3ASTNETLIST_H_
#define _V3ASTNETLIST_H_ 1

#include "V3Ast.h"

class V3AstNetlist {
public:
  static void astNetlist(AstNetlist *nodep);
};

#endif