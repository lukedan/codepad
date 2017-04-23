#include <thread>
#include <fstream>

#include "utilities/event.h"
#include "platform/windows.h"
#include "utilities/font.h"
#include "ui/textrenderer.h"

using namespace codepad;
using namespace codepad::platform;
using namespace codepad::ui;

int main() {
	window wnd(CPTEXT("test"));
	//window wnd2(CPTEXT("test2"));
	//software_renderer rend;
	opengl_renderer rend;
	font fnt("pala.ttf", 18, rend);

	vec2i mpos;
	bool dirty = true;
	wnd.mouse_move += [&mpos, &dirty](mouse_move_info &info) {
		mpos = info.new_pos;
		dirty = true;
	};

	str_t text;
	wnd.keyboard_text += [&text, &dirty](text_info &info) {
		text += info.character;
		dirty = true;
	};

	std::ifstream fin("utilities/textconfig.h");
	str_t s;
	while (getline(fin, s)) {
		text += s + '\n';
	}

	rend.new_window(wnd);
	//rend.new_window(wnd2);

	bool stop = false;
	wnd.close_request += [&stop](void_info&) {
		stop = true;
	};
	while (!stop) {
		wnd.idle();
		//wnd2.idle();

		if (dirty) {
			rend.begin(wnd, wnd.layout().minimum_bounding_box<int>());
			if (wnd.is_mouse_over()) {
				vec2d pos = mpos.convert<double>() - vec2d(100.0, 100.0);
				text_renderer::render_plain_text(text, fnt, pos, colord(0.0, 1.0, 1.0, 1.0));

				vec2d vs[6], us[6];
				colord cs[6] = {
					colord(1.0, 1.0, 1.0, 0.2), colord(1.0, 1.0, 1.0, 0.2), colord(1.0, 1.0, 1.0, 0.2),
					colord(1.0, 1.0, 1.0, 0.2), colord(1.0, 1.0, 1.0, 0.2), colord(1.0, 1.0, 1.0, 0.2)
				};
				vec2d sz = text_renderer::measure_plain_text(text, fnt);
				vs[0] = pos;
				vs[3] = vs[1] = pos + vec2d(sz.x, 0.0);
				vs[2] = vs[5] = pos + vec2d(0.0, sz.y);
				vs[4] = pos + sz;
				rend.draw_triangles(vs, us, cs, 6, 0);
			}
			rend.end();

			//rend.begin(wnd2, wnd2.layout().minimum_bounding_box<int>());

			//vec2d pos = mpos.convert<double>() - vec2d(100.0, 100.0);
			//text_renderer::render_plain_text("fuck this shit I'm out", fnt, pos, colord(1.0, 1.0, 1.0, 1.0));
			//rend.end();

			dirty = false;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds::duration(1));
	}

	rend.delete_window(wnd);
	//rend.delete_window(wnd2);

	return 0;
}
