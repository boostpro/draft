#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <set>
#include <cassert>

namespace {
  bool debug = false;
}

std::string           include_directory;
std::set<std::string> unrecognized_commands;

void report_unrecognized(const std::string& path,
                         const std::size_t  linenum,
                         const std::string& command,
                         const std::string& argument = "")
{
  std::set<std::string>::iterator i =
    unrecognized_commands.find(command + argument);

  if (i == unrecognized_commands.end()) {
    std::cerr << path << ':' << linenum
              << ": Warning: Unrecognized command '"
              << command;

    if (! argument.empty())
      std::cerr << '{' << argument << '}';

    std::cerr << "'" << std::endl;

    unrecognized_commands.insert(command + argument);
  }
}

class tokenizer
{
  std::istream& input;
  size_t linenum;

  enum state_t {
    STATE_NORMAL,
    STATE_LITERAL,
    STATE_TABLE
  } state;

public:
  struct token
  {
    enum kind_t {
      TOK_DIRECTIVE,
      TOK_EQUATION,
      TOK_COMMENT,
      TOK_TEXT,
      TOK_LITERAL,
      TOK_UNBREAKABLE_SPACE,
      TOK_EOF
    } kind;

    std::string text;
    std::string bracket_arg;
    std::list<std::string> brace_args;
    size_t linenum;

    token() : kind(TOK_EOF), linenum(0) {}

    void print_token(std::ostream& out) const {
      switch (kind) {
      case TOK_DIRECTIVE:
        out << "DIRECTIVE " << text << "[" << bracket_arg << "]";
        for (std::list<std::string>::const_iterator
               i = brace_args.begin();
             i != brace_args.end();
             i++)
          out << "{" << *i << "}";
        break;
      case TOK_EQUATION:
        out << "EQUATION $" << text << "$";
        break;
      case TOK_TEXT:
        out << "TEXT (" << text << ")";
        break;
      case TOK_COMMENT:
        out << "COMMENT {" << text << "}";
        break;
      case TOK_UNBREAKABLE_SPACE:
        out << "UNBREAKABLE_SPACE";
        break;
      case TOK_EOF:
        out << "EOF";
        break;
      }
    }
  };

  explicit tokenizer(std::istream& _input)
    : input(_input), state(STATE_NORMAL), linenum(1) {}

  char getchar(std::istream& in) {
    char c;
    in.get(c);
    if (c == '\n')
      linenum++;
    return c;
  }

  char arg_parser(std::istream& in, std::ostream& out,
                  char open_delim, char close_delim)
  {
    int depth = 1;
    char c;
    c = getchar(in);
    if (c != close_delim) {
      do {
        out << c;
        if (c == '\\') {
          c = getchar(in);
          out << c;
        }
        c = getchar(in);

        if (c == close_delim)
          --depth;
        else if (c == open_delim)
          ++depth;
      }
      while (! input.eof() && (c != close_delim || depth > 0));
    }

    assert(c == close_delim);
    c = getchar(in);

    return c;
  }

  std::streamoff skip_whitespace(std::istream& in, char& c)
  {
    std::streamoff pos = in.tellg();
    while (! in.eof() && (std::isspace(c) || c == '\n'))
      c = getchar(in);
    return pos;
  }

