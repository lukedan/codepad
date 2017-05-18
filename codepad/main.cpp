#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "platform/current.h"
#include "utilities/font.h"
#include "ui/textrenderer.h"
#include "ui/commonelements.h"
#include "editor/codebox.h"
#include "editor/docking.h"

using namespace codepad;
using namespace codepad::platform;
using namespace codepad::ui;
using namespace codepad::editor;

int main() {
#if defined(_MSC_VER) && !defined(NDEBUG)
	enable_mem_checking();
#endif

	renderer_base::create_default<opengl_renderer>();

	file_context ctx(U"platform/windows.h");
	font fnt("times.ttf", 18);

	content_host::set_default_font(&fnt);

	tab *codetab = dock_manager::get().new_tab(dock_manager::get().get_focused_tab_host());
	codetab->set_caption(U"code");
	codebox *cp = element::create<codebox>();
	cp->context = &ctx;
	cp->font.normal = cp->font.bold = cp->font.italic = cp->font.bold_italic = &fnt;
	codetab->children().add(*cp);
	manager::get().set_focus(codetab);

	for (size_t i = 0; i < 10; ++i) {
		tab *lbltab = dock_manager::get().new_tab(dock_manager::get().get_focused_tab_host());
		lbltab->set_caption(U"label" + to_str(i));
		label *lbl = element::create<label>();
		lbl->set_margin(thickness(1.0, 1.0, 1.0, 1.0));
		lbl->set_anchor(anchor::none);
		lbl->set_overriden_cursor(cursor::denied);
		lbl->content().set_color(colord(1.0, 0.0, 0.0, 1.0));
		str_t s;
		for (size_t j = 0; j <= i; ++j) {
			s += U"fuck this\n";
		}
		lbl->content().set_text(s);
		lbl->content().set_text_offset(vec2d(0.5, 0.5));
		lbltab->children().add(*lbl);
	}

	while (!dock_manager::get().empty()) {
		manager::get().update();
		dock_manager::get().update();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	manager::get().dispose_marked_elements();

	return 0;
}
