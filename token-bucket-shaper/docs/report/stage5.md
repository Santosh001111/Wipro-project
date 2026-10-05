# Stage 5 — Code quality
* Warning-clean with `-Wall -Wextra -Wpedantic -Werror` (Debug, Release, sanitizer builds).
* `[[nodiscard]]` on `consume()` and `try_push()`; `noexcept` on the Packet move operations; named `constexpr` constants instead of magic numbers; no raw `new/delete`; no TODO/FIXME left; `nfq_set_verdict()` result checked and logged; `ENOBUFS` handled.
* Tests use a `CHECK` macro instead of `assert`, so Release builds still test something.
* Still to run on your machine: `clang-format -i src/*.cpp include/*.hpp tests/*.cpp` and `clang-tidy` (not installed where this was prepared).
