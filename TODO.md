# To-do list for Codepad
This list consists of some unfinished tasks as well as known issues. Note that this list is not complete, and many tasks not listed here are marked in the code using `// TODO` or `/// \todo`.

- Linux
	- The X11 branch (under `os/linux/x11`) has not been maintained for a long time and has outdated interfaces. It also lacks many functionalities.
	- The GTK branch is currently very unstable.
		- `opengl_renderer` doesn't work under Linux.
		- Sometimes `software_renderer` fails silently, rendering nothing throughout the execution of the program.
		- Assertion failure in `fontconfig` when closing the program after any editor has been opened.
- Windows
	- The `open_file_dialog()` function based on `IOpenFileDialog` hangs the program.
- Core
	- The main loop is currently implemented using a busy loop, which may cause high CPU usage.
- Editors
	- Actual editing in the hex editor.
	- Underlying data structures should be switched to red-black trees instead of normal binary trees to guarantee worst-case performance.
	- Support for more encodings.
	- Ligature support.
	- IME support is currently only partial.
	- Optimize the editor for long lines and large files.
- A 'settings' system is currently absent.
- A plugin system is currently absent. The system needs to allow plugins to create classes derived from built-in classes of Codepad. Preferably it should also expose as many as possible functions to plugins as-is. See also <https://github.com/lukedan/apigen>.
