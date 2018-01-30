#pragma once

#include "context.h"

namespace codepad {
	namespace editor {
		namespace code {
			enum class linebreak_type {
				soft,
				hard
			};
			class soft_linebreak_registry {
			public:
				soft_linebreak_registry() = default;
				explicit soft_linebreak_registry(const linebreak_registry &reg) : _reg(&reg) {
				}

				struct node_data {
					node_data() = default;
					node_data(size_t l) : length(l) {
					}

					size_t length = 0;
				};
				struct node_synth_data {
					using node_type = binary_tree_node<node_data, node_synth_data>;
					struct get_node_softbreak_num {
						inline static size_t get(const node_type&) {
							return 1;
						}
					};

					size_t total_length = 0, total_softbreaks = 0;

					using length_property = sum_synthesizer::compact_property<
						size_t, node_synth_data,
						synthesization_helper::field_value_property<size_t, node_data, &node_data::length>,
						&node_synth_data::total_length
					>;
					using softbreaks_property = sum_synthesizer::compact_property<
						size_t, node_synth_data, get_node_softbreak_num, &node_synth_data::total_softbreaks
					>;

					inline static void synthesize(node_type &n) {
						sum_synthesizer::synthesize<length_property, softbreaks_property>(n);
					}
				};
				using tree_type = binary_tree<node_data, node_synth_data>;
				using node_type = typename tree_type::node;
				using iterator = typename tree_type::const_iterator;

				struct softbreak_info {
					softbreak_info() = default;
					softbreak_info(const iterator &it, size_t pc, size_t pl) :
						entry(it), prev_chars(pc), prev_softbreaks(pl) {
					}
					iterator entry;
					size_t prev_chars = 0, prev_softbreaks = 0;
				};

				std::pair<linebreak_registry::linebreak_info, softbreak_info> get_line_info(size_t line) const {
					if (_t.empty()) {
						return {_reg->get_beginning_char_of_line(line), softbreak_info(_t.end(), 0, 0)};
					}
					size_t softc = 0, hardc = 0;
					iterator soft;
					linebreak_registry::iterator hard;
					size_t slb = _find_line_ending(soft, hard, softc, hardc, line);
					return {
						linebreak_registry::linebreak_info(hard, hardc),
						softbreak_info(soft, softc, slb)
					};
				}
				std::pair<size_t, linebreak_type> get_beginning_char_of_visual_line(size_t line) const {
					if (_t.empty()) {
						return {_reg->get_beginning_char_of_line(line).first_char, linebreak_type::hard};
					}
					size_t softc = 0, hardc = 0;
					iterator soft;
					linebreak_registry::iterator hard;
					_find_line_ending(soft, hard, softc, hardc, line);
					if (softc > hardc) {
						return {softc, linebreak_type::soft};
					}
					return {hardc, linebreak_type::hard};
				}
				std::pair<size_t, linebreak_type> get_past_ending_char_of_visual_line(size_t line) const {
					if (_t.empty()) {
						return {_reg->get_past_ending_char_of_line(line), linebreak_type::hard};
					}
					size_t softc = 0, hardc = 0;
					iterator soft;
					linebreak_registry::iterator hard;
					_find_line_ending(soft, hard, softc, hardc, line);
					hardc += hard->nonbreak_chars;
					if (soft != _t.end()) {
						softc += soft->length;
						if (softc < hardc) {
							return {softc, linebreak_type::soft};
						}
					}
					return {hardc, linebreak_type::hard};
				}
				softbreak_info get_softbreak_before_or_at_char(size_t c) const {
					_get_softbreaks_before selector;
					size_t nc = c;
					auto it = _t.find_custom(selector, nc);
					return softbreak_info(it, c - nc, selector.num_softbreaks);
				}
				size_t get_visual_line_of_char(size_t c) const {
					return
						std::get<1>(_reg->get_line_and_column_of_char(c)) +
						get_softbreak_before_or_at_char(c).prev_softbreaks;
				}
				std::pair<size_t, size_t> get_visual_line_and_column_of_char(size_t c) const {
					auto hard = _reg->get_line_and_column_of_char(c);
					_get_softbreaks_before selector;
					auto it = _t.find_custom(selector, c);
					return {std::get<1>(hard) + selector.num_softbreaks, std::min(c, std::get<2>(hard))};
				}
				std::tuple<size_t, size_t, softbreak_info>
					get_visual_line_and_column_and_softbreak_before_or_at_char(size_t c) const {
					size_t nc = c;
					auto hard = _reg->get_line_and_column_of_char(c);
					_get_softbreaks_before selector;
					auto it = _t.find_custom(selector, nc);
					return {
						std::get<1>(hard) + selector.num_softbreaks,
						std::min(nc, std::get<2>(hard)),
						softbreak_info(it, c - nc, selector.num_softbreaks)
					};
				}

