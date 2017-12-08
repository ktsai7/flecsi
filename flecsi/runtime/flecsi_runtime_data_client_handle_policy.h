/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2015 Los Alamos National Security, LLC
 * All rights reserved.
 *~--------------------------------------------------------------------------~*/

#ifndef flecsi_runtime_data_client_handle_policy_h
#define flecsi_runtime_data_client_handle_policy_h

//----------------------------------------------------------------------------//
//! @file
//! @date Initial file creation: Jun 21, 2017
//----------------------------------------------------------------------------//

#include <flecsi-config.h>

//----------------------------------------------------------------------------//
// This section works with the build system to select the correct runtime
// implemenation for the task model. If you add to the possible runtimes,
// remember to edit config/packages.cmake to include a definition using
// the same convention, e.g., -DFLECSI_RUNTIME_MODEL_new_runtime.
//----------------------------------------------------------------------------//

// Legion Policy
#if FLECSI_RUNTIME_MODEL == FLECSI_RUNTIME_MODEL_legion

  #include "flecsi/data/legion/data_client_handle_policy.h"

  namespace flecsi {

  using FLECSI_RUNTIME_DATA_CLIENT_HANDLE_POLICY =
    legion_data_client_handle_policy_t;

  } // namespace flecsi

// MPI Policy
#elif FLECSI_RUNTIME_MODEL == FLECSI_RUNTIME_MODEL_mpi

  #include "flecsi/data/mpi/data_client_handle_policy.h"

  namespace flecsi {

  using FLECSI_RUNTIME_DATA_CLIENT_HANDLE_POLICY =
    mpi_data_client_handle_policy_t;
  } // namespace flecsi

#endif // FLECSI_RUNTIME_MODEL

#endif // flecsi_runtime_data_client_handle_policy_h

/*~-------------------------------------------------------------------------~-*
 * Formatting options for vim.
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~-------------------------------------------------------------------------~-*/