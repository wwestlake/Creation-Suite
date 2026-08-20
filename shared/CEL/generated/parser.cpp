// A Bison parser, made by GNU Bison 3.8.2.

// Skeleton implementation for Bison LALR(1) parsers in C++

// Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// As a special exception, you may create a larger work that contains
// part or all of the Bison parser skeleton and distribute that work
// under terms of your choice, so long as that work isn't itself a
// parser generator using the skeleton or a modified version thereof
// as a parser skeleton.  Alternatively, if you modify or redistribute
// the parser skeleton itself, you may (at your option) remove this
// special exception, which will cause the skeleton and the resulting
// Bison output files to be licensed under the GNU General Public
// License without this special exception.

// This special exception was added by the Free Software Foundation in
// version 2.2 of Bison.

// DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
// especially those whose name start with YY_ or yy_.  They are
// private implementation details that can be changed or removed.





#include "parser.hpp"


// Unqualified %code blocks.
#line 42 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"

    #include "lang/diagnostics.h"
    #include "lang/lexer.h"

    namespace ce::lang {
    static Parser::symbol_type yylex(Lexer& lexer) { return lexer.NextToken(); }

    namespace {
    SourceLocation ToSourceLocation(const Parser::location_type& loc) {
        return SourceLocation{ loc.begin.line, loc.begin.column };
    }
    } // namespace
    } // namespace ce::lang

#line 61 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> // FIXME: INFRINGES ON USER NAME SPACE.
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif


// Whether we are compiled with exception support.
#ifndef YY_EXCEPTIONS
# if defined __GNUC__ && !defined __EXCEPTIONS
#  define YY_EXCEPTIONS 0
# else
#  define YY_EXCEPTIONS 1
# endif
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K].location)
/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

# ifndef YYLLOC_DEFAULT
#  define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
        }                                                               \
    while (false)
# endif


// Enable debugging if requested.
#if YYDEBUG

// A pseudo ostream that takes yydebug_ into account.
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Symbol)         \
  do {                                          \
    if (yydebug_)                               \
    {                                           \
      *yycdebug_ << Title << ' ';               \
      yy_print_ (*yycdebug_, Symbol);           \
      *yycdebug_ << '\n';                       \
    }                                           \
  } while (false)

# define YY_REDUCE_PRINT(Rule)          \
  do {                                  \
    if (yydebug_)                       \
      yy_reduce_print_ (Rule);          \
  } while (false)

# define YY_STACK_PRINT()               \
  do {                                  \
    if (yydebug_)                       \
      yy_stack_print_ ();                \
  } while (false)

#else // !YYDEBUG

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Symbol)  YY_USE (Symbol)
# define YY_REDUCE_PRINT(Rule)           static_cast<void> (0)
# define YY_STACK_PRINT()                static_cast<void> (0)

#endif // !YYDEBUG

#define yyerrok         (yyerrstatus_ = 0)
#define yyclearin       (yyla.clear ())

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)

#line 5 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
namespace ce { namespace lang {
#line 154 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"

  /// Build a parser object.
  Parser::Parser (ce::lang::Lexer& lexer_yyarg, ce::lang::AstArena& arena_yyarg, ce::lang::DiagnosticEngine& diagnostics_yyarg, ce::lang::Program*& result_yyarg)
#if YYDEBUG
    : yydebug_ (false),
      yycdebug_ (&std::cerr),
#else
    :
#endif
      lexer (lexer_yyarg),
      arena (arena_yyarg),
      diagnostics (diagnostics_yyarg),
      result (result_yyarg)
  {}

  Parser::~Parser ()
  {}

  Parser::syntax_error::~syntax_error () YY_NOEXCEPT YY_NOTHROW
  {}

  /*---------.
  | symbol.  |
  `---------*/



  // by_state.
  Parser::by_state::by_state () YY_NOEXCEPT
    : state (empty_state)
  {}

  Parser::by_state::by_state (const by_state& that) YY_NOEXCEPT
    : state (that.state)
  {}

  void
  Parser::by_state::clear () YY_NOEXCEPT
  {
    state = empty_state;
  }

  void
  Parser::by_state::move (by_state& that)
  {
    state = that.state;
    that.clear ();
  }

  Parser::by_state::by_state (state_type s) YY_NOEXCEPT
    : state (s)
  {}

  Parser::symbol_kind_type
  Parser::by_state::kind () const YY_NOEXCEPT
  {
    if (state == empty_state)
      return symbol_kind::S_YYEMPTY;
    else
      return YY_CAST (symbol_kind_type, yystos_[+state]);
  }

  Parser::stack_symbol_type::stack_symbol_type ()
  {}

  Parser::stack_symbol_type::stack_symbol_type (YY_RVREF (stack_symbol_type) that)
    : super_type (YY_MOVE (that.state), YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_decl: // decl
        value.YY_MOVE_OR_COPY< ce::lang::Decl* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_expr: // expr
      case symbol_kind::S_or_expr: // or_expr
      case symbol_kind::S_and_expr: // and_expr
      case symbol_kind::S_eq_expr: // eq_expr
      case symbol_kind::S_rel_expr: // rel_expr
      case symbol_kind::S_add_expr: // add_expr
      case symbol_kind::S_mul_expr: // mul_expr
      case symbol_kind::S_unary_expr: // unary_expr
      case symbol_kind::S_postfix_expr: // postfix_expr
      case symbol_kind::S_primary_expr: // primary_expr
        value.YY_MOVE_OR_COPY< ce::lang::Expr* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_func_decl: // func_decl
        value.YY_MOVE_OR_COPY< ce::lang::FuncDecl* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_global_var_decl: // global_var_decl
        value.YY_MOVE_OR_COPY< ce::lang::GlobalVarDecl* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_param: // param
        value.YY_MOVE_OR_COPY< ce::lang::Param > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_block: // block
      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_closed_stmt: // closed_stmt
      case symbol_kind::S_open_stmt: // open_stmt
      case symbol_kind::S_simple_stmt: // simple_stmt
      case symbol_kind::S_var_decl_stmt: // var_decl_stmt
      case symbol_kind::S_var_decl_no_semi: // var_decl_no_semi
      case symbol_kind::S_assign_stmt: // assign_stmt
      case symbol_kind::S_assign_no_semi: // assign_no_semi
      case symbol_kind::S_for_init_opt: // for_init_opt
      case symbol_kind::S_for_step_opt: // for_step_opt
        value.YY_MOVE_OR_COPY< ce::lang::Stmt* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOAT_LITERAL: // FLOAT_LITERAL
        value.YY_MOVE_OR_COPY< float > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INT_LITERAL: // INT_LITERAL
        value.YY_MOVE_OR_COPY< int64_t > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_STRING_LITERAL: // STRING_LITERAL
      case symbol_kind::S_IDENT: // IDENT
        value.YY_MOVE_OR_COPY< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_decl_list: // decl_list
        value.YY_MOVE_OR_COPY< std::vector<ce::lang::Decl*> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_arg_list_opt: // arg_list_opt
      case symbol_kind::S_arg_list: // arg_list
        value.YY_MOVE_OR_COPY< std::vector<ce::lang::Expr*> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_param_list_opt: // param_list_opt
      case symbol_kind::S_param_list: // param_list
        value.YY_MOVE_OR_COPY< std::vector<ce::lang::Param> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stmt_list: // stmt_list
        value.YY_MOVE_OR_COPY< std::vector<ce::lang::Stmt*> > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

#if 201103L <= YY_CPLUSPLUS
    // that is emptied.
    that.state = empty_state;
#endif
  }