				iterator begin() const {
					return _t.begin();
				}
				iterator end() const {
					return _t.end();
				}

				void clear_softbreaks() {
					_t.clear();
				}
				// assumes that poss is sorted
				void set_softbreaks(const std::vector<size_t> &poss) {
					std::vector<node_data> vs;
					vs.reserve(poss.size());
					size_t last = 0; // no reason to insert softbreak at position 0
					for (size_t cp : poss) {
						assert_true_usage(cp > last, "softbreak list not properly sorted");
						vs.push_back(cp - last);
						last = cp;
					}
					_t = tree_type(vs);
				}

				size_t num_softbreaks() const {
					return _t.root() ? _t.root()->synth_data.total_softbreaks : 0;
				}
				size_t num_visual_lines() const {
					return _reg->num_linebreaks() + num_softbreaks() + 1;
				}
			protected:
				struct _get_softbreaks_before {
					using finder = sum_synthesizer::index_finder<node_synth_data::length_property>;
					int select_find(node_type &n, size_t &target) {
						return finder::template select_find<
							node_synth_data::softbreaks_property
						>(n, target, num_softbreaks);
					}
					size_t num_softbreaks = 0;
				};

				tree_type _t;
				const linebreak_registry *_reg = nullptr;

				// caller needs to ensure that softc == hardc == 0
				// returns the number of soft linebreaks
				size_t _find_line_ending(
					iterator &softit, linebreak_registry::iterator &hardit,
					size_t &softc, size_t &hardc, size_t line
				) const {
					assert_true_logical(softc == 0 && hardc == 0, "irresponsible caller");
					size_t cursc = 0, softl = 0, softlres = 0;
					if (_t.root()->synth_data.total_softbreaks <= line) {
						auto hres = _reg->get_beginning_char_of_line(line - _t.root()->synth_data.total_softbreaks);
						hardit = hres.entry;
						hardc = hres.first_char;
						softit = _t.end();
						softc = _t.root()->synth_data.total_length;
						softlres = _t.root()->synth_data.total_softbreaks;
					}
					for (node_type *n = _t.root(); n; ) {
						size_t sccs = cursc, scls = softl;
						if (n->left) {
							sccs += n->left->synth_data.total_length;
							scls += n->left->synth_data.total_softbreaks;
						}
						if (scls > line) {
							n = n->left;
						} else {
							auto hres = _reg->get_beginning_char_of_line(line - scls);
							if (!(
								(hardc > hres.first_char && hardc < sccs) ||
								(softc > sccs && softc < hres.first_char)
								)) {
								hardit = hres.entry;
								hardc = hres.first_char;
								softit = _t.get_const_iterator_for(n);
								softc = sccs;
								softlres = scls;
							}
							if (hres.first_char < sccs) {
								n = n->left;
							} else {
								cursc = sccs + n->value.length;
								softl = scls + 1;
								n = n->right;
							}
						}
					}
					return softlres;
				}

