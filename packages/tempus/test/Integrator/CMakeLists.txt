INCLUDE_DIRECTORIES(REQUIRED_DURING_INSTALLATION_TESTING
                    ${CMAKE_CURRENT_SOURCE_DIR})

TRIBITS_ADD_EXECUTABLE_AND_TEST(
  Integrator
  SOURCES Tempus_IntegratorTest.cpp ${TEMPUS_UNIT_TEST_MAIN}
  TESTONLYLIBS tempus_test_models
  NUM_MPI_PROCS 1
  )

TRIBITS_COPY_FILES_TO_BINARY_DIR(Test_Integrator_CopyFiles
  DEST_FILES Tempus_default.xml Tempus_IntegratorBasic_ref.xml Tempus_IntegratorBasic_ref2.xml
  EXEDEPS Integrator
  )
