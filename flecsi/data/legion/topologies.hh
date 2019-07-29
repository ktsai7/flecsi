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

#include <flecsi/data/common/data_reference.hh>
#include <flecsi/execution/context.hh>
#include <flecsi/runtime/types.hh>
#include <flecsi/utils/flog.hh>

#if !defined(FLECSI_ENABLE_LEGION)
#error FLECSI_ENABLE_LEGION not defined! This file depends on Legion!
#endif

#include <legion.h>

flog_register_tag(topologies);

namespace flecsi {
namespace data {
// FIXME: get rid of this namespace
namespace legion {

using namespace topology;

template<typename TOPOLOGY_TYPE>
struct topology_instance_u;

/*----------------------------------------------------------------------------*
  Index Topology.
 *----------------------------------------------------------------------------*/

template<>
struct topology_instance_u<index_topology_t> {

  using topology_reference_t = topology_reference_u<index_topology_t>;

  static void create(topology_reference_t const & topology_reference,
    index_topology_t::coloring_t const & coloring) {

    {
      flog_tag_guard(topologies);
      flog_devel(info) << "Set coloring for " << topology_reference.identifier()
                       << std::endl;
    }

    auto legion_runtime = Legion::Runtime::get_runtime();
    auto legion_context = Legion::Runtime::get_context();
    auto & flecsi_context = execution::context_t::instance();

    auto & runtime_data =
      flecsi_context.index_topology_instance(topology_reference.identifier());

    runtime_data.colors = coloring.size();

    // Maybe this line can go away
    runtime_data.index_space_id = unique_isid_t::instance().next();

    LegionRuntime::Arrays::Rect<1> bounds(0, coloring.size() - 1);
    Legion::Domain domain(Legion::Domain::from_rect<1>(bounds));

    runtime_data.index_space =
      legion_runtime->create_index_space(legion_context, domain);

    runtime_data.field_space =
      legion_runtime->create_field_space(legion_context);

    auto & field_info_store = flecsi_context.get_field_info_store(
      index_topology_t::type_identifier_hash, storage_label_t::dense);

    Legion::FieldAllocator allocator = legion_runtime->create_field_allocator(
      legion_context, runtime_data.field_space);

    for(auto const & fi : field_info_store.field_info()) {
      allocator.allocate_field(fi.type_size, fi.fid);
    } // for

    runtime_data.logical_region = legion_runtime->create_logical_region(
      legion_context, runtime_data.index_space, runtime_data.field_space);

    Legion::IndexPartition index_partition =
      legion_runtime->create_equal_partition(
        legion_context, runtime_data.index_space, runtime_data.index_space);

    runtime_data.color_partition = legion_runtime->get_logical_partition(
      legion_context, runtime_data.logical_region, index_partition);

  } // create

  static void destroy(topology_reference_t const & topology_reference) {

    {
      flog_tag_guard(topologies);
      flog_devel(info) << "Set coloring for " << topology_reference.identifier()
                       << std::endl;
    }

    auto legion_runtime = Legion::Runtime::get_runtime();
    auto legion_context = Legion::Runtime::get_context();
    auto & flecsi_context = execution::context_t::instance();

    auto & runtime_data =
      flecsi_context.index_topology_instance(topology_reference.identifier());

    legion_runtime->destroy_logical_region(
      legion_context, runtime_data.logical_region);

    legion_runtime->destroy_field_space(
      legion_context, runtime_data.field_space);

    legion_runtime->destroy_index_space(
      legion_context, runtime_data.index_space);

  } // destroy

}; // index_topology_t specialization

/*----------------------------------------------------------------------------*
  NTree Topology.
 *----------------------------------------------------------------------------*/

template<typename POLICY_TYPE>
struct topology_instance_u<ntree_topology_u<POLICY_TYPE>> {

  using topology_reference_t =
    topology_reference_u<ntree_topology_u<POLICY_TYPE>>;
  using coloring_t = typename ntree_topology_u<POLICY_TYPE>::coloring_t;

  static void create(topology_reference_t const & topology_reference,
    coloring_t const & coloring) {}

  static void destroy(topology_reference_t const & topology_reference) {}

}; // ntree_topology_u specialization

/*----------------------------------------------------------------------------*
  Set Topology.
 *----------------------------------------------------------------------------*/

template<typename POLICY_TYPE>
struct topology_instance_u<set_topology_u<POLICY_TYPE>> {

  using topology_reference_t =
    topology_reference_u<set_topology_u<POLICY_TYPE>>;

}; // set_topology_u specialization

/*----------------------------------------------------------------------------*
  Structured Mesh Topology.
 *----------------------------------------------------------------------------*/

template<typename POLICY_TYPE>
struct topology_instance_u<structured_mesh_topology_u<POLICY_TYPE>> {

  using topology_reference_t =
    topology_reference_u<structured_mesh_topology_u<POLICY_TYPE>>;

}; // structured_mesh_topology_u specialization

/*----------------------------------------------------------------------------*
  Unstructured Mesh Topology.
 *----------------------------------------------------------------------------*/

template<typename POLICY_TYPE>
struct topology_instance_u<unstructured_mesh_topology_u<POLICY_TYPE>> {

  using topology_reference_t =
    topology_reference_u<unstructured_mesh_topology_u<POLICY_TYPE>>;

}; // unstructured_mesh_topology_u specialization

// NOTE THAT THE HANDLE TYPE FOR THIS TYPE WILL NEED TO CAPTURE THE
// UNDERLYING TOPOLOGY TYPE, i.e., topology::mesh_topology_t<MESH_POLICY>

#if 0
template<typename MESH_POLICY>
struct client_handle_specialization_u<topology::mesh_topology_t<MESH_POLICY>> {

  using client_t = topology::mesh_topology_t<MESH_POLICY>;

  template<size_t NAMESPACE, size_t NAME>
  static client_handle_u<client_t, 0> get_client_handle() {
    client_handle_u<client_t, 0> h;
    return h;
  } // get_client_handle

}; // client_handle_specialization_u<topology::mesh_topology_t<MESH_POLICY>>
#endif

} // namespace legion
} // namespace data
} // namespace flecsi
