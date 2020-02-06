# To-do list for Codepad
This list consists of some unfinished tasks as well as known issues. Note that this list is not complete, and many tasks not listed here are marked in the code using `// TODO` or `/// \todo`.

- Linux
	- The X11 branch (under `os/linux/x11`) has not been maintained for a long time.
	- The GTK branch is currently very unstable.
- Windows
	- The `open_file_dialog()` function based on `IOpenFileDialog` hangs the program.
- Editors
	- Actual editing in the hex editor.
	- Underlying data structures should be switched to red-black trees instead of normal binary trees to guarantee worst-case performance.
	- Support for more encodings.
	- IME support is currently only partial.
	- Optimize the editor for long lines and large files.
- The plugin interface is currently extremely hard to use.
