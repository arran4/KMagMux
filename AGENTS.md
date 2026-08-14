## Building and Testing

Docker must not be used for the development/test environment. Instead, this repository uses a shared public KDE/Qt Debian rootfs provided by `kde-dev-rootfs`.

To set up the environment, run the bootstrap script:
```bash
./.jules/bootstrap.sh
```
This script downloads and provisions the shared KDE development rootfs.

To run build and test commands, prefix them with `./.jules/run.sh` to execute them within the chroot environment.

Here are the standard commands for building and testing the repository:

Configure with CMake:
```bash
./.jules/run.sh cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
```
*(Note: Use Ninja if installed in the rootfs, or omit `-G Ninja` for standard make).*

Build:
```bash
./.jules/run.sh cmake --build build -j$(nproc)
```

Run tests:
```bash
./.jules/run.sh ctest --test-dir build --output-on-failure
```

## Null Safety Policy
- When a `deleteLater()` call is made on an object, wrap it with a null pointer check, then execute the deletion, then set the pointer to `nullptr`. (The "free then null" pattern).
- Exception: If the pointer being deleted is captured by value in a lambda (e.g., `[this, watcher]`), it is read-only and assigning `nullptr` to it will cause a compilation error. In this specific case, only call `deleteLater()`. Do not set to `nullptr`.
