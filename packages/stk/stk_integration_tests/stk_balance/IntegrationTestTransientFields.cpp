// Copyright 2002 - 2008, 2010, 2011 National Technology Engineering
// Solutions of Sandia, LLC (NTESS). Under the terms of Contract
// DE-NA0003525 with NTESS, the U.S. Government retains certain rights
// in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
// 
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
// 
//     * Neither the name of NTESS nor the names of its contributors
//       may be used to endorse or promote products derived from this
//       software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stk_balance/balance.hpp>
#include <stk_balance/balanceUtils.hpp>
#include <stk_balance/internal/privateDeclarations.hpp>

#include "test_utils/StkBalanceRunner.hpp"
#include <stk_unit_test_utils/unittestMeshUtils.hpp>
#include <stk_unit_test_utils/MeshFixture.hpp>
#include <stk_unit_test_utils/ioUtils.hpp>
#include "stk_unit_test_utils/GenerateALefRAMesh.hpp"
#include "stk_unit_test_utils/BuildMesh.hpp"
#include "stk_io/FillMesh.hpp"
#include "stk_io/WriteMesh.hpp"
#include <stk_io/StkMeshIoBroker.hpp>

#include "stk_mesh/base/GetEntities.hpp"
#include "stk_io/StkIoUtils.hpp"
#include "stk_io/IossBridge.hpp"
#include "stk_mesh/base/FieldParallel.hpp"
#include <stk_tools/mesh_clone/MeshClone.hpp>
#include <iostream>
#include <limits>

using stk::unit_test_util::build_mesh;

class TransientWriter
{
public:
  TransientWriter(MPI_Comm c, const std::string& name)
    : m_comm(c),
      m_fileBaseName(name),
      m_fieldName("myFieldName"),
      m_varName("TestTime")
  { }

  void set_field_name(const std::string& name)
  {
    m_fieldName = name;
  }

  void set_global_variable_name(const std::string& name)
  {
    m_varName = name;
  }

  void set_time_steps(const std::vector<double>& steps)
  {
    m_timeSteps = steps;
  }

  void write_static_mesh(const std::string& meshDesc) const
  {
    stk::unit_test_util::simple_fields::generated_mesh_to_file_in_serial(meshDesc, m_fileBaseName);
  }

  void write_transient_mesh(const std::string& meshDesc) const
  {
    stk::unit_test_util::simple_fields::generated_mesh_with_transient_data_to_file_in_serial(
          meshDesc, m_fileBaseName, m_fieldName, m_varName, m_timeSteps, m_fieldSetter);
  }

  void write_two_element_mesh_with_sideset(stk::unit_test_util::ElementOrdering elemOrdering) const
  {
    if (stk::parallel_machine_rank(m_comm) == 0) {
      std::shared_ptr<stk::mesh::BulkData> bulkPtr = build_mesh(3, MPI_COMM_SELF);
      stk::mesh::BulkData& bulk = *bulkPtr;

      stk::unit_test_util::simple_fields::create_AB_mesh_with_sideset_and_field(
            bulk, stk::unit_test_util::LEFT, elemOrdering, "dummyField");
      stk::io::write_mesh_with_fields(m_fileBaseName, bulk, 1, 1.0);
    }
  }

private:
  MPI_Comm m_comm;
  std::string m_fileBaseName;
  std::string m_fieldName;
  std::string m_varName;
  std::vector<double> m_timeSteps;
  stk::unit_test_util::IdAndTimeFieldValueSetter m_fieldSetter;
};

class TransientFieldBalance : public stk::unit_test_util::simple_fields::MeshFixture
{
public:
  TransientFieldBalance()
    : fileBaseName("transient.e"),
      balanceRunner(get_comm()),
      writer(get_comm(), fileBaseName),
      verifier(get_comm()),
      m_initialMesh(get_comm()),
      m_balancedMesh(get_comm())
  {
    balanceRunner.set_filename(fileBaseName);
    balanceRunner.set_output_dir(".");
    balanceRunner.set_app_type_defaults("sd");
    balanceRunner.set_decomp_method("rcb");
  }

