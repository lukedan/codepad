#pragma once

/// \file
/// Elements that are commonly used in the UI environment.

#include "element.h"
#include "panel.h"
#include "manager.h"
#include "draw.h"
#include "font_family.h"
#include "../core/misc.h"
#include "../core/encodings.h"
#include "../os/window.h"

namespace codepad::ui {
	/// Used to display text in \ref element "elements".
	///
	/// \todo Need a more elegent way to notify the parent of layout & visual changes.
	class content_host {
	public:
		/// Constructor, binds the object to an \ref element.
		content_host(element &p) : _parent(p) {
		}

		/// Sets the text to display.
		void set_text(str_t s) {
			_text = std::move(s);
			_on_text_size_changed();
		}
		/// Gets the text that's displayed.
		const str_t &get_text() const {
			return _text;
		}

		/// Sets the font used to display text.
		void set_font(std::shared_ptr<const os::font> fnt) {
			_fnt = std::move(fnt);
			_on_text_size_changed();
		}
		/// Returns the font currently used. Note this returns \p nullptr if no font is set,
		/// but the default font may actually be used for display.
		std::shared_ptr<const os::font> get_font() const {
			return _fnt;
		}

		/// Sets the color used to display text.
		void set_color(colord c) {
			_clr = c;
			_parent.invalidate_visual();
		}
		/// Gets the current color used to display text.
		colord get_color() const {
			return _clr;
		}

		/// Sets the default font used when no font is specified for a \ref content_host.
		inline static void set_default_font(std::shared_ptr<const os::font> fnt) {
			auto &deffnt = _default_font::get();
			deffnt.font = std::move(fnt);
			++deffnt.timestamp;
		}
		/// Retruns the default font.
		inline static const std::shared_ptr<const os::font> &get_default_font() {
			return _default_font::get().font;
		}

		/// Sets the offset of the text. The offset is relative to the size of the parent control,
		/// so (0.5, 0.5) puts the text to the center while (1.0, 0.0) puts the text to the top-right corner.
		/// If both dimensions are in range [0, 1] then the text will be fully contained by the
		/// parent control's client region, and vice versa.
		///
		/// \sa element::get_client_region()
		void set_text_offset(vec2d o) {
			_offset = o;
			_parent.invalidate_visual();
		}
		/// Returns the offset of the text.
		vec2d get_text_offset() const {
			return _offset;
		}

		/// Returns the size of the text, measuring it if necessary.
		virtual vec2d get_text_size() const {
			if (_fnt) { // there's a custom font
				if (_szcache_id == _noszcache) { // not cached
					_szcache_id = _hasszcache;
					_szcache = text_renderer::measure_plain_text(_text, _fnt);
				}
			} else { // use the default font
				auto &fnt = _default_font::get();
				if (fnt.font) {
					if (_szcache_id != fnt.timestamp) { // not cached
						_szcache_id = fnt.timestamp;
						return _szcache = text_renderer::measure_plain_text(_text, fnt.font);
					}
				} else { // no default font; set size to 0
					_szcache = vec2d();
				}
			}
			return _szcache;
		}
		/// Returns the position of the top-left corner of the displayed text,
		/// relative to the window's client area.
		vec2d get_text_position() const {
			rectd inner = _parent.get_client_region();
			vec2d spare = inner.size() - get_text_size();
			return inner.xmin_ymin() + vec2d(spare.x * _offset.x, spare.y * _offset.y);
		}

		/// Renders the text.
		void render() const {
			auto &&f = _fnt ? _fnt : _default_font::get().font;
			if (f) {
				text_renderer::render_plain_text(_text, f, get_text_position(), _clr);
			}
		}
	protected:
		constexpr static unsigned char
			_noszcache = 0, ///< Indicates that no size cache is currently available.
			_hasszcache = 1; ///< Indicates that the size cache for the custom font is available.

