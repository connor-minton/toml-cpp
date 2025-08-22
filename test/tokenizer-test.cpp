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
p = +1234
n = -1234
number-with-separators = 123_456_789
nws2 = 1_2_3_4_5
nws3 = -1234_5
y."z" = "\\ hello \"world\""
56 = 78
   'foo.bar'.baz = 'Dale "Rusty Shackleford" Gribble'
   empty-string = ''
   ml-lit-1 = '''hello'''
   ml-lit-2 = '''My name is 'Bob''''
   ml-lit-3 = '''
on a new line... or not?'''
   ml-lit-4 = '''I [dw]on't need \d{2} apples'''
   ml-lit-5 = '''
The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
'''
   ml-lit-6 = '''Here are fifteen quotation marks: """""""""""""""'''
   ml-lit-7 = ''''That,' she said, 'is still pointless.''''

   ml-bas-1 = """
Roses are red
Violets are blue"""

   ml-bas-2 = """
The quick brown \


  fox jumps over \
    the lazy dog."""

   ml-bas-3 = """\
       The quick brown \
       fox jumps over \
       the lazy dog.\
       """

   ml-bas-4 = """Here are two quotation marks: "". Simple enough."""

   ml-bas-5 = """Here are three quotation marks: ""\"."""

   ml-bas-6 = """Here are fifteen quotation marks: ""\"""\"""\"""\"""\"."""

   ml-bas-7 = """"This," she said, "is just a pointless statement.""""
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

   istringstream iss2("x = _123_456");

   try {
      Tokenizer tokenizer(iss2);
      while (tokenizer.more()) {
         tokenizer.next();
      }
      cout << "TEST FAILED: Expected SyntaxError.\n";
   }
   catch (const SyntaxError &ex) {
      cout << "TEST PASSED (got SyntaxError: " << ex.what() << ")\n";
   }

   istringstream iss3("x = -12_");

   try {
      Tokenizer tokenizer(iss3);
      while (tokenizer.more()) {
         tokenizer.next();
      }
      cout << "TEST FAILED: Expected SyntaxError.\n";
   }
   catch (const SyntaxError &ex) {
      cout << "TEST PASSED (got SyntaxError: " << ex.what() << ")\n";
   }

   istringstream iss4("x = 05");

   try {
      Tokenizer tokenizer(iss4);
      while (tokenizer.more()) {
         tokenizer.next();
      }
      cout << "TEST FAILED: Expected SyntaxError.\n";
   }
   catch (const SyntaxError &ex) {
      cout << "TEST PASSED (got SyntaxError: " << ex.what() << ")\n";
   }

   istringstream iss5("x = +");

   try {
      Tokenizer tokenizer(iss5);
      while (tokenizer.more()) {
         tokenizer.next();
      }
      cout << "TEST FAILED: Expected SyntaxError.\n";
   }
   catch (const SyntaxError &ex) {
      cout << "TEST PASSED (got SyntaxError: " << ex.what() << ")\n";
   }

   istringstream iss6("x = -00");

   try {
      Tokenizer tokenizer(iss6);
      while (tokenizer.more()) {
         tokenizer.next();
      }
      cout << "TEST FAILED: Expected SyntaxError.\n";
   }
   catch (const SyntaxError &ex) {
      cout << "TEST PASSED (got SyntaxError: " << ex.what() << ")\n";
   }
}