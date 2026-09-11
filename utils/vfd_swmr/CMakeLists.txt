# HDF5_ENABLE_VFD_SWMR_UTILS is declared in config/ConfigureChecks.cmake
# (processed well before this subdirectory, and before test/ is processed
# too -- see the comment there for why it can't be declared here instead).
# It already controls whether H5_HAVE_AUX_PROCESS is defined, so
# aux_process.c's real implementation is enabled/disabled by that same
# flag; recovery_tool and crasher are plain POSIX C (fork/execvp/waitpid)
# with no such internal guard of their own, so their targets below are
# gated directly on this variable instead.
add_executable (aux_process aux_process.c)

target_include_directories (aux_process PRIVATE "${HDF5_UITLS_DIR};${HDF5_SRC_DIR};${HDF5_SRC_BINARY_DIR};$<$<BOOL:${HDF5_ENABLE_PARALLEL}>:${MPI_C_INCLUDE_DIRS}>")
if (NOT BUILD_SHARED_LIBS)
  TARGET_C_PROPERTIES (aux_process STATIC)
  target_link_libraries (aux_process PRIVATE ${HDF5_LIB_TARGET})
else ()
  TARGET_C_PROPERTIES (aux_process SHARED)
  target_link_libraries (aux_process PRIVATE ${HDF5_LIBSH_TARGET})
endif ()

set (H5_DEP_EXECUTABLES aux_process)

if (HDF5_ENABLE_VFD_SWMR_UTILS)
  add_executable (recovery_tool recovery_tool.c)
  target_include_directories (recovery_tool PRIVATE "${HDF5_UITLS_DIR};${HDF5_SRC_DIR};${HDF5_SRC_BINARY_DIR}")
  set_target_properties (recovery_tool PROPERTIES FOLDER utils)
  set (H5_DEP_EXECUTABLES ${H5_DEP_EXECUTABLES} recovery_tool)

  # crasher is a test-support tool (it deliberately SIGKILLs whatever
  # command it is given) invoked by the shell-driven crash-recovery tests,
  # not a user-facing utility -- build it, but do not install it.
  add_executable (crasher crasher.c)
  target_include_directories (crasher PRIVATE "${HDF5_UITLS_DIR};${HDF5_SRC_DIR};${HDF5_SRC_BINARY_DIR}")
  set_target_properties (crasher PROPERTIES FOLDER utils)
endif ()

##############################################################################
##############################################################################
###           I N S T A L L A T I O N                                      ###
##############################################################################
##############################################################################

#-----------------------------------------------------------------------------
# Rules for Installation of tools using make Install target
#-----------------------------------------------------------------------------
if (HDF5_EXPORTED_TARGETS)
  foreach (exec ${H5_DEP_EXECUTABLES})
    INSTALL_PROGRAM_PDB (${exec} ${HDF5_INSTALL_BIN_DIR} utilsapplications)
  endforeach ()

  install (
      TARGETS
          ${H5_DEP_EXECUTABLES}
      EXPORT
          ${HDF5_EXPORTED_TARGETS}
      RUNTIME DESTINATION ${HDF5_INSTALL_BIN_DIR} COMPONENT utilsapplications
  )
endif ()
