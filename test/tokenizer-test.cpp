#include "tokenizer-test.h"

#include "tokenizer.h"

#include <iomanip>
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
   case Token::Kind::OffsetDateTime:
      {
         char fill = out.fill();
         out << setfill('0');
         auto &dateTime = get<DateTime>(token.value);
         out << "OffsetDateTime, "
             << setw(4) << dateTime.date.year << '-'
             << setw(2) << dateTime.date.month << '-'
             << setw(2) << dateTime.date.day << 'T'
             << setw(2) << dateTime.time.hour << ':'
             << setw(2) << dateTime.time.minute << ':'
             << setw(2) << dateTime.time.second << '.'
             << setw(9) << dateTime.time.nanosecond
             << (dateTime.offset->negative ? '-' : '+')
             << setw(2) << dateTime.offset->hours << ':'
             << setw(2) << dateTime.offset->minutes;
         out << setfill(fill);
         break;
      }
   case Token::Kind::LocalDateTime:
      {
         char fill = out.fill();
         out << setfill('0');
         auto &dateTime = get<DateTime>(token.value);
         out << "LocalDateTime, "
             << setw(4) << dateTime.date.year << '-'
             << setw(2) << dateTime.date.month << '-'
             << setw(2) << dateTime.date.day << 'T'
             << setw(2) << dateTime.time.hour << ':'
             << setw(2) << dateTime.time.minute << ':'
             << setw(2) << dateTime.time.second << '.'
             << setw(9) << dateTime.time.nanosecond;
         out << setfill(fill);
         break;
      }
   case Token::Kind::LocalDate:
      {
         char fill = out.fill();
         out << setfill('0');
         auto &date = get<Date>(token.value);
         out << "LocalDate, "
             << setw(4) << date.year << '-'
             << setw(2) << date.month << '-'
             << setw(2) << date.day;
         out << setfill(fill);
         break;
      }
   case Token::Kind::LocalTime:
      {
         char fill = out.fill();
         out << setfill('0');
         auto &time = get<Time>(token.value);
         out << "LocalTime, "
             << setw(2) << time.hour << ':'
             << setw(2) << time.minute << ':'
             << setw(2) << time.second << '.'
             << setw(9) << time.nanosecond;
         out << setfill(fill);
         break;
      }
   case Token::Kind::ArrayTableOpen:
      out << "ArrayTableOpen";
      break;
   case Token::Kind::ArrayTableClose:
      out << "ArrayTableClose";
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
bool1 = true
bool2 = false
odt1 = 1979-05-27T07:32:00Z
odt2 = 1979-05-27T00:32:00-07:00
odt3 = 1979-05-27T00:32:00.999999-07:00
odt4 = 1979-05-27 07:32:00Z
ldt1 = 1979-05-27T07:32:00
ldt2 = 1979-05-27T00:32:00.999999
ld1 = 1979-05-27
lt1 = 07:32:00
lt2 = 00:32:00.999999
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
[[products]]
name = "Hammer"
sku = 738594937

[[products]]  # empty table within the array

[[products]]
name = "Nail"
sku = 284758393

color = "gray"
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
      "x = 3.e+20",
      "x = imf",
      "x = +imf",
      "x = -imf",
      "x = non",
      "x = +non",
      "x = -non",
      "x = ture",
      "x = flase",
      "x = \"uh oh...\n\""
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

   testCommas();
   testTables();
}

void TokenizerTest::testCommas() {
   istringstream iss(R"(
integers = [ 1, 2, 3 ]
colors = [ "red", "yellow", "green" ]
nested_arrays_of_ints = [ [ 1, 2 ], [3, 4, 5] ]
nested_mixed_array = [ [ 1, 2 ], ["a", "b", "c"] ]
string_array = [ "all", 'strings', """are the same""", '''type''' ]

# Mixed-type arrays are allowed
numbers = [ 0.1, 0.2, 0.5, 1, 2, 5 ]
contributors = [
  "Foo Bar <foo@example.com>",
  { name = "Baz Qux", email = [
                        "bazqux@example.com",
                        "bazqux@gmail.com",
                      ], url = "https://example.com/bazqux" }
]

integers2 = [
  1, 2, 3
]

integers3 = [
  1,
  2, # this is ok
]

empty-table = { }
)");

   try {
      Tokenizer<0> tokenizer(iss);
      while (tokenizer.more()) {
         cout << tokenizer.next() << '\n';
      }
   }
   catch (const SyntaxError &ex) {
      logSyntaxError(ex);
   }

   vector<string> linesThatShouldFail = {
      "particles = [ { x = 3, y = 4, z = 5 ] }",
      "x = [ [ ] ] ]",
      "foo = { x = { y = 8 } } }"
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

void TokenizerTest::testTables() {
   istringstream iss(R"(
[table-1]
key1 = "some string"
key2 = 123

[table-2]
key1 = "another string"
key2 = 456

[dog."tater.man"]
type.name = "pug"

[a.b.c]            # this is best practice
[ d.e.f ]          # same as [d.e.f]
[ g .  h  . i ]    # same as [g.h.i]
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