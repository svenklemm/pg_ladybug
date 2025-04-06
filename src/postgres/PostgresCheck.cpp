#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace clang::tidy;
using namespace clang::ast_matchers;

namespace PostgresCheck {

class BitmapsetCheck: public ClangTidyCheck {
public:
  BitmapsetCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
private:
	void verify_bitmapset_member(const SourceRange callRange, const std::string functionName, const Expr *arg);
};

void BitmapsetCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(callExpr().bind("check_bms_functions"), this);
}

void BitmapsetCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *call = Result.Nodes.getNodeAs<CallExpr>("check_bms_functions");
  const FunctionDecl *funcDecl = call->getDirectCallee();
  if (!funcDecl)
    return;
  std::string functionName = funcDecl->getNameInfo().getAsString();

  if (functionName == "bms_make_singleton") {
		this->verify_bitmapset_member(call->getSourceRange(), functionName, call->getArg(0));
  }
  if (functionName == "bms_add_member" || functionName == "bms_del_member") {
		this->verify_bitmapset_member(call->getSourceRange(), functionName, call->getArg(1));
  }
  if (functionName == "bms_add_range") {
		this->verify_bitmapset_member(call->getSourceRange(), functionName, call->getArg(1));
		this->verify_bitmapset_member(call->getSourceRange(), functionName, call->getArg(2));
  }
}

/*
 * Adding anything but small ints to a bitmapset is usally a mistake and an Index was
 * meant to be added instead. e.g. RelOptInfo->relid (Index) was meant to be added but the code
 * is adding RangeTblEntry->relid (Oid).
 */
void BitmapsetCheck::verify_bitmapset_member(const SourceRange callRange, const std::string functionName, const Expr *arg)
{
  QualType argType = arg->getType();

  if (const ImplicitCastExpr *implicitCast = dyn_cast<ImplicitCastExpr>(arg)) {
		QualType sourceType = implicitCast->getSubExpr()->getType();
    if (sourceType != argType)
		{
			std::string typeName = sourceType.getAsString();

			if (typeName != "AttrNumber" && typeName != "int16" && typeName != "uint16" && typeName != "Index")
			{
				diag(callRange.getBegin(), "function %0 called with %1 argument ") << functionName << sourceType.getAsString();
			}

		}
  }
}

} // namespace PostgresCheck

namespace {

class PostgresCheckModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<PostgresCheck::BitmapsetCheck>("postgres-bitmapset-member");
  }
};

} // namespace

namespace clang::tidy {

// Register the module using this statically initialized variable.
static ClangTidyModuleRegistry::Add<::PostgresCheckModule>
    postgresCheckInit("postgres-check-module", "Adds 'postgres-check' checks.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the module.
volatile int postgresCheckAnchorSource = 0;

} // namespace clang::tidy
