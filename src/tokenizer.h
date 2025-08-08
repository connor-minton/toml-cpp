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
      Printable,
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
   void getBasicString();
   void getLiteralString();
   void getEscapeSequence();
   char expect(Character charClass);
   char expect(char c);
   void throwUnexpectedCharacter(Character expectedCharClass) const;

   static bool test(int c, Character charClass);

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
   if (test(c, Character::Whitespace)) {
      getWhitespace();
      return true;
   }
   if (c == '#') {
      getComment();
      return true;
   }
   if (c == '"') {
      getBasicString();
      return true;
   }
   if (c == '\'') {
      getLiteralString();
      return true;
   }

   static const std::string keyChars = ".[]";
   if (state == State::Key) {
      if (test(c, Character::Id)) {
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
      if (test(c, Character::DigitPlusMinus)) {
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
   while (c != std::char_traits<char>::eof() && test(c, Character::Digit))
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
      throw Exception("Could not parse integer");
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
   token.lexeme += expect(Character::Printable);
}

template<int NLookahead>
void Tokenizer<NLookahead>::getComment() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::Comment;
   token.lexeme += expect('#');

   int c = in.peek();
   while (c != std::char_traits<char>::eof() && c != '\r' && c != '\n') {
      token.lexeme += expect(Character::Printable);
      c = in.peek();
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getBasicString() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::String;
   token.lexeme += expect('"');
   token.value = "";

   int c = in.peek();
   while (c != std::char_traits<char>::eof() && c != '"') {
      if (c == '\\') {
         getEscapeSequence();
      }
      else {
         token.lexeme += expect(Character::Printable);
         std::get<std::string>(token.value) += token.lexeme.back();
      }
      c = in.peek();
   }
   token.lexeme += expect('"');
}

template<int NLookahead>
void Tokenizer<NLookahead>::getLiteralString() {
   Token &token = buffer.emplace_back();
   token.kind = Token::Kind::String;
   token.lexeme += expect('\'');
   token.value = "";

   int c = in.peek();
   while (c != std::char_traits<char>::eof() && c != '\'') {
      token.lexeme += expect(Character::Printable);
      std::get<std::string>(token.value) += token.lexeme.back();
      c = in.peek();
   }
   token.lexeme += expect('\'');
}

template<int NLookahead>
void Tokenizer<NLookahead>::getEscapeSequence() {
   auto &token = buffer.back();

   token.lexeme += expect('\\');
   int c = in.peek();
   switch (c) {
   case 'b':
      token.lexeme += expect('b');
      std::get<std::string>(token.value) += '\b';
      break;
   case 't':
      token.lexeme += expect('t');
      std::get<std::string>(token.value) += '\t';
      break;
   case 'n':
      token.lexeme += expect('n');
      std::get<std::string>(token.value) += '\n';
      break;
   case 'f':
      token.lexeme += expect('f');
      std::get<std::string>(token.value) += '\f';
      break;
   case 'r':
      token.lexeme += expect('r');
      std::get<std::string>(token.value) += '\r';
      break;
   case '"':
      token.lexeme += expect('"');
      std::get<std::string>(token.value) += '"';
      break;
   case '\\':
      token.lexeme += expect('\\');
      std::get<std::string>(token.value) += '\\';
      break;
   case std::char_traits<char>::eof():
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   default:
      throw SyntaxError("Invalid escape sequence", lineNum, colNum);
   }
}

template<int NLookahead>
char Tokenizer<NLookahead>::expect(Character charClass) {
   int c = in.get();
   if (c == std::char_traits<char>::eof()) {
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   }

   if (!test(c, charClass)) {
      throwUnexpectedCharacter(charClass);
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

template<int NLookahead>
void Tokenizer<NLookahead>::throwUnexpectedCharacter(
                               Character expectedCharClass) const
{
   switch (expectedCharClass) {
   case Character::Printable:
      throw SyntaxError("Invalid ASCII", lineNum, colNum);
   case Character::Digit:
      throw SyntaxError("Expected digit", lineNum, colNum);
   case Character::DigitPlusMinus:
      throw SyntaxError("Expected digit, +, or -", lineNum, colNum);
   case Character::Whitespace:
      throw SyntaxError("Expected space or \\t", lineNum, colNum);
   case Character::Id:
      throw SyntaxError("Expected letter, number, _, or -", lineNum, colNum);
   }

   throw Exception("Unsupported character class "
                   + std::to_string(static_cast<int>(expectedCharClass)));
}

template<int NLookahead>
bool Tokenizer<NLookahead>::test(int c, Character charClass) {
   switch (charClass) {
   case Character::Printable:
      // This gives us any printable ASCII character, including spaces and tabs
      // but excluding \r and \n
      return c >= 32 && c <= 126;
   case Character::Digit:
      return isdigit(c);
   case Character::DigitPlusMinus:
      return isdigit(c) || c == '+' || c == '-';
   case Character::Whitespace:
      return isspace(c);
   case Character::Id:
      return isalnum(c) || c == '_' || c == '-';
   default:
      throw Exception("Unsupported character class "
                      + std::to_string(static_cast<int>(charClass)));
   }
}

} // namespace ccm::toml

#endif