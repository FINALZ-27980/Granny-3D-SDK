cmake_minimum_required(VERSION 4.2.0)

# Project Name
project(granny2_static
DESCRIPTION "Granny2 Static Library for x86/x64"
VERSION "1.0.0")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "Target platform" FORCE)

# Configuration Types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported build configurations" FORCE)

# options
option(BUILDING_GRANNY "Build Granny Library For Dll Files" OFF)
option(BUILDING_GRANNY_STATIC "Build Granny Static Library" ON)
option(RAD_NO_LOWERCASE_TYPES "Disable Lowercase Type Definitions" ON)
option(_HAS_ITERATOR_DEBUGGING "Disable Iterator Debugging" OFF)

# initial VS integration
# `cs_setup_vs_defaults` is defined in an external CMake helper in some setups.
# Guard the call so CMake won't error if the helper is missing.
if(COMMAND cs_setup_vs_defaults)
    cs_setup_vs_defaults(STARTUP_PROJECT "${PROJECT_NAME}")
else()
    message(STATUS "cs_setup_vs_defaults not found; skipping VS integration helper")
endif()

# Ignore Packing Dismatch Warnings
add_definitions(-DWINDOWS_IGNORE_PACKING_MISMATCH)

# Add Source Files
add_library(${PROJECT_NAME} STATIC
    #.c source Files
    arithbit.c
    binktc.c
    encode.c

    #.inl source Files
    contain.inl
    granny_contain.inl
    granny_generic_deformer_wrapper.inl
    granny_generic_deformers.inl
    radarith_exact.inl
    sort.inl

    #.cpp source Files
    # rr
    rrBase.cpp
    rrMath.cpp
    
    # A-E
    granny_aggr_alloc.cpp
    granny_animation.cpp
    granny_animation_binding.cpp
    granny_art_tool_info.cpp
    granny_assert.cpp
    granny_back_compat.cpp
    granny_basis_conversion.cpp
    granny_binary_search.cpp
    granny_bink.cpp
    granny_bink0_compression.cpp
    granny_bink1_compression.cpp
    granny_bone_operations.cpp
    granny_bspline.cpp
    granny_bspline_buffers.cpp
    granny_bspline_solver.cpp
    granny_bspline_solver_core.cpp
    granny_camera.cpp
    granny_catenating_reader.cpp
    granny_compress_curve.cpp
    granny_compression_tools.cpp
    granny_constants.cpp
    granny_control.cpp
    granny_controlled_animation.cpp
    granny_controlled_pose.cpp
    granny_crc.cpp
    granny_curve.cpp
    granny_curve_fast.cpp
    granny_data_type_io.cpp
    granny_data_type_conversion.cpp
    granny_data_type_definition.cpp
    granny_deformers.cpp
    granny_degree_of_freedom.cpp
    granny_exporter_info.cpp

    # F-M
    granny_file.cpp
    granny_file_builder.cpp
    granny_file_compressor.cpp
    granny_file_format.cpp
    granny_file_info.cpp
    granny_file_operations.cpp
    granny_file_reader.cpp
    granny_file_writer.cpp
    granny_find_knot.cpp
    granny_fixed_allocator.cpp
    granny_float16.cpp
    granny_floats.cpp
    granny_generic_deformers.cpp
    granny_ik.cpp
    granny_image_operations.cpp
    granny_intersection.cpp
    granny_local_pose.cpp
    granny_log.cpp
    granny_lookat.cpp
    granny_material.cpp
    granny_math.cpp
    granny_matrix_operations.cpp
    granny_member_iterator.cpp
    granny_memory.cpp
    granny_memory_arena.cpp
    granny_memory_file_reader.cpp
    granny_memory_file_writer.cpp
    granny_memory_ops.cpp
    granny_mesh.cpp
    granny_mesh_binding.cpp
    granny_mesh_builder.cpp
    granny_mesh_deformer.cpp
    granny_mirror_specification.cpp
    granny_mod_support.cpp
    granny_model.cpp
    granny_model_control_binding.cpp
    granny_model_instance.cpp

    # N-Z
    granny_oodle0_compression.cpp
    granny_oodle1_compression.cpp
    granny_periodic_loop.cpp
    granny_pixel_layout.cpp
    granny_platform_convert.cpp
    granny_pointer_hash.cpp
    granny_pose_cache.cpp
    granny_quaternion_scaleoffset.cpp
    granny_retargeter.cpp
    granny_s3tc.cpp
    granny_sieve.cpp
    granny_skeleton.cpp
    granny_skeleton_builder.cpp

    # This Is A Windows Build So SPU Is Not Supported -- For PlayStation 3
    # granny_spu_animation.cpp
    # granny_spu_animation_binding.cpp
    # granny_spu_animation_info.cpp
    # granny_spu_commands_ppuside.cpp
    # granny_spu_controlled_animation.cpp
    # granny_spu_track_group.cpp

    granny_stack_allocator.cpp
    granny_string.cpp
    granny_string_comparison_callback.cpp
    granny_string_database.cpp
    granny_string_formatting.cpp
    granny_string_table.cpp
    granny_tangent_frame.cpp
    granny_telemetry.cpp
    granny_test_types.cpp
    granny_texture.cpp
    granny_texture_builder.cpp
    granny_thread_safety.cpp
    granny_track_group.cpp
    granny_track_group_builder.cpp
    granny_track_group_sampler.cpp
    granny_track_mask.cpp
    granny_track_sampler.cpp
    granny_transform.cpp
    granny_tri_topology.cpp
    granny_type_listing.cpp
    granny_units.cpp
    granny_variant_builder.cpp
    granny_version_checking.cpp
    granny_vertex_data.cpp
    granny_world_pose.cpp

    # Global Windows Specific Files
    winxx/winxx_granny_assert.cpp
    winxx/winxx_granny_dll.cpp
    winxx/winxx_granny_file_operations.cpp
    winxx/winxx_granny_file_reader.cpp
    winxx/winxx_granny_file_writer.cpp
    winxx/winxx_granny_memory.cpp
    winxx/winxx_granny_startup.cpp
    winxx/winxx_granny_system_clock.cpp
    winxx/winxx_granny_threads.cpp
    winxx/winxx_granny_windows.cpp
)