  token get_token()
  {
    token next;
    char c;
    std::ostringstream buf;

    if (input.eof()) {
      next.kind = token::TOK_EOF;
    } else {
      c = getchar(input);
      switch (c) {
      case '{':
      case '}':
      case '@':
        next.kind = token::TOK_LITERAL;
        buf << c;
        c = getchar(input);
        break;

      case '\\': {
        c = getchar(input);

        if (c == '\\') {
          next.kind = token::TOK_TEXT;
          buf << "\n";
          c = getchar(input);
          break;
        }
        else if (c == '-') {
          getchar(input);
          return get_token();
        }
        else if (c == '&' ||
                 c == ',' ||
                 c == '>' ||
                 c == '#' ||
                 c == '%' ||
                 c == '^' ||
                 c == '=') {
          next.kind = token::TOK_TEXT;
          buf << c;
          c = getchar(input);
          break;
        }
        else if (c == '{' || c == '}') {
          next.kind = token::TOK_LITERAL;
          buf << c;
          break;
        }
        else {
          next.kind = token::TOK_DIRECTIVE;
        }

        do {
          buf << c;
          c = getchar(input);
        }
        while (! input.eof() && (std::isalnum(c) || c == '_'));

        if (! input.eof() && c == '[') {
          std::ostringstream arg;
          c = arg_parser(input, arg, '[', ']');
          next.bracket_arg = arg.str();
        }
        if (! input.eof() && c == '|') {
          std::ostringstream arg;
          c = arg_parser(input, arg, '|', '|');
          next.brace_args.push_back(arg.str());
        }

        char save = c;
        size_t linenum_save = linenum;
        std::streamoff pos = skip_whitespace(input, c);

        while (! input.eof() && c == '{') {
          std::ostringstream arg;
          c = arg_parser(input, arg, '{', '}');
          next.brace_args.push_back(arg.str());

          save = c;
          linenum_save = linenum;
          pos = skip_whitespace(input, c);
        }

        next.text = buf.str();

        if (next.text != "pnum") {
          input.seekg(pos, std::ios::beg);
          c = save;
          linenum = linenum_save;
        }
        break;
      }

      case '$':
        next.kind = token::TOK_EQUATION;
        do {
          buf << c;
          c = getchar(input);
        }
        while (! input.eof() && c != '$' && c != '\n');
        break;

      case '%':
        next.kind = token::TOK_COMMENT;
        do {
          buf << c;
          c = getchar(input);
        }
        while (! input.eof() && c != '\n');
#if 0
        if (c == '\n') {
          buf << c;
          c = getchar(input);
        }
#endif
        break;

      case '&':
        if (state == STATE_TABLE) {
          next.kind = token::TOK_UNBREAKABLE_SPACE;
          buf << "\n";
          buf << "@tab";
          c = getchar(input);
          break;
        }
        // else, fall through...

      case '~':
        if (c == '~' && state != STATE_LITERAL) {
          next.kind = token::TOK_UNBREAKABLE_SPACE;
          buf << c;
          c = getchar(input);
          break;
        }
        // else, fall through...

      default:
        next.kind = token::TOK_TEXT;
        do {
          assert(c != '\0');
          buf << c;
          c = getchar(input);
        }
        while (! input.eof() &&
               c != '$' &&
               c != '%' &&
               c != '&' &&
               c != '@' &&
               c != '\\' &&
               c != '{' &&
               c != '}' &&
               c != '~');
        break;
      }

      if (! input.eof()) {
        if (c == '\n')
          linenum--;
        input.putback(c);
      }

      next.text    = buf.str();
      next.linenum = linenum;

      if (next.text == "begin") {
        if (next.brace_args.front() == "codeblock")
          state = STATE_LITERAL;
        else if (next.brace_args.front() == "tokentable" ||
                 next.brace_args.front() == "floattable")
          state = STATE_TABLE;
      }
      else if (next.text == "end") {
        if (next.brace_args.front() == "codeblock")
          state = STATE_NORMAL;
        else if (next.brace_args.front() == "tokentable" ||
                 next.brace_args.front() == "floattable")
          state = STATE_NORMAL;
      }
    }
    return next;
  }
};

class texinfo_converter
{
  std::size_t pnum;

public:
  texinfo_converter() : pnum(1) {}

  std::string process_text(const std::string& text,
                           const std::string& path = "") {
    std::istringstream stream(text);
    std::ostringstream converted;

    texinfo_converter converter;
    converter.convert(stream, converted, path);

    return converted.str();
  }