  Parser::stack_symbol_type::stack_symbol_type (state_type s, YY_MOVE_REF (symbol_type) that)
    : super_type (s, YY_MOVE (that.location))
  {
    switch (that.kind ())
    {
      case symbol_kind::S_decl: // decl
        value.move< ce::lang::Decl* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_expr: // expr
      case symbol_kind::S_or_expr: // or_expr
      case symbol_kind::S_and_expr: // and_expr
      case symbol_kind::S_eq_expr: // eq_expr
      case symbol_kind::S_rel_expr: // rel_expr
      case symbol_kind::S_add_expr: // add_expr
      case symbol_kind::S_mul_expr: // mul_expr
      case symbol_kind::S_unary_expr: // unary_expr
      case symbol_kind::S_postfix_expr: // postfix_expr
      case symbol_kind::S_primary_expr: // primary_expr
        value.move< ce::lang::Expr* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_func_decl: // func_decl
        value.move< ce::lang::FuncDecl* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_global_var_decl: // global_var_decl
        value.move< ce::lang::GlobalVarDecl* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_param: // param
        value.move< ce::lang::Param > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_block: // block
      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_closed_stmt: // closed_stmt
      case symbol_kind::S_open_stmt: // open_stmt
      case symbol_kind::S_simple_stmt: // simple_stmt
      case symbol_kind::S_var_decl_stmt: // var_decl_stmt
      case symbol_kind::S_var_decl_no_semi: // var_decl_no_semi
      case symbol_kind::S_assign_stmt: // assign_stmt
      case symbol_kind::S_assign_no_semi: // assign_no_semi
      case symbol_kind::S_for_init_opt: // for_init_opt
      case symbol_kind::S_for_step_opt: // for_step_opt
        value.move< ce::lang::Stmt* > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_FLOAT_LITERAL: // FLOAT_LITERAL
        value.move< float > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_INT_LITERAL: // INT_LITERAL
        value.move< int64_t > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_STRING_LITERAL: // STRING_LITERAL
      case symbol_kind::S_IDENT: // IDENT
        value.move< std::string > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_decl_list: // decl_list
        value.move< std::vector<ce::lang::Decl*> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_arg_list_opt: // arg_list_opt
      case symbol_kind::S_arg_list: // arg_list
        value.move< std::vector<ce::lang::Expr*> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_param_list_opt: // param_list_opt
      case symbol_kind::S_param_list: // param_list
        value.move< std::vector<ce::lang::Param> > (YY_MOVE (that.value));
        break;

      case symbol_kind::S_stmt_list: // stmt_list
        value.move< std::vector<ce::lang::Stmt*> > (YY_MOVE (that.value));
        break;

      default:
        break;
    }

    // that is emptied.
    that.kind_ = symbol_kind::S_YYEMPTY;
  }

#if YY_CPLUSPLUS < 201103L
  Parser::stack_symbol_type&
  Parser::stack_symbol_type::operator= (const stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_decl: // decl
        value.copy< ce::lang::Decl* > (that.value);
        break;

      case symbol_kind::S_expr: // expr
      case symbol_kind::S_or_expr: // or_expr
      case symbol_kind::S_and_expr: // and_expr
      case symbol_kind::S_eq_expr: // eq_expr
      case symbol_kind::S_rel_expr: // rel_expr
      case symbol_kind::S_add_expr: // add_expr
      case symbol_kind::S_mul_expr: // mul_expr
      case symbol_kind::S_unary_expr: // unary_expr
      case symbol_kind::S_postfix_expr: // postfix_expr
      case symbol_kind::S_primary_expr: // primary_expr
        value.copy< ce::lang::Expr* > (that.value);
        break;

      case symbol_kind::S_func_decl: // func_decl
        value.copy< ce::lang::FuncDecl* > (that.value);
        break;

      case symbol_kind::S_global_var_decl: // global_var_decl
        value.copy< ce::lang::GlobalVarDecl* > (that.value);
        break;

      case symbol_kind::S_param: // param
        value.copy< ce::lang::Param > (that.value);
        break;

      case symbol_kind::S_block: // block
      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_closed_stmt: // closed_stmt
      case symbol_kind::S_open_stmt: // open_stmt
      case symbol_kind::S_simple_stmt: // simple_stmt
      case symbol_kind::S_var_decl_stmt: // var_decl_stmt
      case symbol_kind::S_var_decl_no_semi: // var_decl_no_semi
      case symbol_kind::S_assign_stmt: // assign_stmt
      case symbol_kind::S_assign_no_semi: // assign_no_semi
      case symbol_kind::S_for_init_opt: // for_init_opt
      case symbol_kind::S_for_step_opt: // for_step_opt
        value.copy< ce::lang::Stmt* > (that.value);
        break;

      case symbol_kind::S_FLOAT_LITERAL: // FLOAT_LITERAL
        value.copy< float > (that.value);
        break;

      case symbol_kind::S_INT_LITERAL: // INT_LITERAL
        value.copy< int64_t > (that.value);
        break;

      case symbol_kind::S_STRING_LITERAL: // STRING_LITERAL
      case symbol_kind::S_IDENT: // IDENT
        value.copy< std::string > (that.value);
        break;

      case symbol_kind::S_decl_list: // decl_list
        value.copy< std::vector<ce::lang::Decl*> > (that.value);
        break;

      case symbol_kind::S_arg_list_opt: // arg_list_opt
      case symbol_kind::S_arg_list: // arg_list
        value.copy< std::vector<ce::lang::Expr*> > (that.value);
        break;

      case symbol_kind::S_param_list_opt: // param_list_opt
      case symbol_kind::S_param_list: // param_list
        value.copy< std::vector<ce::lang::Param> > (that.value);
        break;

      case symbol_kind::S_stmt_list: // stmt_list
        value.copy< std::vector<ce::lang::Stmt*> > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    return *this;
  }

  Parser::stack_symbol_type&
  Parser::stack_symbol_type::operator= (stack_symbol_type& that)
  {
    state = that.state;
    switch (that.kind ())
    {
      case symbol_kind::S_decl: // decl
        value.move< ce::lang::Decl* > (that.value);
        break;

      case symbol_kind::S_expr: // expr
      case symbol_kind::S_or_expr: // or_expr
      case symbol_kind::S_and_expr: // and_expr
      case symbol_kind::S_eq_expr: // eq_expr
      case symbol_kind::S_rel_expr: // rel_expr
      case symbol_kind::S_add_expr: // add_expr
      case symbol_kind::S_mul_expr: // mul_expr
      case symbol_kind::S_unary_expr: // unary_expr
      case symbol_kind::S_postfix_expr: // postfix_expr
      case symbol_kind::S_primary_expr: // primary_expr
        value.move< ce::lang::Expr* > (that.value);
        break;

      case symbol_kind::S_func_decl: // func_decl
        value.move< ce::lang::FuncDecl* > (that.value);
        break;

      case symbol_kind::S_global_var_decl: // global_var_decl
        value.move< ce::lang::GlobalVarDecl* > (that.value);
        break;

      case symbol_kind::S_param: // param
        value.move< ce::lang::Param > (that.value);
        break;

      case symbol_kind::S_block: // block
      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_closed_stmt: // closed_stmt
      case symbol_kind::S_open_stmt: // open_stmt
      case symbol_kind::S_simple_stmt: // simple_stmt
      case symbol_kind::S_var_decl_stmt: // var_decl_stmt
      case symbol_kind::S_var_decl_no_semi: // var_decl_no_semi
      case symbol_kind::S_assign_stmt: // assign_stmt
      case symbol_kind::S_assign_no_semi: // assign_no_semi
      case symbol_kind::S_for_init_opt: // for_init_opt
      case symbol_kind::S_for_step_opt: // for_step_opt
        value.move< ce::lang::Stmt* > (that.value);
        break;

      case symbol_kind::S_FLOAT_LITERAL: // FLOAT_LITERAL
        value.move< float > (that.value);
        break;

      case symbol_kind::S_INT_LITERAL: // INT_LITERAL
        value.move< int64_t > (that.value);
        break;

      case symbol_kind::S_STRING_LITERAL: // STRING_LITERAL
      case symbol_kind::S_IDENT: // IDENT
        value.move< std::string > (that.value);
        break;

      case symbol_kind::S_decl_list: // decl_list
        value.move< std::vector<ce::lang::Decl*> > (that.value);
        break;

      case symbol_kind::S_arg_list_opt: // arg_list_opt
      case symbol_kind::S_arg_list: // arg_list
        value.move< std::vector<ce::lang::Expr*> > (that.value);
        break;

      case symbol_kind::S_param_list_opt: // param_list_opt
      case symbol_kind::S_param_list: // param_list
        value.move< std::vector<ce::lang::Param> > (that.value);
        break;

      case symbol_kind::S_stmt_list: // stmt_list
        value.move< std::vector<ce::lang::Stmt*> > (that.value);
        break;

      default:
        break;
    }

    location = that.location;
    // that is emptied.
    that.state = empty_state;
    return *this;
  }
#endif

