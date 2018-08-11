#include <thread>
#include <fstream>

#include "core/event.h"
#include "core/tasks.h"
#include "core/bst.h"
#include "os/current.h"
#include "ui/font_family.h"
#include "ui/draw.h"
#include "ui/common_elements.h"
#include "ui/native_commands.h"
#include "editors/tabs.h"
#include "editors/code/codebox.h"
#include "editors/code/components.h"
#include "editors/code/editor.h"
#include "editors/code/buffer.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editor;

int main(int argc, char **argv) {
	codepad::initialize(argc, argv);

	auto fnt = std::make_shared<default_font>(CP_STRLIT(""), 13.0, font_style::normal);
	//font_family codefnt(CP_STRLIT("Fira Code"), 12.0);
	//font_family codefnt(CP_STRLIT(""), 13.0);
	font_family codefnt(CP_STRLIT("Segoe UI"), 13.0);

	texture_table tbl;
	{
		// load skin
		std::ifstream fin("skin/skin.json", std::ios::binary);
		fin.seekg(0, std::ios::end);
		std::streampos sz = fin.tellg();
		auto c = static_cast<char*>(std::malloc(static_cast<size_t>(sz)));
		fin.seekg(0);
		fin.read(c, static_cast<std::streamsize>(sz));
		std::string us(c, static_cast<size_t>(sz));
		std::free(c);
		json::parser_value_t v;
		str_t ss = convert_to_default_encoding(us);
		v.Parse(ss.c_str());
		tbl = manager::get().get_class_visuals().load_json(v);
	}

	{
		// load arrangements
		std::ifstream fin("skin/arrangements.json", std::ios::binary);
		fin.seekg(0, std::ios::end);
		std::streampos sz = fin.tellg();
		auto c = static_cast<char*>(std::malloc(static_cast<size_t>(sz)));
		fin.seekg(0);
		fin.read(c, static_cast<std::streamsize>(sz));
		std::string us(c, static_cast<size_t>(sz));
		std::free(c);
		json::parser_value_t v;
		str_t ss = convert_to_default_encoding(us);
		v.Parse(ss.c_str());
		manager::get().get_class_arrangements().load_json(v);
	}

	{
		// load hotkeys
		std::ifstream fin("keys.json", std::ios::binary);
		fin.seekg(0, std::ios::end);
		std::streampos sz = fin.tellg();
		auto c = static_cast<char*>(std::malloc(static_cast<size_t>(sz)));
		fin.seekg(0);
		fin.read(c, static_cast<std::streamsize>(sz));
		std::string us(c, static_cast<size_t>(sz));
		std::free(c);
		json::parser_value_t v;
		str_t ss = convert_to_default_encoding(us);
		v.Parse(ss.c_str());
		manager::get().get_class_hotkeys().load_json(v);
	}

	content_host::set_default_font(fnt);
	code::editor::set_font(codefnt);

	auto *lbl = manager::get().create_element<label>();
	lbl->content().set_text(CP_STRLIT("Ctrl+O to open a file"));
	lbl->mouse_down += [lbl](ui::mouse_button_info&) {
		command_registry::get().find_command(CP_STRLIT("open_file_dialog"))(lbl->parent()->parent());
	};
	tab *tmptab = tab_manager::get().new_tab();
	tmptab->set_label(CP_STRLIT("welcome"));
	tmptab->children().add(*lbl);

	tbl.load_all("skin/");

	while (!tab_manager::get().empty()) {
		{
			performance_monitor mon(CP_STRLIT("frame"), 0.05);
			manager::get().update();
			tab_manager::get().update();
			callback_buffer::get().flush();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	async_task_pool::get().shutdown();
	manager::get().dispose_marked_elements();

	return 0;
}
