Pingu
=====

Pingu is a library to communicate with the simplebgc gimbal (which uses an AlexMos controller board) written in C. Testing is provided by cpputest, to run the tests simply run `make` from this directory.

You can also enable some additional functionality when testing, eg `make clean && make CUSTOM_FLAGS='-DMOCKBIRD -DDEBUG_LOGGING'` will enable testing the library against an Arduino running the `mockingbird.ino` sketch and debug logging to `stderr`.

`nonsense/` has non-core or throwaway code. `nonsense/hackpan` is a quick serial interface to set pitch/yaw angles on the gimbal (*unsupported*, btw). `nonsense/easypan` just yaws left and right as a PoC that I could control the gimbal.
