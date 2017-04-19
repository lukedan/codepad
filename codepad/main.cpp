#include <thread>

#include "utilities/event.h"
#include "platform/windows.h"
#include "utilities/font.h"

using namespace codepad;
using namespace codepad::platform;
using namespace codepad::ui;

int main() {
	window wnd(CPTEXT("test"));
	software_renderer rend;
	font fnt("Deng.ttf", 15);

	vec2i mpos;
	bool dirty = true;
	wnd.mouse_move += [&mpos, &dirty](mouse_move_info &info) {
		mpos = info.new_pos - vec2i(100.0, 100.0);
		dirty = true;
	};

	rend.new_window(wnd);

	bool stop = false;
	wnd.close_request += [&stop](void_info&) {
		stop = true;
	};
	while (!stop) {
		while (wnd.idle()) {
		}

		if (dirty) {
			rend.begin(wnd, wnd.layout().minimum_bounding_box<int>());
			if (wnd.is_mouse_over()) {
				const font::entry &et = fnt.get_char_entry('A', rend);
				vec2d v[6], u[6];
				colord c[6];
				v[0] = mpos.convert<double>();
				v[1] = mpos.convert<double>() + 10.0 * vec2d(et.placement.width(), et.placement.height());
				v[2] = mpos.convert<double>() + 10.0 * vec2d(0.0, et.placement.height());
				u[0] = vec2d(0.0, 0.0);
				u[1] = vec2d(1.0, 1.0);
				u[2] = vec2d(0.0, 1.0);
				v[3] = mpos.convert<double>();
				v[4] = mpos.convert<double>() + 10.0 * vec2d(et.placement.width(), 0.0);
				v[5] = mpos.convert<double>() + 10.0 * vec2d(et.placement.width(), et.placement.height());
				u[3] = vec2d(0.0, 0.0);
				u[4] = vec2d(1.0, 0.0);
				u[5] = vec2d(1.0, 1.0);
				rend.draw_triangles(v, u, c, 6, et.texture);
			}
			rend.end();
			dirty = false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds::duration(1));
	}

	rend.delete_window(wnd);

	return 0;
}