  template <typename Base>
  void
  Parser::yy_destroy_ (const char* yymsg, basic_symbol<Base>& yysym) const
  {
    if (yymsg)
      YY_SYMBOL_PRINT (yymsg, yysym);
  }

#if YYDEBUG
  template <typename Base>
  void
  Parser::yy_print_ (std::ostream& yyo, const basic_symbol<Base>& yysym) const
  {
    std::ostream& yyoutput = yyo;
    YY_USE (yyoutput);
    if (yysym.empty ())
      yyo << "empty symbol";
    else
      {
        symbol_kind_type yykind = yysym.kind ();
        yyo << (yykind < YYNTOKENS ? "token" : "nterm")
            << ' ' << yysym.name () << " ("
            << yysym.location << ": ";
        YY_USE (yykind);
        yyo << ')';
      }
  }
#endif

  void
  Parser::yypush_ (const char* m, YY_MOVE_REF (stack_symbol_type) sym)
  {
    if (m)
      YY_SYMBOL_PRINT (m, sym);
    yystack_.push (YY_MOVE (sym));
  }

  void
  Parser::yypush_ (const char* m, state_type s, YY_MOVE_REF (symbol_type) sym)
  {
#if 201103L <= YY_CPLUSPLUS
    yypush_ (m, stack_symbol_type (s, std::move (sym)));
#else
    stack_symbol_type ss (s, sym);
    yypush_ (m, ss);
#endif
  }

  void
  Parser::yypop_ (int n) YY_NOEXCEPT
  {
    yystack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  Parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  Parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  Parser::debug_level_type
  Parser::debug_level () const
  {
    return yydebug_;
  }

  void
  Parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif // YYDEBUG

  Parser::state_type
  Parser::yy_lr_goto_state_ (state_type yystate, int yysym)
  {
    int yyr = yypgoto_[yysym - YYNTOKENS] + yystate;
    if (0 <= yyr && yyr <= yylast_ && yycheck_[yyr] == yystate)
      return yytable_[yyr];
    else
      return yydefgoto_[yysym - YYNTOKENS];
  }

  bool
  Parser::yy_pact_value_is_default_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yypact_ninf_;
  }

  bool
  Parser::yy_table_value_is_error_ (int yyvalue) YY_NOEXCEPT
  {
    return yyvalue == yytable_ninf_;
  }

  int
  Parser::operator() ()
  {
    return parse ();
  }

  int
  Parser::parse ()
  {
    int yyn;
    /// Length of the RHS of the rule being reduced.
    int yylen = 0;

    // Error handling.
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// The lookahead symbol.
    symbol_type yyla;

    /// The locations where the error started and ended.
    stack_symbol_type yyerror_range[3];

    /// The return value of parse ().
    int yyresult;

#if YY_EXCEPTIONS
    try
#endif // YY_EXCEPTIONS
      {
    YYCDEBUG << "Starting parse\n";


    /* Initialize the stack.  The initial state will be set in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystack_.clear ();
    yypush_ (YY_NULLPTR, 0, YY_MOVE (yyla));

  /*-----------------------------------------------.
  | yynewstate -- push a new symbol on the stack.  |
  `-----------------------------------------------*/
  yynewstate:
    YYCDEBUG << "Entering state " << int (yystack_[0].state) << '\n';
    YY_STACK_PRINT ();

    // Accept?
    if (yystack_[0].state == yyfinal_)
      YYACCEPT;

    goto yybackup;


  /*-----------.
  | yybackup.  |
  `-----------*/
  yybackup:
    // Try to take a decision without lookahead.
    yyn = yypact_[+yystack_[0].state];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    // Read a lookahead token.
    if (yyla.empty ())
      {
        YYCDEBUG << "Reading a token\n";
#if YY_EXCEPTIONS
        try
#endif // YY_EXCEPTIONS
          {
            symbol_type yylookahead (yylex (lexer));
            yyla.move (yylookahead);
          }
#if YY_EXCEPTIONS
        catch (const syntax_error& yyexc)
          {
            YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
            error (yyexc);
            goto yyerrlab1;
          }
#endif // YY_EXCEPTIONS
      }
    YY_SYMBOL_PRINT ("Next token is", yyla);

    if (yyla.kind () == symbol_kind::S_YYerror)
    {
      // The scanner already issued an error message, process directly
      // to error recovery.  But do not keep the error token as
      // lookahead, it is too special and may lead us to an endless
      // loop in error recovery. */
      yyla.kind_ = symbol_kind::S_YYUNDEF;
      goto yyerrlab1;
    }

    /* If the proper action on seeing token YYLA.TYPE is to reduce or
       to detect an error, take that action.  */
    yyn += yyla.kind ();
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yyla.kind ())
      {
        goto yydefault;
      }

    // Reduce or error.
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
        if (yy_table_value_is_error_ (yyn))
          goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
      }

    // Count tokens shifted since error; after three, turn off error status.
    if (yyerrstatus_)
      --yyerrstatus_;

    // Shift the lookahead token.
    yypush_ ("Shifting", state_type (yyn), YY_MOVE (yyla));
    goto yynewstate;


  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[+yystack_[0].state];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;


  /*-----------------------------.
  | yyreduce -- do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    {
      stack_symbol_type yylhs;
      yylhs.state = yy_lr_goto_state_ (yystack_[yylen].state, yyr1_[yyn]);
      /* Variants are always initialized to an empty instance of the
         correct type. The default '$$ = $1' action is NOT applied
         when using variants.  */
      switch (yyr1_[yyn])
    {
      case symbol_kind::S_decl: // decl
        yylhs.value.emplace< ce::lang::Decl* > ();
        break;

      case symbol_kind::S_expr: // expr
      case symbol_kind::S_or_expr: // or_expr
      case symbol_kind::S_and_expr: // and_expr
      case symbol_kind::S_eq_expr: // eq_expr
      case symbol_kind::S_rel_expr: // rel_expr
      case symbol_kind::S_add_expr: // add_expr
      case symbol_kind::S_mul_expr: // mul_expr
      case symbol_kind::S_unary_expr: // unary_expr
      case symbol_kind::S_postfix_expr: // postfix_expr
      case symbol_kind::S_primary_expr: // primary_expr
        yylhs.value.emplace< ce::lang::Expr* > ();
        break;

      case symbol_kind::S_func_decl: // func_decl
        yylhs.value.emplace< ce::lang::FuncDecl* > ();
        break;

      case symbol_kind::S_global_var_decl: // global_var_decl
        yylhs.value.emplace< ce::lang::GlobalVarDecl* > ();
        break;

      case symbol_kind::S_param: // param
        yylhs.value.emplace< ce::lang::Param > ();
        break;

      case symbol_kind::S_block: // block
      case symbol_kind::S_stmt: // stmt
      case symbol_kind::S_closed_stmt: // closed_stmt
      case symbol_kind::S_open_stmt: // open_stmt
      case symbol_kind::S_simple_stmt: // simple_stmt
      case symbol_kind::S_var_decl_stmt: // var_decl_stmt
      case symbol_kind::S_var_decl_no_semi: // var_decl_no_semi
      case symbol_kind::S_assign_stmt: // assign_stmt
      case symbol_kind::S_assign_no_semi: // assign_no_semi
      case symbol_kind::S_for_init_opt: // for_init_opt
      case symbol_kind::S_for_step_opt: // for_step_opt
        yylhs.value.emplace< ce::lang::Stmt* > ();
        break;

      case symbol_kind::S_FLOAT_LITERAL: // FLOAT_LITERAL
        yylhs.value.emplace< float > ();
        break;

      case symbol_kind::S_INT_LITERAL: // INT_LITERAL
        yylhs.value.emplace< int64_t > ();
        break;

      case symbol_kind::S_STRING_LITERAL: // STRING_LITERAL
      case symbol_kind::S_IDENT: // IDENT
        yylhs.value.emplace< std::string > ();
        break;

      case symbol_kind::S_decl_list: // decl_list
        yylhs.value.emplace< std::vector<ce::lang::Decl*> > ();
        break;

      case symbol_kind::S_arg_list_opt: // arg_list_opt
      case symbol_kind::S_arg_list: // arg_list
        yylhs.value.emplace< std::vector<ce::lang::Expr*> > ();
        break;

      case symbol_kind::S_param_list_opt: // param_list_opt
      case symbol_kind::S_param_list: // param_list
        yylhs.value.emplace< std::vector<ce::lang::Param> > ();
        break;

      case symbol_kind::S_stmt_list: // stmt_list
        yylhs.value.emplace< std::vector<ce::lang::Stmt*> > ();
        break;

      default:
        break;
    }


