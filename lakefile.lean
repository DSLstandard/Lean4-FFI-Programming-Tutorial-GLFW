import Lake
open Lake DSL

package «GlfwTest» where
  -- add package configuration options here

@[default_target]
lean_exe «glfw_test_exe» where
  root := `Main
  moreLinkArgs := #["-lglfw", "-Wl,--allow-shlib-undefined"]

target native.o (pkg : NPackage _package.name) : FilePath := do
  let native_src := "native.c"
  let native_c := pkg.dir / native_src
  let native_o := pkg.buildDir / "native.o"
  buildFileAfterDep native_o (<- inputFile native_c) fun native_src => do
    let lean_dir := (<- getLeanIncludeDir).toString
    compileO "ausr" native_o native_src #["-I", lean_dir, "-fPIC"]

extern_lib native (pkg : NPackage _package.name) := do
  let name := nameToStaticLib "native"
  let native_o <- fetch <| pkg.target ``native.o
  buildStaticLib (pkg.buildDir / "lib" / name) #[native_o]
