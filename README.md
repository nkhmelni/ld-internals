# ld-internals

Reverse-engineered internal struct layouts for Apple's linkers. Assumes usage in a linker hook, with the intent to inspect or modify the linker's metadata, or just whatever research intentions.

## Validated versions

Tested and confirmed against these binaries. Other versions likely work as-is, since core layouts are stable across ld-prime releases.

- **ld-prime**: ld-1115.7.3 (Xcode 16.1), ld-1167.5 (Xcode 16.3), ld-1221.4 (Xcode 26.0), ld-1230.1 (Xcode 26.2), ld-1266.8 (Xcode 26.4)
- **ld64**: ld64-820.1 (Xcode 14.2)

## License

MIT
