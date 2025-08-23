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
just-barely = 9223372036854775807
just-barely-neg = -9223372036854775808
nws2 = 1_2_3_4_5
nws3 = -1234_5
bin1 = 0b1001
bin2 = 0b0110
bin3 = 0b0000
bin4 = 0b0
oct1 = 0o700
oct2 = 0o744
oct3 = 0o000
oct4 = 0o0
oct5 = 0o777
hex1 = 0x42
hex2 = 0x0
hex3 = 0x0000
hex4 = 0x0f
hex5 = 0xFF
flt1 = +1.0
flt2 = 3.1415
flt3 = -0.01
flt4 = 5e+22
flt5 = 1e06
flt6 = -2E-2
flt7 = 6.626e-34
flt8 = 224_617.445_991_228
flt9 = 1_2.34_56
sf1 = inf  # positive infinity
sf2 = +inf # positive infinity
sf3 = -inf # negative infinity
sf4 = nan  # actual sNaN/qNaN encoding is implementation-specific
sf5 = +nan # same as `nan`
sf6 = -nan # valid, actual encoding is implementation-specific
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

   vector<string> linesThatShouldFail = {
      "x = _123_456",
      "x = -12_",
      "x = 05",
      "x = +",
      "x = -00",
      "x = 0x",
      "x = 0b12",
      "x = 0o12345678",
      "x = 1f",
      "x = 9223372036854775808",
      "x = -9223372036854775809",
      "x = .7",
      "x = 7.",
      "x = 3.e+20"
   };

   for (const string &s : linesThatShouldFail) {
      try {
         istringstream iss2(s);
         Tokenizer tokenizer(iss2);
         while (tokenizer.more()) {
            tokenizer.next();
         }
         cout << "TEST FAILED: Expected SyntaxError.\n";
      }
      catch (const SyntaxError &ex) {
         cout << "TEST PASSED (got SyntaxError: " << ex.what() << ")\n";
      }
   }
}