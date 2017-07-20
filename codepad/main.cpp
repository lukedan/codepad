#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "utilities/tasks.h"
#include "os/current.h"
#include "ui/font.h"
#include "ui/textrenderer.h"
#include "ui/commonelements.h"
#include "editors/docking.h"
#include "editors/code/codebox.h"
#include "editors/code/components.h"
#include "editors/code/editor.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editor;

int main() {
#if defined(_MSC_VER) && defined(CP_DETECT_USAGE_ERRORS)
	enable_mem_checking();
#endif

	renderer_base::create_default<opengl_renderer>();

	text_context ctx;
	font fnt("segoeui.ttf", 14), codefnt("UbuntuMono-R.ttf", 16), f1("pala.ttf", 14), f2("times.ttf", 14);
	pen p(colord(0.9, 0.9, 0.9, 1.0));
	texture_brush texb(colord(0.0, 0.6, 1.0, 0.2));
	element_hotkey_group hg;

	hg.register_hotkey({key_gesture(input::key::z, modifier_keys::control)}, [](element *e) {
		codebox_editor *editor = dynamic_cast<codebox_editor*>(e);
		if (editor) {
			editor->try_undo();
		}
	});
	hg.register_hotkey({key_gesture(input::key::y, modifier_keys::control)}, [](element *e) {
		codebox_editor *editor = dynamic_cast<codebox_editor*>(e);
		if (editor) {
			editor->try_redo();
		}
	});

	ctx.load_from_file(U"hugetext.txt");
	content_host::set_default_font(&fnt);
	codebox_editor::set_font(font_family(codefnt, codefnt, codefnt, codefnt));
	codebox_editor::set_caret_pen(&p);
	codebox_editor::set_selection_brush(&texb);

	tab *codetab = dock_manager::get().new_tab();
	codetab->set_caption(U"code");
	codebox *cp = element::create<codebox>();
	codebox_editor *cpe = cp->get_editor();
	codebox_line_number *ln = element::create<codebox_line_number>();
	codebox_minimap *mmp = element::create<codebox_minimap>();
	cpe->set_default_hotkey_group(&hg);
	cp->add_component_left(*ln);
	cp->add_component_right(*mmp);

	cpe->set_context(&ctx);
	cpe->auto_set_line_ending();
	codetab->children().add(*cp);

	auto tok = async_task_pool::get().run_task([&ctx, cpe](async_task_pool::async_task &tk) {
		while (!tk.is_cancel_requested()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			text_theme_data data;
			if (tk.acquire_data(data, std::bind(&text_context::get_text_theme, &ctx))) {
				size_t n5 = std::rand() % 4;
				double n6 = std::rand() / (double)RAND_MAX, n7 = std::rand() / (double)RAND_MAX, n8 = std::rand() / (double)RAND_MAX;
				size_t l1 = rand() % 20;
				data.set_range(
					caret_position(l1, rand() % 10),
					caret_position(l1 + rand() % 20 + 1, rand() % 10),
					text_theme_specification(static_cast<font_style>(n5), colord(n6, n7, n8, 1.0))
				);
				callback_buffer::get().add([d = std::move(data), &ctx, cpe]() {
					ctx.set_text_theme(d);
					cpe->invalidate_visual();
				});
			}
		}
	});

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
		callback_buffer::get().flush();
#ifndef NDEBUG
		double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tstart).count();
		if (ms > 50.0) {
			CP_INFO("update took ", ms, "ms");
		}
#endif
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	manager::get().dispose_marked_elements();
	for (auto i = async_task_pool::get().tasks().begin(); i != async_task_pool::get().tasks().end(); ++i) {
		async_task_pool::get().try_cancel(i);
	}
	while (async_task_pool::get().tasks().size() > 0) {
		async_task_pool::get().wait_finish(async_task_pool::get().tasks().begin());
	}

	return 0;
}
