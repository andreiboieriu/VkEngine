# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/unclegoga/Development/VkEngine

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/unclegoga/Development/VkEngine/build

# Utility rule file for Shaders.

# Include any custom commands dependencies for this target.
include CMakeFiles/Shaders.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/Shaders.dir/progress.make

CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh.vert.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox.vert.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/sprite.frag.spv
CMakeFiles/Shaders: /home/unclegoga/Development/VkEngine/shaders/sprite.vert.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_11) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_12) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp.spv

/home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp.spv: /home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_13) "Generating /home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp -o /home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp.spv

/home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp.spv: /home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_14) "Generating /home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp -o /home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp.spv

/home/unclegoga/Development/VkEngine/shaders/mesh.frag.spv: /home/unclegoga/Development/VkEngine/shaders/mesh.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_15) "Generating /home/unclegoga/Development/VkEngine/shaders/mesh.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/mesh.frag -o /home/unclegoga/Development/VkEngine/shaders/mesh.frag.spv

/home/unclegoga/Development/VkEngine/shaders/mesh.vert.spv: /home/unclegoga/Development/VkEngine/shaders/mesh.vert
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_16) "Generating /home/unclegoga/Development/VkEngine/shaders/mesh.vert.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/mesh.vert -o /home/unclegoga/Development/VkEngine/shaders/mesh.vert.spv

/home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag.spv: /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_17) "Generating /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag -o /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag.spv

/home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert.spv: /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_18) "Generating /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert -o /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert.spv

/home/unclegoga/Development/VkEngine/shaders/skybox.frag.spv: /home/unclegoga/Development/VkEngine/shaders/skybox.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_19) "Generating /home/unclegoga/Development/VkEngine/shaders/skybox.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/skybox.frag -o /home/unclegoga/Development/VkEngine/shaders/skybox.frag.spv

/home/unclegoga/Development/VkEngine/shaders/skybox.vert.spv: /home/unclegoga/Development/VkEngine/shaders/skybox.vert
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_20) "Generating /home/unclegoga/Development/VkEngine/shaders/skybox.vert.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/skybox.vert -o /home/unclegoga/Development/VkEngine/shaders/skybox.vert.spv

/home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag.spv: /home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_21) "Generating /home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag -o /home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag.spv

/home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag.spv: /home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_22) "Generating /home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag -o /home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag.spv

/home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag.spv: /home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_23) "Generating /home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag -o /home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag.spv

/home/unclegoga/Development/VkEngine/shaders/sprite.frag.spv: /home/unclegoga/Development/VkEngine/shaders/sprite.frag
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_24) "Generating /home/unclegoga/Development/VkEngine/shaders/sprite.frag.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/sprite.frag -o /home/unclegoga/Development/VkEngine/shaders/sprite.frag.spv

/home/unclegoga/Development/VkEngine/shaders/sprite.vert.spv: /home/unclegoga/Development/VkEngine/shaders/sprite.vert
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/unclegoga/Development/VkEngine/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_25) "Generating /home/unclegoga/Development/VkEngine/shaders/sprite.vert.spv"
	/usr/bin/glslangValidator -V /home/unclegoga/Development/VkEngine/shaders/sprite.vert -o /home/unclegoga/Development/VkEngine/shaders/sprite.vert.spv

Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass0.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass1.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass2.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass3.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass4.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass5.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass6.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass7.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass8.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/bloom/pass9.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass0.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/fxaa/pass1.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/assets/compute_effects/tone_mapping/pass0.comp.spv
Shaders: CMakeFiles/Shaders
Shaders: /home/unclegoga/Development/VkEngine/shaders/brdf_lut/pass0.comp.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh.vert.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/mesh_pbr.vert.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox.vert.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox_env_to_irradiance.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox_equi_to_cube.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/skybox_prefilter_env.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/sprite.frag.spv
Shaders: /home/unclegoga/Development/VkEngine/shaders/sprite.vert.spv
Shaders: CMakeFiles/Shaders.dir/build.make
.PHONY : Shaders

# Rule to build all files generated by this target.
CMakeFiles/Shaders.dir/build: Shaders
.PHONY : CMakeFiles/Shaders.dir/build

CMakeFiles/Shaders.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/Shaders.dir/cmake_clean.cmake
.PHONY : CMakeFiles/Shaders.dir/clean

CMakeFiles/Shaders.dir/depend:
	cd /home/unclegoga/Development/VkEngine/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/unclegoga/Development/VkEngine /home/unclegoga/Development/VkEngine /home/unclegoga/Development/VkEngine/build /home/unclegoga/Development/VkEngine/build /home/unclegoga/Development/VkEngine/build/CMakeFiles/Shaders.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/Shaders.dir/depend

