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

#if !defined(__FLECSI_PRIVATE__)
#error Do not include this file directly!
#else
#include <flecsi/data/common/privilege.hh>
#include <flecsi/data/common/storage_classes.hh>
#include <flecsi/data/common/topology_accessor.hh>
#include <flecsi/execution/context.hh>
#include <flecsi/topology/ntree/interface.hh>
#include <flecsi/topology/set/interface.hh>
#include <flecsi/topology/structured_mesh/interface.hh>
//#include <flecsi/topology/unstructured_mesh/interface.hh>
#include <flecsi/utils/demangle.hh>
#include <flecsi/utils/tuple_walker.hh>
#endif

#if !defined(FLECSI_ENABLE_LEGION)
#error FLECSI_ENABLE_LEGION not defined! This file depends on Legion!
#endif

#include <legion.h>

flog_register_tag(init_args);

namespace flecsi {
namespace execution {
namespace legion {

using namespace flecsi::data;

/*!
  The init_args_t type can be called to walk task args before the
  task launcher is created. This allows us to gather region requirements
  and to set state on the associated data handles \em before Legion gets
  the task arguments tuple.

  @ingroup execution
*/

struct init_args_t : public flecsi::utils::tuple_walker_u<init_args_t> {

  /*!
    Construct an init_args_t instance.

    @param runtime The Legion task runtime.
    @param context The Legion task runtime context.
   */

  init_args_t(Legion::Runtime * runtime,
    Legion::Context & context,
    const size_t & domain)
    : runtime_(runtime), context_(context), domain_(domain) {}

  std::vector<Legion::RegionRequirement> const & region_requirements() {
    return region_reqs_;
  } // region_requirements

  /*!
    Convert the template privileges to proper Legion privileges.

    @param mode privilege
   */

  static Legion::PrivilegeMode privilege_mode(size_t mode) {
    switch(mode) {
      case size_t(nu):
        return WRITE_DISCARD;
      case size_t(ro):
        return READ_ONLY;
      case size_t(wo):
        return WRITE_DISCARD;
      case size_t(rw):
        return READ_WRITE;
      default:
        flog_fatal("invalid privilege mode");
    } // switch

    return NO_ACCESS;
  } // privilege_mode

  /*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*
    The following methods are specializations on storage class and client
    type, potentially for every permutation thereof.
   *^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

  /*--------------------------------------------------------------------------*
    Global Topology
   *--------------------------------------------------------------------------*/

  template<typename DATA_TYPE, size_t PRIVILEGES>
  void visit(global_topology::accessor_u<DATA_TYPE, PRIVILEGES> & accessor) {
    const auto fid =
      context_t::instance()
        .get_field_info_store(
          global_topology_t::type_identifier_hash, data::storage_label_t::dense)
        .get_field_info(accessor.identifier())
        .fid;

    Legion::LogicalRegion region =
      context_t::instance().global_topology_instance().logical_region;

    static_assert(privilege_count<PRIVILEGES>() == 1,
      "global topology accessor type only takes one privilege");

    if constexpr(get_privilege<0, PRIVILEGES>() > partition_privilege_t::ro) {
      flog_assert(domain_ == 1,
        "global can only be modified from within single launch task");

      Legion::RegionRequirement rr(region,
        privilege_mode(get_privilege<0, PRIVILEGES>()),
        EXCLUSIVE,
        region);

      rr.add_field(fid);
      region_reqs_.push_back(rr);
    }
    else {
      Legion::RegionRequirement rr(region, READ_ONLY, EXCLUSIVE, region);

      rr.add_field(fid);
      region_reqs_.push_back(rr);
    } // if
  } // visit

  /*--------------------------------------------------------------------------*
    Index Topology
   *--------------------------------------------------------------------------*/

  template<typename DATA_TYPE, size_t PRIVILEGES>
  void visit(index_topology::accessor_u<DATA_TYPE, PRIVILEGES> & accessor) {
    auto & flecsi_context = context_t::instance();

    const auto fid =
      flecsi_context
        .get_field_info_store(
          index_topology_t::type_identifier_hash, data::storage_label_t::dense)
        .get_field_info(accessor.identifier())
        .fid;

    auto instance_data =
      flecsi_context.index_topology_instance(accessor.topology_identifier());

    flog_assert(instance_data.colors = domain_,
      "attempting to pass index topology reference with size "
        << instance_data.colors << " into task with launch domain of size "
        << domain_);

    static_assert(privilege_count<PRIVILEGES>() == 1,
      "index topology accessor type only takes one privilege");

    Legion::RegionRequirement rr(instance_data.color_partition,
      0,
      privilege_mode(get_privilege<0, PRIVILEGES>()),
      EXCLUSIVE,
      instance_data.logical_region);

    rr.add_field(fid);
    region_reqs_.push_back(rr);
  } // visit

  /*--------------------------------------------------------------------------*
    NTree Topology
   *--------------------------------------------------------------------------*/

  template<typename POLICY_TYPE, size_t PRIVILEGES>
  using ntree_accessor_u =
    topology_accessor_u<ntree_topology_u<POLICY_TYPE>, PRIVILEGES>;

  template<typename POLICY_TYPE, size_t PRIVILEGES>
  void visit(ntree_accessor_u<POLICY_TYPE, PRIVILEGES> & accessor) {} // visit

  /*--------------------------------------------------------------------------*
    Set Topology
   *--------------------------------------------------------------------------*/

  template<typename POLICY_TYPE, size_t PRIVILEGES>
  using set_accessor_u =
    topology_accessor_u<set_topology_u<POLICY_TYPE>, PRIVILEGES>;

  template<typename POLICY_TYPE, size_t PRIVILEGES>
  void visit(set_accessor_u<POLICY_TYPE, PRIVILEGES> & accessor) {} // visit

  /*--------------------------------------------------------------------------*
    Structured Mesh Topology
   *--------------------------------------------------------------------------*/

  template<typename POLICY_TYPE, size_t PRIVILEGES>
  using structured_mesh_accessor_u =
    topology_accessor_u<structured_mesh_topology_u<POLICY_TYPE>, PRIVILEGES>;

  template<typename POLICY_TYPE, size_t PRIVILEGES>
  void visit(structured_mesh_accessor_u<POLICY_TYPE, PRIVILEGES> & accessor) {
  } // visit

  /*--------------------------------------------------------------------------*
    Non-FleCSI Data Types
   *--------------------------------------------------------------------------*/

  template<typename DATA_TYPE>
  static typename std::enable_if_t<
    !std::is_base_of_v<data::data_reference_base_t, DATA_TYPE>>
  visit(DATA_TYPE &) {
    {
      flog_tag_guard(init_args);
      flog_devel(info) << "Skipping argument with type "
                       << flecsi::utils::type<DATA_TYPE>() << std::endl;
    }
  } // visit

private:
  Legion::Runtime * runtime_;
  Legion::Context & context_;
  size_t domain_;

  std::vector<Legion::RegionRequirement> region_reqs_;

}; // init_args_t

} // namespace legion
} // namespace execution
} // namespace flecsi
