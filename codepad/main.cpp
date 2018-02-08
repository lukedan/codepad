#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "utilities/tasks.h"
#include "utilities/bst.h"
#include "utilities/performance_view.h"
#include "os/current.h"
#include "ui/font_family.h"
#include "ui/draw.h"
#include "ui/commonelements.h"
#include "ui/native_commands.h"
#include "editors/docking.h"
#include "editors/code/codebox.h"
#include "editors/code/components.h"
#include "editors/code/editor.h"
#include "editors/code/buffer.h"
#include "utilities/globals.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editor;

globals gb; // can be a pretty large object

int main() {
#ifdef CP_CAN_DETECT_MEMORY_LEAKS
	enable_mem_checking();
#endif

	// manually trigger global allocation of certain types
	gb.get<performance_monitor>();

	renderer_base::create_default<opengl_renderer>();
	native_commands::register_all();

	auto fnt = std::make_shared<default_font>(CP_STRLIT(""), 13.0, font_style::normal);
	font_family codefnt(CP_STRLIT("Fira Code"), 13.0);
	//font_family codefnt(CP_STRLIT("Segoe UI"), 13.0);

	texture_table tbl;
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
		tbl = class_manager::get().visuals.load_json(v);
	}

	{
		// load hotkeys
		std::ifstream fin("keys.json", std::ios::binary);
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
		class_manager::get().hotkeys.load_json(v);
	}

	content_host::set_default_font(fnt);
	code::editor::set_font(codefnt);
	code::editor::set_selection_brush(std::make_shared<texture_brush>(colord(0.0, 0.6, 1.0, 0.2)));

	label *lbl = element::create<label>();
	lbl->content().set_text(CP_STRLIT("Ctrl+O to open a file"));
	lbl->set_anchor(anchor::none);
	lbl->set_margin(thickness(1.0));
	tab *tmptab = dock_manager::get().new_tab();
	tmptab->set_caption(CP_STRLIT("welcome"));
	tmptab->children().add(*lbl);

	tbl.load_all(make_path("skin/"));
	{
		// load folding gizmo
		editor::code::editor::gizmo gz;
		gz.texture = std::make_shared<os::texture>(os::load_image(make_path("folded.png")));
		gz.width = static_cast<double>(gz.texture->get_width());
		editor::code::editor::set_folding_gizmo(std::move(gz));
	}

	while (!dock_manager::get().empty()) {
		{
			monitor_performance mon(CP_STRLIT("frame"), 0.05);
			manager::get().update();
			dock_manager::get().update();
			callback_buffer::get().flush();
			performance_monitor::get().update();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	for (auto i = async_task_pool::get().tasks().begin(); i != async_task_pool::get().tasks().end(); ++i) {
		async_task_pool::get().try_cancel(i);
	}
	while (!async_task_pool::get().tasks().empty()) {
		async_task_pool::get().wait_finish(async_task_pool::get().tasks().begin());
	}
	manager::get().dispose_marked_elements();

	return 0;
}
