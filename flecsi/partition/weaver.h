/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2015 Los Alamos National Security, LLC
 * All rights reserved.
 *~--------------------------------------------------------------------------~*/

#ifndef flecsi_dmp_weaver_h
#define flecsi_dmp_weaver_h

#include "flecsi/io/simple_definition.h"
#include "flecsi/partition/dcrs_utils.h"
#include "flecsi/partition/parmetis_partitioner.h"
#include "flecsi/partition/mpi_communicator.h"
#include "flecsi/utils/set_utils.h"

namespace flecsi {
namespace dmp {

class weaver {
private:
  using entry_info_t = flecsi::dmp::entry_info_t;

  flecsi::io::mesh_definition_t &sd;

  std::set<size_t> primary_cells;
  std::set<entry_info_t> exclusive_cells;
  std::set<entry_info_t> shared_cells;
  std::set<entry_info_t> ghost_cells;

  std::set<size_t> primary_vertices;
  std::set<entry_info_t> exclusive_vertices;
  std::set<entry_info_t> shared_vertices;
  std::set<entry_info_t> ghost_vertices;

public:
  weaver(flecsi::io::mesh_definition_t &mesh) : sd(mesh) {
    int size;
    int rank;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Create a mesh definition from file.
    //flecsi::io::simple_definition_t sd("simple2d-4x4.msh");

    // Create the dCRS representation for the distributed
    // partitioner.
    auto dcrs = flecsi::dmp::make_dcrs(sd);

    // Create a partitioner instance to generate the primary partition.
    auto partitioner = std::make_shared<flecsi::dmp::parmetis_partitioner_t>();

    // Create the primary partition.
    //auto primary = partitioner->partition(dcrs);
    primary_cells = partitioner->partition(dcrs);
//    std::cout << "rank: " << rank << ", primary cells: ";
//    for(auto i: primary_cells) {
//      std::cout << i << " ";
//    } // for

    // Compute the dependency closure of the primary cell partition
    // through vertex intersections (specified by last argument "1").
    // To specify edge or face intersections, use 2 (edges) or 3 (faces).
    // FIXME: We may need to replace this with a predicate function.
    auto closure = flecsi::io::cell_closure(sd, primary_cells, 1);

    // Subtracting out the initial set leaves just the nearest
    // neighbors. This is similar to the image of the adjacency
    // graph of the initial indices.
    auto nearest_neighbors = flecsi::utils::set_difference(closure, primary_cells);

    // The closure of the nearest neighbors intersected with
    // the initial indeces gives the shared indices. This is similar to
    // the preimage of the nearest neighbors.
    auto nearest_neighbor_closure =
      flecsi::io::cell_closure(sd, nearest_neighbors, 1);

    // We can iteratively add halos of nearest neighbors, e.g.,
    // here we add the next nearest neighbors. For most mesh types
    // we actually need information about the ownership of these indices
    // so that we can deterministically assign rank ownership to vertices.
    auto next_nearest_neighbors =
      flecsi::utils::set_difference(nearest_neighbor_closure, closure);

    // The union of the nearest and next-nearest neighbors gives us all
    // of the cells that might reference a vertex that we need.
    auto all_neighbors = flecsi::utils::set_union(nearest_neighbors,
      next_nearest_neighbors);

    // Create a communicator instance to get neighbor information.
    auto communicator = std::make_shared<flecsi::dmp::mpi_communicator_t>();

    // Get the rank and offset information for our nearest neighbor
    // dependencies. This also gives information about the ranks
    // that access our shared cells.
    auto cell_nn_info = communicator->get_cell_info(primary_cells,
      nearest_neighbors);

    //
    auto cell_all_info = communicator->get_cell_info(primary_cells,
      all_neighbors);

    // TODO: seperate this part into its own function.
    // Create a map version of the local info for lookups below.
    std::unordered_map <size_t, size_t> primary_indices_map;
    {
      size_t offset(0);
      for (auto i: primary_cells) {
        primary_indices_map[offset++] = i;
      } // for
    } // scope

    // Create a map version of the remote info for lookups below.
    std::unordered_map <size_t, entry_info_t> remote_info_map;
    for (auto i: std::get<1>(cell_all_info)) {
      remote_info_map[i.id] = i;
    } // for


    // Populate exclusive and shared cell information.
    {
      size_t offset(0);
      for (auto i: std::get<0>(cell_nn_info)) {
        if (i.size()) {
          shared_cells.insert(entry_info_t(primary_indices_map[offset],
            rank, offset, i));
        } else {
          exclusive_cells.insert(entry_info_t(primary_indices_map[offset],
            rank, offset, i));
        } // if
        ++offset;
      } // for
    } // scope

    // Populate ghost cell information.
    {
      size_t offset(0);
      for (auto i: std::get<1>(cell_nn_info)) {
        ghost_cells.insert(i);
      } // for
    } // scope


    // TODO: move this into is own function
    // Create a map version for lookups below.
    std::unordered_map <size_t, entry_info_t> shared_cells_map;
    {
      for (auto i: shared_cells) {
        shared_cells_map[i.id] = i;
      } // for
    } // scope

    // Form the vertex closure
    auto vertex_closure = flecsi::io::vertex_closure(sd, closure);

    // Assign vertex ownership
    std::vector <std::set<size_t>> vertex_requests(size);
    std::set <entry_info_t> vertex_info;

    size_t offset(0);
    for (auto i: vertex_closure) {

      // Get the set of cells that reference this vertex.
      auto referencers = flecsi::io::vertex_referencers(sd, i);
      size_t min_rank(std::numeric_limits<size_t>::max());
      std::set <size_t> shared_vertices;
//      if (i == 31) {
//        std::cout << "rank: " << rank << ", vertex 31, referencers: ";
//        for (auto i : referencers) {
//          std::cout << i << " ";
//        }
//        std::cout << std::endl;
//      }
      for (auto c: referencers) {

        // If the referencing cell isn't in the remote info map
        // it is a local cell.
        if (remote_info_map.find(c) != remote_info_map.end()) {
//          if (i == 31) {
//            std::cout << "rank: " << rank
//                      << ", vertex 31 is reference by remote cell: "
//                      << c << "\n";
//          }
          min_rank = std::min(min_rank, remote_info_map[c].rank);
          shared_vertices.insert(remote_info_map[c].rank);
        } else {
          min_rank = std::min(min_rank, size_t(rank));

          // If the local cell is shared, we need to add all of
          // the ranks that reference it.
          if (shared_cells_map.find(c) != shared_cells_map.end()) {
            shared_vertices.insert(shared_cells_map[c].shared.begin(),
              shared_cells_map[c].shared.end());
//            if (i == 31) {
//              std::cout << "rank: " << rank
//                        << ", vertex 31 is reference by local cell: "
//                        << c << "\n";
//            }
          } // if
        } // if
      } // for

//      std::cout << "rank: " << rank << ", vertex: " << i << ", shared by ranks: ";
//      for (auto i : shared_vertices) {
//        std::cout << i << " ";
//      }
//      std::cout << std::endl;
      if (min_rank == rank) {
        // This is a vertex that belongs to our rank.
        //auto entry = entry_info_t(i, rank, offset, shared_vertices);
        vertex_info.insert(entry_info_t(i, rank, offset++, shared_vertices));
      } else {
        // Add remote vertex to the request for offset information.
        vertex_requests[min_rank].insert(i);
      } // fi
    } // for

    auto vertex_offset_info =
      communicator->get_vertex_info(vertex_info, vertex_requests);

    for (auto i: vertex_info) {
      if (i.shared.size()) {
        shared_vertices.insert(i);
      } else {
        exclusive_vertices.insert(i);
      } // if
    } // for

   //filling out primary partition for vertices
   primary_vertices = flecsi::io::vertex_closure(sd, primary_cells);   

    {
      size_t r(0);
      for (auto i: vertex_requests) {

        auto offset(vertex_offset_info[r].begin());
        for (auto s: i) {
          ghost_vertices.insert(entry_info_t(s, r, *offset));
          ++offset;
        } // for

        ++r;
      } // for
    } // scope
  }

  // FIXME: should I return const & of the data member instead of copy?
  std::set<size_t> get_primary_cells() {
    return primary_cells;
  }

  std::set<entry_info_t> get_exclusive_cells() {
    return exclusive_cells;
  }

  std::set<entry_info_t> get_shared_cells() {
    return shared_cells;
  }

  std::set<entry_info_t> get_ghost_cells() {
    return ghost_cells;
  }

  std::set<size_t> get_primary_vertices() {
    return primary_vertices;
  }

  std::set<entry_info_t> get_exclusive_vertices() {
    return exclusive_vertices;
  }

  std::set<entry_info_t> get_shared_vertices() {
    return shared_vertices;
  }

  std::set<entry_info_t> get_ghost_vertices() {
    return ghost_vertices;
  }

}; // class weaver
} // namespace dmp
} // namespace flecsi

#endif // flecsi_dmp_weaver_h
