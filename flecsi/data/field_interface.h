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

/*!
  @file

  This file contains the C++ data model interface \em field_interface_t
  for FleCSI field data structures. The \em field_interface_t type is a
  specialization of the \em field_interface_u type on the backend runtimne
  policy that is selected at compile time.
 */

#include <flecsi/data/common/data_reference.h>
#include <flecsi/execution/context.h>
#include <flecsi/utils/common.h>
#include <flecsi/utils/hash.h>

namespace flecsi {
namespace data {

/*!
  The field_interface_u type provides a high-level field interface that
  is implemented by the given data policy.

  @tparam DATA_POLICY The backend data policy.

  @ingroup data
 */

template<typename DATA_POLICY>
struct field_interface_u {

  /*!
    Register a field with the FleCSI runtime. This method should be thought
    of as registering a field attribute on the given data client type.
    All instances of the client type will have this attribute. However,
    this does not mean that each data client instance will have an
    an instance of the attribute. Attribute instances will be created
    only when they are actually mapped into a task.

    @tparam TOPOLOGY_TYPE The data client type on which the data
                          attribute should be registered.
    @tparam STORAGE_CLASS The storage type for the data attribute.
    @tparam DATA_TYPE     The data type, e.g., double. This may be
                          P.O.D. or a user-defined type that is
                          trivially-copyable.
    @tparam NAMESPACE     The namespace key. Namespaces allow separation
                          of attribute names to avoid collisions.
    @tparam NAME          The attribute name.
    @tparam VERSIONS      The number of versions that shall be associated
                          with this attribute.
    @tparam INDEX_SPACE   The index space identifier.

    @param name The string version of the field name.

    @ingroup data
   */

  template<typename TOPOLOGY_TYPE,
    size_t STORAGE_CLASS,
    typename DATA_TYPE,
    size_t NAMESPACE,
    size_t NAME,
    size_t VERSIONS,
    size_t INDEX_SPACE = 0>
  static bool register_field(std::string const & name) {
    static_assert(VERSIONS <= utils::hash::field_max_versions,
      "max field versions exceeded");

    using unique_fid_t =
      utils::unique_id_t<field_id_t, FLECSI_GENERATED_ID_MAX>;

    field_info_t fi;

    fi.namespace_hash = NAMESPACE;
    fi.name_hash = NAME;
    fi.type_size = sizeof(DATA_TYPE);
    fi.versions = VERSIONS;
    fi.index_space = INDEX_SPACE;

    flog(internal) << "Registering field" << std::endl
                   << "\tname: " << name << std::endl
                   << "\ttype: " << utils::demangle(typeid(DATA_TYPE).name())
                   << std::endl;

    for(size_t version(0); version < VERSIONS; ++version) {
      fi.fid = unique_fid_t::instance().next();
      fi.key = utils::hash::field_hash<NAMESPACE, NAME>(version);

      execution::context_t::instance().register_field_info(
        TOPOLOGY_TYPE::type_identifier_hash, STORAGE_CLASS, fi);
    } // for

    return true;
  } // register_field

  /*!
    Return the handle associated with the given parameters and data client.

    @tparam TOPOLOGY_TYPE The data client type on which the data
                          attribute should be registered.
    @tparam STORAGE_CLASS The storage type for the data attribute.
    @tparam DATA_TYPE     The data type, e.g., double. This may be
                          P.O.D. or a user-defined type that is
                          trivially-copyable.
    @tparam NAMESPACE     The namespace key. Namespaces allow separation
                          of attribute names to avoid collisions.
    @tparam NAME          The attribute name.
    @tparam INDEX_SPACE   The index space identifier.
    @tparam VERSION       The data version.

    @ingroup data
   */

  template<typename TOPOLOGY_TYPE,
    size_t STORAGE_CLASS,
    typename DATA_TYPE,
    size_t NAMESPACE,
    size_t NAME,
    size_t VERSION = 0>
  static decltype(auto) get_field(
    topology_reference_u<TOPOLOGY_TYPE> const & topology_reference) {

    static_assert(
      VERSION < utils::hash::field_max_versions, "max field version exceeded");

    using storage_class_t =
      typename DATA_POLICY::template storage_class_u<STORAGE_CLASS,
        TOPOLOGY_TYPE>;

    return storage_class_t::
      template get_reference<DATA_TYPE, NAMESPACE, NAME, VERSION>(
        topology_reference);
  } // get_field

#if 0
  /*!
    Return the mutator associated with the given parameters and data client.

    @tparam TOPOLOGY_TYPE The data client type on which the data
                          attribute should be registered.
    @tparam STORAGE_CLASS The storage type for the data attribute.
    @tparam DATA_TYPE     The data type, e.g., double. This may be
                          P.O.D. or a user-defined type that is
                          trivially-copyable.
    @tparam NAMESPACE     The namespace key. Namespaces allow separation
                          of attribute names to avoid collisions.
    @tparam NAME          The attribute name.
    @tparam INDEX_SPACE   The index space identifier.
    @tparam VERSION       The data version.

    @ingroup data
   */

  template<typename TOPOLOGY_TYPE,
    size_t STORAGE_CLASS,
    typename DATA_TYPE,
    size_t NAMESPACE,
    size_t NAME,
    size_t VERSION = 0,
    size_t PERMISSIONS>
  static decltype(auto) get_mutator(
    const client_handle_u<TOPOLOGY_TYPE, PERMISSIONS> & client_handle,
    size_t slots) {
    static_assert(
      VERSION < utils::hash::field_max_versions, "max field version exceeded");

    using storage_class_t =
      typename DATA_POLICY::template storage_class_u<STORAGE_CLASS>;

    return storage_class_t::template get_mutator<TOPOLOGY_TYPE, DATA_TYPE,
      NAMESPACE, NAME, VERSION>(client_handle, slots);
  } // get_mutator
#endif

}; // struct field_interface_u

} // namespace data
} // namespace flecsi

//----------------------------------------------------------------------------//
// This include file defines the FLECSI_RUNTIME_DATA_POLICY used below.
//----------------------------------------------------------------------------//

#include <flecsi/runtime/data_policy.h>

namespace flecsi {
namespace data {

/*!
  The field_interface_t type is the high-level interface to the FleCSI
  field data model.

  @ingroup data
 */

using field_interface_t = field_interface_u<FLECSI_RUNTIME_DATA_POLICY>;

/*!
  The global_accessor_u type is the high-level accessor type for FleCSI
  field data on the global topology.
 */

template<typename DATA_TYPE, size_t PRIVILEGES>
using global_accessor_u =
  typename FLECSI_RUNTIME_DATA_POLICY::global_accessor_u<DATA_TYPE, PRIVILEGES>;

/*!
  The color_accessor_u type is the high-level accessor type for FleCSI
  field data on the color topology.
 */

template<typename DATA_TYPE, size_t PRIVILEGES>
using index_accessor_u =
  typename FLECSI_RUNTIME_DATA_POLICY::index_accessor_u<DATA_TYPE, PRIVILEGES>;

} // namespace data
} // namespace flecsi
