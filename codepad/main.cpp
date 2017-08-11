#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "utilities/tasks.h"
#include "os/current.h"
#include "ui/font.h"
#include "ui/draw.h"
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

	code::text_context ctx;
	font fnt("segoeui.ttf", 14), codefnt("UbuntuMono-R.ttf", 16), f1("pala.ttf", 14), f2("times.ttf", 14);
	pen p(colord(0.9, 0.9, 0.9, 1.0));
	texture_brush texb(colord(0.0, 0.6, 1.0, 0.2));
	texture_brush viewb(colord(0.5, 0.5, 1.0, 0.2));
	element_hotkey_group hg;

	hg.register_hotkey({key_gesture(input::key::z, modifier_keys::control)}, [](element *e) {
		code::editor *editor = dynamic_cast<code::editor*>(e);
		if (editor) {
			editor->try_undo();
		}
	});
	hg.register_hotkey({key_gesture(input::key::y, modifier_keys::control)}, [](element *e) {
		code::editor *editor = dynamic_cast<code::editor*>(e);
		if (editor) {
			editor->try_redo();
		}
	});

	content_host::set_default_font(&fnt);
	code::editor::set_font(font_family(codefnt, codefnt, codefnt, codefnt));
	code::editor::set_caret_pen(&p);
	code::editor::set_selection_brush(&texb);
	code::minimap::set_viewport_brush(&viewb);

	ctx.load_from_file(U"hugetext.cpp");
	ctx.auto_set_default_line_ending();

	tab *codetab = dock_manager::get().new_tab();
	codetab->set_caption(U"code");
	code::codebox *cp = element::create<code::codebox>();
	code::editor *cpe = cp->get_editor();
	code::line_number *ln = element::create<code::line_number>();
	code::minimap *mmp = element::create<code::minimap>();
	cpe->set_default_hotkey_group(&hg);
	cp->add_component_left(*ln);
	cp->add_component_right(*mmp);

	cpe->set_context(&ctx);
	codetab->children().add(*cp);

	auto tok = async_task_pool::get().run_task([&ctx, cpe](async_task_pool::async_task &tk) {
		code::text_theme_data data;
		for (size_t i = 0; i < 1340000; i += 10) {
			double n6 = std::rand() / (double)RAND_MAX, n7 = std::rand() / (double)RAND_MAX, n8 = std::rand() / (double)RAND_MAX;
			data.set_range(
				code::caret_position(i, 1),
				code::caret_position(i + 10, 0),
				code::text_theme_specification(font_style::normal, colord(n6, n7, n8, 1.0))
			);
		}
		callback_buffer::get().add([d = std::move(data), &ctx, cpe]() {
			ctx.set_text_theme(d);
			cpe->invalidate_visual();
			logger::get().log_stacktrace();
		});
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
		auto tstart = std::chrono::high_resolution_clock::now();
		manager::get().update();
		dock_manager::get().update();
		callback_buffer::get().flush();
		double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tstart).count();
		if (ms > 50.0) {
			logger::get().log_info(CP_HERE, "update took ", ms, "ms");
		}
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
