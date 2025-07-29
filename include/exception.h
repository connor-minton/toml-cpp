#ifndef CCM_TOML_EXCEPTION_H
#define CCM_TOML_EXCEPTION_H

#include <string>

class Exception {
public:
   Exception(const std::string &error)
      : error(error)
      { }

   virtual ~Exception() = default;

   const char *what() const noexcept
      { return error.c_str(); }

private:
   std::string error;
};

class SyntaxError : public Exception {
public:
   SyntaxError(const std::string &error, int line, int col)
      : Exception(error),
        line(line),
        col(col)
      { }

   virtual ~SyntaxError() = default;

   const int line;
   const int col;
};

#endif