#include "lookahead-istream.h"

using namespace std;

namespace ccm::toml {

LookaheadIStream::LookaheadIStream(istream &in)
   : in(in)
{
}

int LookaheadIStream::get() {
   if (buffer.empty()) {
      return in.get();
   }

   int res = buffer[0];
   buffer.erase(buffer.begin());
   return res;
}

int LookaheadIStream::peek(size_t index) {
   if (index < buffer.size()) {
      return buffer[index];
   }

   while (index > buffer.size()) {
      int c = in.get();
      if (c == char_traits<char>::eof()) {
         return c;
      }
      buffer.push_back(c);
   }

   return in.peek();
}

}