      // Default location.
      {
        stack_type::slice range (yystack_, yylen);
        YYLLOC_DEFAULT (yylhs.location, range, yylen);
        yyerror_range[1].location = yylhs.location;
      }

      // Perform the reduction.
      YY_REDUCE_PRINT (yyn);
#if YY_EXCEPTIONS
      try
#endif // YY_EXCEPTIONS
        {
          switch (yyn)
            {
  case 2: // program: decl_list
#line 89 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
              {
        Program* prog = arena.NewProgram();
        prog->decls = std::move(yystack_[0].value.as < std::vector<ce::lang::Decl*> > ());
        result = prog;
    }
#line 925 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 3: // decl_list: %empty
#line 97 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
           { yylhs.value.as < std::vector<ce::lang::Decl*> > () = {}; }
#line 931 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 4: // decl_list: decl_list decl
#line 98 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   { yylhs.value.as < std::vector<ce::lang::Decl*> > () = std::move(yystack_[1].value.as < std::vector<ce::lang::Decl*> > ()); yylhs.value.as < std::vector<ce::lang::Decl*> > ().push_back(yystack_[0].value.as < ce::lang::Decl* > ()); }
#line 937 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 5: // decl: global_var_decl
#line 102 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Decl* > () = arena.NewDecl(DeclKind::Var); yylhs.value.as < ce::lang::Decl* > ()->varDecl = yystack_[0].value.as < ce::lang::GlobalVarDecl* > (); }
#line 943 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 6: // decl: func_decl
#line 103 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Decl* > () = arena.NewDecl(DeclKind::Func); yylhs.value.as < ce::lang::Decl* > ()->funcDecl = yystack_[0].value.as < ce::lang::FuncDecl* > (); }
#line 949 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 7: // global_var_decl: "var" IDENT ":" IDENT "=" expr ";"
#line 107 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                       {
        yylhs.value.as < ce::lang::GlobalVarDecl* > () = arena.NewGlobalVarDecl();
        yylhs.value.as < ce::lang::GlobalVarDecl* > ()->name = yystack_[5].value.as < std::string > (); yylhs.value.as < ce::lang::GlobalVarDecl* > ()->declaredType = yystack_[3].value.as < std::string > (); yylhs.value.as < ce::lang::GlobalVarDecl* > ()->initExpr = yystack_[1].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::GlobalVarDecl* > ()->loc = ToSourceLocation(yystack_[6].location);
    }
#line 958 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 8: // global_var_decl: "var" IDENT "=" expr ";"
#line 111 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                             {
        yylhs.value.as < ce::lang::GlobalVarDecl* > () = arena.NewGlobalVarDecl();
        yylhs.value.as < ce::lang::GlobalVarDecl* > ()->name = yystack_[3].value.as < std::string > (); yylhs.value.as < ce::lang::GlobalVarDecl* > ()->initExpr = yystack_[1].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::GlobalVarDecl* > ()->loc = ToSourceLocation(yystack_[4].location);
    }
#line 967 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 9: // func_decl: "func" IDENT "(" param_list_opt ")" "->" IDENT block
#line 118 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                                         {
        yylhs.value.as < ce::lang::FuncDecl* > () = arena.NewFuncDecl();
        yylhs.value.as < ce::lang::FuncDecl* > ()->name = yystack_[6].value.as < std::string > (); yylhs.value.as < ce::lang::FuncDecl* > ()->params = std::move(yystack_[4].value.as < std::vector<ce::lang::Param> > ()); yylhs.value.as < ce::lang::FuncDecl* > ()->returnType = yystack_[1].value.as < std::string > (); yylhs.value.as < ce::lang::FuncDecl* > ()->body = yystack_[0].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::FuncDecl* > ()->loc = ToSourceLocation(yystack_[7].location);
    }
#line 976 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 10: // func_decl: "func" IDENT "(" param_list_opt ")" block
#line 122 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                              {
        yylhs.value.as < ce::lang::FuncDecl* > () = arena.NewFuncDecl();
        yylhs.value.as < ce::lang::FuncDecl* > ()->name = yystack_[4].value.as < std::string > (); yylhs.value.as < ce::lang::FuncDecl* > ()->params = std::move(yystack_[2].value.as < std::vector<ce::lang::Param> > ()); yylhs.value.as < ce::lang::FuncDecl* > ()->body = yystack_[0].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::FuncDecl* > ()->loc = ToSourceLocation(yystack_[5].location);
    }
#line 985 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 11: // param_list_opt: %empty
#line 129 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
               { yylhs.value.as < std::vector<ce::lang::Param> > () = {}; }
#line 991 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 12: // param_list_opt: param_list
#line 130 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
               { yylhs.value.as < std::vector<ce::lang::Param> > () = std::move(yystack_[0].value.as < std::vector<ce::lang::Param> > ()); }
#line 997 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 13: // param_list: param
#line 134 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                           { yylhs.value.as < std::vector<ce::lang::Param> > () = {}; yylhs.value.as < std::vector<ce::lang::Param> > ().push_back(std::move(yystack_[0].value.as < ce::lang::Param > ())); }
#line 1003 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 14: // param_list: param_list "," param
#line 135 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                           { yylhs.value.as < std::vector<ce::lang::Param> > () = std::move(yystack_[2].value.as < std::vector<ce::lang::Param> > ()); yylhs.value.as < std::vector<ce::lang::Param> > ().push_back(std::move(yystack_[0].value.as < ce::lang::Param > ())); }
#line 1009 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 15: // param: IDENT ":" IDENT
#line 139 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Param > () = Param{ yystack_[2].value.as < std::string > (), yystack_[0].value.as < std::string > (), ToSourceLocation(yystack_[2].location) }; }
#line 1015 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 16: // block: "{" stmt_list "}"
#line 143 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                      {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Block, ToSourceLocation(yystack_[2].location));
        yylhs.value.as < ce::lang::Stmt* > ()->statements = std::move(yystack_[1].value.as < std::vector<ce::lang::Stmt*> > ());
    }
#line 1024 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 17: // stmt_list: %empty
#line 150 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                        { yylhs.value.as < std::vector<ce::lang::Stmt*> > () = {}; }
#line 1030 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 18: // stmt_list: stmt_list stmt
#line 151 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                         { yylhs.value.as < std::vector<ce::lang::Stmt*> > () = std::move(yystack_[1].value.as < std::vector<ce::lang::Stmt*> > ()); yylhs.value.as < std::vector<ce::lang::Stmt*> > ().push_back(yystack_[0].value.as < ce::lang::Stmt* > ()); }
#line 1036 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 19: // stmt: open_stmt
#line 163 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1042 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 20: // stmt: closed_stmt
#line 164 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1048 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 21: // closed_stmt: simple_stmt
#line 168 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1054 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 22: // closed_stmt: block
#line 169 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1060 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 23: // closed_stmt: "if" "(" expr ")" closed_stmt "else" closed_stmt
#line 170 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                                     {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::If, ToSourceLocation(yystack_[6].location));
        yylhs.value.as < ce::lang::Stmt* > ()->condition = yystack_[4].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->thenBranch = yystack_[2].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::Stmt* > ()->elseBranch = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1069 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 24: // closed_stmt: "while" "(" expr ")" closed_stmt
