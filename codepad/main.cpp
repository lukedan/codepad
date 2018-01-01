#include <thread>
#include <fstream>

#include "utilities/globals.h"
#include "utilities/event.h"
#include "utilities/tasks.h"
#include "utilities/bst.h"
#include "utilities/performance_view.h"
#include "os/current.h"
#include "ui/font_family.h"
#include "ui/draw.h"
#include "ui/commonelements.h"
#include "editors/docking.h"
#include "editors/code/codebox.h"
#include "editors/code/components.h"
#include "editors/code/editor.h"
#include "editors/code/buffer.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editor;

int main() {
#ifdef CP_CAN_DETECT_MEMORY_LEAKS
	enable_mem_checking();
#endif

	globals gb;
	// manually trigger global allocation of certain types
	gb.get<performance_monitor>();

	renderer_base::create_default<opengl_renderer>();

	auto fnt = std::make_shared<default_font>(CP_STRLIT(""), 13.0, font_style::normal);
	font_family codefnt(CP_STRLIT("Fira Code"), 13.0);
	//font_family codefnt(CP_STRLIT("Segoe UI"), 13.0);
	element_hotkey_group hg;

	hg.register_hotkey({input::key::left}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_left(false);
		}
		});
	hg.register_hotkey({input::key::right}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_right(false);
		}
		});
	hg.register_hotkey({input::key::up}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_up(false);
		}
		});
	hg.register_hotkey({input::key::down}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_down(false);
		}
		});
	hg.register_hotkey({input::key::home}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_to_line_beginning_advanced(false);
		}
		});
	hg.register_hotkey({input::key::end}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_to_line_ending(false);
		}
		});

	hg.register_hotkey({key_gesture(input::key::left, modifier_keys::shift)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_left(true);
		}
		});
	hg.register_hotkey({key_gesture(input::key::right, modifier_keys::shift)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_right(true);
		}
		});
	hg.register_hotkey({key_gesture(input::key::up, modifier_keys::shift)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_up(true);
		}
		});
	hg.register_hotkey({key_gesture(input::key::down, modifier_keys::shift)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_down(true);
		}
		});
	hg.register_hotkey({key_gesture(input::key::home, modifier_keys::shift)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_to_line_beginning_advanced(true);
		}
		});
	hg.register_hotkey({key_gesture(input::key::end, modifier_keys::shift)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->move_all_carets_to_line_ending(true);
		}
		});

	hg.register_hotkey({input::key::backspace}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->delete_selection_or_char_before();
		}
		});
	hg.register_hotkey({input::key::del}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->delete_selection_or_char_after();
		}
		});

	hg.register_hotkey({input::key::insert}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->toggle_insert_mode();
		}
		});

	hg.register_hotkey({key_gesture(input::key::z, modifier_keys::control)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->try_undo();
		}
		});
	hg.register_hotkey({key_gesture(input::key::y, modifier_keys::control)}, [](element *e) {
		auto *editor = dynamic_cast<code::codebox*>(e);
		if (editor) {
			editor->get_editor()->try_redo();
		}
		});
	hg.register_hotkey({key_gesture(input::key::f, modifier_keys::control)}, [](element *e) {
		auto *cb = dynamic_cast<code::codebox*>(e);
		if (cb) {
			code::editor *editor = cb->get_editor();
			auto rgn = editor->get_carets().carets.rbegin()->first;
			code::view_formatting::fold_region fr = std::minmax(rgn.first, rgn.second);
			if (fr.first != fr.second) {
				logger::get().log_info(CP_HERE, "folding region: (", fr.first, ", ", fr.second, ")");
				editor->add_folded_region(fr);
			}
		}
		});
	hg.register_hotkey({key_gesture(input::key::u, modifier_keys::control)}, [](element *e) {
		auto *cb = dynamic_cast<code::codebox*>(e);
		if (cb) {
			code::editor *editor = cb->get_editor();
			while (editor->get_formatting().get_folding().folded_region_count() > 0) {
				editor->remove_folded_region(editor->get_formatting().get_folding().begin());
			}
		}
		});

	hg.register_hotkey({key_gesture(input::key::s, modifier_keys::control)}, [](element *e) {
		auto *cb = dynamic_cast<code::codebox*>(e);
		if (cb) {
			cb->get_editor()->get_context()->save_to_file(CP_STRLIT("testsave.txt"));
		}
		});

	content_host::set_default_font(fnt);
	code::editor::set_font(codefnt);
	code::editor::set_selection_brush(std::make_shared<texture_brush>(colord(0.0, 0.6, 1.0, 0.2)));

	tab *codetab1 = dock_manager::get().new_tab();
	codetab1->set_caption(CP_STRLIT("code1"));
	code::codebox *cp1 = element::create<code::codebox>();
	cp1->set_default_hotkey_group(&hg);
	cp1->add_component_left(*element::create<code::line_number>());
	cp1->add_component_right(*element::create<code::minimap>());

	tab *codetab2 = dock_manager::get().new_tab();
	codetab2->set_caption(CP_STRLIT("code2"));
	code::codebox *cp2 = element::create<code::codebox>();
	cp2->set_default_hotkey_group(&hg);
	cp2->add_component_left(*element::create<code::line_number>());
	cp2->add_component_right(*element::create<code::minimap>());

	auto ctx = std::make_shared<code::text_context>();
	ctx->load_from_file(CP_STRLIT("hugetext.txt"));
	//ctx->load_from_file(CP_STRLIT("mix.txt"));
	//ctx->load_from_file(CP_STRLIT("longline.txt"));
	//ctx->load_from_file(CP_STRLIT("editors/code/editor.h"));
	ctx->auto_set_default_line_ending();
	cp1->get_editor()->set_context(ctx);
	cp2->get_editor()->set_context(ctx);
	//while (true) {
	//	cp1->get_editor()->test_full_layout();
	//}

	std::function<void(async_task_pool::async_task&)> f;
	f = [&f, ctx](async_task_pool::async_task &tsk) { // just for demonstration purposes
		code::linebreak_registry lbs;
		code::string_buffer::string_type str = tsk.acquire_data([ctx, &lbs]() {
			lbs = ctx->get_linebreak_registry(); // bottleneck
			return ctx->substring(0, ctx->get_string_buffer().num_codepoints());
			});
		code::text_theme_data data;
		std::set<std::u32string> kwds{
			U"if", U"for", U"while", U"else", U"return", U"auto", U"decltype", U"typedef", U"using", U"do",
			U"class", U"struct", U"namespace", U"union", U"public", U"private", U"static", U"this",
			U"template", U"typename", U"inline", U"static_assert", U"true", U"false",
			U"ifdef", U"ifndef", U"define", U"undef", U"include", U"endif", U"pragma",
			U"int", U"double", U"float", U"void", U"char", U"unsigned", U"long", U"bool",
			U"static_cast", U"dynamic_cast", U"reinterpret_cast", U"const", U"constexpr",
			U"new", U"delete", U"friend"
		};
		codepoint_iterator_base<decltype(str)::const_iterator> it(str.begin(), str.end());
		size_t prebeg = 0;
		std::u32string curtok;
		std::locale loc;
		for (; !it.at_end(); it.next()) {
			char32_t c = it.current_codepoint();
			if (std::isalpha(c, loc) || c == U'_') {
				curtok.push_back(c);
			} else {
				if (curtok.size() > 0) {
					if (kwds.find(curtok) != kwds.end()) {
						data.set_range(
							lbs.position_codepoint_to_char(prebeg + 1),
							lbs.position_codepoint_to_char(it.codepoint_position()),
							code::text_theme_specification(font_style::italic, colord(0.6, 0.6, 1.0, 1.0))
						);
					}
					curtok.clear();
				}
				prebeg = it.codepoint_position();
			}
		}
		callback_buffer::get().add([&f, d{std::move(data)}, ctx]() mutable {
			ctx->set_text_theme(std::move(d));
			//async_task_pool::get().run_task(f);
		});
	};

	async_task_pool::get().run_task(f);

	codetab1->children().add(*cp1);
	codetab2->children().add(*cp2);


	for (size_t i = 0; i < 10; ++i) {
		tab *lbltab = dock_manager::get().new_tab();
		lbltab->set_caption(to_str(i));
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

	{
		// load skin
		std::ifstream fin("skin/skin.json", std::ios::binary);
		fin.seekg(0, std::ios::end);
		std::streampos sz = fin.tellg();
		char *c = static_cast<char*>(std::malloc(static_cast<size_t>(sz)));
		fin.seekg(0);
		fin.read(c, static_cast<std::streamsize>(sz));
		u8str_t us(reinterpret_cast<char8_t*>(c), static_cast<size_t>(sz));
		std::free(c);
		json::parser_value_t v;
		str_t ss = convert_to_current_encoding(us);
		v.Parse(ss.c_str());
		visual_manager::load_config(v).load_all(CP_STRLIT("skin/"));

		// load folding gizmo
		editor::code::editor::gizmo gz;
		gz.texture = std::make_shared<os::texture>(os::load_image(CP_STRLIT("folded.png")));
		gz.width = static_cast<double>(gz.texture->get_width());
		editor::code::editor::set_folding_gizmo(std::move(gz));
	}

	while (!dock_manager::get().empty()) {
		auto tstart = std::chrono::high_resolution_clock::now();
		{
			monitor_performance mon(CP_STRLIT("Frame"));
			manager::get().update();
			dock_manager::get().update();
			callback_buffer::get().flush();
			performance_monitor::get().update();
		}

		double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tstart).count();
		if (ms > 50.0) {
			logger::get().log_warning(CP_HERE, "update took ", ms, "ms");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	for (auto i = async_task_pool::get().tasks().begin(); i != async_task_pool::get().tasks().end(); ++i) {
		async_task_pool::get().try_cancel(i);
	}
	while (!async_task_pool::get().tasks().empty()) {
		async_task_pool::get().wait_finish(async_task_pool::get().tasks().begin());
	}

	return 0;
}
