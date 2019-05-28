/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#include <flecsi-config.h>

#include "types.h"
#include "utils.h"

#include <iostream>

namespace flecsi {
namespace utils {
namespace flog {

/*!
  Function always returning true. Used for defaults.
 */

inline bool
true_state() {
  return true;
} // true_state

/*!
  The log_message_t type provides a base class for implementing
  formatted logging utilities.
 */

template<typename P>
struct log_message_t {

  /*!
    Constructor.

    @param file            The current file (where the log message was
                            created). In general, this will always use the
                            __FILE__ parameter from the calling macro.
    @param line            The current line (where the log message was
                           called). In general, this will always use the
                           __LINE__ parameter from the calling macro.
    @param predicate       The predicate function to determine whether or not
                           the calling runtime should produce output.
    @param can_send_to_one A boolean indicating whether the calling scope can
                           route messages through one rank.
*/

  log_message_t(const char * file,
    int line,
    P && predicate,
    bool can_send_to_one = true)
    : file_(file), line_(line), predicate_(predicate),
      can_send_to_one_(can_send_to_one), clean_color_(false) {
#if defined(FLOG_ENABLE_DEBUG)
    std::cerr << FLOG_COLOR_LTGRAY << "FLOG: log_message_t constructor " << file
              << " " << line << FLOG_COLOR_PLAIN << std::endl;
#endif
  } // log_message_t

  virtual ~log_message_t() {
#if defined(FLOG_ENABLE_DEBUG)
    std::cerr << FLOG_COLOR_LTGRAY << "FLOG: log_message_t destructor "
              << FLOG_COLOR_PLAIN << std::endl;
#endif

#if defined(FLOG_ENABLE_MPI)
    if(can_send_to_one_ && flog_t::instance().initialized()) {
#if defined(FLOG_BUFFER_MPI)
      mpi_state_t::instance().packets().push_back(
        {flog_t::instance().buffer_stream().str().c_str()});
#else
      send_to_one(flog_t::instance().buffer_stream().str().c_str());
#endif
    }
    else {
      if(!flog_t::instance().initialized()) {
#if defined(FLOG_ENABLE_EXTERNAL)
        std::cout << flog_t::instance().buffer_stream().str();
#endif // FLOG_ENABLE_EXTERNAL
      }
      else {
        flog_t::instance().stream() << flog_t::instance().buffer_stream().str();
      } // if
    } // if
#else
    flog_t::instance().stream() << flog_t::instance().buffer_stream().str();
#endif // FLOG_ENABLE_MPI

    flog_t::instance().buffer_stream().str(std::string{});
  } // ~log_message_t

  /*!
    Return the output stream. Override this method to add additional
    formatting to a particular severity output.
   */

  virtual std::ostream & stream() {
    return flog_t::instance().severity_stream(predicate_());
  } // stream

protected:
  const char * file_;
  int line_;
  P & predicate_;
  bool can_send_to_one_;
  bool clean_color_;

}; // struct log_message_t

/*----------------------------------------------------------------------------*
  Convenience macro to define severity levels.

  Log message types defined using this macro always use the default
  predicate function, true_state().
 *----------------------------------------------------------------------------*/

#define severity_message_t(severity, P, format)                                \
  struct severity##_log_message_t : public log_message_t<P> {                  \
    severity##_log_message_t(const char * file,                                \
      int line,                                                                \
      P && predicate = true_state,                                             \
      bool can_send_to_one = true)                                             \
      : log_message_t<P>(file, line, predicate, can_send_to_one) {}            \
                                                                               \
    ~severity##_log_message_t() {                                              \
      /* Clean colors from the stream */                                       \
      if(clean_color_) {                                                       \
        auto str = flog_t::instance().buffer_stream().str();                   \
        if(str.back() == '\n') {                                               \
          str = str.substr(0, str.size() - 1);                                 \
          str += FLOG_COLOR_PLAIN;                                             \
          str += '\n';                                                         \
          flog_t::instance().buffer_stream().str(std::string{});               \
          flog_t::instance().buffer_stream() << str;                           \
        }                                                                      \
        else {                                                                 \
          flog_t::instance().buffer_stream() << FLOG_COLOR_PLAIN;              \
        }                                                                      \
      }                                                                        \
    }                                                                          \
                                                                               \
    std::ostream &                                                             \
    stream() override /* This is replaced by the scoped logic */               \
      format                                                                   \
  }

