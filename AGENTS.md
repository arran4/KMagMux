## Building and Testing with Docker

Since the standard Ubuntu LTS environments used in typical sandboxes do not include up-to-date KDE Frameworks 6 packages (`kf6-kxmlgui-dev`), you should build and test the project using the provided Docker container.

To build and run tests using Docker with Arch Linux and the latest Qt6/KF6 dependencies, execute the following script from the root of the repository:
```bash
./.jules/build-docker.sh
```

If you need to make changes to the `Dockerfile`, you can find it at `.jules/Dockerfile`.

## Null Safety Policy
- When a `deleteLater()` call is made on an object, wrap it with a null pointer check, then execute the deletion, then set the pointer to `nullptr`. (The "free then null" pattern).
- Exception: If the pointer being deleted is captured by value in a lambda (e.g., `[this, watcher]`), it is read-only and assigning `nullptr` to it will cause a compilation error. In this specific case, only call `deleteLater()`. Do not set to `nullptr`.
