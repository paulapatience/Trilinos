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
// 
#include <gtest/gtest.h>                // for AssertHelper, EXPECT_EQ, etc
#include <stk_mesh/base/CommListUpdater.hpp>  // for CommListUpdater
#include <stk_mesh/base/EntityCommDatabase.hpp>  // for EntityCommDatabase
#include <stk_mesh/base/EntityCommListInfo.hpp>
#include <stk_topology/topology.hpp>    // for topology, etc
#include "stk_mesh/base/Entity.hpp"     // for Entity
#include "stk_mesh/base/EntityKey.hpp"  // for EntityKey
#include "stk_mesh/base/Types.hpp"      // for EntityCommInfo
#include "stk_util/util/NamedPair.hpp"

TEST(EntityCommDatabase, testCommMapChangeListener)
{
  stk::mesh::EntityCommDatabase commDB;
  stk::mesh::EntityCommListInfoVector comm_list;
  std::vector<stk::mesh::EntityComm*> entityComms(200);
  stk::mesh::CommListUpdater comm_list_updater(comm_list, entityComms);
  commDB.setCommMapChangeListener(&comm_list_updater);

  int owner = 0;
  stk::mesh::EntityKey key(stk::topology::NODE_RANK, 99);
  unsigned ghost_id = 3;
  int proc = 4;
  stk::mesh::EntityCommInfo value(ghost_id, proc);
  commDB.insert(key, value, owner);

  //CommListUpdater only manages removing entries from comm-list,
  //so we must add an entry manually to set up the test.
  stk::mesh::EntityCommListInfo comm_list_info =
  {key, stk::mesh::Entity(), commDB.entity_comm(key)};
  comm_list.push_back(comm_list_info);

  EXPECT_EQ(1u, comm_list.size());

  //now clear an entry from commDB and expect that it also
  //set the entity_comm ptr in comm_list to NULL.
  commDB.comm_clear(key);

  EXPECT_EQ(1u, comm_list.size());
  comm_list_info = comm_list[0];
  EXPECT_EQ(nullptr, comm_list_info.entity_comm);
}

TEST(EntityCommDatabase, insertAndErase)
{
  stk::mesh::EntityCommDatabase commDB;
  std::vector<stk::mesh::EntityComm*> entityComms(2, nullptr);
  std::vector<stk::mesh::EntityKey> entityKeys =
  {stk::mesh::EntityKey(stk::topology::NODE_RANK, 1),
   stk::mesh::EntityKey(stk::topology::NODE_RANK, 2)};
  int owner = 0;

  const unsigned SHARED = 0;
  const unsigned GHOSTED = 1;
  const int otherProc = 1;

  for (unsigned i=0; i<entityKeys.size(); ++i) {
    std::pair<stk::mesh::EntityComm*, bool> result =
        commDB.insert(entityKeys[i], stk::mesh::EntityCommInfo(SHARED, otherProc), owner);
    EXPECT_TRUE(result.second);
    EXPECT_FALSE(result.first == nullptr);
    entityComms[i] = result.first;
    EXPECT_TRUE(entityComms[i]->isShared);
    EXPECT_FALSE(entityComms[i]->isGhost);
  }

  for (unsigned i=0; i<entityKeys.size(); ++i) {
    std::pair<stk::mesh::EntityComm*, bool> result =
        commDB.insert(entityKeys[i], stk::mesh::EntityCommInfo(GHOSTED, otherProc), owner);
    EXPECT_TRUE(result.second);
    EXPECT_TRUE(entityComms[i]->isShared);
    EXPECT_TRUE(entityComms[i]->isGhost);
  }

  commDB.erase(entityKeys[0], stk::mesh::EntityCommInfo(SHARED, otherProc));
  EXPECT_FALSE(entityComms[0]->isShared);
  EXPECT_TRUE(entityComms[0]->isGhost);

  commDB.erase(entityKeys[1], stk::mesh::EntityCommInfo(GHOSTED, otherProc));
  EXPECT_TRUE(entityComms[1]->isShared);
  EXPECT_FALSE(entityComms[1]->isGhost);

  commDB.erase(entityKeys[0], stk::mesh::EntityCommInfo(GHOSTED, otherProc));
  EXPECT_TRUE(commDB.entity_comm(entityKeys[0]) == nullptr);

  commDB.erase(entityKeys[1], stk::mesh::EntityCommInfo(SHARED, otherProc));
  EXPECT_TRUE(commDB.entity_comm(entityKeys[1]) == nullptr);
}