		mutable unsigned char _szcache_id = _noszcache; ///< Used to mark whether the current cache is valid.
		mutable vec2d _szcache; ///< Measured size of the current text, using the current font.
		str_t _text; ///< The text to display.
		std::shared_ptr<const os::font> _fnt; ///< The custom font.
		colord _clr; ///< The color used to display the text.
		vec2d _offset; ///< The offset of the text.
		element &_parent; ///< The \ref element that this belongs to.

		/// Called when the size of the text has changed, to mark that the size cache is invalid,
		/// and to mark the parent for relayout if its size is `automatic'.
		virtual void _on_text_size_changed() {
			_szcache_id = _noszcache;
			if (
				_parent.get_width_allocation_type() == size_allocation_type::automatic ||
				_parent.get_height_allocation_type() == size_allocation_type::automatic
				) {
				_parent.invalidate_layout();
			}
			_parent.invalidate_visual();
		}

		/// The struct that holds the default font and the timestamp.
		struct _default_font {
			std::shared_ptr<const os::font> font; ///< The default font.
			unsigned char timestamp = 0; ///< The timestamp. Incremented each time \ref font is changed.

			/// Gets the global \ref _default_font instance.
			static _default_font &get();
		};
	};

	/// A label that displays plain text. Non-focusable by default.
	class label : public element {
	public:
		/// Returns the underlying \ref content_host.
		content_host & content() {
			return _content;
		}
		/// Const version of content().
		const content_host &content() const {
			return _content;
		}

		/// Returns the combined width of the text and the padding in pixels.
		std::pair<double, bool> get_desired_width() const override {
			return {_content.get_text_size().x + _padding.width(), true};
		}
		/// Returns the combined height of the text and the padding in pixels.
		std::pair<double, bool> get_desired_height() const override {
			return {_content.get_text_size().y + _padding.height(), true};
		}

		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("label");
		}
	protected:
		/// Renders the text.
		void _custom_render() override {
			_content.render();
		}

		/// Initializes the element.
		void _initialize() override {
			element::_initialize();
			_can_focus = false;
		}

