#include <cstdio>
#include <vector>
#include <memory>

#include "V3Global.h"
#include "V3File.h"
#include "V3Ast.h"
#include "V3AstNetlist.h"
#include "V3Graph.h"

class AstNetlistGraph : public V3Graph {
public:
  void dumpNetlistGraphFile(const string &filename) const;
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
  static int debug() {
	  static int level = -1;
	  if (VL_UNLIKELY(level < 0))
      level = v3Global.opt.debugSrcLevel(__FILE__);
	  return level;
  }
  void iterateNewStmt(AstNode *nodep) {
    UINFO(6, "New Stmt " << nodep << endl);
    std::cout << "  New stmt\n";
    logicVertexp = new AstNetlistLogicVertex(&graph, scopep, nodep, activep);
    nodep->iterateChildren(*this);
    logicVertexp = nullptr;
  }
  AstNetlistVarVertex *makeVarVertex(AstVarScope *varScp) {
    UINFO(6, "New var vertex " << varScp << endl);
    std::cout << "  New vertex\n";
    AstNetlistVarVertex *vertexp = (AstNetlistVarVertex*)(varScp->user1p());
    if (!vertexp) {
      vertexp = new AstNetlistVarVertex(&graph, scopep, varScp);
      varScp->user1p(vertexp);
    }
    return vertexp;
  }
  AstNetlistRegVertex *makeRegVertex(AstVarScope *varScp) {
    UINFO(6, "New reg vertex " << varScp << endl);
    std::cout << "  New register\n";
    AstNetlistRegVertex *vertexp = new AstNetlistRegVertex(&graph, scopep, varScp);
    return vertexp;
  }
public:
  explicit AstNetlistVisitor(AstNode *nodep) :
      logicVertexp(nullptr), scopep(nullptr), modulep(nullptr), activep(nullptr) {
    nodep->accept(*this);
  }
  virtual void visit(AstNetlist *nodep) {
    nodep->iterateChildren(*this);
    graph.dumpNetlistGraphFile("netlist.graph");
    std::cout << "DONE!\n";
  }
  virtual void visit(AstNodeModule *nodep) {
    std::cout<<"module\n";
    modulep = nodep;
    nodep->iterateChildren(*this);
    modulep = nullptr;
  }
  virtual void visit(AstScope *nodep) {
    std::cout<<"scope\n";
    scopep = nodep;
    nodep->iterateChildren(*this);
    scopep = nullptr;
  }
  virtual void visit(AstActive *nodep) {
    std::cout<<"active\n";
    activep = nodep;
    nodep->iterateChildren(*this);
    activep = nullptr;
  }
  virtual void visit(AstNodeVarRef *nodep) {
    std::cout<<"varref\n";
    if (scopep) {
      if (!logicVertexp) nodep->v3fatalSrc("Var ref not under a logic block");
      AstVarScope *varScp = nodep->varScopep();
      if (!varScp) nodep->v3fatalSrc("Var didn't get varscoped in V3Scope.cpp");
      // Add edge.
      if (nodep->lvalue()) {
        if (inDly) {
          AstNetlistRegVertex* vvertexp = makeRegVertex(varScp);
          new V3GraphEdge(&graph, logicVertexp, vvertexp, 1);
        } else {
          AstNetlistVarVertex* vvertexp = makeVarVertex(varScp);
          new V3GraphEdge(&graph, logicVertexp, vvertexp, 1);
        }
      } else {
        AstNetlistVarVertex* vvertexp = makeVarVertex(varScp);
        new V3GraphEdge(&graph, vvertexp, logicVertexp, 1);
      }
    }
  }
  virtual void visit(AstAlways *nodep) {
    std::cout<<"always\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAlwaysPublic *nodep) {
    std::cout<<"alwayspublic\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstSenItem *nodep) {
    std::cout<<"senitem\n";
    if (logicVertexp) {
      nodep->iterateChildren(*this);
    } else {
      iterateNewStmt(nodep);
    }
  }
  virtual void visit(AstSenGate *nodep) {
    std::cout<<"sengate\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstInitial *nodep) {
    std::cout<<"initial\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssignAlias *nodep) {
    std::cout<<"assignalias\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssignW *nodep) {
    std::cout<<"assignw\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstAssignDly* nodep) {
    std::cout<<"assigndly\n";
    inDly = true;
    nodep->iterateChildren(*this);
    inDly = false;
  }
  virtual void visit(AstCoverToggle *nodep) {
    std::cout<<"covertoggle\n";
    iterateNewStmt(nodep);
  }
  virtual void visit(AstTraceInc *nodep) {
    std::cout<<"traceinc\n";
    iterateNewStmt(nodep);
  }
  // Default.
  virtual void visit(AstNode *nodep) {
    nodep->iterateChildren(*this);
  }
};

void AstNetlistGraph::dumpNetlistGraphFile(const string& filename) const {
  const vl_unique_ptr<ofstream> logp (V3File::new_ofstream(v3Global.debugFilename(filename)));
  if (logp->fail())
    v3fatalSrc("Can't write "<<filename);
  map <V3GraphVertex*, int> numMap;
  int n = 0;
  // Print vertices.
  for (V3GraphVertex* vertexp = verticesBeginp(); vertexp; vertexp=vertexp->verticesNextp()) {
    *logp << "VERTEX " << std::dec << n << " ";
    if (AstNetlistVarVertex* vvertexp = dynamic_cast<AstNetlistVarVertex*>(vertexp)) {
      *logp << "VAR " << vvertexp->varScp()->prettyName();
      *logp << " @ " << vvertexp->varScp()->fileline();
	  } if (AstNetlistRegVertex* vvertexp = dynamic_cast<AstNetlistRegVertex*>(vertexp)) {
      *logp << "REG " << vvertexp->varScp()->prettyName();
      *logp << " @ " << vvertexp->varScp()->fileline();
    } else if (AstNetlistLogicVertex* vvertexp = dynamic_cast<AstNetlistLogicVertex*>(vertexp)) {
      *logp << "LOGIC " << vvertexp->scopep()->prettyName();
      *logp << " @ " << vvertexp->nodep()->fileline();
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
    AstNetlistVisitor visitor(nodep);
  }  // Destruct before checking
  V3Global::dumpCheckGlobalTree("ast_netlist", 0, v3Global.opt.dumpTreeLevel(__FILE__) >= 3);
}