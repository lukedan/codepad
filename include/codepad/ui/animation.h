// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Animation-related classes and structs.

#include <optional>

#include "misc.h"
#include "../core/json/storage.h"
#include "property_path.h"

namespace codepad::ui {
	class manager;
	class animation_definition_base;

	using animation_clock_t = std::chrono::high_resolution_clock; ///< Type of the clock used for animation updating.
	using animation_time_point_t = animation_clock_t::time_point; ///< Represents a time point in an animation.
	using animation_duration_t = animation_clock_t::duration; ///< Represents a duration in an animation.

	/// Transition functions used in animations.
	namespace transition_functions {
		/// The linear transition function.
		[[nodiscard]] inline double linear(double v) {
			return v;
		}

		/// The smoothstep transition function.
		[[nodiscard]] inline double smoothstep(double v) {
			return v * v * (3.0 - 2.0 * v);
		}

		/// The concave quadratic transition function.
		[[nodiscard]] inline double concave_quadratic(double v) {
			return v * v;
		}
		/// The convex quadratic transition function.
		[[nodiscard]] inline double convex_quadratic(double v) {
			v = 1.0 - v;
			return 1.0 - v * v;
		}

		/// The concave cubic transition function.
		[[nodiscard]] inline double concave_cubic(double v) {
			return v * v * v;
		}
		/// The convex cubic transition function.
		[[nodiscard]] inline double convex_cubic(double v) {
			v = 1.0 - v;
			return 1.0 - v * v * v;
		}
	}

