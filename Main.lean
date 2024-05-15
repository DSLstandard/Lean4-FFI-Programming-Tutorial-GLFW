
@[extern "lean_glfwInit"]
opaque glfwInit : IO Bool

@[extern "lean_glfwTerminate"]
opaque glfwTerminate : IO Unit

opaque WindowP : NonemptyType
def Window := WindowP.type

@[extern "lean_glfwCreateWindow"]
opaque glfwCreateWindow (width : UInt32) (height : UInt32) (title : String) : IO Window

@[extern "lean_glfwWindowShouldClose"]
opaque glfwWindowShouldClose (win : Window) : IO Bool

@[extern "lean_glfwPollEvents"]
opaque glfwPollEvents : IO Unit

def main : IO Unit := do
  unless (<- glfwInit) do
    (<- IO.getStderr).putStrLn "Cannot initialize GLFW!!"
    return -- TODO: how to exit with EXIT_FAILURE?

  let win <- glfwCreateWindow 800 600 "My GLFW window's title"

  while (not (<- glfwWindowShouldClose win)) do
    glfwPollEvents

  glfwTerminate
  (<- IO.getStdout).putStrLn "Goodbye."
