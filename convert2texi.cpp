#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <cassert>

namespace {
  bool debug = false;
}

class tokenizer
{
  std::istream& input;

  enum state_t {
    STATE_NORMAL,
    STATE_LITERAL
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

    token() : kind(TOK_EOF) {}

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
    : input(_input), state(STATE_NORMAL) {}

  char arg_parser(std::istream& in, std::ostream& out,
                  char open_delim, char close_delim)
  {
    int depth = 1;
    char c;
    in.get(c);
    if (c != close_delim) {
      do {
        out << c;
        if (c == '\\') {
          in.get(c);
          out << c;
        }
        in.get(c);

        if (c == close_delim)
          --depth;
        else if (c == open_delim)
          ++depth;
      }
      while (! input.eof() && (c != close_delim || depth > 0));
    }

    assert(c == close_delim);
    in.get(c);

    return c;
  }

  std::streamoff skip_whitespace(std::istream& in, char& c)
  {
    std::streamoff pos = in.tellg();
    while (! in.eof() && (std::isspace(c) || c == '\n'))
      in.get(c);
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
      input.get(c);

      switch (c) {
      case '{':
      case '}':
      case '@':
        next.kind = token::TOK_LITERAL;
        next.text = c;
        break;

      case '\\': {
        input.get(c);

        if (c == '\\') {
          next.kind = token::TOK_TEXT;
          next.text = "\n";
          break;
        }
        else if (c == '{' || c == '}') {
          next.kind = token::TOK_LITERAL;
          next.text = c;
          break;
        }
        else {
          next.kind = token::TOK_DIRECTIVE;
        }

        do {
          buf << c;
          input.get(c);
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
        std::streamoff pos = skip_whitespace(input, c);

        while (! input.eof() && c == '{') {
          std::ostringstream arg;
          c = arg_parser(input, arg, '{', '}');
          next.brace_args.push_back(arg.str());

          save = c;
          pos = skip_whitespace(input, c);
        }

        input.seekg(pos, std::ios::beg);
        c = save;
        break;
      }

      case '$':
        next.kind = token::TOK_EQUATION;
        do {
          buf << c;
          input.get(c);
        }
        while (! input.eof() && c != '$');
        break;

      case '%':
        next.kind = token::TOK_COMMENT;
        do {
          buf << c;
          input.get(c);
        }
        while (! input.eof() && c != '\n');

        if (c == '\n') {
          buf << c;
          input.get(c);
        }
        break;

      case '~':
        if (state != STATE_LITERAL) {
          next.kind = token::TOK_UNBREAKABLE_SPACE;
          buf << c;
          input.get(c);
          break;
        }
        // else, fall through...

      default:
        next.kind = token::TOK_TEXT;
        do {
          assert(c != '\0');
          buf << c;
          input.get(c);
        }
        while (! input.eof() && c != '\\' && c != '%');
        break;
      }

      if (! input.eof())
        input.putback(c);

      next.text = buf.str();

      if (next.text == "begin") {
        if (next.brace_args.front() == "codeblock")
          state = STATE_LITERAL;
      }
      else if (next.text == "end") {
        if (next.brace_args.front() == "codeblock")
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
          out << "\\input texinfo  @c -*-texinfo-*-";
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
          out << "@node" << std::endl;
          out << "@chapter " << tok.brace_args.front()
              << " [" << tok.bracket_arg << "]";
          pnum = 1;
        }
        else if (tok.text == "rSec1") {
          out << "@node" << std::endl;
          out << "@section " << tok.brace_args.front()
              << " [" << tok.bracket_arg << "]";
          pnum = 1;
        }
        else if (tok.text == "rSec2") {
          out << "@subsection " << tok.brace_args.front()
              << " [" << tok.bracket_arg << "]";
          pnum = 1;
        }
        else if (tok.text == "pnum") {
          out << pnum++ << ".  ";
        }
        else if (tok.text == "include") {
#if 1
          std::string include_path(tok.brace_args.front() + ".tex");
          std::ifstream file(include_path.c_str());
          assert(file.good() && ! file.eof());
          texinfo_converter converter;
          converter.convert(file, out, include_path);
#else
          out << "@include " << tok.brace_args.front() << ".texi";
#endif
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
          else if (tok.brace_args.front() == "codeblock")
            out << "@example";
          else if (tok.brace_args.front() == "ncsimplebnf")
            out << "@smallexample";
          else if (tok.brace_args.front() == "ncbnftab")
            out << "@smallexample";
          else
            std::cerr << path << ": Warning: Unrecognized command '\\begin{"
                      << tok.brace_args.front() << "}'" << std::endl;
        }
        else if (tok.text == "item") {
          out << "@item";
        }
        else if (tok.text == "end") {
          if (tok.brace_args.front() == "document")
            ;
          else if (tok.brace_args.front() == "itemize")
            out << "@end itemize";
          else if (tok.brace_args.front() == "codeblock")
            out << "@end example";
          else if (tok.brace_args.front() == "ncsimplebnf")
            out << "@end smallexample";
          else if (tok.brace_args.front() == "ncbnftab")
            out << "@end smallexample";
          else
            std::cerr << path << ": Warning: Unrecognized command '\\end{"
                      << tok.brace_args.front() << "}'" << std::endl;
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
        else if (tok.text == "grammarterm") {
          out << "@code{" << tok.brace_args.front() << "}";
        }
        else if (tok.text == "indextext") {
          out << "@cindex " << tok.brace_args.front();
        }
        else if (tok.text == "footnote" ||
                 tok.text == "terminal") {
          std::istringstream text(tok.brace_args.front());
          std::ostringstream converted;

          texinfo_converter converter;
          converter.convert(text, converted, path);

          if (tok.text == "footnote")
            out << "@footnote{" << converted.str() << "}";
          else
            out << converted.str();
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
        else
        {
          std::cerr << path << ": Warning: Unrecognized command '"
                    << tok.text << "'" << std::endl;
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
    std::ifstream file(argv[i]);
    texinfo_converter converter;
    converter.convert(file, std::cout);
  }
  return 0;
}
