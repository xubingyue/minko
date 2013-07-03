-- A project defines one build target
project "minko-framework"
  kind "StaticLib"
  language "C++"
  files { "**.hpp", "**.h", "**.cpp", "**.c" }
  includedirs {
    "src",
    "lib/jsoncpp/src"
  }
  defines {
    "JSON_IS_AMALGAMATION"
  }
  
  configuration { "debug"}
    defines { "DEBUG" }
    flags { "Symbols" }
    targetdir "bin/debug"

  configuration { "release" }
    defines { "NDEBUG" }
    flags { "OptimizeSpeed" }
    targetdir "bin/release"

  -- linux
  configuration { "linux" }
    links { "GL", "GLU" }
    buildoptions "-std=c++0x"
    linkoptions "-std=c++0x"

  -- windows
  configuration { "windows", "x32" }
    includedirs { "lib/glew/include" }

  -- macos
  configuration { "macosx" }
    buildoptions { "-std=c++11", "-stdlib=libc++" }
    linkoptions { "-std=c++11", "-stdlib=libc++" }
    libdirs { "/opt/local/lib/" }
    includedirs { "/opt/local/include/" }
