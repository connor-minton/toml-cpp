#include "toml-test.h"
#include "tokenizer-test.h"
#include "lookahead-istream-test.h"

int main() {
   LookaheadIStreamTest{}.run();
   TomlTest{}.run();
   TokenizerTest{}.run();
}