  stk::unit_test_util::simple_fields::MeshFromFile& get_initial_mesh()
  {
    if (m_initialMesh.is_empty()) read_initial_mesh();
    return m_initialMesh;
  }

  stk::unit_test_util::simple_fields::MeshFromFile& get_balanced_mesh()
  {
    if (m_balancedMesh.is_empty()) read_balanced_mesh();
    return m_balancedMesh;
  }

  void cleanup_files()
  {
    unlink_serial_file(fileBaseName);
    unlink_parallel_file(fileBaseName);
  }

private:
  void read_initial_mesh()
  {
    m_initialMesh.fill_from_serial(fileBaseName);
  }

  void read_balanced_mesh()
  {
    m_balancedMesh.fill_from_parallel(fileBaseName);
  }

  void unlink_parallel_file(const std::string& baseName)
  {
    std::string parallelName = stk::io::construct_parallel_filename(baseName, get_parallel_size(), get_parallel_rank());
    unlink_serial_file(parallelName);
  }

  void unlink_serial_file(const std::string& fileName)
  {
    unlink(fileName.c_str());
  }

protected:
  const std::string fileBaseName;
  stk::integration_test_utils::StkBalanceRunner balanceRunner;
  TransientWriter writer;
  const stk::unit_test_util::simple_fields::TransientVerifier verifier;

private:
  stk::unit_test_util::simple_fields::MeshFromFile m_initialMesh;
  stk::unit_test_util::simple_fields::MeshFromFile m_balancedMesh;
};

TEST_F(TransientFieldBalance, verifyStaticDataTransfer)
{
  if (get_parallel_size() == 2 ||
      get_parallel_size() == 4 ||
      get_parallel_size() == 8 ||
      get_parallel_size() == 16) {

    writer.write_static_mesh("1x4x4");

    stk::unit_test_util::simple_fields::MeshFromFile& initialMesh = get_initial_mesh();
    verifier.verify_time_steps(initialMesh, {});
    verifier.verify_num_transient_fields(initialMesh, 0u);

    balanceRunner.run_end_to_end();

    stk::unit_test_util::simple_fields::MeshFromFile& balancedMesh = get_balanced_mesh();
    verifier.verify_time_steps(balancedMesh, {});
    verifier.verify_num_transient_fields(balancedMesh, 0u);

    cleanup_files();
  }
}

TEST_F(TransientFieldBalance, verifyNumberOfSteps)
{
  if (get_parallel_size() != 2) return;

  const std::vector<double> timeSteps = {0.0, 1.0, 2.0, 3.0, 4.0};

  writer.set_time_steps(timeSteps);
  writer.write_transient_mesh("1x1x20");

  stk::unit_test_util::simple_fields::MeshFromFile& initialMesh = get_initial_mesh();
  verifier.verify_time_steps(initialMesh, timeSteps);
  verifier.verify_num_transient_fields(initialMesh, 2u);

  balanceRunner.run_end_to_end();

  stk::unit_test_util::simple_fields::MeshFromFile& balancedMesh = get_balanced_mesh();
  verifier.verify_time_steps(balancedMesh, timeSteps);

  cleanup_files();
}

TEST_F(TransientFieldBalance, verifyGlobalVariable)
{
  if (get_parallel_size() != 2) return;

  const std::vector<double> timeSteps = {0.0, 1.0, 2.0, 3.0, 4.0};
  const std::string globalVariableName = "test_time";

  writer.set_time_steps(timeSteps);
  writer.set_global_variable_name(globalVariableName);
  writer.write_transient_mesh("1x1x20");

  stk::unit_test_util::simple_fields::MeshFromFile& initialMesh = get_initial_mesh();
  verifier.verify_time_steps(initialMesh, timeSteps);
  verifier.verify_num_transient_fields(initialMesh, 2u);

  balanceRunner.run_end_to_end();

  stk::unit_test_util::simple_fields::MeshFromFile& balancedMesh = get_balanced_mesh();
  verifier.verify_global_variables_at_each_time_step(balancedMesh, globalVariableName, timeSteps);

  cleanup_files();
}

