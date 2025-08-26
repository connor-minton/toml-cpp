#ifndef CCM_TOML_TOKEN_H
#define CCM_TOML_TOKEN_H

#include "date-time.h"

#include <cstdint>
#include <string>
#include <variant>

namespace ccm::toml {

struct Token {
   enum class Kind {
      // The token is a simple character. lexeme[0] contains the character.
      Char,

      // The token represents an identifier. In most cases, lexeme and
      // get<string>(value) both contain the identifier. However, when the ID is
      // a string, lexeme contains the quote characters and escape sequences
      // from the input, whereas get<string>(value) contains the actual ID to be
      // used as a key.
      Id,

      // The token is a contiguous seqeuence of non-newline whitespace (spaces
      // and/or tabs). lexeme contains the sequence.
      Whitespace,

      // The token is a contiguous sequence of newlines. Each newline is either
      // "\n" or "\r\n".
      Newline,

      // The token is a comment. lexeme contains the hash character until the
      // end of the line (but not including the newline character(s)).
      Comment,

      // get<int64_t>(value) contains the value of the integer.
      Integer,

      // get<double>(value) contains the value of the floating point number.
      Float,

      // get<bool>(value) contains the boolean value.
      Boolean,

      // The token is a fully parsed string. get<string>(value) contains the
      // data that the string should contain.
      String,

      // An RFC 3339 date with offset from UTC. get<DateTime>(value) contains
      // the parsed date, which is not checked for validity.
      OffsetDateTime,

      // A partial RFC 3339 date, consisting of the date and time but no offset.
      // get<DateTime>(value) contains the parsed date, but the offset field
      // does not exist.
      LocalDateTime,

      // A partial RFC 3339 date, consisting of just the date without a time or
      // offset. get<Date>(value) contains the parsed date.
      LocalDate,

      // A partial RFC 3339 time, consisting of just the time without a date or
      // offset. get<Time>(value) contains the parsed time.
      LocalTime,

      // lexeme == "[["
      ArrayTableOpen,

      // lexeme == "]]"
      ArrayTableClose
   };

   using Value = std::variant<std::int64_t,
                              double,
                              bool,
                              std::string,
                              DateTime,
                              Date,
                              Time>;

   Kind kind;
   Value value;
   std::string lexeme;
};

}

#endif