	/// Type of the transition function used to control the process of a \ref playing_animation_base. This function
	/// accepts a double in the range of [0, 1] and return a double in the same range. The input indicates the
	/// current process of the animation, and the output is used to linearly interpolate the current value between
	/// the starting value and the destination value.
	using transition_function = std::function<double(double)>;
	/// Parser for \ref transition_function.
	template <> struct managed_json_parser<transition_function> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<transition_function> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};


	/// Stores generic keyframe animation parameters that can be further processed into a
	/// \ref keyframe_animation_definition.
	struct generic_keyframe_animation_definition {
		/// A key frame.
		struct keyframe {
			/// Stores the target value which has not yet been parsed.
			///
			/// \sa keyframe_animation_definition::keyframe::target
			json::value_storage target;
			animation_duration_t duration{ 0 }; ///< \sa keyframe_animation_definition::keyframe::duration
			transition_function transition_func; ///< \sa keyframe_animation_definition::keyframe::transition_func
		};

		std::vector<keyframe> keyframes; ///< The list of key frames.
		std::size_t repeat_times = 1; ///< \sa keyframe_animation_definition::repeat_times 
	};
	/// Parser for \ref generic_keyframe_animation_definition::keyframe.
	template <> struct managed_json_parser<generic_keyframe_animation_definition::keyframe> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<generic_keyframe_animation_definition::keyframe> operator()(
			const Value &val
			) const {
			if (auto obj = val.template cast<typename Value::object_type>()) {
				if (auto it = obj->find_member(u8"to"); it != obj->member_end()) {
					generic_keyframe_animation_definition::keyframe res;
					res.target = json::store(it.value());
					if (auto dur = obj->template parse_optional_member<animation_duration_t>(u8"duration")) {
						res.duration = dur.value();
					}
					if (auto trans = obj->template parse_optional_member<transition_function>(
						u8"transition", managed_json_parser<transition_function>(_manager)
						)) {
						res.transition_func = trans.value();
					}
					return res;
				}
			}
			return std::nullopt;
		}
	protected:
		manager &_manager; ///< The associated \ref manager.
	};
	/// Parser for \ref generic_keyframe_animation_definition.
	template <> struct managed_json_parser<generic_keyframe_animation_definition> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &m) : _manager(m) {
		}

		/// The parser interface.
		template <typename Value> std::optional<generic_keyframe_animation_definition> operator()(
			const Value &val
		) const {
			using _keyframe = generic_keyframe_animation_definition::keyframe;

			managed_json_parser<_keyframe> keyframe_parser{ _manager };
			json::array_parser<_keyframe, managed_json_parser<_keyframe>> keyframe_list_parser{ keyframe_parser };
			if (auto obj = val.template try_cast<typename Value::object_type>()) {
				generic_keyframe_animation_definition res;
				if (auto frames = obj->template parse_optional_member<std::vector<_keyframe>>(
					u8"frames", keyframe_list_parser
					)) {
					res.keyframes = std::move(frames.value());
				} else if (auto frame = val.template try_parse<_keyframe>(keyframe_parser)) {
					res.keyframes.emplace_back(std::move(frame.value()));
				} else {
					val.template log<log_level::error>(CP_HERE) << "no keyframe found in animation";
					return std::nullopt;
				}
				if (auto it = obj->find_member(u8"repeat"); it != obj->member_end()) {
					auto repeat_val = it.value();
					if (auto num = repeat_val.template try_cast<std::uint64_t>()) {
						res.repeat_times = num.value();
					} else if (auto boolean = repeat_val.template try_cast<bool>()) {
						res.repeat_times = boolean.value() ? 0 : 1;
					} else {
						repeat_val.template log<log_level::error>(CP_HERE) <<
							"invalid repeat for keyframe animation";
					}
				}
				return res;
			} else if (val.template is<typename Value::array_type>()) {
				if (auto frames = val.template parse<std::vector<_keyframe>>(keyframe_list_parser)) {
					generic_keyframe_animation_definition res;
					res.keyframes = std::move(frames.value());
					return res;
				}
			} else {
				generic_keyframe_animation_definition res;
				auto &kf = res.keyframes.emplace_back();
				kf.target = json::store(val);
				return res;
			}
			return std::nullopt;
		}
	protected:
		manager &_manager; ///< The associated \ref manager.
	};


	/// Used to parse and lerp values used in animations.
	class animation_value_handler_base {
	public:
		/// Default virtual destructor.
		virtual ~animation_value_handler_base() = default;

		/// Parses a keyframe animation.
		virtual std::shared_ptr<animation_definition_base> parse_keyframe_animation(
			const generic_keyframe_animation_definition&
		) const = 0;
		/// Parses and sets the value of the given property for the given object.
		virtual void set_value(
			property_path::any_ptr, const property_path::accessors::accessor_base&, const json::value_storage&
		) const = 0;
	};
	/// Used to interpolate between values using \ref lerp(). If such interpolation is not possible, returns the from
	/// value.
	template <typename T> struct default_lerp {
	public:
		/// Calls \ref lerp() if \ref _can_lerp::value is \p true.
		T operator()(T from, [[maybe_unused]] T to, [[maybe_unused]] double perc) const {
			if constexpr (_can_lerp::value) {
				return lerp(from, to, perc);
			} else {
				return from;
			}
		}
	protected:
		/// Used to test if lerping is possible for this type.
		struct _can_lerp {
			/// Matches if all necessary operators are implemented.
			template <typename U = T> static std::true_type test(decltype(
				std::declval<U>() + (std::declval<U>() - std::declval<U>()) * 0.5, 0
			));
			/// Matches otherwise.
			template <typename U = T> static std::false_type test(...);

			/// The result.
			constexpr static bool value = std::is_same_v<decltype(test(0)), std::true_type>;
		};
	};
	/// Base class for value handlers for specific types.
	template <typename T> class typed_animation_value_handler : public animation_value_handler_base {
	public:
		/// Parses the value. By default this function uses the \ref default_parser.
		[[nodiscard]] virtual std::optional<T> parse(const json::value_storage&) const = 0;
		/// Linearly interpolates between the two values if possible; if not this should return the first value. By
		/// default this function simply returns the result of \ref lerp().
		[[nodiscard]] virtual T lerp(const T &from, const T &to, double prog) const {
			return default_lerp<T>{}(from, to, prog);
		}

		/// Parses a \ref keyframe_animation_definition.
		std::shared_ptr<animation_definition_base> parse_keyframe_animation(
			const generic_keyframe_animation_definition&
		) const override;
		/// Parses and sets the value if the \ref property_path::accessors::accessor_base is of the correct type.
		void set_value(
			property_path::any_ptr obj, const property_path::accessors::accessor_base &access,
			const json::value_storage &value
		) const override {
			if (auto *typed_access = dynamic_cast<const property_path::accessors::typed_accessor<T>*>(&access)) {
				if (auto val = parse(value)) {
					typed_access->set_value(obj, val.value());
				} else {
					logger::get().log_error(CP_HERE) << "failed to parse value";
				}
			} else {
				logger::get().log_error(CP_HERE) << "accessor has incorrect type";
			}
		}
	};
	/// A typed value handler that uses the default JSON parser.
	template <typename T> class default_animation_value_handler : public typed_animation_value_handler<T> {
	public:
		/// Parses the value. By default this function uses the \ref default_parser.
		[[nodiscard]] std::optional<T> parse(const json::value_storage &val) const override {
			return val.get_value().parse<T>(json::default_parser<T>());
		}
	};
	/// A \ref typed_animation_value_handler where parsing a value requires a \ref manager.
	template <typename T> class managed_animation_value_handler : public typed_animation_value_handler<T> {
	public:
		/// Initializes \ref _manager.
		explicit managed_animation_value_handler(manager &m) : _manager(m) {
		}

		// TODO remove this and the special handling of std::shared_ptr<bitmap>
		/// Tries to parse the given JSON value into a specific value. This function simply calls
		/// \ref json::storage::value_t::parse() for most types, but handles special cases such as images.
		static std::optional<T> parse_static(const json::value_storage&, manager&);

		/// Simply invokes \ref parse_static().
		std::optional<T> parse(const json::value_storage &value) const override {
			return parse_static(value, _manager);
		}
	protected:
		manager &_manager; ///< The manager associated with this value handler.
	};


	/// Basic interface of an ongoing animation.
	class playing_animation_base {
	public:
		/// Default virtual destructor.
		virtual ~playing_animation_base() = default;

		/// Updates the animation.
		///
		/// \return The time before this animation needs to be updated again. Return \p std::nullopt to end the
		///         animation.
		virtual std::optional<animation_duration_t> update(animation_time_point_t) = 0;

		/// Returns the subject of this animation.
		virtual const property_path::accessors::accessor_base &get_accessor() const = 0;
	};
	/// Basic interface for animation definitions.
	class animation_definition_base {
	public:
		/// Default virtual destructor.
		virtual ~animation_definition_base() = default;

		/// Starts the animation for the given object and \ref property_path::accessors::accessor_base, and returns
		/// the newly created \ref playing_animation_base.
		virtual std::unique_ptr<playing_animation_base> start(
			property_path::any_ptr, std::shared_ptr<property_path::accessors::accessor_base>
		) const = 0;
	};


	/// Defines a keyframe animation. Key frames contain target values, durations, and transition functions.
	template <typename T> class keyframe_animation_definition : public animation_definition_base {
	public:
		/// A key frame.
		struct keyframe {
			/// Default constructor.
			keyframe() = default;
			/// Initializes all fields of this struct.
			keyframe(T tar, animation_duration_t d, transition_function t) :
				target(std::move(tar)), duration(d), transition_func(std::move(t)) {
			}

			T target{}; ///< The target value.
			/// The duration of this key frame, i.e., the time after the last key frame and before this key frame.
			animation_duration_t duration{0};
			/// The transition function. If this is \p nullptr, then the animation will immediately reach \ref target
			/// value at this \ref keyframe.
			transition_function transition_func;
		};

		/// Default constructor.
		keyframe_animation_definition() = default;
		/// Initializes all fields of this struct.
		keyframe_animation_definition(
			std::vector<keyframe> kfs, const typed_animation_value_handler<T> &handler, std::size_t repeat
		) : keyframes(std::move(kfs)), value_handler(handler), repeat_times(repeat) {
		}

		/// Starts a \ref playing_keyframe_animation.
		std::unique_ptr<playing_animation_base> start(
			property_path::any_ptr, std::shared_ptr<property_path::accessors::accessor_base>
		) const override;

		std::vector<keyframe> keyframes; ///< The list of key frames.
		const typed_animation_value_handler<T> &value_handler; ///< Used to interpolate the values.
		/// The number of times to repeat the whole animation. If this is 0, then the animation will be repeated
		/// indefinitely.
		std::size_t repeat_times = 1;
	};


	/// An ongoing \ref keyframe_animation_definition.
	template <typename T> class playing_keyframe_animation : public playing_animation_base {
	public:
		/// The maximum number of key frames to advance per update. This is to prevent repeating key frames with zero
		/// duration from locking up the program.
		constexpr static std::size_t maximum_frames_per_update = 1000;

		using definition_t = keyframe_animation_definition<T>; ///< The type of animation definition.

		/// Initializes this playing animation.
		playing_keyframe_animation(
			const definition_t &def, property_path::any_ptr subject,
			std::shared_ptr<property_path::accessors::typed_accessor<T>> access
		) : _keyframe_start(animation_clock_t::now()), _subject(subject), _accessor(std::move(access)), _def(&def) {
			if (auto from_val = _accessor->get_value(_subject)) {
				_from = from_val.value();
			} else {
				logger::get().log_error(CP_HERE) << "failed to retrieve animation starting value. using default";
			}
		}

		/// Updates this animation.
		///
		/// \param now The time of now.
		/// \return The time before this \ref playing_keyframe_animation needs to be updated again.
		std::optional<animation_duration_t> update(animation_time_point_t now) override {
			for (std::size_t i = 0; i < maximum_frames_per_update; ++i) { // go through the frames
				if (_cur_frame >= _def->keyframes.size()) { // animation has finished
					if (!_def->keyframes.empty()) { // protect against animations that failed to parse
						_accessor->set_value(_subject, _def->keyframes.back().target);
					}
					return std::nullopt;
				}
				const typename definition_t::keyframe &f = _def->keyframes[_cur_frame];
				animation_time_point_t key_frame_end = _keyframe_start + f.duration;
				if (key_frame_end > now) { // at the correct frame
					if (f.transition_func) {
						double progress = f.transition_func(
							std::chrono::duration<double>(now - _keyframe_start) / f.duration
						);
						_accessor->set_value(_subject, _def->value_handler.lerp(_from, f.target, progress));
						return animation_duration_t(0); // update immediately
					} else { // wait until this key frame is over
						_accessor->set_value(_subject, _from);
						return key_frame_end - now;
					}
				}
				// advance frame
				_keyframe_start += f.duration;
				_from = f.target; // next frame starts at the target value of this frame
				if (++_cur_frame == _def->keyframes.size()) { // at the end, check if should repeat
					++_repeated;
					if (_def->repeat_times == 0 || _repeated < _def->repeat_times) { // yes
						_cur_frame = 0;
					} else { // this animation has finished
						_accessor->set_value(_subject, f.target);
						return std::nullopt;
					}
				}
			}
			logger::get().log_warning(CP_HERE) << "potential zero-duration loop in animation";
			return std::nullopt;
		}

		/// Returns \ref _accessor.
		const property_path::accessors::accessor_base &get_accessor() const override {
			return *_accessor;
		}
	protected:
		T _from{}; ///< The value of the last key frame, or the original value.
		/// Time when the last \ref keyframe_animation_definition::keyframe was reached.
		animation_time_point_t _keyframe_start;
		std::size_t
			_cur_frame = 0, ///< The index of the current \ref keyframe_animation_definition::keyframe.
			_repeated = 0; ///< The number of times that this animation has been repeated.
		property_path::any_ptr _subject; ///< Subject of the animation, used with \ref _accessor.
		/// Used to access the value being animated from \ref _subject.
		std::shared_ptr<property_path::accessors::typed_accessor<T>> _accessor;
		const definition_t *_def = nullptr; ///< The definition of this animation.
	};


	template <typename T> std::unique_ptr<playing_animation_base> keyframe_animation_definition<T>::start(
		property_path::any_ptr subject, std::shared_ptr<property_path::accessors::accessor_base> access
	) const {
		if (auto typed = std::dynamic_pointer_cast<property_path::accessors::typed_accessor<T>>(access)) {
			return std::make_unique<playing_keyframe_animation<T>>(*this, subject, typed);
		}
		logger::get().log_warning(CP_HERE) << "the given subject of the animation is not typed";
		return nullptr;
	}


	struct property_info;
	class component_property_accessor_builder;
	/// Functions used to find a property from an object.
	namespace property_finders {
		/// Builds a \ref property_path::accessors::address_accessor for the given property path, starting with an
		/// object with type \p Object. This default implementation assumes that the type we're starting with is
		/// already a primitive type and that the path should terminate immediately.
		template <typename Object> [[nodiscard]] property_info find_property_info(
			component_property_accessor_builder&
		);

		/// Specialization for \ref vec2d.
		template <> property_info find_property_info<vec2d>(component_property_accessor_builder&);
		/// Specialization for \ref colord.
		template <> property_info find_property_info<colord>(component_property_accessor_builder&);

		/// Specialization for \ref thickness.
		template <> property_info find_property_info<thickness>(component_property_accessor_builder&);
		/// Specialization for \ref font_parameters.
		template <> property_info find_property_info<font_parameters>(component_property_accessor_builder&);


		/// Similar to \ref find_property_info, but the parsing of the values may require a \ref manager. By default
		/// this function uses \ref component_property_accessor_builder::finish_and_create_property_info_managed().
		template <typename Object> [[nodiscard]] property_info find_property_info_managed(
			component_property_accessor_builder&, manager&
		);


		/// Builds a \ref property_path::accessors::address_accessor for the given property path, starting with an
		/// array of \p Elem. This function appends a
		/// \ref property_path::address_accessor_components::array_component, and then calls the given \p ElemHandler
		/// if the path has not ended.
		template <
			typename Elem, auto ElemHandler = find_property_info<Elem>, typename ...ExtraArgs
		> [[nodiscard]] property_info find_array_property_info(
			component_property_accessor_builder &builder, ExtraArgs &&...args
		);
	}

	/// Objects used to access and modify a property of an object.
	struct property_info {
	public:
		std::shared_ptr<property_path::accessors::accessor_base> accessor; ///< Used to access the property.
		std::shared_ptr<animation_value_handler_base> value_handler; ///< Used to parse and lerp values.

		/// Creates a \ref property_info using a \ref property_path::accessors::address_accessor and the first
		/// component being the given member pointer. In addition, if \p Source is different from the owner type of
		/// \p MemberPtr, a \p dynamic_cast component is added before the member pointer component.
		template <
			auto MemberPtr, typename Source
		> [[nodiscard]] inline static property_info find_member_pointer_property_info(
			const property_path::component_list &list, std::function<void(property_path::any_ptr)> callback = nullptr
		) {
			using _value_t = typename member_pointer_traits<decltype(MemberPtr)>::value_type;
			component_property_accessor_builder builder(list.begin(), list.end(), std::move(callback));
			_add_member_pointer_property_components<MemberPtr, Source>(builder);
			return property_finders::template find_property_info<_value_t>(builder);
		}
		/// Similar to \ref find_member_pointer_property_info_managed(), but using
		/// \ref property_finders::find_property_info_managed().
		template <
			auto MemberPtr, typename Source
		> [[nodiscard]] inline static property_info find_member_pointer_property_info_managed(
			const property_path::component_list &list, manager &man,
			std::function<void(property_path::any_ptr)> callback = nullptr
		) {
			using _value_t = typename member_pointer_traits<decltype(MemberPtr)>::value_type;
			component_property_accessor_builder builder(list.begin(), list.end(), std::move(callback));
			_add_member_pointer_property_components<MemberPtr, Source>(builder);
			return property_finders::template find_property_info_managed<_value_t>(builder, man);
		}
		/// Creates a \ref property_info using a \ref property_path::accessors::getter_setter_accessor.
		template <
			typename Owner, typename T, typename StaticOwner = Owner, typename Getter, typename Setter
		> [[nodiscard]] inline static property_info make_getter_setter_property_info(
			Getter &&getter, Setter &&setter
		) {
			property_info result;
			if constexpr (std::is_same_v<Owner, StaticOwner>) {
				result.accessor = std::make_shared<property_path::accessors::getter_setter_accessor<T, Owner>>(
					std::forward<Getter>(getter), std::forward<Setter>(setter)
				);
			} else {
				result.accessor = std::make_shared<property_path::accessors::getter_setter_accessor<T, StaticOwner>>(
					[get = std::forward<Getter>(getter)](const StaticOwner &owner) -> std::optional<T> {
						if (auto *obj = dynamic_cast<const Owner*>(&owner)) {
							return get(*obj);
						}
						logger::get().log_error(CP_HERE) <<
							"incorrect dynamic type: expected " << demangle(typeid(Owner).name());
						return std::nullopt;
					},
					[set = std::forward<Setter>(setter)](StaticOwner &owner, T val) {
						if (auto *obj = dynamic_cast<Owner*>(&owner)) {
							set(*obj, std::move(val));
						} else {
							logger::get().log_error(CP_HERE) <<
								"incorrect dynamic type: expected " << demangle(typeid(Owner).name());
						}
					}
				);
			}
			result.value_handler = std::make_shared<default_animation_value_handler<T>>();
			return result;
		}

		/// Wraps around the given callable object to try to cast a \ref property_path::any_ptr to the desired type.
		template <
			typename Base, typename Derived = Base, typename Callback
		> [[nodiscard]] inline static std::function<void(property_path::any_ptr)> make_typed_modification_callback(
			Callback &&cb
		) {
			static_assert(std::is_base_of_v<Base, Derived>, "Base should be a base class of Derived");
			return [callback = std::forward<Callback>(cb)](property_path::any_ptr ptr) {
				if (auto *typed_ptr = ptr.get<Base>()) {
					Derived *desired_ptr = nullptr;
					if constexpr (std::is_same_v<Base, Derived>) {
						desired_ptr = typed_ptr;
					} else {
						desired_ptr = dynamic_cast<Derived*>(typed_ptr);
						if (desired_ptr == nullptr) {
							logger::get().log_error(CP_HERE) <<
								"pointer for modification callback has incorrect dynamic type";
							return;
						}
					}
					callback(*desired_ptr);
				} else {
					logger::get().log_error(CP_HERE) <<
						"failed to cast any_ptr to " << demangle(typeid(Base).name());
				}
			};
		}
	protected:
		/// Adds the \p dynamic_cast component and the first component for a member pointer property.
		template <auto MemberPtr, typename Source> inline static void _add_member_pointer_property_components(
			component_property_accessor_builder&
		);
	};

	/// Used to build a \ref property_path::accessors::address_accessor.
	class component_property_accessor_builder {
	public:
		/// Initializes all fields of this struct.
		component_property_accessor_builder(
			property_path::component_list::const_iterator beg, property_path::component_list::const_iterator end,
			std::function<void(property_path::any_ptr)> mod_cb = nullptr
		) : _modification_callback(std::move(mod_cb)), _begin(beg), _end(end) {
		}

		/// Returns the current component. This should only be called when \ref ended() returns \p false.
		[[nodiscard]] const property_path::component &current_component() const {
			return *_begin;
		}
		/// Moves to the next component.
		///
		/// \return Whether there are more components to process, i.e., whether \ref ended() will return \p false.
		bool move_next() {
			if (_begin->index.has_value() && !_index_used) {
				logger::get().log_error(CP_HERE) <<
					"unused index in property path: " << property_path::to_string(_begin, _end);
			}
			++_begin;
			_index_used = false;
			return _begin != _end;
		}
		/// Peeks the next component. Returns \p nullptr if there are no more components.
		[[nodiscard]] const property_path::component *peek_next() const {
			if (has_ended()) {
				return nullptr;
			}
			auto next_it = _begin;
			if (++next_it == _end) {
				return nullptr;
			}
			return &*next_it;
		}
		/// Returns whether there are no more property path components to process.
		[[nodiscard]] bool has_ended() const {
			return _begin == _end;
		}
		/// Sets \ref _index_used. If it's already set, logs an error.
		void mark_index_used() {
			if (_index_used) {
				logger::get().log_error(CP_HERE) << "mark_index_used() called multiple times for the same index";
			}
			_index_used = true;
		}

		/// Appends a component to \ref _components.
		void append_accessor_component(
			std::unique_ptr<property_path::address_accessor_components::component_base> comp
		) {
			_components.emplace_back(std::move(comp));
		}
		/// Finishes component generation and returns the resulting \ref property_path::accessors::address_accessor.
		/// This builder should not be used afterwards.
		template <
			typename T
		> [[nodiscard]] std::shared_ptr<property_path::accessors::accessor_base> finish_and_create_accessor() {
			return std::make_shared<property_path::accessors::address_accessor<T>>(
				std::move(_components), std::move(_modification_callback)
			);
		}


		// convenience functions
		/// Logs an error if the current component's type is not empty and is not the given type.
		void expect_type(std::u8string_view type) const {
			if (!_begin->is_type_or_empty(type)) {
				logger::get().log_error(CP_HERE) << "incorrect type: expected " << type << ", got " << _begin->type;
			}
		}

		/// Creates and appends a component.
		template <typename Comp, typename ...Args> Comp &make_append_accessor_component(Args &&...args) {
			auto comp = std::make_unique<Comp>(std::forward<Args>(args)...);
			auto &res = *comp;
			append_accessor_component(std::move(comp));
			return res;
		}
		/// Creates and appends a \ref property_path::address_accessor_components::member_pointer_component.
		template <auto MemberPtr> void make_append_member_pointer_component() {
			make_append_accessor_component<
				property_path::address_accessor_components::member_pointer_component<MemberPtr>
			>();
		}

		/// Finishes component generation and returns a \ref property_info. Its accessor will be the result of
		/// \ref finish_and_create_accessor(), and its value handler will be a \ref typed_animation_value_handler.
		template <typename T> [[nodiscard]] property_info finish_and_create_property_info() {
			property_info result;
			result.accessor = finish_and_create_accessor<T>();
			result.value_handler = std::make_shared<default_animation_value_handler<T>>();
			return result;
		}
		/// Similar to \ref finish_and_create_property_info(), except this function creates a
		/// \ref managed_animation_value_handler for the value handler.
		template <typename T> [[nodiscard]] property_info finish_and_create_property_info_managed(manager &m) {
			property_info result;
			result.accessor = finish_and_create_accessor<T>();
			result.value_handler = std::make_shared<managed_animation_value_handler<T>>(m);
			return result;
		}
		/// Logs an error message and returns an empty \ref property_info.
		[[nodiscard]] property_info fail() {
			auto entry = logger::get().log_error(CP_HERE);
			entry << "failed to find property path";
			if (!has_ended()) {
				entry << ": " << property_path::to_string(_begin, _end);
			}
			return property_info();
		}

		/// Appends a \ref property_path::address_accessor_components::member_pointer_component, and calls
		/// \ref property_finders::find_property_info() to continue to find the property.
		template <auto MemberPtr> property_info append_member_and_find_property_info() {
			using _value_type = typename member_pointer_traits<decltype(MemberPtr)>::value_type;
			make_append_member_pointer_component<MemberPtr>();
			return property_finders::find_property_info<_value_type>(*this);
		}
		/// Similar to \ref append_member_and_find_property_info(), but uses
		/// \ref property_finders::find_property_info_managed() instead.
		template <auto MemberPtr> property_info append_member_and_find_property_info_managed(manager &man) {
			using _value_type = typename member_pointer_traits<decltype(MemberPtr)>::value_type;
			make_append_member_pointer_component<MemberPtr>();
			return property_finders::find_property_info_managed<_value_type>(*this, man);
		}

		/// Appends a \ref property_path::address_accessor_components::member_pointer_component and a
		/// \ref property_path::address_accessor_components::variant_component, and calls
		/// \ref property_finders::find_property_info() to continue to find the property.
		template <auto MemberPtr, typename Target> property_info append_variant_and_find_property_info() {
			using _variant_type = typename member_pointer_traits<decltype(MemberPtr)>::value_type;
			make_append_member_pointer_component<MemberPtr>();
			make_append_accessor_component<
				property_path::address_accessor_components::variant_component<Target, _variant_type>
			>();
			return property_finders::find_property_info<Target>(*this);
		}
		/// Similar to \ref append_variant_and_find_property_info(), but uses
		/// \ref property_finders::find_property_info_managed() instead.
		template <
			auto MemberPtr, typename Target
		> property_info append_variant_and_find_property_info_managed(manager &man) {
			using _variant_type = typename member_pointer_traits<decltype(MemberPtr)>::value_type;
			make_append_member_pointer_component<MemberPtr>();
			make_append_accessor_component<
				property_path::address_accessor_components::variant_component<Target, _variant_type>
			>();
			return property_finders::find_property_info_managed<Target>(*this, man);
		}
	protected:
		/// Reference to a list of assembled components.
		std::vector<std::unique_ptr<property_path::address_accessor_components::component_base>> _components;
		/// The modification callback used when this builder finishes.
		std::function<void(property_path::any_ptr)> _modification_callback;
		property_path::component_list::const_iterator
			_begin, ///< Iterator to the first component.
			_end; ///< Iterator past the last component.
		/// Whether the index component of \ref _begin is actually used. This is marked by \ref mark_index_used() and
		/// cleared in \ref move_next(). \ref move_next() also tests this flag before advancing, and if there's an
		/// index that's not used, the function emits an error.
		bool _index_used = false;
	};


	namespace property_finders {
		template <typename Object> inline property_info find_property_info(
			component_property_accessor_builder &builder
		) {
			if (builder.move_next()) {
				logger::get().log_error(CP_HERE) << "primitive types have no properties";
			}
			return builder.finish_and_create_property_info<Object>();
		}

		template <typename Object> inline property_info find_property_info_managed(
			component_property_accessor_builder &builder, manager &man
		) {
			if (builder.move_next()) {
				logger::get().log_error(CP_HERE) << "primitive types have no properties";
			}
			return builder.finish_and_create_property_info_managed<Object>(man);
		}

		template <
			typename Elem, auto ElemHandler, typename ...ExtraArgs
		> [[nodiscard]] property_info find_array_property_info(
			component_property_accessor_builder &builder, ExtraArgs &&...args
		) {
			if (!builder.current_component().index.has_value()) {
				return builder.fail();
			}
			builder.make_append_accessor_component<
				property_path::address_accessor_components::array_component<Elem>
			>(builder.current_component().index.value());
			builder.mark_index_used();
			return ElemHandler(builder, std::forward<ExtraArgs>(args)...);
		}
	}


	template <
		auto MemberPtr, typename Source
	> inline static void property_info::_add_member_pointer_property_components(
		component_property_accessor_builder &builder
	) {
		using _owner_t = typename member_pointer_traits<decltype(MemberPtr)>::owner_type;
		if constexpr (!std::is_same_v<Source, _owner_t>) {
			builder.make_append_accessor_component<
				property_path::address_accessor_components::dynamic_cast_component<_owner_t, Source>
			>();
		}
		builder.make_append_accessor_component<
			property_path::address_accessor_components::member_pointer_component<MemberPtr>
		>();
	}
}
