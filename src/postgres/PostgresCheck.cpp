#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Lex/Lexer.h"
#include "clang-tidy/ClangTidyCheck.h"
#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

using namespace clang;
using namespace clang::tidy;
using namespace clang::ast_matchers;

namespace PostgresCheck {

class BitmapsetReturnValueUsed: public ClangTidyCheck {
public:
  BitmapsetReturnValueUsed(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(callExpr().bind("bitmapset_return_value_used"), this);
  }

  void check(const MatchFinder::MatchResult &Result) override {
    if (const CallExpr *callExpr = Result.Nodes.getNodeAs<CallExpr>("bitmapset_return_value_used"))
    {
      const FunctionDecl *funcDecl = callExpr->getDirectCallee();
      if (!funcDecl)
        return;

      std::string functionName = funcDecl->getNameInfo().getAsString();

      if (
        functionName == "bms_add_member" ||
        functionName == "bms_add_members" ||
        functionName == "bms_add_range" ||
        functionName == "bms_del_member"  ||
        functionName == "bms_del_members" ||
        functionName == "bms_difference" ||
        functionName == "bms_intersect" ||
        functionName == "bms_int_members" ||
        functionName == "bms_join" ||
        functionName == "bms_make_singleton" ||
        functionName == "bms_union"
        ) {
        this->verify_return_value_used(Result, callExpr);
      }

    }
  }
private:
  void verify_return_value_used(const MatchFinder::MatchResult &Result, const CallExpr *callExpr)
  {
    if (!isReturnValueUsed(*Result.Context, callExpr)) {
      diag(callExpr->getBeginLoc(), "function return value not used", DiagnosticIDs::Error);
    }
  }

  bool isReturnValueUsed(ASTContext &Context, const CallExpr *Call) {
    const auto &Parents = Context.getParents(*Call);
    if (Parents.empty()) {
      return false; // No parent, likely top-level expression statement.
    }

    // Check the first parent.
    const auto *ParentStmt = Parents[0].get<Stmt>();
    if (!ParentStmt) {
      // Parent is not a statement (could be a Decl, etc.).
      // For simplicity, we'll consider this 'used' to avoid false positives.
      return true;
    }

    // If the direct parent is just the expression statement wrapper, it's unused.
    // e.g., foo(); <-- CallExpr parent is CompoundStmt, grandparent is FunctionDecl
    // but the *immediate* wrapper in the AST is often an implicit ExprWithCleanups
    // or similar, whose parent *is* the CompoundStmt. We need to check if the
    // CallExpr is the *entire* statement.
    if (isa<ExprWithCleanups>(ParentStmt)) {
       ParentStmt = Context.getParents(*ParentStmt)[0].get<Stmt>();
       if (!ParentStmt) return true;
    }

    // If the parent is a CompoundStmt, it means the call is a standalone statement.
    if (isa<CompoundStmt>(ParentStmt)) {
        return false;
    }

    // If the parent is a CastExpression, check the parent of the cast.
    // e.g., (void)foo();
    if (const auto *Cast = dyn_cast<CastExpr>(ParentStmt)) {
        // Check if the cast itself is used. Recurse upwards.
        // We need a way to check the cast's usage. Let's reuse the parent logic.
        // This simple check assumes casting to void means unused.
        if (Cast->getCastKind() == CK_ToVoid) {
            return false;
        }
        // Otherwise, assume the casted value *is* used somewhere else.
         return true;
    }

    // If the parent is a BinaryOperator (like assignment) or used in a variable
    // declaration, or returned, it's used.
    if (isa<BinaryOperator>(ParentStmt) || isa<DeclStmt>(ParentStmt) ||
        isa<ReturnStmt>(ParentStmt) || isa<ConditionalOperator>(ParentStmt)) {
        return true;
    }

    // Default assumption: If it's part of a larger expression/statement, it's used.
    // This avoids false positives in complex cases.
    return true;
  }

};

class BitmapsetMemberType: public ClangTidyCheck {
public:
  BitmapsetMemberType(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(callExpr().bind("bitmapset_member_type"), this);
  }

