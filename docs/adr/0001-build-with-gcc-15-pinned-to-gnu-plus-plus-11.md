# Build with GCC 15, pinned to gnu++11

The upstream makefile targets arm-none-eabi GCC 4.9, which no longer ships for current macOS. We build with Arm GNU Toolchain 15.3 instead, through `./firmware.sh`, and pin `-std=gnu++11`. GCC 15 defaults to C++17, which rejects the `register` keyword in ST's HAL headers (196 errors). Patching the vendor headers would put every upstream merge at risk, so we pin the language standard instead. Don't remove the flag or "modernise" the standard without first replacing the HAL.

## Considered options

- **Install GCC 4.9:** unmaintained, and not offered for current macOS.
- **Patch the HAL headers to drop `register`:** works, but diverges from the upstream vendor code.
