#include <thread>
#include <fstream>

#include "utilities/globals.h"
#include "utilities/event.h"
#include "utilities/tasks.h"
#include "os/current.h"
#include "ui/font_family.h"
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
#ifdef CP_CAN_DETECT_MEMORY_LEAKS
	enable_mem_checking();
#endif

	globals gb;

	renderer_base::create_default<opengl_renderer>();

	auto fnt = std::make_shared<default_font>(U"", 12.0, font_style::normal);
	font_family codefnt(U"iosevka", 13.0);
	element_hotkey_group hg;

	{
		std::ifstream fin("skin.json", std::ios::binary);
		fin.seekg(0, std::ios::end);
		size_t sz = fin.tellg();
		char *c = static_cast<char*>(std::malloc(sz));
		fin.seekg(0);
		fin.read(c, sz);
		u8str_t us(reinterpret_cast<unsigned char*>(c), sz);
		std::free(c);
		json::parser_value_t v;
		str_t ss = convert_to_utf32(us);
		v.Parse(ss.c_str());
		visual_manager::load_config(v);
	}

	hg.register_hotkey({key_gesture(input::key::z, modifier_keys::control)}, [](element *e) {
		code::codebox *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->try_undo();
		}
	});
	hg.register_hotkey({key_gesture(input::key::y, modifier_keys::control)}, [](element *e) {
		code::codebox *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->try_redo();
		}
	});
	hg.register_hotkey({key_gesture(input::key::f, modifier_keys::control)}, [](element *e) {
		code::codebox *cb = dynamic_cast<code::codebox*>(e);
		if (cb) {
			code::editor *editor = cb->get_editor();
			auto rgn = editor->get_carets().carets.rbegin()->first;
			code::editor::fold_region fr = std::minmax(rgn.first, rgn.second);
			if (fr.first != fr.second) {
				logger::get().log_info(CP_HERE, "folding region: (", fr.first.column, ", ", fr.first.line, ") -> (", fr.second.column, ", ", fr.second.line, ")");
				std::vector<code::editor::fold_region> list = editor->add_fold_region(fr);
				for (auto i = list.begin(); i != list.end(); ++i) {
					logger::get().log_info(CP_HERE, "  overwrote region: (", i->first.column, ", ", i->first.line, ") -> (", i->second.column, ", ", i->second.line, ")");
				}
			}
		}
	});
	hg.register_hotkey({key_gesture(input::key::u, modifier_keys::control)}, [](element *e) {
		code::codebox *cb = dynamic_cast<code::codebox*>(e);
		if (cb) {
			code::editor *editor = cb->get_editor();
			while (editor->get_folding_info().size() > 0) {
				editor->remove_fold_region(editor->get_folding_info().begin());
			}
		}
	});

	content_host::set_default_font(fnt);
	code::editor::set_font(codefnt);
	code::editor::set_insert_caret_brush(std::make_shared<texture_brush>(colord(0.0, 0.6, 1.0, 0.2)));
	code::editor::set_selection_brush(std::make_shared<texture_brush>(colord(0.0, 0.6, 1.0, 0.2)));
	texture_brush viewb(colord(0.5, 0.5, 1.0, 0.2));
	code::minimap::set_viewport_brush(&viewb);

	tab *codetab1 = dock_manager::get().new_tab();
	codetab1->set_caption(U"code1");
	code::codebox *cp1 = element::create<code::codebox>();
	cp1->set_default_hotkey_group(&hg);
	cp1->add_component_left(*element::create<code::line_number>());
	cp1->add_component_right(*element::create<code::minimap>());

	tab *codetab2 = dock_manager::get().new_tab();
	codetab2->set_caption(U"code2");
	code::codebox *cp2 = element::create<code::codebox>();
	cp2->set_default_hotkey_group(&hg);
	cp2->add_component_left(*element::create<code::line_number>());
	cp2->add_component_right(*element::create<code::minimap>());

	{
		auto ctx = std::make_shared<code::text_context>();
		//ctx->load_from_file(U"hugetext.cpp");
		ctx->load_from_file(U"editors/code/context.h");
		ctx->auto_set_default_line_ending();
		cp1->get_editor()->set_context(ctx);
		cp2->get_editor()->set_context(ctx);
		async_task_pool::get().run_task([ctx](async_task_pool::async_task&) {
			code::text_theme_data data;
			for (size_t i = 0; i < 1340000; i += 10) {
				double n6 = std::rand() / (double)RAND_MAX, n7 = std::rand() / (double)RAND_MAX, n8 = std::rand() / (double)RAND_MAX;
				data.set_range(
					code::caret_position(i, 1),
					code::caret_position(i + 10, 0),
					code::text_theme_specification(font_style::normal, colord(n6, n7, n8, 1.0))
				);
			}
			callback_buffer::get().add([d = std::move(data), ctx]() {
				ctx->set_text_theme(d);
				logger::get().log_stacktrace();
			});
		});
	}

	codetab1->children().add(*cp1);
	codetab2->children().add(*cp2);


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
