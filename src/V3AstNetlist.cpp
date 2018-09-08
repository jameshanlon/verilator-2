#include <cstdio>
#include <vector>
#include <memory>
#include <unordered_set>

#include "V3Global.h"
#include "V3File.h"
#include "V3Ast.h"
#include "V3AstNetlist.h"
#include "V3Graph.h"

class AstNetlistGraph : public V3Graph {
public:
  void dumpNetlistGraphFile(const std::unordered_set<std::string> regs,
                            const string &filename) const;
};

class AstNetlistEitherVertex : public V3GraphVertex {
  AstScope *scopePtr;
public:
  AstNetlistEitherVertex(V3Graph *graphp, AstScope *scopePtr) :
      V3GraphVertex(graphp), scopePtr(scopePtr) {}
  AstScope* scopep() const { return scopePtr; }
};

class AstNetlistVarVertex : public AstNetlistEitherVertex {
  AstVarScope *varScope;
public:
  AstNetlistVarVertex(V3Graph *graphp, AstScope *scopep, AstVarScope *varScope) :
      AstNetlistEitherVertex(graphp, scopep), varScope(varScope) {}
  AstVarScope *varScp() const { return varScope; }
};

class AstNetlistRegVertex : public AstNetlistEitherVertex {
  AstVarScope *varScope;
public:
  AstNetlistRegVertex(V3Graph *graphp, AstScope *scopep, AstVarScope *varScope) :
      AstNetlistEitherVertex(graphp, scopep), varScope(varScope) {}
  AstVarScope *varScp() const { return varScope; }
};

class AstNetlistLogicVertex : public AstNetlistEitherVertex {
  AstNode *nodePtr;
  AstActive *activePtr;
public:
  AstNetlistLogicVertex(V3Graph *graphp, AstScope *scopep, AstNode *nodep, AstActive *activep) :
      AstNetlistEitherVertex(graphp, scopep), nodePtr(nodep), activePtr(activep) {}
  AstNode *nodep() const { return nodePtr; }
  AstActive *activep() const { return activePtr; }
};

