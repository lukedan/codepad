#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "os/current.h"
#include "ui/font.h"
#include "ui/textrenderer.h"
#include "ui/commonelements.h"
#include "editor/codebox.h"
#include "editor/docking.h"
#include "editor/components.h"
#include "editor/editor_code.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editor;

int main() {
#if defined(_MSC_VER) && !defined(NDEBUG)
	enable_mem_checking();
#endif

	renderer_base::create_default<opengl_renderer>();

	editor_code_context ctx;
	font fnt("segoeui.ttf", 14), codefnt("UbuntuMono-R.ttf", 16);
	pen p(colord(0.9, 0.9, 0.9, 1.0));
	texture_brush texb(colord(0.0, 0.6, 1.0, 0.2));
	element_hotkey_group hg;

	hg.register_hotkey({ key_gesture(input::key::z, modifier_keys::control) }, [&](element *e) {
		codebox_editor_code *ctx = dynamic_cast<codebox_editor_code*>(e);
		if (ctx) {
			ctx->try_undo();
		}
	});
	hg.register_hotkey({ key_gesture(input::key::y, modifier_keys::control) }, [&](element *e) {
		codebox_editor_code *ctx = dynamic_cast<codebox_editor_code*>(e);
		if (ctx) {
			ctx->try_redo();
		}
	});

	ctx.load_from_file(U"editor/codebox.h");
	//ctx.load_from_file(U"main.cpp");
	content_host::set_default_font(&fnt);
	codebox_editor_code::set_font(font_family(codefnt, codefnt, codefnt, codefnt));
	codebox_editor_code::set_caret_pen(&p);
	codebox_editor_code::set_selection_brush(&texb);

	tab *codetab = dock_manager::get().new_tab();
	codetab->set_caption(U"code");
	codebox *cp = element::create<codebox>();
	codebox_editor_code *cpe = cp->create_editor<codebox_editor_code>();
	codebox_line_number *ln = element::create<codebox_line_number>();
	cpe->set_context(&ctx);
	cpe->auto_set_line_ending();
	cpe->set_default_hotkey_group(&hg);
	cp->add_component_left(*ln);
	codetab->children().add(*cp);

	for (size_t i = 0; i < 10; ++i) {
		tab *lbltab = dock_manager::get().new_tab();
		lbltab->set_caption(U"label" + to_str(i));
		scroll_bar *sb = element::create<scroll_bar>();
		if (i % 2 == 0) {
			sb->set_anchor(anchor::stretch_vertically);
			sb->set_orientation(orientation::vertical);
		} else {
			sb->set_anchor(anchor::stretch_horizontally);
			sb->set_orientation(orientation::horizontal);
		}
		sb->set_margin(thickness(1.0, 1.0, 1.0, 1.0));
		lbltab->children().add(*sb);
	}

	while (!dock_manager::get().empty()) {
#ifndef NDEBUG
		auto tstart = std::chrono::high_resolution_clock::now();
#endif
		manager::get().update();
		dock_manager::get().update();
#ifndef NDEBUG
		double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tstart).count();
		if (ms > 50.0) {
			CP_INFO("_on_keypress took ", ms, "ms");
		}
#endif
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	manager::get().dispose_marked_elements();

	return 0;
}
