# Some 32-bit targets need libatomic for the library's 64-bit counters.
function(ixwebsocket_check_atomic)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  # A static-library try_compile would hide unresolved atomic runtime symbols.
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
  set(atomic_test_source "
    #include <atomic>
    #include <cstdint>
    int main(int argc, char**) {
      std::atomic<std::uint64_t> counter(0);
      counter.fetch_add(static_cast<std::uint64_t>(argc));
      return static_cast<int>(counter.load());
    }
  ")
  check_cxx_source_compiles("${atomic_test_source}" IXWEBSOCKET_HAVE_BUILTIN_ATOMICS)
  if (NOT IXWEBSOCKET_HAVE_BUILTIN_ATOMICS)
    set(CMAKE_REQUIRED_LIBRARIES atomic)
    check_cxx_source_compiles("${atomic_test_source}" IXWEBSOCKET_HAVE_LIBATOMIC)
    if (NOT IXWEBSOCKET_HAVE_LIBATOMIC)
      message(FATAL_ERROR "Unable to link 64-bit C++ atomics, even with libatomic. Install libatomic for the target toolchain.")
    endif()
    target_link_libraries(ixwebsocket PRIVATE atomic)
    set(IXWEBSOCKET_PRIVATE_LIBS "-latomic" PARENT_SCOPE)
  endif()
  cmake_pop_check_state()
endfunction()

ixwebsocket_check_atomic()