				// TODO find correct O(log(mn)) algorithm
				//bool _find_line_ending_pass(
				//	const node_type *&soft, const linebreak_registry::node_type *&hard,
				//	size_t &softlb, size_t &hardlb, size_t &softc, size_t &hardc, size_t line
				//) const {
				//	do {
				//		size_t sltot = softlb, hltot = hardlb, sctot = softc, hctot = hardc;
				//		if (soft) {
				//			sctot += soft->value.length;
				//			if (soft->left) {
				//				sltot += soft->left->synth_data.total_softbreaks;
				//				sctot += soft->left->synth_data.total_length;
				//			}
				//		}
				//		if (hard) {
				//			hctot += hard->value.nonbreak_chars;
				//			if (hard->left) {
				//				hltot += hard->left->synth_data.total_linebreaks;
				//				hctot += hard->left->synth_data.total_chars;
				//			}
				//		}
				//		if (sltot + hltot + 1 > line) {
				//			if (sctot < hctot) {
				//				hard = hard->left;
				//			} else {
				//				soft = soft->left;
				//			}
				//		} else if (sltot + hltot + 1 < line) {
				//			if (sctot < hctot) {
				//				softlb = sltot + 1;
				//				softc = sctot;
				//				soft = soft->right;
				//			} else {
				//				size_t hv = linebreak_registry::line_synth_data::get_node_linebreak_num::get(*hard);
				//				hardlb = hltot + hv;
				//				hardc = hctot + hv;
				//				hard = hard->right;
				//			}
				//		} else {
				//			return true;
				//		}
				//	} while (soft != nullptr && hard != nullptr);
				//	return false;
				//}
				//void _find_line_ending(
				//	const node_type *&snr, const linebreak_registry::node_type *&hnr,
				//	size_t &scr, size_t &hcr, size_t line
				//) const {
				//	const node_type *soft = _t.root(); // caller must check if soft == nullptr
				//	const linebreak_registry::node_type *hard = _reg->_t.root();
				//	size_t
				//		softlb = 0, hardlb = 0, softc = 0, hardc = 0,
				//		mindiff = std::numeric_limits<size_t>::max();
				//	while (_find_line_ending_pass(soft, hard, softlb, hardlb, softc, hardc, line)) {
				//		size_t diff = static_cast<size_t>(std::abs(static_cast<int>(softc - hardc)));
				//		if (diff < mindiff) {
				//			mindiff = diff;
				//			snr = soft;
				//			hnr = hard;
				//			scr = softc;
				//			hcr = hardc;
				//		}
				//		if (softc < hardc) {
				//			soft = soft->right;
				//			hard = hard->left;
				//		} else {
				//			soft = soft->left;
				//			hard = hard->right;
				//		}
				//		if (soft == nullptr || hard == nullptr) {
				//			break;
				//		}
				//	}
				//	if (hard != nullptr) {
				//		do {
				//		} while (hard != nullptr);
				//	} else if (soft != nullptr) {
				//		do {
				//		} while (soft != nullptr);
				//	}
				//}
			};

			struct folding_registry {
				friend class view_formatting;
			public:
				struct fold_region_data {
					fold_region_data() = default;
					fold_region_data(size_t b, size_t e, size_t bl, size_t el) :
						begin(b), end(e), begin_line(bl), end_line(el) {
					}

					size_t begin = 0, end = 0, begin_line = 0, end_line = 0;
				};
				struct fold_region_node_data {
					fold_region_node_data() = default;
					fold_region_node_data(size_t g, size_t r, size_t gl, size_t rl) :
						gap(g), range(r), gap_lines(gl), folded_lines(rl) {
					}

					size_t gap = 0, range = 0, gap_lines = 0, folded_lines = 0;
				};
				struct fold_region_synth_data {
					using node_type = binary_tree_node<fold_region_node_data, fold_region_synth_data>;
					struct get_node_span {
						inline static size_t get(const node_type &n) {
							return n.value.gap + n.value.range;
						}
					};
					struct get_node_line_span {
						inline static size_t get(const node_type &n) {
							return n.value.gap_lines + n.value.folded_lines;
						}
					};
					struct get_node_size {
						inline static size_t get(const node_type&) {
							return 1;
						}
					};

					size_t
						total_length = 0, total_folded_chars = 0,
						total_lines = 0, total_folded_lines = 0, tree_size = 0;

					using span_property = sum_synthesizer::compact_property<
						size_t, fold_region_synth_data, get_node_span, &fold_region_synth_data::total_length
					>;
					using folded_chars_property = sum_synthesizer::compact_property<
						size_t, fold_region_synth_data, synthesization_helper::field_value_property<
						size_t, fold_region_node_data, &fold_region_node_data::range
						>, &fold_region_synth_data::total_folded_chars
					>;
					using line_span_property = sum_synthesizer::compact_property<
						size_t, fold_region_synth_data, get_node_line_span, &fold_region_synth_data::total_lines
					>;
					using folded_lines_property = sum_synthesizer::compact_property<
						size_t, fold_region_synth_data, synthesization_helper::field_value_property<
						size_t, fold_region_node_data, &fold_region_node_data::folded_lines
						>, &fold_region_synth_data::total_folded_lines
					>;
					using tree_size_property = sum_synthesizer::compact_property<
						size_t, fold_region_synth_data, get_node_size, &fold_region_synth_data::tree_size
					>;
					// only used by finder (folded to unfolded)
					template <
						size_t fold_region_node_data::*Node,
						size_t fold_region_synth_data::*TreeTotal, size_t fold_region_synth_data::*TreeFolded
					> struct unfolded_property_base {
						inline static size_t get_node_value(const node_type &n) {
							return n.value.*Node;
						}
						inline static size_t get_node_synth_value(const node_type &n) {
							return get_node_value(n);
						}
						inline static size_t get_tree_synth_value(const node_type &n) {
							return n.synth_data.*TreeTotal - n.synth_data.*TreeFolded;
						}
					};
					using unfolded_length_property = unfolded_property_base<
						&fold_region_node_data::gap,
						&fold_region_synth_data::total_length, &fold_region_synth_data::total_folded_chars
					>;
					using unfolded_lines_property = unfolded_property_base<
						&fold_region_node_data::gap_lines,
						&fold_region_synth_data::total_lines, &fold_region_synth_data::total_folded_lines
					>;

