#include "lookahead-istream-test.h"

#include "lookahead-istream.h"

#include <iostream>
#include <sstream>

using namespace std;
using namespace ccm::toml;

namespace {

string display(int c) {
   if (c == char_traits<char>::eof()) {
      return "EOF";
   }
   string res;
   res += c;
   return res;
}

void gotExpected(int got, int expected) {
   cout << "got " << display(got) << " | expected " << display(expected)
        << '\n';
}

} // namespace

void LookaheadIStreamTest::run() {
   istringstream iss("every good boy does fine");
   LookaheadIStream in(iss);

   int c = in.get();
   gotExpected(c, 'e');

   c = in.peek();
   gotExpected(c, 'v');

   c = in.get();
   gotExpected(c, 'v');

   c = in.peek(2);
   gotExpected(c, 'y');

   c = in.peek(1);
   gotExpected(c, 'r');

   c = in.peek(0);
   gotExpected(c, 'e');

   c = in.get();
   gotExpected(c, 'e');

   c = in.peek(0);
   gotExpected(c, 'r');

   c = in.peek(1);
   gotExpected(c, 'y');

   c = in.peek(2);
   gotExpected(c, ' ');

   // consume until "fine"
   for (int i = 0; i < 17; ++i) {
      c = in.get();
      if (c == char_traits<char>::eof()) {
         cout << "UNEXPECTED EOF\n";
         return;
      }
   }

   c = in.peek(1);
   gotExpected(c, 'i');

   c = in.peek(5);
   gotExpected(c, char_traits<char>::eof());

   c = in.peek(4);
   gotExpected(c, char_traits<char>::eof());

   c = in.peek();
   gotExpected(c, 'f');

   c = in.get();
   gotExpected(c, 'f');

   c = in.peek();
   gotExpected(c, 'i');

   c = in.get();
   gotExpected(c, 'i');

   c = in.peek();
   gotExpected(c, 'n');

   c = in.get();
   gotExpected(c, 'n');

   c = in.peek();
   gotExpected(c, 'e');

   c = in.get();
   gotExpected(c, 'e');

   c = in.peek();
   gotExpected(c, char_traits<char>::eof());

   c = in.get();
   gotExpected(c, char_traits<char>::eof());
}