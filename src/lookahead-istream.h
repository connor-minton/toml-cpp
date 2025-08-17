#ifndef CCM_TOML_LOOKAHEAD_ISTREAM_H
#define CCM_TOML_LOOKAHEAD_ISTREAM_H

#include <istream>
#include <vector>

namespace ccm::toml {

class LookaheadIStream {
public:
   LookaheadIStream(std::istream &in);

   int get();
   int peek(size_t index=0);

private:
   std::istream &in;
   std::vector<int> buffer;
};

}

#endif