#line 174 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                     {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::While, ToSourceLocation(yystack_[4].location));
        yylhs.value.as < ce::lang::Stmt* > ()->condition = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->body = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1078 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 25: // closed_stmt: "for" "(" for_init_opt ";" expr ";" for_step_opt ")" closed_stmt
#line 178 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                                                     {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::For, ToSourceLocation(yystack_[8].location));
        yylhs.value.as < ce::lang::Stmt* > ()->forInit = yystack_[6].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::Stmt* > ()->forCond = yystack_[4].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->forStep = yystack_[2].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::Stmt* > ()->body = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1087 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 26: // open_stmt: "if" "(" expr ")" stmt
#line 185 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                           {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::If, ToSourceLocation(yystack_[4].location));
        yylhs.value.as < ce::lang::Stmt* > ()->condition = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->thenBranch = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1096 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 27: // open_stmt: "if" "(" expr ")" closed_stmt "else" open_stmt
#line 189 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::If, ToSourceLocation(yystack_[6].location));
        yylhs.value.as < ce::lang::Stmt* > ()->condition = yystack_[4].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->thenBranch = yystack_[2].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::Stmt* > ()->elseBranch = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1105 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 28: // open_stmt: "while" "(" expr ")" open_stmt
#line 193 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::While, ToSourceLocation(yystack_[4].location));
        yylhs.value.as < ce::lang::Stmt* > ()->condition = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->body = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1114 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 29: // open_stmt: "for" "(" for_init_opt ";" expr ";" for_step_opt ")" open_stmt
#line 197 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                                                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::For, ToSourceLocation(yystack_[8].location));
        yylhs.value.as < ce::lang::Stmt* > ()->forInit = yystack_[6].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::Stmt* > ()->forCond = yystack_[4].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->forStep = yystack_[2].value.as < ce::lang::Stmt* > (); yylhs.value.as < ce::lang::Stmt* > ()->body = yystack_[0].value.as < ce::lang::Stmt* > ();
    }
#line 1123 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 30: // simple_stmt: var_decl_stmt
#line 204 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1129 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 31: // simple_stmt: assign_stmt
#line 205 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1135 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 32: // simple_stmt: "break" ";"
#line 206 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Break, ToSourceLocation(yystack_[1].location)); }
#line 1141 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 33: // simple_stmt: "continue" ";"
#line 207 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Continue, ToSourceLocation(yystack_[1].location)); }
#line 1147 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 34: // simple_stmt: "return" ";"
#line 208 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Return, ToSourceLocation(yystack_[1].location)); }
#line 1153 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 35: // simple_stmt: "return" expr ";"
#line 209 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                      {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Return, ToSourceLocation(yystack_[2].location));
        yylhs.value.as < ce::lang::Stmt* > ()->returnValue = yystack_[1].value.as < ce::lang::Expr* > ();
    }
#line 1162 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 36: // simple_stmt: expr ";"
#line 213 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
             {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::ExprStmt, yystack_[1].value.as < ce::lang::Expr* > ()->loc);
        yylhs.value.as < ce::lang::Stmt* > ()->expr = yystack_[1].value.as < ce::lang::Expr* > ();
    }
#line 1171 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 37: // var_decl_stmt: var_decl_no_semi ";"
#line 220 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                         { yylhs.value.as < ce::lang::Stmt* > () = yystack_[1].value.as < ce::lang::Stmt* > (); }
#line 1177 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 38: // var_decl_no_semi: "var" IDENT ":" IDENT "=" expr
#line 224 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::VarDecl, ToSourceLocation(yystack_[5].location));
        yylhs.value.as < ce::lang::Stmt* > ()->name = yystack_[4].value.as < std::string > (); yylhs.value.as < ce::lang::Stmt* > ()->declaredType = yystack_[2].value.as < std::string > (); yylhs.value.as < ce::lang::Stmt* > ()->initExpr = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1186 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 39: // var_decl_no_semi: "var" IDENT "=" expr
#line 228 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                         {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::VarDecl, ToSourceLocation(yystack_[3].location));
        yylhs.value.as < ce::lang::Stmt* > ()->name = yystack_[2].value.as < std::string > (); yylhs.value.as < ce::lang::Stmt* > ()->initExpr = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1195 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 40: // assign_stmt: assign_no_semi ";"
#line 235 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                       { yylhs.value.as < ce::lang::Stmt* > () = yystack_[1].value.as < ce::lang::Stmt* > (); }
#line 1201 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 41: // assign_no_semi: expr "=" expr
#line 257 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                  {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Assign, yystack_[2].value.as < ce::lang::Expr* > ()->loc);
        yylhs.value.as < ce::lang::Stmt* > ()->assignTarget = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->assignOp = AssignOp::Assign; yylhs.value.as < ce::lang::Stmt* > ()->assignValue = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1210 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 42: // assign_no_semi: expr "+=" expr
#line 261 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Assign, yystack_[2].value.as < ce::lang::Expr* > ()->loc);
        yylhs.value.as < ce::lang::Stmt* > ()->assignTarget = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->assignOp = AssignOp::AddAssign; yylhs.value.as < ce::lang::Stmt* > ()->assignValue = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1219 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 43: // assign_no_semi: expr "-=" expr
#line 265 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Assign, yystack_[2].value.as < ce::lang::Expr* > ()->loc);
        yylhs.value.as < ce::lang::Stmt* > ()->assignTarget = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->assignOp = AssignOp::SubAssign; yylhs.value.as < ce::lang::Stmt* > ()->assignValue = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1228 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 44: // assign_no_semi: expr "*=" expr
#line 269 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Assign, yystack_[2].value.as < ce::lang::Expr* > ()->loc);
        yylhs.value.as < ce::lang::Stmt* > ()->assignTarget = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->assignOp = AssignOp::MulAssign; yylhs.value.as < ce::lang::Stmt* > ()->assignValue = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1237 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 45: // assign_no_semi: expr "/=" expr
#line 273 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Stmt* > () = arena.NewStmt(StmtKind::Assign, yystack_[2].value.as < ce::lang::Expr* > ()->loc);
        yylhs.value.as < ce::lang::Stmt* > ()->assignTarget = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Stmt* > ()->assignOp = AssignOp::DivAssign; yylhs.value.as < ce::lang::Stmt* > ()->assignValue = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1246 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 46: // for_init_opt: %empty
#line 280 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                      { yylhs.value.as < ce::lang::Stmt* > () = nullptr; }
#line 1252 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 47: // for_init_opt: var_decl_no_semi
#line 281 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                      { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1258 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 48: // for_init_opt: assign_no_semi
#line 282 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                      { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1264 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 49: // for_step_opt: %empty
#line 286 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = nullptr; }
#line 1270 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 50: // for_step_opt: assign_no_semi
#line 287 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                    { yylhs.value.as < ce::lang::Stmt* > () = yystack_[0].value.as < ce::lang::Stmt* > (); }
#line 1276 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 51: // arg_list_opt: %empty
#line 291 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
              { yylhs.value.as < std::vector<ce::lang::Expr*> > () = {}; }
#line 1282 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 52: // arg_list_opt: arg_list
#line 292 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
              { yylhs.value.as < std::vector<ce::lang::Expr*> > () = std::move(yystack_[0].value.as < std::vector<ce::lang::Expr*> > ()); }