#define message_stamp timestamp() << " " << rstrip<'/'>(file_) << ":" << line_

#if defined(FLOG_ENABLE_MPI)
#define mpi_stamp " c" << mpi_state_t::instance().rank()
#else
#define mpi_stamp ""
#endif

// Utility
severity_message_t(utility, decltype(flecsi::utils::flog::true_state), {
  std::ostream & stream =
    flog_t::instance().severity_stream(flog_t::instance().tag_enabled());
  return stream;
});

// Internal
severity_message_t(internal, decltype(flecsi::utils::flog::true_state), {
  std::ostream & stream = flog_t::instance().severity_stream(
    FLOG_STRIP_LEVEL < 2 && predicate_() && flog_t::instance().tag_enabled());

  {
    stream << FLOG_OUTPUT_LTBLUE("[INT") << FLOG_OUTPUT_PURPLE(message_stamp);
    stream << FLOG_OUTPUT_LTGRAY(mpi_stamp);
    stream << FLOG_OUTPUT_LTBLUE("] ") << FLOG_COLOR_LTGRAY;
  } // scope

  return stream;
});

// Trace
severity_message_t(trace, decltype(flecsi::utils::flog::true_state), {
  std::ostream & stream = flog_t::instance().severity_stream(
    FLOG_STRIP_LEVEL < 1 && predicate_() && flog_t::instance().tag_enabled());

  {
    stream << FLOG_OUTPUT_CYAN("[T") << FLOG_OUTPUT_LTGRAY(message_stamp);
    stream << FLOG_OUTPUT_CYAN(mpi_stamp);
    stream << FLOG_OUTPUT_CYAN("] ");
  } // scope

  return stream;
});

// Info
severity_message_t(info, decltype(flecsi::utils::flog::true_state), {
  std::ostream & stream = flog_t::instance().severity_stream(
    FLOG_STRIP_LEVEL < 2 && predicate_() && flog_t::instance().tag_enabled());

  {
    stream << FLOG_OUTPUT_GREEN("[I") << FLOG_OUTPUT_LTGRAY(message_stamp);
    stream << FLOG_OUTPUT_CYAN(mpi_stamp);
    stream << FLOG_OUTPUT_GREEN("] ");
  } // scope

  return stream;
});

// Warn
severity_message_t(warn, decltype(flecsi::utils::flog::true_state), {
  std::ostream & stream = flog_t::instance().severity_stream(
    FLOG_STRIP_LEVEL < 3 && predicate_() && flog_t::instance().tag_enabled());

  {
    stream << FLOG_OUTPUT_BROWN("[W") << FLOG_OUTPUT_LTGRAY(message_stamp);
    stream << FLOG_OUTPUT_CYAN(mpi_stamp);
    stream << FLOG_OUTPUT_BROWN("] ") << FLOG_COLOR_YELLOW;
  } // scope

  clean_color_ = true;
  return stream;
});

// Error
severity_message_t(error, decltype(flecsi::utils::flog::true_state), {
  std::ostream & stream = flog_t::instance().severity_stream(
    FLOG_STRIP_LEVEL < 4 && predicate_() && flog_t::instance().tag_enabled());

  {
    stream << FLOG_OUTPUT_RED("[E") << FLOG_OUTPUT_LTGRAY(message_stamp);
    stream << FLOG_OUTPUT_CYAN(mpi_stamp);
    stream << FLOG_OUTPUT_RED("] ") << FLOG_COLOR_LTRED;
  } // scope

  clean_color_ = true;
  return stream;
});

#undef severity_message_t

} // namespace flog
} // namespace utils
} // namespace flecsi