# ADD ADDITIONAL SOURCE FILES FROM WINXX FOLDER, And Add x86/x64 Check With CMake For Source Files
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_sources(${PROJECT_NAME} PRIVATE
        # x86 Specific Files
        # .inl source Files
        winxx/win32/x86_granny_accelerated_deformer_wrapper.inl
        winxx/win32/x86_granny_accelerated_deformers_custom.inl
        winxx/win32/x86_granny_accelerated_deformers_generated.inl
        winxx/win32/x86_granny_matrix_operations_ops.inl

        # .cpp source Files
        winxx/win32_rrAtomics.cpp
        winxx/win32/granny.cpp
        winxx/win32/x86_granny_accelerated_deformers.cpp
        winxx/win32/x86_granny_bone_operations.cpp
        winxx/win32/x86_granny_cpu_queries.cpp
        winxx/win32/x86_granny_matrix_operations.cpp
    )
else()
    target_sources(${PROJECT_NAME} PRIVATE
        # x64 platform Specific Files
        winxx/win64/ansi_granny_accelerated_deformers.cpp
        winxx/win64/ansi_granny_bone_operations.cpp
        winxx/win64/ansi_granny_matrix_operations.cpp
        winxx/win64/granny.cpp
        winxx/win64/win64_rrAtomics.cpp
    )
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_include_directories(${PROJECT_NAME} PUBLIC
        winxx/
        winxx/win32/
        ../source/
    )
else()
    target_include_directories(${PROJECT_NAME} PUBLIC
        ../source/
        winxx/win64/
        winxx/
    )
endif()

# 为目标添加预处理器定义：BUILDING_GRANNY_STATIC=1
target_compile_definitions(${PROJECT_NAME}
  PRIVATE  # 该定义仅对当前目标有效，不传递给依赖它的其他目标
  BUILDING_GRANNY_STATIC=1
  RAD_NO_LOWERCASE_TYPES=0
  _HAS_ITERATOR_DEBUGGING=0
)