					inline static void synthesize(node_type &n) {
						sum_synthesizer::synthesize<
							span_property, folded_chars_property,
							line_span_property, folded_lines_property, tree_size_property
						>(n);
					}
				};
				using tree_type = binary_tree<fold_region_node_data, fold_region_synth_data>;
				using node_type = tree_type::node;
				using iterator = tree_type::const_iterator;

				size_t folded_to_unfolded_line_number(size_t line) const {
					_folded_to_unfolded_line finder;
					_t.find_custom(finder, line);
					return finder.total + line;
				}
				size_t unfolded_to_folded_line_number(size_t line) const {
					_unfolded_to_folded_line finder;
					auto it = _t.find_custom(finder, line);
					if (it != _t.end()) {
						line = std::min(line, it->gap_lines);
					}
					return finder.total_unfolded + line;
				}
				size_t folded_to_unfolded_caret_pos(size_t pos) const {
					_folded_to_unfolded_pos finder;
					_t.find_custom(finder, pos);
					return finder.total + pos;
				}
				size_t unfolded_to_folded_caret_pos(size_t pos) const {
					_unfolded_to_folded_pos finder;
					auto it = _t.find_custom(finder, pos);
					if (it != _t.end()) {
						pos = std::min(pos, it->gap);
					}
					return finder.total_unfolded + pos;
				}

				size_t get_beginning_line_of_folded_lines(size_t line) const {
					return folded_to_unfolded_line_number(unfolded_to_folded_line_number(line));
				}
				size_t get_past_ending_line_of_folded_lines(size_t line) const {
					return folded_to_unfolded_line_number(unfolded_to_folded_line_number(line) + 1);
				}

				struct fold_region_info {
					fold_region_info() = default;
					fold_region_info(const iterator &it, size_t pc, size_t pl) :
						entry(it), prev_chars(pc), prev_lines(pl) {
					}
					iterator entry;
					size_t prev_chars = 0, prev_lines = 0;

					void move_next() {
						prev_chars += entry->gap + entry->range;
						prev_lines += entry->gap_lines + entry->folded_lines;
						++entry;
					}
					void move_prev() {
						--entry;
						prev_chars -= entry->gap + entry->range;
						prev_lines -= entry->gap_lines + entry->folded_lines;
					}
				};

				fold_region_info find_region_containing_open(size_t cp) const {
					_find_region_open finder;
					auto it = _t.find_custom(finder, cp);
					if (it != _t.end() && cp > it->gap) {
						return fold_region_info(it, finder.total_chars, finder.total_lines);
					}
					return fold_region_info(_t.end(), 0, 0);
				}
				fold_region_info find_region_containing_closed(size_t cp) const {
					_find_region_closed finder;
					auto it = _t.find_custom(finder, cp);
					if (it != _t.end() && cp >= it->gap) {
						return fold_region_info(it, finder.total_chars, finder.total_lines);
					}
					return fold_region_info(_t.end(), 0, 0);
				}
				fold_region_info find_region_containing_or_first_after_open(size_t cp) const {
					_find_region_open finder;
					auto it = _t.find_custom(finder, cp);
					return fold_region_info(it, finder.total_chars, finder.total_lines);
				}
				fold_region_info find_region_containing_or_first_before_open(size_t cp) const {
					_find_region_closed finder;
					auto it = _t.find_custom(finder, cp);
					if (it == _t.end() || cp <= it->gap) {
						if (it == _t.begin()) {
							return fold_region_info(_t.end(), 0, 0);
						}
						--it;
						finder.total_chars -= it->gap + it->range;
						finder.total_lines -= it->gap_lines + it->folded_lines;
					}
					return fold_region_info(it, finder.total_chars, finder.total_lines);
				}
				fold_region_info find_region_containing_or_first_after_closed(size_t cp) const {
					_find_region_closed finder;
					auto it = _t.find_custom(finder, cp);
					return fold_region_info(it, finder.total_chars, finder.total_lines);
				}
				fold_region_info find_region_containing_or_first_before_closed(size_t cp) const {
					_find_region_open finder;
					auto it = _t.find_custom(finder, cp);
					if (it == _t.end() || cp < it->gap) {
						if (it == _t.begin()) {
							return fold_region_info(_t.end(), 0, 0);
						}
						--it;
						finder.total_chars -= it->gap + it->range;
						finder.total_lines -= it->gap_lines + it->folded_lines;
					}
					return fold_region_info(it, finder.total_chars, finder.total_lines);
				}