  void convert(std::istream& in, std::ostream& out,
               const std::string& path = "")
  {
    tokenizer reader(in);

    for (tokenizer::token tok = reader.get_token();
         tok.kind != tokenizer::token::TOK_EOF;
         tok = reader.get_token()) {
      if (debug) {
        tok.print_token(std::cout);
        std::cout << std::endl;
      }

      switch (tok.kind) {
      case tokenizer::token::TOK_TEXT:
      case tokenizer::token::TOK_EQUATION:
        out << tok.text;
        break;

      case tokenizer::token::TOK_LITERAL:
        out << '@' << tok.text;
        break;

      case tokenizer::token::TOK_UNBREAKABLE_SPACE:
        out << "@tie{}";
        break;

      case tokenizer::token::TOK_COMMENT:
        out << "\n";
        break;

      case tokenizer::token::TOK_DIRECTIVE:
        if (tok.text == "documentclass") {
          out << "\\input texinfo  @c -*-texinfo-*-\n";
          out << "@setfilename std.info\n";
          out << "@settitle Title\n";
          out << "@contents\n";
          out << "@ifnottex\n";
          out << "@node Top,  , (dir), (dir)\n";
          out << "@top Overview\n\n";
          out << "@insertcopying\n";
          out << "@end ifnottex\n";

        }
        else if (tok.text == "usepackage" ||
                 tok.text == "input" ||
                 tok.text == "makeindex" ||
                 tok.text == "chapterstyle" ||
                 tok.text == "pagestyle" ||
                 tok.text == "frontmatter") {
          ;                     // ignore
        }
        else if (tok.text == "hyphenation") {
          ;                     // ignore
        }
        else if (tok.text == "rSec0") {
          out << "@node\n";
          out << "@chapter "
              << process_text(tok.brace_args.front(), path) << "\n";
          out << "@anchor{" << tok.bracket_arg << "}";
          pnum = 1;
        }
        else if (tok.text == "rSec1") {
          out << "@node\n";
          out << "@section "
              << process_text(tok.brace_args.front(), path) << "\n";
          out << "@anchor{" << tok.bracket_arg << "}";
          pnum = 1;
        }
        else if (tok.text == "rSec2") {
          out << "@node\n";
          out << "@subsection "
              << process_text(tok.brace_args.front(), path) << "\n";
          out << "@anchor{" << tok.bracket_arg << "}";
          pnum = 1;
        }
        else if (tok.text == "rSec3") {
          out << "@node\n";
          out << "@subsubsection "
              << process_text(tok.brace_args.front(), path) << "\n";
          out << "@anchor{" << tok.bracket_arg << "}";
          pnum = 1;
        }
        else if (tok.text == "rSec4") {
          out << "@subsubheading "
              << process_text(tok.brace_args.front(), path) << "\n";
          out << "@anchor{" << tok.bracket_arg << "}";
          pnum = 1;
        }
        else if (tok.text == "pnum") {
          out << "@noindent " << pnum++ << ".  ";
        }
        else if (tok.text == "include") {
          std::string name = tok.brace_args.front();
          if (name != "xref") {
            std::string   include_path(include_directory + '/' +
                                       name + ".tex");
            std::ifstream file(include_path.c_str());
            if (! file.good()) {
              std::cerr << "Error: Could not open file '"
                        << include_path << "'" << std::endl;
            } else {
              texinfo_converter converter;
              converter.convert(file, out, include_path);
            }
          }
        }
        else if (tok.text == "tcode") {
          out << "@code{" << tok.brace_args.front() << "}";
        }
        else if (tok.text == "term") {
          out << "@samp{" << tok.brace_args.front() << "}";
        }
        else if (tok.text == "ref") {
          out << "@ref{" << tok.brace_args.front() << "}";
        }
        else if (tok.text == "begin") {
          if (tok.brace_args.front() == "document")
            ;
          else if (tok.brace_args.front() == "itemize")
            out << "@itemize @bullet";
          else if (tok.brace_args.front() == "enumerate")
            out << "@enumerate";
          else if (tok.brace_args.front() == "codeblock")
            out << "@example";
          else if (tok.brace_args.front() == "ncsimplebnf")
            out << "@smallexample";
          else if (tok.brace_args.front() == "ncbnftab")
            out << "@smallexample";
          else if (tok.brace_args.front() == "tokentable" ||
                   tok.brace_args.front() == "floattable")
            out << "@multitable";
          else
            report_unrecognized(path, tok.linenum, "begin",
                                tok.brace_args.front());
        }
        else if (tok.text == "item") {
          out << "@item";
        }
        else if (tok.text == "end") {
          if (tok.brace_args.front() == "document")
            ;
          else if (tok.brace_args.front() == "itemize")
            out << "@end itemize";
          else if (tok.brace_args.front() == "enumerate")
            out << "@end enumerate";
          else if (tok.brace_args.front() == "codeblock")
            out << "@end example";
          else if (tok.brace_args.front() == "ncsimplebnf")
            out << "@end smallexample";
          else if (tok.brace_args.front() == "ncbnftab")
            out << "@end smallexample";
          else if (tok.brace_args.front() == "tokentable" ||
                   tok.brace_args.front() == "floattable")
            out << "@end multitable";
          else
            report_unrecognized(path, tok.linenum, "end",
                                tok.brace_args.front());
        }
        else if (tok.text == "enterexample") {
          out << "[@emph{Example:}";
        }
        else if (tok.text == "exitexample") {
          out << "---@emph{end example}]";
        }
        else if (tok.text == "enternote") {
          out << "[@emph{Note:}";
        }
        else if (tok.text == "exitnote") {
          out << "---@emph{end note}]";
        }
        else if (tok.text == "textit") {
          out << "@emph{" << tok.brace_args.front() << "}";
        }
        else if (tok.text == "grammarterm") {
          out << "@code{" << tok.brace_args.front() << "}";
        }
        else if (tok.text == "indextext") {
          out << "@cindex " << tok.brace_args.front();
        }
        else if (tok.text == "footnote" ||
                 tok.text == "terminal") {
          if (tok.text == "footnote")
            out << "@footnote{"
                << process_text(tok.brace_args.front(), path) << "}";
          else
            out << process_text(tok.brace_args.front(), path);
        }
        else if (tok.text == "terminal") {
          out << tok.brace_args.front();
        }
        else if (tok.text == "br") {
          ;                     // ignore
        }
        else if (tok.text == "opt") {
          out << "[opt]";
        }
        else {
          report_unrecognized(path, tok.linenum, tok.text);
        }
        break;

      default:
        break;
      }
    }
  }
};

int main(int argc, char *argv[])
{
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-I") == 0) {
      include_directory = argv[++i];
    } else {
      std::ifstream file(argv[i]);
      if (! file.good()) {
        std::cerr << "Error: Could not open file '"
                  << argv[i] << "'" << std::endl;
      } else {
        texinfo_converter converter;
        converter.convert(file, std::cout, argv[i]);
      }
    }
  }
  return 0;
}
