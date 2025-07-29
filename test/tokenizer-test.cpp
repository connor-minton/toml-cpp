#include "tokenizer-test.h"

#include "tokenizer.h"

#include <iostream>
#include <sstream>

using namespace std;
using namespace ccm::toml;

namespace {

ostream & operator<<(ostream &out, const Token &token) {
   out << "<";

   switch (token.kind) {
   case Token::Kind::Char:
      out << "Char, " << token.lexeme[0];
      break;
   case Token::Kind::Id:
      out << "Id, " << token.lexeme;
      break;
   case Token::Kind::Whitespace:
      out << "Whitespace";
      break;
   case Token::Kind::Newline:
      out << "Newline";
      break;
   case Token::Kind::Comment:
      out << "Comment, " << token.lexeme;
      break;
   case Token::Kind::Integer:
      out << "Integer, " << get<int64_t>(token.value);
      break;
   case Token::Kind::Float:
      out << "Float, " << get<double>(token.value);
      break;
   case Token::Kind::Boolean:
      out << "Boolean, " << (get<bool>(token.value) ? "true" : "false");
      break;
   case Token::Kind::String:
      out << "String, " << get<string>(token.value);
      break;
   }

   return out << ">";
};

void logSyntaxError(const SyntaxError &ex) {
   cerr << "Syntax error at Line " << ex.line << " Character " << ex.col
        << ": " << ex.what() << '\n';
}

} // namespace

void TokenizerTest::run() {
   istringstream iss(R"(
# this is a comment
x = 1234
y.z = 42
56 = 78
)");

   try {
      Tokenizer tokenizer(iss);
      while (tokenizer.more()) {
         cout << tokenizer.next() << '\n';
      }
   }
   catch (const SyntaxError &ex) {
      logSyntaxError(ex);
   }
}