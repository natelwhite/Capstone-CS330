# all .cpp files
set(
  sources
  main.cpp
  SceneManager.cpp
  BufferManager.cpp
  Camera.cpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${sources})
