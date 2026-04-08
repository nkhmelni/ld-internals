# ld-internals

Reverse-engineered internal struct layouts for Apple's linkers. Assumes usage in a linker hook, with the intent to inspect or modify the linker's metadata, or just whatever research intentions.

## Supported versions

- **ld-prime** (closed-source): ld-1115.7.3 (Xcode 16.1), ld-1230.1 (Xcode 26)
- **ld64** (classic): ld64-820.1 (Xcode 14.2)

## License

MIT