  void check(const MatchFinder::MatchResult &Result) override {
    if (const CallExpr *callExpr = Result.Nodes.getNodeAs<CallExpr>("bitmapset_member_type"))
    {
      SourceLocation loc = callExpr->getBeginLoc();

      const FunctionDecl *funcDecl = callExpr->getDirectCallee();
      if (!funcDecl)
        return;

      std::string functionName = funcDecl->getNameInfo().getAsString();

      if (functionName == "bms_add_member" || functionName == "bms_del_member") {
        this->verify_bitmapset_member(loc, functionName, callExpr->getArg(1));
      }
      if (functionName == "bms_make_singleton") {
        this->verify_bitmapset_member(loc, functionName, callExpr->getArg(0));
      }
      if (functionName == "bms_add_range") {
        this->verify_bitmapset_member(loc, functionName, callExpr->getArg(1));
        this->verify_bitmapset_member(loc, functionName, callExpr->getArg(2));
      }
    }
  }

private:
  /*
   * Adding anything but small ints to a bitmapset is usally a mistake and an Index was
   * meant to be added instead. e.g. RelOptInfo->relid (Index) was meant to be added but the code
   * is adding RangeTblEntry->relid (Oid).
   */
  void verify_bitmapset_member(SourceLocation loc, const std::string functionName, const Expr *arg)
  {
    QualType argType = arg->getType();

    if (const ImplicitCastExpr *implicitCast = dyn_cast<ImplicitCastExpr>(arg)) {
      QualType sourceType = implicitCast->getSubExpr()->getType();
      if (sourceType != argType)
      {
        std::string typeName = sourceType.getAsString();

        if (typeName != "AttrNumber" && typeName != "int16" && typeName != "uint16" && typeName != "Index")
        {
          diag(loc, "potential wrong function argument. %0 called with datatype %1", DiagnosticIDs::Error) << functionName << typeName;
        }

      }
    }
  }

  void verify_return_value_used(const MatchFinder::MatchResult &Result, const CallExpr *callExpr)
  {
    if (!isReturnValueUsed(*Result.Context, callExpr)) {
      diag(callExpr->getBeginLoc(), "function return value not used", DiagnosticIDs::Error);
    }
  }

  bool isReturnValueUsed(ASTContext &Context, const CallExpr *Call) {
    const auto &Parents = Context.getParents(*Call);
    if (Parents.empty()) {
      return false; // No parent, likely top-level expression statement.
    }

    // Check the first parent.
    const auto *ParentStmt = Parents[0].get<Stmt>();
    if (!ParentStmt) {
      // Parent is not a statement (could be a Decl, etc.).
      // For simplicity, we'll consider this 'used' to avoid false positives.
      return true;
    }

    // If the direct parent is just the expression statement wrapper, it's unused.
    // e.g., foo(); <-- CallExpr parent is CompoundStmt, grandparent is FunctionDecl
    // but the *immediate* wrapper in the AST is often an implicit ExprWithCleanups
    // or similar, whose parent *is* the CompoundStmt. We need to check if the
    // CallExpr is the *entire* statement.
    if (isa<ExprWithCleanups>(ParentStmt)) {
       ParentStmt = Context.getParents(*ParentStmt)[0].get<Stmt>();
       if (!ParentStmt) return true;
    }

    // If the parent is a CompoundStmt, it means the call is a standalone statement.
    if (isa<CompoundStmt>(ParentStmt)) {
        return false;
    }

    // If the parent is a CastExpression, check the parent of the cast.
    // e.g., (void)foo();
    if (const auto *Cast = dyn_cast<CastExpr>(ParentStmt)) {
        // Check if the cast itself is used. Recurse upwards.
        // We need a way to check the cast's usage. Let's reuse the parent logic.
        // This simple check assumes casting to void means unused.
        if (Cast->getCastKind() == CK_ToVoid) {
            return false;
        }
        // Otherwise, assume the casted value *is* used somewhere else.
         return true;
    }

    // If the parent is a BinaryOperator (like assignment) or used in a variable
    // declaration, or returned, it's used.
    if (isa<BinaryOperator>(ParentStmt) || isa<DeclStmt>(ParentStmt) ||
        isa<ReturnStmt>(ParentStmt) || isa<ConditionalOperator>(ParentStmt)) {
        return true;
    }

    // Default assumption: If it's part of a larger expression/statement, it's used.
    // This avoids false positives in complex cases.
    return true;
  }

};

} // namespace PostgresCheck

namespace {

class PostgresCheckModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<PostgresCheck::BitmapsetReturnValueUsed>("postgres-bitmapset-return");
    CheckFactories.registerCheck<PostgresCheck::BitmapsetMemberType>("postgres-bitmapset-member");
  }
};

} // namespace

namespace clang::tidy {

// Register the module using this statically initialized variable.
static ClangTidyModuleRegistry::Add<::PostgresCheckModule>
    X("postgres-check-module", "Adds 'postgres-check' checks.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the module.
volatile int postgresCheckAnchorSource = 0;

} // namespace clang::tidy
