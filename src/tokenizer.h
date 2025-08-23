#ifndef CCM_TOML_TOKENIZER_H
#define CCM_TOML_TOKENIZER_H

#include "exception.h"
#include "lookahead-istream.h"
#include "token.h"

#include <cctype>
#include <charconv>
#include <istream>
#include <limits>
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
      DecimalDigit,
      DecimalDigitPlusMinus,
      BinaryDigit,
      OctalDigit,
      HexDigit,
      Whitespace,
      Id
   };

   void fillBuffer();
   bool getToken();
   void getNumber();
   void getDateTime();
   void getLocalTime();
   Time getTimePart();
   void getNewlines();
   void getWhitespace();
   void getId();
   void getChar();
   void getComment();
   void getBasicString();
   void getMLBasicString();
   void trimWhitespace();
   void getLiteralString();
   void getMLLiteralString();
   void getEscapeSequence();
   char expect(Character charClass);
   char expect(char c);
   void throwUnexpectedCharacter(Character expectedCharClass) const;

   static bool test(int c, Character charClass);

   LookaheadIStream in;
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
      if (test(in.peek(0), Character::DecimalDigit)
          && test(in.peek(1), Character::DecimalDigit)
          && test(in.peek(2), Character::DecimalDigit)
          && test(in.peek(3), Character::DecimalDigit)
          && in.peek(4) == '-')
      {
         // We check for dates and times first since they can be confused for
         // numbers.
         getDateTime();
         return true;
      }
      else if (test(in.peek(0), Character::DecimalDigit)
               && test(in.peek(1), Character::DecimalDigit)
               && in.peek(2) == ':')
      {
         getLocalTime();
         return true;
      }
      else if (test(c, Character::DecimalDigitPlusMinus) || c == 'i'
               || c == 'n')
      {
         // includes `inf` and `nan` floats
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

   // Let's get inf and nan out of the way...
   if (in.peek(0) == 'i'
       || in.peek(0) == '-' && in.peek(1) == 'i'
       || in.peek(0) == '+' && in.peek(1) == 'i')
   {
      bool neg = false;
      if (in.peek(0) == '-') {
         neg = true;
         token.lexeme += expect('-');
      }
      else if (in.peek(0) == '+') {
         token.lexeme += expect('+');
      }
      token.lexeme += expect('i');
      token.lexeme += expect('n');
      token.lexeme += expect('f');
      token.kind = Token::Kind::Float;
      token.value = std::numeric_limits<double>::infinity();
      if (neg) {
         token.value = -std::get<double>(token.value);
      }
      return;
   }
   else if (in.peek(0) == 'n'
            || in.peek(0) == '-' && in.peek(1) == 'n'
            || in.peek(0) == '+' && in.peek(1) == 'n')
   {
      bool neg = false;
      if (in.peek(0) == '-') {
         neg = true;
         token.lexeme += expect('-');
      }
      else if (in.peek(0) == '+') {
         token.lexeme += expect('+');
      }
      token.lexeme += expect('n');
      token.lexeme += expect('a');
      token.lexeme += expect('n');
      token.kind = Token::Kind::Float;
      token.value = std::numeric_limits<double>::quiet_NaN();
      if (neg) {
         token.value = -std::get<double>(token.value);
      }
      return;
   }

   token.lexeme += expect(Character::DecimalDigitPlusMinus);

   // num will contain only the characters that are necessary to parse the
   // number.
   std::string num;
   int base = 10;
   Character digitType = Character::DecimalDigit;

   if (token.lexeme[0] == '0') {
      int c = in.peek();
      switch (c) {
      case 'b':
         base = 2;
         digitType = Character::BinaryDigit;
         break;
      case 'o':
         base = 8;
         digitType = Character::OctalDigit;
         break;
      case 'x':
         base = 16;
         digitType = Character::HexDigit;
         break;
      }

      if (base != 10) {
         // Binary, octal, and hex numbers can have leading zeros. Scoop up the
         // prefix, the first digit, and the rest of the leading zeros (if any)
         token.lexeme += expect(c);
         token.lexeme += expect(digitType);
         if (token.lexeme.back() != '0') {
            num += token.lexeme.back();
         }
         else {
            c = in.peek();
            while (c == '0') {
               token.lexeme += expect('0');
               c = in.peek();
            }
            // okay... there's a chance that leading zeros are the only digits,
            // so remember that for later.
         }
      }
      else if (test(c, Character::DecimalDigit)) {
         // But decimal integers generally cannot have leading zeros. So let's
         // go ahead and rule that out here.
         throw SyntaxError("Integer has leading zero(s)", lineNum, colNum);
      }
   }

   // If we start with a '+' or '-', we MUST be followed by a decimal digit,
   // AND, if that digit is zero, it must be the only digit.
   else if (token.lexeme[0] == '+' || token.lexeme[0] == '-') {
      if (token.lexeme[0] == '-') {
         num += '-';
      }
      token.lexeme += expect(Character::DecimalDigit);
      if (token.lexeme.back() == '0'
          && test(in.peek(), Character::DecimalDigit))
      {
         throw SyntaxError("Integer has leading zero(s)", lineNum, colNum);
      }

      if (token.lexeme.back() != '0') {
         num += token.lexeme.back();
      }
   }

   // Otherwise we got a nonzero decimal digit
   else {
      num += token.lexeme.back();
   }

   // At this point, we have determined the base of the number. If the base is
   // not 10, then the prefix and any leading zeros have been ignored. In some
   // cases, a nonzero digit may have already been extracted. If so, it has been
   // appended to the lexeme and `num`.

   int c = in.peek();

   // Slurp up the remaining digits. In the loop condition, we allow any digit
   // no matter what base we're parsing, but inside the loop we expect digits of
   // the appropriate base. This makes for some good error messages if the user
   // accidentally mixes bases.
   while (c != std::char_traits<char>::eof()
          && (test(c, Character::HexDigit) || c == '_'))
   {
      // ...but since 'e' and 'E' are considered hex digits, they get flagged
      // here even though we expect them to appear in some floating point
      // numbers! So if we see an e/E, we'll break so we can start parsing a
      // float.
      if (c == 'e' || c == 'E') {
         break;
      }
      else if (c == '_') {
         token.lexeme += expect('_');
         token.lexeme += expect(digitType);
      }
      else {
         token.lexeme += expect(digitType);
      }
      num += token.lexeme.back();
      c = in.peek();
   }

   // Remember it's possible to end up with an empty `num` string if a non-base-
   // 10 number contained only leading zeros.
   if (num.empty()) {
      token.value = 0;
      return;
   }

   if (base == 10 && (c == '.' || c == 'e' || c == 'E')) {
      // A decimal integer followed by a '.' or e/E indicates a floating point
      // number.
      bool gotFraction = false;
      bool gotExponent = false;
      // Remember: 'e' and 'E' are hex digits :)
      while (test(c, Character::HexDigit) || c == '.' || c == '_')
      {
         if (c == '.') {
            if (gotExponent) {
               throw SyntaxError("Decimal point after exponent",
                                 lineNum, colNum);
            }
            else if (gotFraction) {
               throw SyntaxError("Floating point number with more than one "
                                 "decimal point", lineNum, colNum);
            }
            else {
               token.lexeme += expect('.');
               num += '.';
               token.lexeme += expect(Character::DecimalDigit);
               num += token.lexeme.back();
               gotFraction = true;
            }
         }
         else if (c == 'e' || c == 'E') {
            if (gotExponent) {
               throw SyntaxError("Floating point number with more than one "
                                 "exponent part", lineNum, colNum);
            }
            else {
               token.lexeme += expect(c);
               num += c;
               token.lexeme += expect(Character::DecimalDigitPlusMinus);
               num += token.lexeme.back();
               if (!test(token.lexeme.back(), Character::DecimalDigit)) {
                  token.lexeme += expect(Character::DecimalDigit);
                  num += token.lexeme.back();
               }
               gotExponent = true;
            }
         }
         else if (c == '_') {
            token.lexeme += expect('_');
            token.lexeme += expect(Character::DecimalDigit);
            num += token.lexeme.back();
         }
         else {
            token.lexeme += expect(Character::DecimalDigit);
            num += token.lexeme.back();
         }

         c = in.peek();
      }

      double value = 0;
      auto result = std::from_chars(num.c_str(),
                                    num.c_str() + num.size(),
                                    value);
      if (result.ec == std::errc::result_out_of_range) {
         throw SyntaxError("Floating point overflow/underflow",
                           startLine, startCol);
      }
      else if (result.ec != std::errc{}) {
         throw Exception("Could not parse floating point number");
      }

      token.kind = Token::Kind::Float;
      token.value = value;
   }
   else {
      int64_t value = 0;
      auto result = std::from_chars(num.c_str(),
                                    num.c_str() + num.size(),
                                    value,
                                    base);
      if (result.ec == std::errc::result_out_of_range) {
         throw SyntaxError("Integer overflows 64 bits", startLine, startCol);
      }
      else if (result.ec != std::errc{}) {
         throw Exception("Could not parse integer");
      }

      token.kind = Token::Kind::Integer;
      token.value = value;
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getDateTime() {
   auto &token = buffer.emplace_back();

   DateTime dateTime;
   std::string buf;

   // Get the year
   for (int i = 0; i < 4; ++i) {
      buf += expect(Character::DecimalDigit);
   }
   token.lexeme += buf;
   dateTime.date.year = std::stoi(buf);
   buf.clear();

   token.lexeme += expect('-');

   // Get the month
   for (int i = 0; i < 2; ++i) {
      buf += expect(Character::DecimalDigit);
   }
   token.lexeme += buf;
   dateTime.date.month = std::stoi(buf);
   buf.clear();

   token.lexeme += expect('-');

   // Get the day
   for (int i = 0; i < 2; ++i) {
      buf += expect(Character::DecimalDigit);
   }
   token.lexeme += buf;
   dateTime.date.day = std::stoi(buf);
   buf.clear();

   int c = in.peek();
   if (c == ' ' && test(in.peek(1), Character::DecimalDigit)
       || c == 't' || c == 'T')
   {
      token.lexeme += expect(c);
      dateTime.time = getTimePart();
      c = in.peek();
      if (c == 'Z' || c == 'z') {
         token.lexeme += expect(c);
         dateTime.offset = DateTime::Offset{};
      }
      else if (c == '+' || c == '-') {
         token.lexeme += expect(c);
         dateTime.offset = DateTime::Offset{};
         dateTime.offset->negative = (c == '-');
         // Get the hours
         for (int i = 0; i < 2; ++i) {
            buf += expect(Character::DecimalDigit);
         }
         token.lexeme += buf;
         dateTime.offset->hours = std::stoi(buf);
         buf.clear();

         token.lexeme += expect(':');

         // Get the minutes
         for (int i = 0; i < 2; ++i) {
            buf += expect(Character::DecimalDigit);
         }
         token.lexeme += buf;
         dateTime.offset->minutes = std::stoi(buf);
      }

      token.value = dateTime;
      if (dateTime.offset) {
         token.kind = Token::Kind::OffsetDateTime;
      }
      else {
         token.kind = Token::Kind::LocalDateTime;
      }
   }
   else {
      // This is a local date without a time or offset.
      token.value = dateTime.date;
      token.kind = Token::Kind::LocalDate;
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getLocalTime() {
   auto &token = buffer.emplace_back();
   token.kind = Token::Kind::LocalTime;
   auto &time = token.value.emplace<Time>();
   time = getTimePart();
   int c = in.peek();
   if (c == '+' || c == '-' || c == 'z' || c == 'Z') {
      throw SyntaxError("Lone time can have no offset", lineNum, colNum);
   }
}

template<int NLookahead>
Time Tokenizer<NLookahead>::getTimePart() {
   auto &token = buffer.back();
   Time time;

   std::string buf;

   // Get hour
   for (int i = 0; i < 2; ++i) {
      buf += expect(Character::DecimalDigit);
   }
   token.lexeme += buf;
   time.hour = std::stoi(buf);
   buf.clear();

   token.lexeme += expect(':');

   // Get minute
   for (int i = 0; i < 2; ++i) {
      buf += expect(Character::DecimalDigit);
   }
   token.lexeme += buf;
   time.minute = std::stoi(buf);
   buf.clear();

   token.lexeme += expect(':');

   // Get second
   for (int i = 0; i < 2; ++i) {
      buf += expect(Character::DecimalDigit);
   }
   token.lexeme += buf;
   time.second = std::stoi(buf);
   buf.clear();

   // Get optional fractional seconds
   if (in.peek() == '.') {
      token.lexeme += expect('.');
      buf += expect(Character::DecimalDigit);
      // We support nanoseconds precision (up to .999999999)
      int c = in.peek();
      while (buf.size() < 9 && test(c, Character::DecimalDigit)) {
         buf += expect(Character::DecimalDigit);
         c = in.peek();
      }
      token.lexeme += buf;
      while (buf.size() < 9) {
         // Right pad buf with zeros before we use stoi()
         buf += '0';
      }
      time.nanosecond = std::stoi(buf);
   }

   return time;
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

   // At this point, we've consumed two double quotes. If there were any
   // characters between the quotes, we're done. However, if it was an empty
   // string, we have to be sure that it was an empty string and not the
   // beginning of a multiline string.

   if (token.lexeme.length() == 2 // 2 quotes and that's it
       && in.peek() == '"')
   { 
      token.lexeme += expect('"');
      getMLBasicString();
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getMLBasicString() {
   auto &token = buffer.back();

   int c = in.peek();
   if (c == '\r') {
      token.lexeme += expect('\r');
      token.lexeme += expect('\n');
      c = in.peek();
   }
   else if (c == '\n') {
      token.lexeme += expect('\n');
      c = in.peek();
   }

   // A multiline basic string must end with at least 3 quote marks but can
   // end with as many as 5 (up to 2 adjacent quotes are allowed inside an ML
   // string)
   int numQuotes = 0;
   while (c != std::char_traits<char>::eof() && numQuotes < 5) {
      if (c == '"') {
         ++numQuotes;
         token.lexeme += expect('"');
      }
      else if (numQuotes >= 3) {
         break;
      }
      else {
         while (numQuotes > 0) {
            // we've been racking up quotes, and we just found out they're
            // actually part of the string, so add them to the value
            std::get<std::string>(token.value) += '"';
            --numQuotes;
         }

         if (c == '\\') {
            c = in.peek(1);
            if (c == '\r' || c == '\n') {
               token.lexeme += expect('\\');
               trimWhitespace();
            }
            else {
               getEscapeSequence();
            }
         }
         else {
            token.lexeme += c;
            std::get<std::string>(token.value) += c;
            expect(c);
         }
      }

      c = in.peek();
   }

   if (numQuotes < 3) {
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   }

   while (numQuotes > 3) {
      std::get<std::string>(token.value) += '"';
      --numQuotes;
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::trimWhitespace() {
   auto &token = buffer.back();

   int c = in.peek();
   while (true) {
      if (c == '\r') {
         token.lexeme += expect('\r');
         token.lexeme += expect('\n');
      }
      else if (c == '\n') {
         token.lexeme += expect('\n');
      }
      else if (test(c, Character::Whitespace)) {
         token.lexeme += expect(Character::Whitespace);
      }
      else {
         break;
      }

      c = in.peek();
   }
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

   // At this point, we've consumed two single quotes. If there were any
   // characters between the quotes, we're done. However, if it was an empty
   // string, we have to be sure that it was an empty string and not the
   // beginning of a multiline string.

   if (token.lexeme.length() == 2 // 2 quotes and that's it
       && in.peek() == '\'')
   { 
      token.lexeme += expect('\'');
      getMLLiteralString();
   }
}

template<int NLookahead>
void Tokenizer<NLookahead>::getMLLiteralString() {
   auto &token = buffer.back();

   int c = in.peek();
   if (c == '\r') {
      token.lexeme += expect('\r');
      token.lexeme += expect('\n');
      c = in.peek();
   }
   else if (c == '\n') {
      token.lexeme += expect('\n');
      c = in.peek();
   }

   // A multiline literal string must end with at least 3 quote marks but can
   // end with as many as 5 (up to 2 adjacent quotes are allowed inside an ML
   // string)
   int numQuotes = 0;
   while (c != std::char_traits<char>::eof() && numQuotes < 5) {
      if (c == '\'') {
         ++numQuotes;
         token.lexeme += expect('\'');
      }
      else if (numQuotes >= 3) {
         break;
      }
      else {
         while (numQuotes > 0) {
            // we've been racking up quotes, and we just found out they're
            // actually part of the string, so add them to the value
            std::get<std::string>(token.value) += '\'';
            --numQuotes;
         }
         token.lexeme += c;
         std::get<std::string>(token.value) += c;
         expect(c);
      }

      c = in.peek();
   }

   if (numQuotes < 3) {
      throw SyntaxError("Unexpected EOF", lineNum, colNum);
   }

   while (numQuotes > 3) {
      std::get<std::string>(token.value) += '\'';
      --numQuotes;
   }
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
   case Character::DecimalDigit:
      throw SyntaxError("Expected decimal digit", lineNum, colNum);
   case Character::DecimalDigitPlusMinus:
      throw SyntaxError("Expected decimal digit, +, or -", lineNum, colNum);
   case Character::BinaryDigit:
      throw SyntaxError("Expected 0 or 1", lineNum, colNum);
   case Character::OctalDigit:
      throw SyntaxError("Expected octal digit", lineNum, colNum);
   case Character::HexDigit:
      throw SyntaxError("Expected hex digit", lineNum, colNum);
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
   case Character::DecimalDigit:
      return isdigit(c);
   case Character::DecimalDigitPlusMinus:
      return isdigit(c) || c == '+' || c == '-';
   case Character::BinaryDigit:
      return c == '0' || c == '1';
   case Character::OctalDigit:
      return c >= '0' && c <= '7';
   case Character::HexDigit:
      return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
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