#line 1288 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 53: // arg_list: expr
#line 296 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                        { yylhs.value.as < std::vector<ce::lang::Expr*> > () = {}; yylhs.value.as < std::vector<ce::lang::Expr*> > ().push_back(yystack_[0].value.as < ce::lang::Expr* > ()); }
#line 1294 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 54: // arg_list: arg_list "," expr
#line 297 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                        { yylhs.value.as < std::vector<ce::lang::Expr*> > () = std::move(yystack_[2].value.as < std::vector<ce::lang::Expr*> > ()); yylhs.value.as < std::vector<ce::lang::Expr*> > ().push_back(yystack_[0].value.as < ce::lang::Expr* > ()); }
#line 1300 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 55: // expr: or_expr
#line 309 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
              { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1306 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 56: // or_expr: or_expr "||" and_expr
#line 312 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Or; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1314 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 57: // or_expr: and_expr
#line 315 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
             { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1320 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 58: // and_expr: and_expr "&&" eq_expr
#line 319 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::And; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1328 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 59: // and_expr: eq_expr
#line 322 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
            { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1334 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 60: // eq_expr: eq_expr "==" rel_expr
#line 326 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Eq; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1342 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 61: // eq_expr: eq_expr "!=" rel_expr
#line 329 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Neq; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1350 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 62: // eq_expr: rel_expr
#line 332 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
             { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1356 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 63: // rel_expr: rel_expr "<" add_expr
#line 336 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Lt; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1364 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 64: // rel_expr: rel_expr ">" add_expr
#line 339 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Gt; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1372 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 65: // rel_expr: rel_expr "<=" add_expr
#line 342 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                           {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Le; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1380 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 66: // rel_expr: rel_expr ">=" add_expr
#line 345 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                           {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Ge; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1388 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 67: // rel_expr: add_expr
#line 348 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
             { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1394 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 68: // add_expr: add_expr "+" mul_expr
#line 352 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Add; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1402 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 69: // add_expr: add_expr "-" mul_expr
#line 355 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Sub; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1410 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 70: // add_expr: mul_expr
#line 358 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
             { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1416 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 71: // mul_expr: mul_expr "*" unary_expr
#line 362 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                            {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Mul; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1424 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 72: // mul_expr: mul_expr "/" unary_expr
#line 365 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                            {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Div; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1432 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 73: // mul_expr: mul_expr "%" unary_expr
#line 368 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                            {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Binary, ToSourceLocation(yystack_[2].location)); yylhs.value.as < ce::lang::Expr* > ()->binaryOp = BinaryOp::Mod; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->rhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1440 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 74: // mul_expr: unary_expr
#line 371 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
               { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1446 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 75: // unary_expr: "-" unary_expr
#line 375 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Unary, ToSourceLocation(yystack_[1].location)); yylhs.value.as < ce::lang::Expr* > ()->unaryOp = UnaryOp::Neg; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1454 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 76: // unary_expr: "!" unary_expr
#line 378 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Unary, ToSourceLocation(yystack_[1].location)); yylhs.value.as < ce::lang::Expr* > ()->unaryOp = UnaryOp::Not; yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[0].value.as < ce::lang::Expr* > ();
    }
#line 1462 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 77: // unary_expr: postfix_expr
#line 381 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                 { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1468 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 78: // postfix_expr: postfix_expr "." IDENT
#line 385 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                           {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Member, yystack_[2].value.as < ce::lang::Expr* > ()->loc); yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[2].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->text = yystack_[0].value.as < std::string > ();
    }
#line 1476 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 79: // postfix_expr: postfix_expr "(" arg_list_opt ")"
#line 388 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                                      {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Call, yystack_[3].value.as < ce::lang::Expr* > ()->loc); yylhs.value.as < ce::lang::Expr* > ()->lhs = yystack_[3].value.as < ce::lang::Expr* > (); yylhs.value.as < ce::lang::Expr* > ()->args = std::move(yystack_[1].value.as < std::vector<ce::lang::Expr*> > ());
    }
#line 1484 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 80: // postfix_expr: primary_expr
#line 391 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                 { yylhs.value.as < ce::lang::Expr* > () = yystack_[0].value.as < ce::lang::Expr* > (); }
#line 1490 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 81: // primary_expr: "(" expr ")"
#line 395 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                 { yylhs.value.as < ce::lang::Expr* > () = yystack_[1].value.as < ce::lang::Expr* > (); }
#line 1496 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 82: // primary_expr: IDENT
#line 396 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
          {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::Identifier, ToSourceLocation(yystack_[0].location)); yylhs.value.as < ce::lang::Expr* > ()->text = yystack_[0].value.as < std::string > ();
    }
#line 1504 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 83: // primary_expr: INT_LITERAL
#line 399 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::IntLiteral, ToSourceLocation(yystack_[0].location)); yylhs.value.as < ce::lang::Expr* > ()->intValue = yystack_[0].value.as < int64_t > ();
    }
#line 1512 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 84: // primary_expr: FLOAT_LITERAL
#line 402 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                  {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::FloatLiteral, ToSourceLocation(yystack_[0].location)); yylhs.value.as < ce::lang::Expr* > ()->floatValue = yystack_[0].value.as < float > ();
    }
#line 1520 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 85: // primary_expr: STRING_LITERAL
#line 405 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
                   {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::StringLiteral, ToSourceLocation(yystack_[0].location)); yylhs.value.as < ce::lang::Expr* > ()->text = yystack_[0].value.as < std::string > ();
    }
#line 1528 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 86: // primary_expr: "true"
#line 408 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
           {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::BoolLiteral, ToSourceLocation(yystack_[0].location)); yylhs.value.as < ce::lang::Expr* > ()->boolValue = true;
    }
#line 1536 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;

  case 87: // primary_expr: "false"
#line 411 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
            {
        yylhs.value.as < ce::lang::Expr* > () = arena.NewExpr(ExprKind::BoolLiteral, ToSourceLocation(yystack_[0].location)); yylhs.value.as < ce::lang::Expr* > ()->boolValue = false;
    }
#line 1544 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"
    break;


#line 1548 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"

            default:
              break;
            }
        }
#if YY_EXCEPTIONS
      catch (const syntax_error& yyexc)
        {
          YYCDEBUG << "Caught exception: " << yyexc.what() << '\n';
          error (yyexc);
          YYERROR;
        }
#endif // YY_EXCEPTIONS
      YY_SYMBOL_PRINT ("-> $$ =", yylhs);
      yypop_ (yylen);
      yylen = 0;

      // Shift the result of the reduction.
      yypush_ (YY_NULLPTR, YY_MOVE (yylhs));
    }
    goto yynewstate;


  /*--------------------------------------.
  | yyerrlab -- here on detecting error.  |
  `--------------------------------------*/
  yyerrlab:
    // If not already recovering from an error, report this error.
    if (!yyerrstatus_)
      {
        ++yynerrs_;
        context yyctx (*this, yyla);
        std::string msg = yysyntax_error_ (yyctx);
        error (yyla.location, YY_MOVE (msg));
      }


    yyerror_range[1].location = yyla.location;
    if (yyerrstatus_ == 3)
      {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        // Return failure if at end of input.
        if (yyla.kind () == symbol_kind::S_YYEOF)
          YYABORT;
        else if (!yyla.empty ())
          {
            yy_destroy_ ("Error: discarding", yyla);
            yyla.clear ();
          }
      }

    // Else will try to reuse lookahead token after shifting the error token.
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and
       the label yyerrorlab therefore never appears in user code.  */
    if (false)
      YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();
    goto yyerrlab1;


  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;   // Each real token shifted decrements this.
    // Pop stack until we find a state that shifts the error token.
    for (;;)
      {
        yyn = yypact_[+yystack_[0].state];
        if (!yy_pact_value_is_default_ (yyn))
          {
            yyn += symbol_kind::S_YYerror;
            if (0 <= yyn && yyn <= yylast_
                && yycheck_[yyn] == symbol_kind::S_YYerror)
              {
                yyn = yytable_[yyn];
                if (0 < yyn)
                  break;
              }
          }

        // Pop the current state because it cannot handle the error token.
        if (yystack_.size () == 1)
          YYABORT;

        yyerror_range[1].location = yystack_[0].location;
        yy_destroy_ ("Error: popping", yystack_[0]);
        yypop_ ();
        YY_STACK_PRINT ();
      }
    {
      stack_symbol_type error_token;

      yyerror_range[2].location = yyla.location;
      YYLLOC_DEFAULT (error_token.location, yyerror_range, 2);

      // Shift the error token.
      error_token.state = state_type (yyn);
      yypush_ ("Shifting", YY_MOVE (error_token));
    }
    goto yynewstate;


  /*-------------------------------------.
  | yyacceptlab -- YYACCEPT comes here.  |
  `-------------------------------------*/
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;


  /*-----------------------------------.
  | yyabortlab -- YYABORT comes here.  |
  `-----------------------------------*/
  yyabortlab:
    yyresult = 1;
    goto yyreturn;


  /*-----------------------------------------------------.
  | yyreturn -- parsing is finished, return the result.  |
  `-----------------------------------------------------*/
  yyreturn:
    if (!yyla.empty ())
      yy_destroy_ ("Cleanup: discarding lookahead", yyla);

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    YY_STACK_PRINT ();
    while (1 < yystack_.size ())
      {
        yy_destroy_ ("Cleanup: popping", yystack_[0]);
        yypop_ ();
      }

    return yyresult;
  }
