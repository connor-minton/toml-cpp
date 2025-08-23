#ifndef CCM_TOML_DATE_TIME_H
#define CCM_TOML_DATE_TIME_H

#include <optional>

namespace ccm::toml {

struct Date {
   int year = 1970;
   int month = 1;
   int day = 1;
};

struct Time {
   int hour = 0;
   int minute = 0;
   int second = 0;
   int nanosecond = 0;
};

struct DateTime {
   struct Offset {
      bool negative = false;
      int hours = 0;
      int minutes = 0;
   };

   Date date;
   Time time;
   std::optional<Offset> offset;
};

} // namespace ccm::toml

#endif