		content_host _content{*this}; ///< Manages the contents to display.
	};

	/// Base class of button-like elements, that provides all interfaces only internally.
	class button_base : public element {
	public:
		/// Indicates when the click event is triggered.
		enum class trigger_type {
			mouse_down, ///< The event is triggered as soon as the button is pressed.
			mouse_up ///< The event is triggered after the user presses then releases the button.
		};

		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("button_base");
		}
	protected:
		bool
			_trigbtn_down = false, ///< Indicates whether the trigger button is currently down.
			/// If \p true, the user will be able to cancel the click after pressing the trigger button,
			/// by moving the cursor out of the button and releasing the trigger button,
			/// when trigger type is set to trigger_type::mouse_up.
			_allow_cancel = true;
		trigger_type _trigtype = trigger_type::mouse_up; ///< The trigger type of this button.
		/// The mouse button that triggers the button.
		os::input::mouse_button _trigbtn = os::input::mouse_button::primary;

		/// Checks for clicks.
		void _on_mouse_down(mouse_button_info &p) override {
			_on_update_mouse_pos(p.position);
			if (p.button == _trigbtn) {
				_trigbtn_down = true;
				get_window()->set_mouse_capture(*this); // capture the mouse
				if (_trigtype == trigger_type::mouse_down) {
					_on_click();
				}
			}
			element::_on_mouse_down(p);
		}
		/// Called when the user releases the trigger button, or when the capture is lost.
		void _on_trigger_button_up() {
			if (_trigbtn_down) {
				get_window()->release_mouse_capture();
				_trigbtn_down = false;
			}
		}
		/// Calls _on_trigger_button_up().
		void _on_capture_lost() override {
			_on_trigger_button_up();
		}
		/// Checks if this is a valid click.
		void _on_mouse_up(mouse_button_info &p) override {
			_on_update_mouse_pos(p.position);
			if (p.button == _trigbtn) {
				bool valid = is_mouse_over() && _trigbtn_down;
				_on_trigger_button_up();
				if (valid && _trigtype == trigger_type::mouse_up) {
					_on_click();
				}
			}
			element::_on_mouse_up(p);
		}
		/// Called when the mouse position need to be updated. Checks if the mouse is still over the element.
		/// Changes the visual state if \ref _allow_cancel is set to \p true.
		void _on_update_mouse_pos(vec2d pos) {
			if (_allow_cancel) {
				if (hit_test(pos)) {
					_set_state_bit(manager::get().get_predefined_states().mouse_over, true);
				} else {
					_set_state_bit(manager::get().get_predefined_states().mouse_over, false);
				}
			}
		}
		/// Updates the mouse position.
		void _on_mouse_move(mouse_move_info &p) override {
			_on_update_mouse_pos(p.new_pos);
			element::_on_mouse_move(p);
		}

		/// Callback that is called when the user clicks the button.
		virtual void _on_click() = 0;
	};
	/// Simple implementation of a button.
	class button : public button_base {
	public:
		/// Returns \p true if the button is currently pressed.
		bool is_trigger_button_pressed() const {
			return _trigbtn_down;
		}

		/// Sets the mouse button used to press the button, the `trigger button'.
		void set_trigger_button(os::input::mouse_button btn) {
			_trigbtn = btn;
		}
		/// Returns the current trigger button.
		os::input::mouse_button get_trigger_button() const {
			return _trigbtn;
		}

		/// Sets when the button click is triggered.
		///
		/// \sa button_base::trigger_type
		void set_trigger_type(trigger_type t) {
			_trigtype = t;
		}
		/// Returns the current trigger type.
		trigger_type get_trigger_type() const {
			return _trigtype;
		}

		/// Sets whether the user is allowed to cancel the click halfway.
		void set_allow_cancel(bool v) {
			_allow_cancel = v;
		}
		/// Returns \p true if the user is allowed to cancel the click.
		bool get_allow_cancel() const {
			return _allow_cancel;
		}

		event<void> click; ///< Triggered when the button is clicked.

		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("button");
		}
	protected:
		/// Invokes \ref click.
		void _on_click() override {
			click.invoke();
		}
	};

	class scroll_bar;
	/// The draggable button of a scroll bar.
	class scroll_bar_drag_button : public button_base {
	public:
		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("scroll_bar_drag_button");
		}
	protected:
		double _doffset = 0.0; ///< The offset of the mouse when the button is being dragged.

		/// Returns the \ref scroll_bar that this button belongs to.
		scroll_bar *_get_bar() const;

		/// Initializes the element.
		void _initialize() override {
			button_base::_initialize();
			_trigtype = trigger_type::mouse_down;
			_allow_cancel = false;
			set_can_focus(false);
		}

		/// Overridden so that instances this element can be created.
		void _on_click() override {
		}

		/// Sets \ref _doffset accordingly if dragging starts.
		void _on_mouse_down(mouse_button_info&) override;
		/// Updates the value of the parent \ref scroll_bar when dragging.
		void _on_mouse_move(mouse_move_info&) override;
	};
	/// A scroll bar.
	class scroll_bar : public panel_base {
	public:
		/// Sets the orientation of the scroll bar.
		void set_orientation(orientation o) {
			if (_ori != o) {
				_ori = o;
				invalidate_layout();
			}
		}
		/// Returns the current orientation of the scroll bar.
		orientation get_orientation() const {
			return _ori;
		}

		/// Returns the default desired width of the scroll bar.
		///
		/// \todo Extract the constants.
		std::pair<double, bool> get_desired_width() const override {
			if (get_orientation() == orientation::vertical) {
				return {10.0, true};
			}
			return {1.0, false};
		}
		/// Returns the default desired height of the scroll bar.
		///
		/// \todo Extract the constants.
		std::pair<double, bool> get_desired_height() const override {
			if (get_orientation() == orientation::horizontal) {
				return {10.0, true};
			}
			return {1.0, false};
		}

		/// Sets the current value of the scroll bar.
		void set_value(double v) {
			double ov = _curv;
			_curv = std::clamp(v, 0.0, _totrng - _range);
			revalidate_layout();
			value_changed.invoke_noret(ov);
		}
		/// Returns the current value of the scroll bar.
		double get_value() const {
			return _curv;
		}
		/// Sets the parameters of the scroll bar.
		///
		/// \param tot The total length of the region.
		/// \param vis The visible length of the region.
		void set_params(double tot, double vis) {
			assert_true_usage(vis <= tot, "scrollbar visible range too large");
			_totrng = tot;
			_range = vis;
			set_value(_curv);
		}
		/// Returns the total length of the region.
		double get_total_range() const {
			return _totrng;
		}
		/// Returns the visible length of the region.
		double get_visible_range() const {
			return _range;
		}

		/// Scrolls the scroll bar so that a certain point is in the visible region.
		/// Note that the point appears at the very edge of the region if it's not visible before.
		void make_point_visible(double v) {
			if (_curv > v) {
				set_value(v);
			} else {
				v -= _range;
				if (_curv < v) {
					set_value(v);
				}
			}
		}

		/// Overrides the layout of the three buttons.
		bool override_children_layout() const override {
			return true;
		}

		/// Invoked when the value of the scrollbar is changed.
		event<value_update_info<double>> value_changed;

		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("scroll_bar");
		}
		/// Returns the default class of the `page up' button.
		inline static str_t get_page_up_button_class() {
			return CP_STRLIT("scroll_bar_page_up_button");
		}
		/// Returns the default class of the `page down' button.
		inline static str_t get_page_down_button_class() {
			return CP_STRLIT("scroll_bar_page_down_button");
		}
	protected:
		orientation _ori = orientation::vertical; ///< The orientation.
		double
			_totrng = 1.0, ///< The length of the whole range.
			_curv = 0.0, ///< The minimum visible position.
			_range = 0.1; ///< The length of the visible range.
		scroll_bar_drag_button *_drag = nullptr; ///< The drag button.
		button
			*_pgup = nullptr, ///< The `page up' button.
			*_pgdn = nullptr; ///< The `page down' button.

		/// Calculates the layout of the three buttons.
		void _finish_layout() override {
			rectd cln = get_client_region();
			if (_ori == orientation::vertical) {
				double
					tszratio = cln.height() / _totrng,
					mid1 = cln.ymin + tszratio * _curv,
					mid2 = mid1 + tszratio * _range;
				_child_set_layout(_drag, rectd(cln.xmin, cln.xmax, mid1, mid2));
				_child_set_layout(_pgup, rectd(cln.xmin, cln.xmax, cln.ymin, mid1));
				_child_set_layout(_pgdn, rectd(cln.xmin, cln.xmax, mid2, cln.ymax));
			} else {
				double
					tszratio = cln.width() / _totrng,
					mid1 = cln.xmin + tszratio * _curv,
					mid2 = mid1 + tszratio * _range;
				_child_set_layout(_drag, rectd(mid1, mid2, cln.ymin, cln.ymax));
				_child_set_layout(_pgup, rectd(cln.xmin, mid1, cln.ymin, cln.ymax));
				_child_set_layout(_pgdn, rectd(mid2, cln.xmax, cln.ymin, cln.ymax));
			}
			element::_finish_layout();
		}

		/// Initializes the three buttons and adds them as children.
		void _initialize() override {
			panel_base::_initialize();
			_can_focus = false;

			_drag = element::create<scroll_bar_drag_button>();
			_children.add(*_drag);

			_pgup = element::create<button>();
			_pgup->set_trigger_type(button_base::trigger_type::mouse_down);
			_pgup->set_can_focus(false);
			_pgup->set_class(get_page_up_button_class());
			_pgup->click += [this]() {
				set_value(get_value() - get_visible_range());
			};
			_children.add(*_pgup);

			_pgdn = element::create<button>();
			_pgdn->set_trigger_type(button_base::trigger_type::mouse_down);
			_pgdn->set_can_focus(false);
			_pgdn->set_class(get_page_down_button_class());
			_pgdn->click += [this]() {
				set_value(get_value() + get_visible_range());
			};
			_children.add(*_pgdn);
		}
	};
}