				iterator add_fold_region(const fold_region_data &fr) {
					fold_region_info
						beg = find_region_containing_or_first_after_open(fr.begin),
						end = find_region_containing_or_first_before_open(fr.end);
					iterator &endit = end.entry;
					if (endit != _t.end()) {
						end.prev_chars += endit->gap + endit->range;
						end.prev_lines += endit->gap_lines + endit->folded_lines;
						++endit;
					} else {
						endit = _t.begin();
					}
					if (endit != _t.end()) {
						auto mod = _t.get_modifier(endit.get_node());
						// can overflow, but as expected
						mod->gap += end.prev_chars - fr.end;
						mod->gap_lines += end.prev_lines - fr.end_line;
					}
					_t.erase(beg.entry, endit); // TODO maybe save the regions
					return _t.insert_node_before(
						endit,
						fr.begin - beg.prev_chars, fr.end - fr.begin,
						fr.begin_line - beg.prev_lines, fr.end_line - fr.begin_line
					);
				}
				void remove_folded_region(iterator it) {
					assert_true_logical(it != _t.end(), "invalid iterator");
					iterator next = it;
					++next;
					size_t dp = it->gap + it->range, dl = it->gap_lines + it->folded_lines;
					_t.erase(it);
					if (next != _t.end()) {
						auto mod = _t.get_modifier(next.get_node());
						mod->gap += dp;
						mod->gap_lines += dl;
					}
				}
				void clear_folded_regions() {
					_t.clear();
				}

				void fixup_positions(const caret_fixup_info &edt) {
					if (_t.empty()) {
						return;
					}
					for (const modification_positions &mod : edt.mods) {
						fold_region_info pfirst = find_region_containing_or_first_after_open(mod.position);
						if (pfirst.entry == _t.end()) {
							break;
						}
						if (mod.removed_range > 0) {
							size_t endp = mod.position + mod.removed_range;
							fold_region_info plast = find_region_containing_or_first_before_open(endp);
							if (plast.entry != _t.end()) {
								size_t
									ffbeg = pfirst.prev_chars + pfirst.entry->gap,
									ffend = ffbeg + pfirst.entry->range;
								if (pfirst.entry == plast.entry && endp < ffend) {
									if (mod.position >= ffbeg) {
										_t.get_modifier(pfirst.entry.get_node())->range -= mod.removed_range;
									} else {
										auto modder = _t.get_modifier(pfirst.entry.get_node());
										modder->range = ffend - endp;
										modder->gap = mod.position - pfirst.prev_chars;
									}
								} else {
									size_t
										lfbeg = plast.prev_chars + plast.entry->gap,
										lfend = lfbeg + plast.entry->range,
										addlen = 0;
									if (endp < lfend) {
										_t.get_modifier(plast.entry.get_node())->range -= endp - lfbeg;
									} else {
										plast.move_next();
										addlen -= endp - plast.prev_chars;
									}
									// the stats in plast becomes invalid since here
									if (mod.position > ffbeg) {
										_t.get_modifier(pfirst.entry.get_node())->range -= ffend - mod.position;
										pfirst.move_next();
									} else {
										addlen += mod.position - pfirst.prev_chars;
									}
									_t.erase(pfirst.entry, plast.entry);
									if (plast.entry != _t.end()) {
										_t.get_modifier(plast.entry.get_node())->gap += addlen;
									}
								}
							} else {
								_t.begin().get_modifier()->gap -= mod.removed_range;
							}
						}
						if (mod.added_range > 0) {
							if (mod.position <= pfirst.prev_chars + pfirst.entry->gap) {
								_t.get_modifier(pfirst.entry.get_node())->gap += mod.added_range;
							} else {
								_t.get_modifier(pfirst.entry.get_node())->range += mod.added_range;
							}
						}
						// TODO also fixup lines
					}
				}

