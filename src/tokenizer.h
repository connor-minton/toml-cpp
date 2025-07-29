#ifndef CCM_TOML_TOKENIZER_H
#define CCM_TOML_TOKENIZER_H

#include "exception.h"
#include "token.h"

#include <cctype>
#include <charconv>
#include <istream>
#include <string>
#include <vector>

namespace ccm::toml {

template<int NLookahead=1>
class Tokenizer {
   static_assert(NLookahead >= 0);

public:
   Tokenizer(std::istream &in)
      : in(in)
      { }

   bool more()
   { 
      if (state == State::Init) {
         state = State::Key;
         fillBuffer();
      }
      return !buffer.empty();
   }

   Token next() {
      if (!more()) {
         throw Exception("Tokenizer::next(): no more tokens");
      }
      Token t = buffer[0];
      buffer.erase(buffer.begin());
      fillBuffer();
      return t;
   }

   const Token &peek(int which=0) const {
      if (which < 0 || which >= buffer.size()) {
         throw Exception("Tokenizer::peek(): " + std::to_string(which)
                         + " is out of range");
      }
      return buffer[which];
   }

private:
   enum class State {
      Init,
      Key,
      Value
   };

   enum class Character {
      Any,
      Digit,
      DigitPlusMinus,
      Whitespace,
      Id
   };

   void fillBuffer();
   bool getToken();
   void getNumber();
   void getNewlines();
   void getWhitespace();
   void getId();
   void getChar();
   void getComment();
   char expect(Character charClass);
   char expect(char c);

   std::istream &in;
   std::vector<Token> buffer;
   State state = State::Init;
   int lineNum = 1;
   int colNum = 1;
};

template<int NLookahead>
void Tokenizer<NLookahead>::fillBuffer() {
   constexpr int bufferSize = NLookahead + 1;
   while (buffer.size() < bufferSize && getToken())
      ;
}

template<int NLookahead>
bool Tokenizer<NLookahead>::getToken() {
   int c = in.peek();
   if (c == std::char_traits<char>::eof())
      return false;

   if (c == '\r' || c == '\n') {
      state = State::Key;
      getNewlines();
      return true;
   }
   if (isspace(c)) {
      getWhitespace();
      return true;
   }
   if (c == '#') {
      getComment();
      return true;
   }

   static const std::string keyChars = ".[]";
   if (state == State::Key) {
      if (isalnum(c) || c == '_' || c == '-') {
         getId();
         return true;
      }
      if (c == '=') {
         state = State::Value;
         getChar();
         return true;
      }
      if (keyChars.find(c) != std::string::npos) {
         getChar();
         return true;
      }
   }
   else {
      if (isdigit(c) || c == '+' || c == '-') {
         getNumber();
         return true;
      }
   }

   throw SyntaxError("Unexpected character", lineNum, colNum);
}

template<int NLookahead>
void Tokenizer<NLookahead>::getNumber() {
   int startLine = lineNum;
   int startCol = colNum;

   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Integer;
   token.lexeme += expect(Character::DigitPlusMinus);

   // If we start with a '+' or '-', we MUST be followed by a digit
   if (!isdigit(token.lexeme[0])) {
      token.lexeme += expect(Character::Digit);
   }

   int c = in.peek();

   // If the first digit is a 0, there can be no further digits.
   if (token.lexeme.back() == '0'
       && c != std::char_traits<char>::eof()
       && isdigit(c))
   {
      throw SyntaxError("Nonzero integer cannot have leading zero",
                        startLine, startCol);
   }

   // Slurp up the remaining digits
   while (c != std::char_traits<char>::eof() && isdigit(c))
   {
      token.lexeme += expect(Character::Digit);
      c = in.peek();
   }

   // Parse the lexeme as an integer
   const char *first = (token.lexeme[0] == '+')
                       ? token.lexeme.c_str() + 1
                       : token.lexeme.c_str();
   int64_t value = 0;
   auto result = std::from_chars(first, 
                                 token.lexeme.c_str() + token.lexeme.size(),
                                 value);
   if (result.ec == std::errc::result_out_of_range) {
      throw SyntaxError("Integer overflows 64 bits", startLine, startCol);
   }
   else if (result.ec != std::errc{}) {
      throw Exception("This is a bug in toml++");
   }

   token.value = value;
}

template<int NLookahead>
void Tokenizer<NLookahead>::getNewlines() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Newline;

   int c = in.peek();
   if (c == std::char_traits<char>::eof()) {
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   }
   if (c != '\r' && c != '\n') {
      throw SyntaxError("Expected \\r or \\n", lineNum, colNum);
   }

   while (c != std::char_traits<char>::eof()
          && (c == '\n' || c == '\r'))
   {
      if (c == '\n') {
         token.lexeme += expect('\n');
      }
      else {
         token.lexeme += expect('\r');
         token.lexeme += expect('\n');
      }
      c = in.peek();
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getWhitespace() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Whitespace;
   token.lexeme += expect(Character::Whitespace);

   int c = in.peek();
   while (c != std::char_traits<char>::eof() && isspace(c)) {
      token.lexeme += expect(Character::Whitespace);
      c = in.peek();
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getId() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Id;
   token.lexeme += expect(Character::Id);

   int c = in.peek();
   while (c != std::char_traits<char>::eof()
          && (isalnum(c) || c == '_' || c == '-'))
   {
      token.lexeme += expect(Character::Id);
      c = in.peek();
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getChar() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Char;
   token.lexeme += expect(Character::Any);
}

template<int NLookahead>
void Tokenizer<NLookahead>::getComment() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Comment;
   token.lexeme += expect('#');

   int c = in.peek();
   while (c != std::char_traits<char>::eof() && c != '\r' && c != '\n') {
      token.lexeme += expect(Character::Any);
      c = in.peek();
   }
}

template<int NLookahead>
char Tokenizer<NLookahead>::expect(Character charClass) {
   int c = in.get();
   if (c == std::char_traits<char>::eof()) {
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   }
   switch (charClass)
   {
   case Character::Any:
      break;
   case Character::Digit:
      if (!isdigit(c)) {
         throw SyntaxError("Expected digit", lineNum, colNum);
      }
      break;
   case Character::DigitPlusMinus:
      if (!isdigit(c) && c != '+' && c != '-') {
         throw SyntaxError("Expected digit, +, or -", lineNum, colNum);
      }
      break;
   case Character::Whitespace:
      if (!isspace(c)) {
         throw SyntaxError("Expected space or \\t", lineNum, colNum);
      }
      break;
   case Character::Id:
      if (!isalnum(c) && c != '_' && c != '-') {
         throw SyntaxError("Expected letter, number, _, or -", lineNum, colNum);
      }
      break;
   default:
      throw Exception("Unsupported character class "
                      + std::to_string(static_cast<int>(charClass)));
   }

   ++colNum;
   return c;
}

template<int NLookahead>
char Tokenizer<NLookahead>::expect(char c) {
   int _c = in.get();
   if (_c == std::char_traits<char>::eof()) {
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   }
   if (_c != c) {
      throw SyntaxError("Unexpected character", lineNum, colNum);
   }
   if (c == '\n') {
      ++lineNum;
      colNum = 0;
   }
   ++colNum;
   return c;
}

} // namespace ccm::toml

#endif