TEST_F(TransientFieldBalance, verifyTransientDataTransferOnFourProcessors)
{
  if (get_parallel_size() != 4) return;

  const std::vector<double> timeSteps = {0.0, 1.0, 2.0, 3.0, 4.0};
  const std::string fieldName = "myField";

  writer.set_time_steps(timeSteps);
  writer.set_field_name(fieldName);
  writer.write_transient_mesh("1x4x4");

  stk::unit_test_util::simple_fields::MeshFromFile& initialMesh = get_initial_mesh();
  verifier.verify_time_steps(initialMesh, timeSteps);
  verifier.verify_num_transient_fields(initialMesh, 2u);
  verifier.verify_transient_field_names(initialMesh, fieldName);

  balanceRunner.run_end_to_end();

  stk::unit_test_util::simple_fields::MeshFromFile& balancedMesh = get_balanced_mesh();
  verifier.verify_time_steps(balancedMesh, timeSteps);
  verifier.verify_num_transient_fields(balancedMesh, 2u);
  verifier.verify_transient_field_names(balancedMesh, fieldName);

  verifier.compare_entity_rank_names(initialMesh, balancedMesh);

  verifier.verify_transient_fields(balancedMesh);

  cleanup_files();
}

TEST_F(TransientFieldBalance, verifyTransientDataTransferWithSidesets)
{
  if (get_parallel_size() != 2) return;

  const stk::mesh::EntityId expectedId = 1;
  const stk::mesh::ConnectivityOrdinal expectedOrdinal = 5;

  const int initialSidesetProc = 1;
  const int balancedSidesetProc = 1;

  const stk::mesh::EntityIdProcVec expectedInitialDecomp = { {1u,1}, {2u,0} };
  const stk::mesh::EntityIdProcVec expectedBalancedDecomp = { {1u,1}, {2u,0} };

  writer.write_two_element_mesh_with_sideset(stk::unit_test_util::INCREASING);

  stk::unit_test_util::simple_fields::MeshFromFile& initialMesh = get_initial_mesh();
  verifier.verify_sideset_orientation(initialMesh, initialSidesetProc, expectedId, expectedOrdinal);
  verifier.verify_decomp(initialMesh, expectedInitialDecomp);

  balanceRunner.set_decomp_method("rib");
  balanceRunner.run_end_to_end();

  stk::unit_test_util::simple_fields::MeshFromFile& balancedMesh = get_balanced_mesh();
  verifier.verify_sideset_orientation(balancedMesh, balancedSidesetProc, expectedId, expectedOrdinal);
  verifier.verify_decomp(balancedMesh, expectedBalancedDecomp);

  cleanup_files();
}

TEST_F(TransientFieldBalance, verifyTransientDataTransferWithSidesetsOnMovedElements)
{
  if (get_parallel_size() != 2) return;

  const stk::mesh::EntityId expectedId = 2;
  const stk::mesh::ConnectivityOrdinal expectedOrdinal = 5;

  const int initialSidesetProc = 1;
  const int balancedSidesetProc = 0;

  const stk::mesh::EntityIdProcVec expectedInitialDecomp = { {1u,0}, {2u,1} };
  const stk::mesh::EntityIdProcVec expectedBalancedDecomp = { {1u,1}, {2u,0} };

  writer.write_two_element_mesh_with_sideset(stk::unit_test_util::DECREASING);

  stk::unit_test_util::simple_fields::MeshFromFile& initialMesh = get_initial_mesh();
  verifier.verify_sideset_orientation(initialMesh, initialSidesetProc, expectedId, expectedOrdinal);
  verifier.verify_decomp(initialMesh, expectedInitialDecomp);

  balanceRunner.run_end_to_end();

  stk::unit_test_util::simple_fields::MeshFromFile& balancedMesh = get_balanced_mesh();
  verifier.verify_sideset_orientation(balancedMesh, balancedSidesetProc, expectedId, expectedOrdinal);
  verifier.verify_decomp(balancedMesh, expectedBalancedDecomp);

  cleanup_files();
}