				iterator begin() const {
					return _t.begin();
				}
				iterator end() const {
					return _t.end();
				}

				size_t folded_linebreaks() const {
					return _t.root() ? _t.root()->synth_data.total_folded_lines : 0;
				}
				size_t folded_region_count() const {
					return _t.root() ? _t.root()->synth_data.tree_size : 0;
				}
				const tree_type &get_raw() const {
					return _t;
				}
			protected:
				template <typename Prop, typename SynProp> struct _unfolded_to_folded {
					using finder = sum_synthesizer::index_finder<Prop>;
					int select_find(const node_type &n, size_t &v) {
						return finder::template select_find<SynProp>(n, v, total_unfolded);
					}
					size_t total_unfolded = 0;
				};
				using _unfolded_to_folded_pos = _unfolded_to_folded<
					fold_region_synth_data::span_property, fold_region_synth_data::unfolded_length_property
				>;
				using _unfolded_to_folded_line = _unfolded_to_folded<
					fold_region_synth_data::line_span_property, fold_region_synth_data::unfolded_lines_property
				>;

				template <typename Prop, typename SynProp> struct _folded_to_unfolded {
					using finder = sum_synthesizer::index_finder<Prop, false, std::less_equal<size_t>>;
					int select_find(const node_type &n, size_t &v) {
						return finder::template select_find<SynProp>(n, v, total);
					}
					size_t total = 0;
				};
				using _folded_to_unfolded_pos = _folded_to_unfolded<
					fold_region_synth_data::unfolded_length_property, fold_region_synth_data::span_property
				>;
				using _folded_to_unfolded_line = _folded_to_unfolded<
					fold_region_synth_data::unfolded_lines_property, fold_region_synth_data::line_span_property
				>;

				template <typename Cmp> struct _find_region {
					using finder = sum_synthesizer::index_finder<
						fold_region_synth_data::span_property, false, Cmp
					>;
					int select_find(const node_type &n, size_t &v) {
						return finder::template select_find<
							fold_region_synth_data::span_property, fold_region_synth_data::line_span_property
						>(n, v, total_chars, total_lines);
					}
					size_t total_chars = 0, total_lines = 0;
				};
				using _find_region_closed = _find_region<std::less_equal<size_t>>;
				using _find_region_open = _find_region<std::less<size_t>>;

				tree_type _t;
				soft_linebreak_registry *_lbr = nullptr;
			};

			class view_formatting {
			public:
				using fold_region = std::pair<size_t, size_t>;

				view_formatting() = default;
				view_formatting(const linebreak_registry &reg) : _lbr(reg) {
				}

				void set_softbreaks(const std::vector<size_t> &breaks) {
					_lbr.set_softbreaks(breaks);
					recalc_foldreg_lines();
				}
				void clear_softbreaks() {
					_lbr.clear_softbreaks();
					recalc_foldreg_lines();
				}

				folding_registry::iterator add_folded_region(const fold_region &rgn) {
					assert_true_usage(rgn.second > rgn.first, "invalid fold region");
					return _fr.add_fold_region(folding_registry::fold_region_data(
						rgn.first, rgn.second,
						_lbr.get_visual_line_of_char(rgn.first), _lbr.get_visual_line_of_char(rgn.second)
					));
				}
				void remove_folded_region(folding_registry::iterator it) {
					_fr.remove_folded_region(it);
				}
				void clear_folded_regions() {
					_fr.clear_folded_regions();
				}

				void fixup_folding_positions(const caret_fixup_info &info) {
					_fr.fixup_positions(info);
				}
				void recalc_foldreg_lines() {
					size_t plines = 0, totc = 0;
					for (auto i = _fr._t.begin(); i != _fr._t.end(); ++i) {
						totc += i->gap;
						size_t bl = _lbr.get_visual_line_of_char(totc);
						i.get_value_rawmod().gap_lines = bl - plines;
						totc += i->range;
						size_t el = _lbr.get_visual_line_of_char(totc);
						i.get_value_rawmod().folded_lines = el - bl;
						plines = el;
					}
					_fr._t.refresh_tree_synthesized_result();
				}

				const soft_linebreak_registry &get_linebreaks() const {
					return _lbr;
				}
				const folding_registry &get_folding() const {
					return _fr;
				}
			protected:
				soft_linebreak_registry _lbr;
				folding_registry _fr;
			};

			inline view_formatting text_context::create_view_formatting() {
				return view_formatting(_lbr);
			}
		}
	}
}
