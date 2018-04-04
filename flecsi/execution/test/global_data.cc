/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2018, Los Alamos National Security, LLC
   All rights reserved.
                                                                              */

///
/// \file
/// \date Initial file creation: Apr 3, 2018
///

#include <cinchtest.h>

#include <flecsi/execution/execution.h>
#include <flecsi/data/global_accessor.h>

using namespace flecsi;

template<size_t PERMISSION>
using gint = global_accessor__<int, PERMISSION>;

void set_global_int(gint<rw> global, int value, const char * message) {
  auto& context = execution::context_t::instance();
  auto rank = context.color();

  std::cout<< "[" << rank << "] starting " << message << std::endl;

  std::cout << "[" << rank << "] global = " << global.data() << std::endl;
  if (rank == 0) {
    std::cout << "[" << rank << "] setting value" << std::endl;
    global = value;
  } else {
    std::cout << "[" << rank << "] do nothing" << std::endl;
  }
  std::cout << "[" << rank << "] global = " << global.data() << std::endl;

  std::cout << "[" << rank << "] done " << message << std::endl;
}

void print_global_int(gint<ro> global, const char * message) {
  auto& context = execution::context_t::instance();
  auto rank = context.color();
  std::cout<< "[" << rank << "] starting " << message << std::endl;
  std::cout << "[" << rank << "] global = " << global.data() << std::endl;
  std::cout << "[" << rank << "] done " << message << std::endl;
}

void hello_world() {
  auto& context = execution::context_t::instance();
  auto rank = context.color();
  std::cout << "Hello world from rank " << rank << std::endl;
}

flecsi_register_task_simple(set_global_int, loc, single);
flecsi_register_task_simple(print_global_int, loc, single);
flecsi_register_task_simple(hello_world, loc, single);

flecsi_register_global(global, int1, int, 1);
flecsi_register_global(global, int2, int, 1);

namespace flecsi {
namespace execution {

//----------------------------------------------------------------------------//
// Specialization driver.
//----------------------------------------------------------------------------//

void specialization_tlt_init(int argc, char ** argv) {
  auto gh0 = flecsi_get_global(global, int1, int, 0);
  auto gh1 = flecsi_get_global(global, int2, int, 0);

  gh0 = 1042;
  gh1 = 2042;

  // // rank 0
  // flecsi_execute_task_simple(set_global_int, single, gh0, 1042, "tlt_init g0");

  // // rank 1
  // //flecsi_execute_task_simple(hello_world, single);

  // // CAN'T SET TWO DIFFERENT GLOBAS HERE
  // // rank 0
  // flecsi_execute_task_simple(set_global_int, single, gh1, 2042, "tlt_init g1");

  // flecsi_execute_task_simple(print_global_int, single, /*gh0,*/ "tlt_init");
  // flecsi_execute_task_simple(print_global_int, single, gh1, "tlt_init");
  flecsi_execute_task_simple(hello_world, single);

} // specialization_tlt_init

void specialization_spmd_init(int argc, char ** argv) {
  auto gh0 = flecsi_get_global(global, int1, int, 0);
  auto gh1 = flecsi_get_global(global, int2, int, 0);

  flecsi_execute_task_simple(print_global_int, single, gh0, "spmd_init g0");
  flecsi_execute_task_simple(print_global_int, single, gh1, "spmd_init g1");

  flecsi_execute_task_simple(hello_world, single);

} // specialization_spmd_init

//----------------------------------------------------------------------------//
// User driver.
//----------------------------------------------------------------------------//

void driver(int argc, char ** argv) {
  auto gh0 = flecsi_get_global(global, int1, int, 0);
  auto gh1 = flecsi_get_global(global, int2, int, 0);

  flecsi_execute_task_simple(print_global_int, single, gh0, "driver g0");
  flecsi_execute_task_simple(print_global_int, single, gh1, "driver g1");

  flecsi_execute_task_simple(hello_world, single);

  // auto& context = execution::context_t::instance();
  // if(context.color() == 0){
  //   ASSERT_TRUE(CINCH_EQUAL_BLESSED("global_data.blessed"));
  // }

} // specialization_driver

//----------------------------------------------------------------------------//
// TEST.
//----------------------------------------------------------------------------//

TEST(dense_data, testname) {

} // TEST

} // namespace execution
} // namespace flecsi

/*~------------------------------------------------------------------------~--*
 * Formatting options for vim.
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~------------------------------------------------------------------------~--*/
