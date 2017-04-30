#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "platform/windows.h"
#include "utilities/font.h"
#include "ui/textrenderer.h"
#include "editor/codebox.h"

using namespace codepad;
using namespace codepad::platform;
using namespace codepad::ui;
using namespace codepad::editor;

int main() {
	file_context ctx(U"platform/windows.h");
	//software_renderer rend;
	opengl_renderer rend;
	font fnt("UbuntuMono-R.ttf", 18, rend);

	window *wnd = element::create<window>();
	codebox *cp = element::create<codebox>();
	wnd->children().add(*cp);
	cp->context = &ctx;
	cp->font.normal = cp->font.bold = cp->font.italic = cp->font.bold_italic = &fnt;
	cp->set_margin(thickness(100.0));

	rend.new_window(*wnd);

	bool stop = false;
	wnd->close_request += [&stop](void_info&) {
		stop = true;
	};
	while (!stop) {
		wnd->idle();

		manager::default().update_invalid_layouts();
		manager::default().update_invalid_visuals(rend);

		std::this_thread::sleep_for(std::chrono::milliseconds::duration(1));
	}

	rend.delete_window(*wnd);

	delete wnd;

	return 0;
}