class AstNetlistVisitor : public AstNVisitor {
private:
  AstNetlistGraph        graph;
  AstNetlistLogicVertex* logicVertexp; // Current statement being tracked, NULL=ignored.
  AstScope*		           scopep;	     // Current scope being processed.
  AstNodeModule*	       modulep;	     // Current module.
  AstActive*		         activep;	     // Current active.
  bool                   inDly;        // In a delayed assignment statement.
  std::unordered_set<std::string> regs;
  static int debug() {
	  static int level = -1;
	  if (VL_UNLIKELY(level < 0))
      level = v3Global.opt.debugSrcLevel(__FILE__);
	  return level;
  }
  void iterateNewStmt(AstNode *nodep) {
    // A statment must have a scope for variable references to occur in.
    if (scopep) {
      UINFO(5, "New stmt " << nodep << " @ " << nodep->fileline() << endl);
      logicVertexp = new AstNetlistLogicVertex(&graph, scopep, nodep, activep);
      nodep->iterateChildren(*this);
      logicVertexp = NULL;
    }
  }
  AstNetlistVarVertex *makeVarVertex(AstVarScope *varScp) {
    UINFO(5, "New var vertex " << varScp->prettyName() << " @ " << varScp->fileline() << endl);
    AstNetlistVarVertex *vertexp = (AstNetlistVarVertex*)(varScp->user1p());
    if (!vertexp) {
      vertexp = new AstNetlistVarVertex(&graph, scopep, varScp);
      varScp->user1p(vertexp);
    }
    return vertexp;
  }
  AstNetlistRegVertex *makeRegVertex(AstVarScope *varScp) {
    UINFO(5, "New reg vertex " << varScp->prettyName() << " @ " << varScp->fileline() << endl);
    AstNetlistRegVertex *vertexp = new AstNetlistRegVertex(&graph, scopep, varScp);
    regs.insert(varScp->prettyName());
    return vertexp;
  }
public:
  explicit AstNetlistVisitor(AstNode *nodep) :
      logicVertexp(NULL), scopep(NULL), modulep(NULL), activep(NULL), inDly(false) {
    nodep->accept(*this);
  }
  virtual void visit(AstNetlist *nodep) {
    nodep->iterateChildren(*this);
    graph.dumpNetlistGraphFile(regs, "netlist.graph");
    UINFO(6, "DONE!" << endl);
  }
  virtual void visit(AstNodeModule *nodep) {
    UINFO(6, "Module" << endl);
    modulep = nodep;
    nodep->iterateChildren(*this);
    modulep = NULL;
  }
  virtual void visit(AstScope *nodep) {
    UINFO(6, "Scope" << endl);
    scopep = nodep;
    nodep->iterateChildren(*this);
    scopep = NULL;
  }
  virtual void visit(AstActive *nodep) {
    UINFO(6, "Block" << endl);
    activep = nodep;
    nodep->iterateChildren(*this);
    activep = NULL;
  }
  virtual void visit(AstNodeVarRef *nodep) {
    UINFO(6, "VarRef" << endl);
    if (scopep) {
      if (!logicVertexp) nodep->v3fatalSrc("Var ref not under a logic block");
      AstVarScope *varScp = nodep->varScopep();
      if (!varScp) nodep->v3fatalSrc("Var didn't get varscoped in V3Scope.cpp");
      // Add edge.
      if (nodep->lvalue()) {
        if (inDly) {
          AstNetlistRegVertex* vvertexp = makeRegVertex(varScp);
          new V3GraphEdge(&graph, logicVertexp, vvertexp, 1);
          UINFO(5, "New edge from   " << logicVertexp->nodep() << " @ " << logicVertexp->nodep()->fileline() << endl);
          UINFO(5, "New edge to REG " << varScp->prettyName() << " @ " << varScp->fileline() << endl);
        } else {
          AstNetlistVarVertex* vvertexp = makeVarVertex(varScp);
          new V3GraphEdge(&graph, logicVertexp, vvertexp, 1);
          UINFO(5, "New edge from   " << logicVertexp->nodep() << " @ " << logicVertexp->nodep()->fileline() << endl);
          UINFO(5, "New edge to     " << varScp->prettyName() << " @ " << varScp->fileline() << endl);
        }
      } else {
        AstNetlistVarVertex* vvertexp = makeVarVertex(varScp);
        new V3GraphEdge(&graph, vvertexp, logicVertexp, 1);
        UINFO(5, "New edge from " << varScp->prettyName() << " @ " << varScp->fileline() << endl);
        UINFO(5, "New edge to   " << logicVertexp->nodep() << " @ " << logicVertexp->nodep()->fileline() << endl);
      }
    }
  }
  virtual void visit(AstAlways *nodep) {
    UINFO(6, "Always" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAlwaysPublic *nodep) {
    UINFO(6, "AlwaysPublic" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstCFunc* nodep) {
    UINFO(6, "CFunc" << endl)
	  iterateNewStmt(nodep);
  }
  virtual void visit(AstSenItem *nodep) {
    UINFO(6, "SenItem" << endl);
    if (logicVertexp) {
      nodep->iterateChildren(*this);
    } else {
      iterateNewStmt(nodep);
    }
  }
  virtual void visit(AstSenGate *nodep) {
    UINFO(6, "SenGate" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstInitial *nodep) {
    UINFO(6, "Initial" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssign *nodep) {
    UINFO(6, "AssignAlias" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssignAlias *nodep) {
    UINFO(6, "AssignAlias" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssignW *nodep) {
    UINFO(6, "AssignW" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssignDly* nodep) {
    UINFO(6, "AssignDly" << endl);
    inDly = true;
    nodep->iterateChildren(*this);
    inDly = false;
  }
  virtual void visit(AstCoverToggle *nodep) {
    UINFO(6, "CoverToggle" << endl);
    iterateNewStmt(nodep);
  }
  virtual void visit(AstTraceInc *nodep) {
    UINFO(6, "TraceInc" << endl);
    iterateNewStmt(nodep);
  }
  // Default.
  virtual void visit(AstNode *nodep) {
    nodep->iterateChildren(*this);
  }
};

void AstNetlistGraph::dumpNetlistGraphFile(const std::unordered_set<std::string> regs,
                                           const string& filename) const {
  const vl_unique_ptr<ofstream> logp (V3File::new_ofstream(v3Global.opt.exeName()));
  if (logp->fail())
    v3fatalSrc("Can't write "<<filename);
  map <V3GraphVertex*, int> numMap;
  int n = 1; // Vertex 0 is the NULL ID.
  // Print vertices.
  for (V3GraphVertex* vertexp = verticesBeginp(); vertexp; vertexp=vertexp->verticesNextp()) {
    *logp << "VERTEX " << std::dec << n << " ";
    if (AstNetlistVarVertex* vvertexp = dynamic_cast<AstNetlistVarVertex*>(vertexp)) {
      const std::string &prettyName = vvertexp->varScp()->prettyName();
      AstVarType varType = vvertexp->varScp()->varp()->varType();
      FileLine *fileLine = vvertexp->varScp()->fileline();
      bool isReg = regs.count(prettyName);
      *logp << (isReg ? "REG_SRC" : "VAR");
      if (!isReg && varType != AstVarType::VAR &&
                    varType != AstVarType::MODULETEMP &&
                    varType != AstVarType::BLOCKTEMP)
        *logp << "_" << varType;
      *logp << " " << prettyName;
      *logp << " @ " << fileLine->ascii();
	  } if (AstNetlistRegVertex* vvertexp = dynamic_cast<AstNetlistRegVertex*>(vertexp)) {
      AstVarType varType = vvertexp->varScp()->varp()->varType();
      FileLine *fileLine = vvertexp->varScp()->fileline();
      assert (varType == AstVarType::VAR ||
              varType == AstVarType::MODULETEMP ||
              varType == AstVarType::BLOCKTEMP ||
              varType == AstVarType::OUTPUT);
      *logp << "REG_DST" << (varType == AstVarType::OUTPUT ? "_OUTPUT" : "");
      *logp << " " << vvertexp->varScp()->prettyName();
      *logp << " @ " << fileLine->ascii();
    } else if (AstNetlistLogicVertex* vvertexp = dynamic_cast<AstNetlistLogicVertex*>(vertexp)) {
      FileLine *fileLine = vvertexp->nodep()->fileline();
      *logp << "LOGIC ";
      *logp << vvertexp->scopep()->prettyName();
      *logp << " @ " << fileLine->ascii();
	}
    *logp << "\n";
    numMap[vertexp] = n++;
  }
  // Print edges.
  for (V3GraphVertex* vertexp = verticesBeginp(); vertexp; vertexp=vertexp->verticesNextp()) {
    for (V3GraphEdge* edgep = vertexp->outBeginp(); edgep; edgep=edgep->outNextp()) {
      if (edgep->weight()) {
        int fromVnum = numMap[edgep->fromp()];
        int toVnum = numMap[edgep->top()];
        *logp << "EDGE " << std::dec << fromVnum << " -> " << toVnum << "\n";
      }
    }
  }
  logp->close();
}

void V3AstNetlist::astNetlist(AstNetlist *nodep) {
  UINFO(2, __FUNCTION__ << ": " << endl);
  {
    //nodep->dumpTree();
    AstNetlistVisitor visitor(nodep);
  }  // Destruct before checking
  V3Global::dumpCheckGlobalTree("ast_netlist", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
}
