# The Architecture of Codepad

- ## The User Interface

	Codepad uses a custom user interface library based on vector graphics libraries. The architecture of the user interface is heavily influenced by WPF and HTML/CSS. The user interface is composed of hierarchies of elements, which in some other UI libraries are called controls or widgets.

	- ### Events

		The user interface responds to user input by listening to events similar to those in C#. One can register for an event using the `+=` operator, which returns a token that's used when unregistering for the event (unlike C# events). Mouse events are handled in a basic manner where each parent simply checks and forwards the event to any child that's been affected. Keyboard events are directly sent to the element that have keyboard focus at the moment. There are other events used for various purposes, and some non-UI components also use this system.

		All of the user interface logic runs on the main thread, which is essentially a loop that keeps checking for new events from the system. For background tasks, there is a function for executing function objects on the main thread from another thread.

	- ### Element Properties

		Each element has a set of properties that can be accessed and animated using the JSON configuration file. A specific part of a property can be accessed via a property path similar to that in WPF. These properties dictate the behavior of an element.

	- ### Composite Elements

		`panel` is the base class of all elements that have a collection of child elements. Elements that inherit from `panel` can expect child elements with certain names and types to be present in the hierarchy, and use them for specific interactions. For example, an input dialog may expect a `label` for a prompt, a `text_edit` for input, and a `button` for confirmation. In the arrangement configuration file the user can set their layout and visual properties as they wish or add extra elements such as a `scroll_viewport`.

- ## Buffers and Interpretations

	All binary and text editing components are implemented as a plugin in `plugins/editors`.

	For the storage of document data, codepad uses a modified red-black tree structure similar to a segment tree, where each node contains not only the data of the node itself, but also a summary of all data in the entire subtree whose root is that node. For example, if each node contains a sequence of bytes, then the summary data could be the total length of all sequences in the subtree. This makes it so that position queries can be done in O(nlogn) time.

	A design goal of codepad is to enable users to edit the raw binary representation of any file. Thus, the representation of a text file is multi-layered.

	- The first layer is the `buffer`: raw binary data of the file. This can be modified independently of other layers.
	- On top of this, there can be multiple different `interpretations` of a `buffer`. An interpretation is the binary buffer decoded using a particular encoding (e.g., UTF-8). For an interpretation, two data structures are stored:

		- A registry of byte positions of select codepoint boundaries, used to speed up decoding.
		- A registry containing information of all lines and linebreaks.

		When the buffer is modified, the modified content is decoded to update those codepoint boundaries and line information.

	- Finally, in each interpretation, there are also additional trees that store text theme information (color, font style, font weight, etc.), and decorations (used for e.g. marking text where a syntax error is detected).

	Using this organization, it's possible to simultaneously edit the file as binary and as text, and/or using multiple encodings.

- ## The editor

	The editor is implemented as a composite element that contains horizontal and vertical scrollbars, and a `contents_region_base`: a virtual class that contains methods for retrieving and setting current scrolling position, handling text input, toggling between insert and overwrite modes, etc. A templated class `interactive_contents_region_base` inherits from `contents_region_base` and contains the interface for querying and modifying carets, and hit testing. This allows mouse interaction code to be reused between the code and binary editors: a `contents_region` contains an `interaction_manager` to which all mouse events are forwarded. `interaction_mode_activator`s can be added to the manager to enable various mouse interactions. When a mouse interaction starts, the corresponding activator produces an `interaction_mode` that takes over all mouse input until it's released.

	The binary `contents_region` is trivial as it just renders all bytes using the specified radix. The code `contents_region` is more sophisticated as it needs to handle text theme, decorations, as well as per-view features such as code folding and word wrapping. Each `contents_region` contains registries for folded regions and soft line break positions. All these features are handled using a `fragment_generator`. The generator contains multiple components that predict the position of the next fragment that would be generated by this component. A `fragment_assembler` is used to accumulate position information and materialize the fragments, which can then be used for rendering and hit testing. Alongside the fragment generator and assembler are caret and decoration gatherers that gather the position information of carets and decorations so that they can be rendered later.

- ## Plugins

	TODO