#if YY_EXCEPTIONS
    catch (...)
      {
        YYCDEBUG << "Exception caught: cleaning lookahead and stack\n";
        // Do not try to display the values of the reclaimed symbols,
        // as their printers might throw an exception.
        if (!yyla.empty ())
          yy_destroy_ (YY_NULLPTR, yyla);

        while (1 < yystack_.size ())
          {
            yy_destroy_ (YY_NULLPTR, yystack_[0]);
            yypop_ ();
          }
        throw;
      }
#endif // YY_EXCEPTIONS
  }

  void
  Parser::error (const syntax_error& yyexc)
  {
    error (yyexc.location, yyexc.what ());
  }

  const char *
  Parser::symbol_name (symbol_kind_type yysymbol)
  {
    static const char *const yy_sname[] =
    {
    "end of file", "error", "invalid token", "var", "func", "if", "else",
  "while", "for", "break", "continue", "return", "true", "false", "{", "}",
  "(", ")", ",", ";", ":", ".", "->", "=", "+=", "-=", "*=", "/=", "==",
  "!=", "<", ">", "<=", ">=", "&&", "||", "+", "-", "*", "/", "%", "!",
  "INT_LITERAL", "FLOAT_LITERAL", "STRING_LITERAL", "IDENT", "$accept",
  "program", "decl_list", "decl", "global_var_decl", "func_decl",
  "param_list_opt", "param_list", "param", "block", "stmt_list", "stmt",
  "closed_stmt", "open_stmt", "simple_stmt", "var_decl_stmt",
  "var_decl_no_semi", "assign_stmt", "assign_no_semi", "for_init_opt",
  "for_step_opt", "arg_list_opt", "arg_list", "expr", "or_expr",
  "and_expr", "eq_expr", "rel_expr", "add_expr", "mul_expr", "unary_expr",
  "postfix_expr", "primary_expr", YY_NULLPTR
    };
    return yy_sname[yysymbol];
  }



  // Parser::context.
  Parser::context::context (const Parser& yyparser, const symbol_type& yyla)
    : yyparser_ (yyparser)
    , yyla_ (yyla)
  {}

  int
  Parser::context::expected_tokens (symbol_kind_type yyarg[], int yyargn) const
  {
    // Actual number of expected tokens
    int yycount = 0;

    const int yyn = yypact_[+yyparser_.yystack_[0].state];
    if (!yy_pact_value_is_default_ (yyn))
      {
        /* Start YYX at -YYN if negative to avoid negative indexes in
           YYCHECK.  In other words, skip the first -YYN actions for
           this state because they are default actions.  */
        const int yyxbegin = yyn < 0 ? -yyn : 0;
        // Stay within bounds of both yycheck and yytname.
        const int yychecklim = yylast_ - yyn + 1;
        const int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
        for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
          if (yycheck_[yyx + yyn] == yyx && yyx != symbol_kind::S_YYerror
              && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
            {
              if (!yyarg)
                ++yycount;
              else if (yycount == yyargn)
                return 0;
              else
                yyarg[yycount++] = YY_CAST (symbol_kind_type, yyx);
            }
      }

    if (yyarg && yycount == 0 && 0 < yyargn)
      yyarg[0] = symbol_kind::S_YYEMPTY;
    return yycount;
  }






  int
  Parser::yy_syntax_error_arguments_ (const context& yyctx,
                                                 symbol_kind_type yyarg[], int yyargn) const
  {
    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yyla) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yyla.  (However, yyla is currently not documented for users.)
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state merging
         (from LALR or IELR) and default reductions corrupt the expected
         token list.  However, the list is correct for canonical LR with
         one exception: it will still contain any token that will not be
         accepted due to an error action in a later state.
    */

    if (!yyctx.lookahead ().empty ())
      {
        if (yyarg)
          yyarg[0] = yyctx.token ();
        int yyn = yyctx.expected_tokens (yyarg ? yyarg + 1 : yyarg, yyargn - 1);
        return yyn + 1;
      }
    return 0;
  }

  // Generate an error message.
  std::string
  Parser::yysyntax_error_ (const context& yyctx) const
  {
    // Its maximum.
    enum { YYARGS_MAX = 5 };
    // Arguments of yyformat.
    symbol_kind_type yyarg[YYARGS_MAX];
    int yycount = yy_syntax_error_arguments_ (yyctx, yyarg, YYARGS_MAX);

    char const* yyformat = YY_NULLPTR;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
      default: // Avoid compiler warnings.
        YYCASE_ (0, YY_("syntax error"));
        YYCASE_ (1, YY_("syntax error, unexpected %s"));
        YYCASE_ (2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_ (3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_ (4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_ (5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    std::string yyres;
    // Argument number.
    std::ptrdiff_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += symbol_name (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  const signed char Parser::yypact_ninf_ = -111;

  const signed char Parser::yytable_ninf_ = -1;

  const short
  Parser::yypact_[] =
  {
    -111,     2,    56,  -111,   -41,   -37,  -111,  -111,  -111,     1,
       6,   -10,   111,    -6,    31,  -111,  -111,   111,   111,   111,
    -111,  -111,  -111,  -111,    52,    60,    53,    88,    98,    71,
     100,  -111,   -15,  -111,   124,    74,   127,  -111,   111,   129,
    -111,  -111,  -111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   102,   104,    36,
      -6,    78,  -111,    53,    88,    98,    98,    71,    71,    71,
      71,   100,   100,  -111,  -111,  -111,   133,   139,  -111,  -111,
    -111,  -111,   106,  -111,  -111,  -111,  -111,   111,     4,   144,
    -111,   114,   145,   146,   147,   141,   148,    77,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,   149,  -111,   150,    79,  -111,
      33,   111,   111,    39,  -111,  -111,  -111,   151,  -111,  -111,
    -111,   111,   111,   111,   111,   111,   119,   111,   154,   155,
    -111,  -111,   156,    43,  -111,  -111,  -111,  -111,  -111,  -111,
     142,  -111,    20,    20,   111,   111,  -111,   160,  -111,  -111,
     157,  -111,    20,   111,  -111,  -111,  -111,   161,    20,  -111,
    -111
  };

  const signed char
  Parser::yydefact_[] =
  {
       3,     0,     2,     1,     0,     0,     4,     5,     6,     0,
       0,     0,     0,    11,     0,    86,    87,     0,     0,     0,
      83,    84,    85,    82,     0,    55,    57,    59,    62,    67,
      70,    74,    77,    80,     0,     0,    12,    13,     0,     0,
      75,    76,     8,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    51,     0,     0,     0,
       0,     0,    81,    56,    58,    60,    61,    63,    64,    65,
      66,    68,    69,    71,    72,    73,     0,    52,    53,    78,
      15,    17,     0,    10,    14,     7,    79,     0,     0,     0,
      54,     0,     0,     0,     0,     0,     0,     0,    16,    22,
      18,    20,    19,    21,    30,     0,    31,     0,     0,     9,
       0,     0,     0,    46,    32,    33,    34,     0,    37,    40,
      36,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      47,    48,     0,     0,    35,    41,    42,    43,    44,    45,
       0,    39,     0,     0,     0,     0,    26,    20,    24,    28,
       0,    38,     0,    49,    23,    27,    50,     0,     0,    25,
      29
  };

  const short
  Parser::yypgoto_[] =
  {
    -111,  -111,  -111,  -111,  -111,  -111,  -111,  -111,   113,   -49,
    -111,    32,   -64,   -66,  -111,  -111,    64,  -111,  -110,  -111,
    -111,  -111,  -111,   -12,  -111,   136,   137,    80,    87,    91,
      19,  -111,  -111
  };

  const unsigned char
  Parser::yydefgoto_[] =
  {
       0,     1,     2,     6,     7,     8,    35,    36,    37,    99,
      88,   100,   101,   102,   103,   104,   105,   106,   107,   132,
     157,    76,    77,   108,    25,    26,    27,    28,    29,    30,
      31,    32,    33
  };

  const unsigned char
  Parser::yytable_[] =
  {
      24,    56,     3,   131,     9,    39,    57,    91,    10,    92,
      83,    93,    94,    95,    96,    97,    15,    16,    81,    98,
      17,    11,    13,    91,    12,    92,    61,    93,    94,    95,
      96,    97,    15,    16,    81,    14,    17,    40,    41,    34,
     109,    18,    91,   156,    78,    19,    20,    21,    22,    23,
      81,    15,    16,   126,    38,    17,   127,    18,    82,     4,
       5,    19,    20,    21,    22,    23,   121,   122,   123,   124,
     125,    42,    73,    74,    75,    90,    18,   149,   147,   148,
      19,    20,    21,    22,    23,   117,   155,    44,   154,    15,
      16,    59,   160,    17,   159,    43,   116,    85,   120,   128,
     129,   133,   121,   122,   123,   124,   125,    51,    52,   135,
     136,   137,   138,   139,    18,   141,    45,    46,    19,    20,
      21,    22,    23,    15,    16,    65,    66,    17,    47,    48,
      49,    50,   150,   151,    67,    68,    69,    70,    53,    54,
      55,   133,    71,    72,    58,    60,    62,    79,    18,    80,
      86,    89,    19,    20,    21,    22,    23,    87,    81,   110,
     114,   111,   112,   113,   140,   145,   152,   115,   118,   119,
     134,   142,   143,    84,   146,   144,   153,   130,   158,    63,
       0,    64
  };

  const short
  Parser::yycheck_[] =
  {
      12,    16,     0,   113,    45,    17,    21,     3,    45,     5,
      59,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    20,    16,     3,    23,     5,    38,     7,     8,     9,
      10,    11,    12,    13,    14,    45,    16,    18,    19,    45,
      89,    37,     3,   153,    56,    41,    42,    43,    44,    45,
      14,    12,    13,    20,    23,    16,    23,    37,    22,     3,
       4,    41,    42,    43,    44,    45,    23,    24,    25,    26,
      27,    19,    53,    54,    55,    87,    37,   143,   142,   143,
      41,    42,    43,    44,    45,    97,   152,    34,   152,    12,
      13,    17,   158,    16,   158,    35,    19,    19,    19,   111,
     112,   113,    23,    24,    25,    26,    27,    36,    37,   121,
     122,   123,   124,   125,    37,   127,    28,    29,    41,    42,
      43,    44,    45,    12,    13,    45,    46,    16,    30,    31,
      32,    33,   144,   145,    47,    48,    49,    50,    38,    39,
      40,   153,    51,    52,    20,    18,    17,    45,    37,    45,
      17,    45,    41,    42,    43,    44,    45,    18,    14,    45,
      19,    16,    16,    16,    45,    23,     6,    19,    19,    19,
      19,    17,    17,    60,   142,    19,    19,   113,    17,    43,
      -1,    44
  };

  const signed char
  Parser::yystos_[] =
  {
       0,    47,    48,     0,     3,     4,    49,    50,    51,    45,
      45,    20,    23,    16,    45,    12,    13,    16,    37,    41,
      42,    43,    44,    45,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    45,    52,    53,    54,    23,    69,
      76,    76,    19,    35,    34,    28,    29,    30,    31,    32,
      33,    36,    37,    38,    39,    40,    16,    21,    20,    17,
      18,    69,    17,    71,    72,    73,    73,    74,    74,    74,
      74,    75,    75,    76,    76,    76,    67,    68,    69,    45,
      45,    14,    22,    55,    54,    19,    17,    18,    56,    45,
      69,     3,     5,     7,     8,     9,    10,    11,    15,    55,
      57,    58,    59,    60,    61,    62,    63,    64,    69,    55,
      45,    16,    16,    16,    19,    19,    19,    69,    19,    19,
      19,    23,    24,    25,    26,    27,    20,    23,    69,    69,
      62,    64,    65,    69,    19,    69,    69,    69,    69,    69,
      45,    69,    17,    17,    19,    23,    57,    58,    58,    59,
      69,    69,     6,    19,    58,    59,    64,    66,    17,    58,
      59
  };

  const signed char
  Parser::yyr1_[] =
  {
       0,    46,    47,    48,    48,    49,    49,    50,    50,    51,
      51,    52,    52,    53,    53,    54,    55,    56,    56,    57,
      57,    58,    58,    58,    58,    58,    59,    59,    59,    59,
      60,    60,    60,    60,    60,    60,    60,    61,    62,    62,
      63,    64,    64,    64,    64,    64,    65,    65,    65,    66,
      66,    67,    67,    68,    68,    69,    70,    70,    71,    71,
      72,    72,    72,    73,    73,    73,    73,    73,    74,    74,
      74,    75,    75,    75,    75,    76,    76,    76,    77,    77,
      77,    78,    78,    78,    78,    78,    78,    78
  };

  const signed char
  Parser::yyr2_[] =
  {
       0,     2,     1,     0,     2,     1,     1,     7,     5,     8,
       6,     0,     1,     1,     3,     3,     3,     0,     2,     1,
       1,     1,     1,     7,     5,     9,     5,     7,     5,     9,
       1,     1,     2,     2,     2,     3,     2,     2,     6,     4,
       2,     3,     3,     3,     3,     3,     0,     1,     1,     0,
       1,     0,     1,     1,     3,     1,     3,     1,     3,     1,
       3,     3,     1,     3,     3,     3,     3,     1,     3,     3,
       1,     3,     3,     3,     1,     2,     2,     1,     3,     4,
       1,     3,     1,     1,     1,     1,     1,     1
  };




#if YYDEBUG
  const short
  Parser::yyrline_[] =
  {
       0,    89,    89,    97,    98,   102,   103,   107,   111,   118,
     122,   129,   130,   134,   135,   139,   143,   150,   151,   163,
     164,   168,   169,   170,   174,   178,   185,   189,   193,   197,
     204,   205,   206,   207,   208,   209,   213,   220,   224,   228,
     235,   257,   261,   265,   269,   273,   280,   281,   282,   286,
     287,   291,   292,   296,   297,   309,   312,   315,   319,   322,
     326,   329,   332,   336,   339,   342,   345,   348,   352,   355,
     358,   362,   365,   368,   371,   375,   378,   381,   385,   388,
     391,   395,   396,   399,   402,   405,   408,   411
  };

  void
  Parser::yy_stack_print_ () const
  {
    *yycdebug_ << "Stack now";
    for (stack_type::const_iterator
           i = yystack_.begin (),
           i_end = yystack_.end ();
         i != i_end; ++i)
      *yycdebug_ << ' ' << int (i->state);
    *yycdebug_ << '\n';
  }

  void
  Parser::yy_reduce_print_ (int yyrule) const
  {
    int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    // Print the symbols being reduced, and their result.
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
               << " (line " << yylno << "):\n";
    // The symbols being reduced.
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
                       yystack_[(yynrhs) - (yyi + 1)]);
  }
#endif // YYDEBUG


#line 5 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"
} } // ce::lang
#line 2087 "D:/CreationSuite-Workspaces/CreationSuite-Codex/apps/CreationEngine/build/CreationSuiteSharedCel/generated/parser.cpp"

#line 416 "D:/CreationSuite-Workspaces/CreationSuite-Codex/shared/CEL/grammar/cel.y"


namespace ce::lang {

void Parser::error(const location_type& loc, const std::string& msg) {
    diagnostics.Report(DiagCode::SyntaxError, DiagnosticSeverity::Error, ToSourceLocation(loc), msg);
}

} // namespace ce::lang
