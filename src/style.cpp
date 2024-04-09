#include "html.h"
#include "style.h"
#include "css_parser.h"
#include "internal.h"
//#include <iso646.h>

// All functions here assume that whitespace have been removed.

namespace litehtml
{

std::map<string_id, string> style::m_valid_values =
{
	{ _display_, style_display_strings },
	{ _visibility_, visibility_strings },
	{ _position_, element_position_strings },
	{ _float_, element_float_strings },
	{ _clear_, element_clear_strings },
	{ _overflow_, overflow_strings },
	{ _box_sizing_, box_sizing_strings },

	{ _text_align_, text_align_strings },
	{ _vertical_align_, vertical_align_strings },
	{ _text_transform_, text_transform_strings },
	{ _white_space_, white_space_strings },

	{ _font_style_, font_style_strings },
	{ _font_variant_, font_variant_strings },
	{ _font_weight_, font_weight_strings },

	{ _list_style_type_, list_style_type_strings },
	{ _list_style_position_, list_style_position_strings },

	{ _border_left_style_, border_style_strings },
	{ _border_right_style_, border_style_strings },
	{ _border_top_style_, border_style_strings },
	{ _border_bottom_style_, border_style_strings },
	{ _border_collapse_, border_collapse_strings },

	// these 4 properties are comma-separated lists of keywords, see parse_keyword_comma_list
	{ _background_attachment_, background_attachment_strings },
	{ _background_repeat_, background_repeat_strings },
	{ _background_clip_, background_box_strings },
	{ _background_origin_, background_box_strings },

	{ _flex_direction_, flex_direction_strings },
	{ _flex_wrap_, flex_wrap_strings },
	{ _justify_content_, flex_justify_content_strings },
	{ _align_content_, flex_align_content_strings },
	{ _align_items_, flex_align_items_strings },
	{ _align_self_, flex_align_items_strings },

	{ _caption_side_, caption_side_strings },
};

void style::add(const string& txt, const string& baseurl, document_container* container)
{
	auto tokens = normalize(txt, f_componentize);
	add(tokens, baseurl, container);
}

void style::add(const css_token_vector& tokens, const string& baseurl, document_container* container)
{
	raw_declaration::vector decls;
	raw_rule::vector rules;
	css_parser(tokens).consume_style_block_contents(decls, rules);
	if (!rules.empty())
		css_parse_error("rule inside a style block");
	if (decls.empty())
		return;

	// Parse each declaration
	for (auto& decl : decls)
	{
		remove_whitespace(decl.value);
		// Note: decl.value is already componentized, see consume_qualified_rule and consume_style_block_contents.
		// Note: decl.value may be empty.
		string name = decl.name.substr(0, 2) == "--" ? decl.name : lowcase(decl.name);
		add_property(_id(name), decl.value, baseurl, decl.important, container);
	}
}

bool has_var(const css_token_vector& tokens)
{
	for (auto& tok : tokens)
	{
		if (tok.type == CV_FUNCTION && lowcase(tok.name) == "var")
			return true;
		if (tok.is_component_value() && has_var(tok.value))
			return true;
	}
	return false;
}

bool is_shorthand_property(string_id /*name*/)
{
	return false;
}

vector<string_id> get_atomic_properties(string_id /*name*/)
{
	return {};
}

void style::add_length_property(string_id name, css_token val, string keywords, int options, bool important)
{
	css_length length;
	if (length.from_token(val, options, keywords))
		add_parsed_property(name, property_value(length, important));
}

//string parse_ident(const css_token_vector& tokens)
//{
//	if (tokens.size() == 1 && tokens[0].type == IDENT)
//		return lowcase(tokens[0].name);
//	return "";
//}

// `value` is a list of component values with all whitespace tokens removed, including those inside component values
void style::add_property(string_id name, const css_token_vector& value, const string& baseurl, bool important, document_container* container)
{
	// Note: empty value is a valid value of a custom property.
	if (value.empty() && _s(name).substr(0, 2) != "--")
		return;
	
	if (has_var(value))
		return add_parsed_property(name, property_value(value, important, prop_type_var));
	
	// valid only if value contains a single token
	css_token val = value.size() == 1 ? value[0] : css_token();
	// nonempty if value is a single identifier
	string ident = val.ident();

	if (ident == "inherit")
	{
		if (is_shorthand_property(name))
		{
			for (auto atomic : get_atomic_properties(name))
				add_parsed_property(atomic, property_value(important, prop_type_inherit));
		}
		else
			add_parsed_property(name, property_value(important, prop_type_inherit));
		return;
	}

	int idx[4];
	web_color clr[4];
	css_length len[4];

	switch (name)
	{
	//  =============================  SINGLE KEYWORD  =============================

	case _display_:
	case _visibility_:
	case _position_:
	case _float_:
	case _clear_:
	case _box_sizing_:
	case _overflow_:

	case _text_align_:
	case _vertical_align_:
	case _text_transform_:
	case _white_space_:

	case _font_style_:
	case _font_variant_:
	case _font_weight_:

	case _list_style_type_:
	case _list_style_position_:

	case _border_top_style_:
	case _border_bottom_style_:
	case _border_left_style_:
	case _border_right_style_:
	case _border_collapse_:

	case _flex_direction_:
	case _flex_wrap_:
	case _justify_content_:
	case _align_content_:

	case _caption_side_:

		if (int index = value_index(ident, m_valid_values[name]); index >= 0)
			add_parsed_property(name, property_value(index, important));
		break;

	//  =============================  LENGTH  =============================

	// auto | <integer>  https://developer.mozilla.org/en-US/docs/Web/CSS/z-index#formal_syntax
	case _z_index_:
		return add_length_property(name, val, "auto", f_integer, important);

	// <length-percentage>  https://developer.mozilla.org/en-US/docs/Web/CSS/text-indent#formal_syntax
	case _text_indent_:
		return add_length_property(name, val, "", f_length_percentage, important);
	
	// <length-percentage [0,∞]>  https://developer.mozilla.org/en-US/docs/Web/CSS/padding-left
	case _padding_left_:
	case _padding_right_:
	case _padding_top_:
	case _padding_bottom_:
		return add_length_property(name, val, "", f_length_percentage|f_positive, important);

	// auto | <length-percentage>  https://developer.mozilla.org/en-US/docs/Web/CSS/left
	case _left_:
	case _right_:
	case _top_:
	case _bottom_:
	case _margin_left_:
	case _margin_right_:
	case _margin_top_:
	case _margin_bottom_:
		return add_length_property(name, val, "auto", f_length_percentage, important);
	
	// auto | min-content | max-content | fit-content | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/width#formal_syntax
	case _width_:
	case _height_:
	case _min_width_:
	case _min_height_:
		return add_length_property(name, val, "auto", f_length_percentage|f_positive, important);

	// none | min-content | max-content | fit-content | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/max-width#formal_syntax
	case _max_width_:
	case _max_height_:
		return add_length_property(name, val, "none", f_length_percentage|f_positive, important);

	//  normal | <number [0,∞]> | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/line-height#formal_syntax
	case _line_height_:
		return add_length_property(name, val, "normal", f_number|f_length_percentage|f_positive, important);

	// font-size = <absolute-size> | <relative-size> | <length-percentage [0,∞]>   https://developer.mozilla.org/en-US/docs/Web/CSS/font-size#formal_syntax
	case _font_size_:
		return add_length_property(name, val, font_size_strings, f_length_percentage|f_positive, important);

	case _margin_:
		if (int n = parse_1234_lengths(value, len, f_length_percentage, "auto"))
			add_four_properties(_margin_top_, len, n, important);
		break;

	case _padding_:
		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive))
			add_four_properties(_padding_top_, len, n, important);
		break;
		
	//  =============================  COLOR  =============================

	case _color_:
	case _background_color_:
	case _border_top_color_:
	case _border_bottom_color_:
	case _border_left_color_:
	case _border_right_color_:
		if (clr->from_token(val, container))
			add_parsed_property(name, property_value(*clr, important));
		break;

	//  =============================  BACKGROUND  =============================

	case _background_:
		parse_background(value, baseurl, important, container);
		break;

	case _background_image_:
		parse_background_image(value, baseurl, important, container);
		break;

	case _background_position_:
		parse_background_position(value, important);
		break;

	case _background_size_:
		parse_background_size(value, important);
		break;

	case _background_repeat_:
	case _background_attachment_:
	case _background_origin_:
	case _background_clip_:
		parse_keyword_comma_list(name, value, important);
		break;

	//  =============================  BORDER  =============================

	case _border_:
		parse_border(value, important, container);
		break;

	case _border_left_:
	case _border_right_:
	case _border_top_:
	case _border_bottom_:
		parse_border_side(name, value, important, container);
		break;

	case _border_width_:
		if (int n = parse_1234_values(value, len, parse_border_width))
			add_four_properties(_border_top_width_, len, n, important);
		break;
	case _border_style_:
		if (int n = parse_1234_values(value, idx, parse_keyword, (string)border_style_strings, 0))
			add_four_properties(_border_top_style_, idx, n, important);
		break;
	case _border_color_:
		if (int n = parse_1234_values(value, clr, parse_color, container))
			add_four_properties(_border_top_color_, clr, n, important);
		break;

	case _border_top_width_:
	case _border_bottom_width_:
	case _border_left_width_:
	case _border_right_width_:
		if (parse_border_width(val, *len))
			add_parsed_property(name, property_value(*len, important));
		break;

	// border-bottom-left-radius = <length-percentage [0,∞]>{1,2}  https://developer.mozilla.org/en-US/docs/Web/CSS/border-bottom-left-radius
	case _border_bottom_left_radius_:
	case _border_bottom_right_radius_:
	case _border_top_right_radius_:
	case _border_top_left_radius_:
		if (parse_two_lengths(value, len, f_length_percentage | f_positive))
		{
			add_parsed_property(_id(_s(name) + "-x"), property_value(len[0], important));
			add_parsed_property(_id(_s(name) + "-y"), property_value(len[1], important));
		}
		break;

	case _border_radius_x_:
	case _border_radius_y_:
	{
		string_id top_left = name == _border_radius_x_ ? 
			_border_top_left_radius_x_ : 
			_border_top_left_radius_y_;

		if (int n = parse_1234_lengths(value, len, f_length_percentage | f_positive))
			add_four_properties(top_left, len, n, important);
		break;
	}

	case _border_radius_:
		parse_border_radius(value, important);
		break;

	// border-spacing = <length>{1,2}  https://developer.mozilla.org/en-US/docs/Web/CSS/border-spacing
	case _border_spacing_:
		if (parse_two_lengths(value, len, f_length))
		{
			add_parsed_property(__litehtml_border_spacing_x_, property_value(len[0], important));
			add_parsed_property(__litehtml_border_spacing_y_, property_value(len[1], important));
		}
		break;

	//  =============================  LIST  =============================

	case _list_style_image_:
		if (string url; parse_list_style_image(val, url))
		{
			add_parsed_property(_list_style_image_,			property_value(url,     important));
			add_parsed_property(_list_style_image_baseurl_, property_value(baseurl, important));
		}
		break;

	case _list_style_:
		parse_list_style(value, baseurl, important);
		break;

	//  =============================  FONT  =============================

	case _font_:
		parse_font(value, important);
		break;

	//  =============================  FLEX  =============================

	case _flex_:
		parse_flex(value, important);
		break;

	case _flex_grow_:   // <number [0,∞]>
	case _flex_shrink_:
		if (val.type == NUMBER && val.n.number >= 0)
			add_parsed_property(name, property_value(val.n.number, important));
		break;

	case _flex_basis_:
		add_length_property(name, val, flex_basis_strings, f_length_percentage|f_positive, important);
		break;

	case _flex_flow_:
		parse_flex_flow(value, important);
		break;

	case _align_items_:
	case _align_self_:
		parse_align_self(name, value, important);
		break;

	case _order_: // <integer>
		if (val.type == NUMBER && val.n.number_type == css_number_integer)
			add_parsed_property(name, property_value((int)val.n.number, important));
		break;

	//  =============================  COUNTER, CONTENT  =============================

	case _counter_increment_:
	case _counter_reset_:
	{
		// TODO: parse it properly here
		string_vector strings;
		for (const auto& tok : value) strings.push_back(tok.get_repr(true));
		add_parsed_property(name, property_value(strings, important));
		break;
	}

	case _content_:
	{
		// TODO: parse it properly here
		string str = get_repr(value, 0, -1, true);
		add_parsed_property(name, property_value(str, important));
		break;
	}

	//  =============================  CUSTOM PROPERTY  =============================
	
	// https://drafts.csswg.org/css-variables-2/#defining-variables
	default:
		if (_s(name).substr(0, 2) == "--" && _s(name).size() >= 3 && 
			(value.empty() || is_declaration_value(value)))
			add_parsed_property(name, property_value(value, important, prop_type_custom));
	}
}

void style::add_property(string_id name, const string& value, const string& baseurl, bool important, document_container* container)
{
	auto tokens = normalize(value, f_componentize | f_remove_whitespace);
	add_property(name, tokens, baseurl, important, container);
}

// This should be the same as parse_bg_image, but list-style-image is currently a string (not an image).
bool style::parse_list_style_image(const css_token& tok, string& url)
{
	if (tok.ident() == "none")
	{
		url = "";
		return true;
	}

	return parse_url(tok, url);
}

// https://drafts.csswg.org/css-lists/#list-style-property
// <'list-style-position'> || <'list-style-image'> || <'list-style-type'>
void style::parse_list_style(const css_token_vector& tokens, string baseurl, bool important)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/list-style#formal_definition
	int type     = list_style_type_disc;
	int position = list_style_position_outside;
	string image = ""; // none

	bool type_found = false;
	bool position_found = false;
	bool image_found = false;

	for (const auto& token : tokens)
	{
		if (!type_found && parse_keyword(token, type, list_style_type_strings))
			type_found = true;
		else if (!position_found && parse_keyword(token, position, list_style_position_strings))
			position_found = true;
		else if (!image_found && parse_list_style_image(token, image))
			image_found = true;
		else
			return;
	}

	add_parsed_property(_list_style_type_,			property_value(type,	 important));
	add_parsed_property(_list_style_position_,		property_value(position, important));
	add_parsed_property(_list_style_image_,			property_value(image,	 important));
	add_parsed_property(_list_style_image_baseurl_,	property_value(baseurl,	 important));
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/border-radius
// border-radius = <length-percentage [0,∞]>{1,4} [ / <length-percentage [0,∞]>{1,4} ]?  
void style::parse_border_radius(const css_token_vector& tokens, bool important)
{
	int i;
	for (i = 0; i < tokens.size() && tokens[i].ch != '/'; i++) {}
		
	if (i == tokens.size()) // no '/'
	{
		css_length len[4];
		if (int n = parse_1234_lengths(tokens, len, f_length_percentage | f_positive))
		{
			add_four_properties(_border_top_left_radius_x_, len, n, important);
			add_four_properties(_border_top_left_radius_y_, len, n, important);
		}
	}
	else
	{
		auto raduis_x = slice(tokens, 0, i);
		auto raduis_y = slice(tokens, i + 1);
		
		css_length rx[4], ry[4];
		int n = parse_1234_lengths(raduis_x, rx, f_length_percentage | f_positive);
		int m = parse_1234_lengths(raduis_y, ry, f_length_percentage | f_positive);
		
		if (n && m)
		{
			add_four_properties(_border_top_left_radius_x_, rx, n, important);
			add_four_properties(_border_top_left_radius_y_, ry, m, important);
		}
	}
}

bool parse_border_width(const css_token& token, css_length& w)
{
	css_length width;
	if (!width.from_token(token, f_length | f_positive, border_width_strings))
		return false;
	
	if (width.is_predefined())
		width.set_value(border_width_values[width.predef()], css_units_px);
	
	w = width;
	return true;
}

// https://drafts.csswg.org/css-backgrounds/#propdef-border
// <line-width> || <line-style> || <color>
// <line-width> = <length [0,∞]> | thin | medium | thick
// <line-style> = none | hidden | dotted | dashed | solid | double | groove | ridge | inset | outset
bool parse_border_helper(const css_token_vector& tokens, document_container* container,
	css_length& width, border_style& style, web_color& color)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/border#formal_definition
	css_length   _width = css_length::predef_value(border_width_medium);
	border_style _style = border_style_none;
	web_color    _color = web_color::current_color;

	bool width_found = false;
	bool style_found = false;
	bool color_found = false;

	for (const auto& token : tokens)
	{
		if (!width_found && parse_border_width(token, _width))
			width_found = true;
		else if (!style_found && parse_keyword(token, (int&)_style, border_style_strings))
			style_found = true;
		else if (!color_found && _color.from_token(token, container))
			color_found = true;
		else
			return false;
	}

	width = _width;
	style = _style;
	color = _color;
	return true;
}

void style::parse_border(const css_token_vector& tokens, bool important, document_container* container)
{
	css_length width;
	border_style style;
	web_color color;
	
	if (!parse_border_helper(tokens, container, width, style, color))
		return;

	for (auto name : {_border_left_width_, _border_right_width_, _border_top_width_, _border_bottom_width_})
		add_parsed_property(name, property_value(width, important));

	for (auto name : {_border_left_style_, _border_right_style_, _border_top_style_, _border_bottom_style_})
		add_parsed_property(name, property_value(style, important));

	for (auto name : {_border_left_color_, _border_right_color_, _border_top_color_, _border_bottom_color_})
		add_parsed_property(name, property_value(color, important));
}

// https://drafts.csswg.org/css-backgrounds/#border-shorthands
// border-top, border-right, border-bottom, border-left
void style::parse_border_side(string_id name, const css_token_vector& tokens, bool important, document_container* container)
{
	css_length width;
	border_style style;
	web_color color;

	if (!parse_border_helper(tokens, container, width, style, color))
		return;

	add_parsed_property(_id(_s(name) + "-width"), property_value(width, important));
	add_parsed_property(_id(_s(name) + "-style"), property_value(style, important));
	add_parsed_property(_id(_s(name) + "-color"), property_value(color, important));
}


bool parse_color(const css_token& tok, web_color& color, document_container* container)
{
	return color.from_token(tok, container);
}

bool parse_length(const css_token& tok, css_length& length, int options, string keywords)
{
	return length.from_token(tok, options, keywords);
}

// parses 1 or 2 lengths, but always returns 2 lengths
bool parse_two_lengths(const css_token_vector& tokens, css_length len[2], int options)
{
	auto n = tokens.size();
	if (n != 1 && n != 2) return false;

	css_length a, b;
	if (!a.from_token(tokens[0], options)) return false;
	if (n == 1) b = a;

	if (n == 2 && !b.from_token(tokens[1], options)) return false;

	len[0] = a;
	len[1] = b;
	return true;
}

// parses 1,2,3 or 4 tokens, returns number of tokens parsed or 0 if error
template<class T, class... Args>
int parse_1234_values(const css_token_vector& tokens, T result[4], bool (*parse)(const css_token&, T&, Args...), Args... args)
{
	if (tokens.size() > 4)
		return 0;

	for (size_t i = 0; i < tokens.size(); i++)
	{
		if (!parse(tokens[i], result[i], args...))
			return 0;
	}
	return (int)tokens.size();
}

int parse_1234_lengths(const css_token_vector& tokens, css_length len[4], int options, string keywords)
{
	return parse_1234_values(tokens, len, parse_length, options, keywords);
}

// This function implements the logic of the kind "if two values are specified, the first one applies to
// top and bottom, the second one to left and right". Works in conjunction with parse_1234_values.
template<class T> 
void style::add_four_properties(string_id top_name, T val[4], int n, bool important)
{
	// These always go in trbl order, see comment for "CSS property names" in string_id.
	string_id top    = top_name; // top-left for corners
	string_id right  = string_id(top_name + 1);
	string_id bottom = string_id(top_name + 2);
	string_id left   = string_id(top_name + 3);

	// n      4 3 2 1
	// top    0 0 0 0  0
	// right  1 1 1 0  n>1
	// bottom 2 2 0 0  n/3*2
	// left   3 1 1 0  n/2+n/4

	add_parsed_property(top,    property_value(val[0], important));
	add_parsed_property(right,  property_value(val[n>1], important));
	add_parsed_property(bottom, property_value(val[n/3*2], important));
	add_parsed_property(left,   property_value(val[n/2+n/4], important));
}

void style::parse_background(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	web_color color;
	std::vector<image> images; 
	length_vector x_positions, y_positions;
	size_vector sizes;
	int_vector repeats, attachments, origins, clips;

	for (size_t i = 0; i < layers.size(); i++)
	{
		background bg;
		if (!parse_bg_layer(layers[i], container, bg, i == layers.size() - 1))
			return;
		
		color = bg.m_color;
		images.push_back(bg.m_image[0]);
		x_positions.push_back(bg.m_position_x[0]);
		y_positions.push_back(bg.m_position_y[0]);
		sizes.push_back(bg.m_size[0]);
		repeats.push_back(bg.m_repeat[0]);
		attachments.push_back(bg.m_attachment[0]);
		origins.push_back(bg.m_origin[0]);
		clips.push_back(bg.m_clip[0]);
	}

	add_parsed_property(_background_color_,			property_value(color,		important));
	add_parsed_property(_background_image_,			property_value(images,		important));
	add_parsed_property(_background_image_baseurl_, property_value(baseurl,		important));
	add_parsed_property(_background_position_x_,	property_value(x_positions, important));
	add_parsed_property(_background_position_y_,	property_value(y_positions, important));
	add_parsed_property(_background_size_,			property_value(sizes,		important));
	add_parsed_property(_background_repeat_,		property_value(repeats,		important));
	add_parsed_property(_background_attachment_,	property_value(attachments, important));
	add_parsed_property(_background_origin_,		property_value(origins,		important));
	add_parsed_property(_background_clip_,			property_value(clips,		important));
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-layer
// <bg-layer> = <bg-image> || <bg-position> [ / <bg-size> ]? || <repeat-style> || <attachment> || <visual-box> || <visual-box>
// <final-bg-layer> =  <bg-layer> || <'background-color'>
bool style::parse_bg_layer(const css_token_vector& tokens, document_container* container, background& bg, bool final_layer)
{
	bg.m_color = web_color::transparent;
	bg.m_image = {{}};
	bg.m_position_x = { css_length(0, css_units_percentage) };
	bg.m_position_y = { css_length(0, css_units_percentage) };
	bg.m_size = { css_size(css_length::predef_value(background_size_auto), css_length::predef_value(background_size_auto)) };
	bg.m_repeat = { background_repeat_repeat };
	bg.m_attachment = { background_attachment_scroll };
	bg.m_origin = { background_box_padding };
	bg.m_clip = { background_box_border };

	bool color_found = false;
	bool image_found = false;
	bool position_found = false;
	bool repeat_found = false;
	bool attachment_found = false;
	bool origin_found = false;
	bool clip_found = false;

	for (int i = 0; i < tokens.size(); i++)
	{
		if (!color_found && final_layer && bg.m_color.from_token(tokens[i], container))
			color_found = true;
		else if (!image_found && parse_bg_image(tokens[i], bg.m_image[0], container))
			image_found = true;
		else if (!position_found && parse_bg_position_size(tokens, i, bg.m_position_x[0], bg.m_position_y[0], bg.m_size[0]))
			position_found = true, i--;
		// Note: double keyword <repeat-style> values are not supported yet.
		else if (!repeat_found && parse_keyword(tokens[i], bg.m_repeat[0], background_repeat_strings))
			repeat_found = true;
		else if (!attachment_found && parse_keyword(tokens[i], bg.m_attachment[0], background_attachment_strings))
			attachment_found = true;
		// If one <visual-box> value is present then it sets both background-origin and background-clip to that value. 
		// If two values are present, then the first sets background-origin and the second background-clip.
		else if (!origin_found && parse_keyword(tokens[i], bg.m_origin[0], background_box_strings))
			origin_found = true, bg.m_clip[0] = bg.m_origin[0];
		else if (!clip_found && parse_keyword(tokens[i], bg.m_clip[0], background_box_strings))
			clip_found = true;
		else
			return false;
	}
	return true;
}

template<typename Enum>
bool parse_keyword(const css_token& tok, Enum& val, string keywords, int first_keyword_value)
{
	int idx = value_index(tok.ident(), keywords);
	if (idx == -1) return false;
	val = (Enum)(first_keyword_value + idx);
	return true;
}

//template<typename Enum>
//bool parse_keyword(const css_token_vector& tokens, int& index, Enum& val, string keywords)
//{
//	if (parse_keyword(at(tokens, index), (int&)val, keywords))
//	{
//		index++;
//		return true;
//	}
//	return false;
//}

// <bg-position> [ / <bg-size> ]?
bool parse_bg_position_size(const css_token_vector& tokens, int& index, css_length& x, css_length& y, css_size& size)
{
	if (!parse_bg_position(tokens, index, x, y, true))
		return false;

	if (at(tokens, index).ch != '/')
		return true; // no [ / <bg-size> ]
	
	if (!parse_bg_size(tokens, ++index, size))
	{
		index--; // restore index to point to '/'
		return false; // has '/', but <bg-size> failed to parse
	}

	return true; // both <bg-position> and <bg-size> parsed successfully
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-size
// <bg-size> = [ <length-percentage [0,∞]> | auto ]{1,2} | cover | contain
bool parse_bg_size(const css_token_vector& tokens, int& index, css_size& size)
{
	css_length a, b;

	if (!a.from_token(at(tokens, index), f_length_percentage | f_positive, background_size_strings))
		return false;
	
	// cover | contain
	if (a.is_predefined() && a.predef() != background_size_auto)
	{
		size.width = size.height = a;
		index++;
		return true;
	}

	if (b.from_token(at(tokens, index + 1), f_length_percentage | f_positive, "auto"))
		index += 2;
	else
	{
		b.predef(background_size_auto); // If only one value is given the second is assumed to be auto.
		index++;
	}

	size.width = a;
	size.height = b;
	return true;
}

bool is_one_of_predef(const css_length& x, int idx1, int idx2)
{
	return x.is_predefined() && is_one_of(x.predef(), idx1, idx2);
}

// https://drafts.csswg.org/css-backgrounds/#typedef-bg-position
// https://www.w3.org/TR/css-values-4/#position
// <bg-position> = [ left | center | right | top | bottom | <length-percentage> ] |
//   [ left | center | right | <length-percentage> ]  [ top | center | bottom | <length-percentage> ]
// Side-relative values (like bottom 10%) are not supported, so only 1 or 2 values are parsed (not 3 or 4).
bool parse_bg_position(const css_token_vector& tokens, int& index, css_length& x, css_length& y, bool convert_keywords_to_percents)
{
	enum {
		left   = background_position_left,
		right  = background_position_right,
		top    = background_position_top,
		bottom = background_position_bottom,
		center = background_position_center
	};
	
	css_length a, b;

	if (!a.from_token(at(tokens, index), f_length_percentage, background_position_strings))
		return false;

	if (!b.from_token(at(tokens, index + 1), f_length_percentage, background_position_strings))
	{
		// If only one value is specified, the second value is assumed to be center.
		b.predef(center);

		// fix wrong order
		if (is_one_of_predef(a, top, bottom))
			swap(a, b);

		index++;
	}
	else // two values
	{
		// try to fix wrong order
		// A pair of keywords can be reordered, while a combination of keyword and length or percentage cannot.
		if (is_one_of_predef(a, top, bottom) && b.is_predefined())
			swap(a, b);
		
		// check for wrong order
		if (is_one_of_predef(a, top, bottom) || is_one_of_predef(b, left, right))
			return false;

		index += 2;
	}

	if (convert_keywords_to_percents)
	{
		if (a.is_predefined())
			a.set_value(background_position_percentages[a.predef()], css_units_percentage);
		if (b.is_predefined())
			b.set_value(background_position_percentages[b.predef()], css_units_percentage);
	}
	
	x = a;
	y = b;
	return true;
}

void style::parse_background_image(const css_token_vector& tokens, const string& baseurl, bool important, document_container* container)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;
	
	std::vector<image> images;

	for (const auto& layer : layers)
	{
		image image;
		if (layer.size() != 1)
			return;
		if (!parse_bg_image(layer[0], image, container))
			return;
		images.push_back(image);
	}

	add_parsed_property(_background_image_,			property_value(images, important));
	add_parsed_property(_background_image_baseurl_, property_value(baseurl, important));
}

// <bg-image> = <image> | none
// <image> = <url> | <gradient>
bool parse_bg_image(const css_token& tok, image& bg_image, document_container* container)
{
	if (tok.ident() == "none")
	{
		bg_image.type = image::type_none;
		return true;
	}

	string url;
	if (parse_url(tok, url))
	{
		bg_image.type = image::type_url;
		bg_image.url = url;
		return true;
	}

	if (parse_gradient(tok, bg_image.gradient, container))
	{
		bg_image.type = image::type_gradient;
		return true;
	}

	return false;
}

// https://drafts.csswg.org/css-values-4/#urls
bool parse_url(const css_token& tok, string& url)
{
	if (tok.type == URL) // legacy syntax without quotes: url(x.com)
	{
		url = trim(tok.str);
		return true;
	}

	if (tok.type == CV_FUNCTION && is_one_of(lowcase(tok.name), "url", "src") &&
		// note: relying on whitespace having been removed from tok.value
		tok.value.size() == 1 && tok.value[0].type == STRING)
	{
		url = trim(tok.value[0].str);
		return true;
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////
// These combinators are used rarely because the code is usually shorter without them.

using parse_fn = std::function<bool(const css_token_vector& tokens, int& index)>;

// a?
parse_fn opt(parse_fn a)
{
	return [=](auto&... x)
	{
		a(x...);
		return true;
	};
}

// a b
parse_fn seq(parse_fn a, parse_fn b)
{
	return [=](auto& t, auto& i)
	{
		auto save = i;
		bool result = a(t, i) && b(t, i);
		if (!result) i = save; // backtrack
		return result;
	};
}

// a | b
//parse_fn _or(parse_fn a, parse_fn b)
//{
//	return [=](auto& t, auto& i)
//	{
//		return fase;
//	};
//}

// Not overloading operator|| because it is easier to get a bug: a || b || c does the wrong thing, 
// see the note at https://www.w3.org/TR/css-values-4/#component-combinators.
// a || b
parse_fn oror(parse_fn a, parse_fn b)
{
	return [=](auto&... x)
	{
		if (a(x...))
		{
			b(x...);
			return true;
		}
		else if (b(x...))
		{
			a(x...);
			return true;
		}
		return false;
	};
}

parse_fn operator""_x(const char* str, size_t len)
{
	return [=](const css_token_vector& tokens, int& index)
	{
		if (at(tokens, index).ident() == string(str, len))
		{
			index++;
			return true;
		}
		return false;
	};
}

bool end(const css_token_vector& tokens, int index)
{
	return index == tokens.size();
}

////////////////////////////////////////////////////////////////////////////////////////////

// https://drafts.csswg.org/css-images-4/#gradients
// 
// <gradient> =
//   <linear-gradient()> | <repeating-linear-gradient()> |
//   <radial-gradient()> | <repeating-radial-gradient()> |
//   <conic-gradient()>  | <repeating-conic-gradient()>
// 
bool parse_gradient(const css_token& token, gradient& result, document_container* container)
{
	if (token.type != CV_FUNCTION)
		return false;

	auto type = _id(lowcase(token.name));

	if (!is_one_of(type,
		_linear_gradient_, _repeating_linear_gradient_,
		_radial_gradient_, _repeating_radial_gradient_,
		_conic_gradient_, _repeating_conic_gradient_))
		return false;

	gradient grad(type);

	if (!grad.is_linear()) {
		// radial and conic position defaults to 'center'  
		// https://drafts.csswg.org/css-images-3/#valdef-radial-gradient-position
		// https://drafts.csswg.org/css-images-4/#valdef-conic-gradient-position
		grad.m_side = gradient_side_x_center | gradient_side_y_center;
	}
	
	auto list = parse_comma_separated_list(token.value);
	if (list.empty()) return false;

	bool ok;
	
	if (grad.is_linear())
		ok = parse_linear_gradient_direction_and_interpolation(list[0], grad);
	else if (grad.is_radial())
		ok = parse_radial_gradient_shape_size_position_interpolation(list[0], grad);
	else
		ok = parse_conic_gradient_angle_position_interpolation(list[0], grad);

	if (ok) remove(list, 0);

	
	if (grad.is_conic())
		ok = parse_color_stop_list<float>(list, grad, container);
	else
		ok = parse_color_stop_list<css_length>(list, grad, container);

	if (!ok) return false;
	

	result = grad;
	return true;
}

// parse <length-percentage> or <angle-percentage>
bool parse_lenang(const css_token& tok, css_length& length)
{
	return parse_length(tok, length, f_length_percentage);
}
bool parse_lenang(const css_token& tok, float& angle)
{
	return parse_angle(tok, angle, true);
}

// <color-hint> = <length-percentage> | <angle-percentage>
template<class T> // T == css_length or float
bool parse_color_hint(const css_token_vector& tokens, vector<gradient::color_stop>& color_stops)
{
	T lenang;
	if (tokens.size() == 1 && parse_lenang(tokens[0], lenang))
	{
		color_stops.push_back(lenang);
		return true;
	}
	return false;
}

// <linear-color-stop> = <color> <length-percentage>{1,2}?
// <angular-color-stop> = <color> <angle-percentage>{1,2}?
template<class T> // T == css_length or float
bool parse_color_stop(const css_token_vector& tokens, vector<gradient::color_stop>& color_stops, document_container* container)
{
	if (tokens.empty() || tokens.size() > 3)
		return false;

	web_color color;
	if (!color.from_token(tokens[0], container))
		return false;

	if (tokens.size() == 1) // <color>
	{
		color_stops.push_back(color);
		return true;
	}
	else if (tokens.size() == 2) // <color> <length-angle-percentage>
	{
		T lenang;
		if (parse_lenang(tokens[1], lenang))
		{
			color_stops.push_back({color, lenang});
			return true;
		}
	}
	else if (tokens.size() == 3) // <color> <length-angle-percentage> <length-angle-percentage>
	{
		T lenang1, lenang2;
		if (parse_lenang(tokens[1], lenang1) &&
			parse_lenang(tokens[2], lenang2))
		{
			color_stops.push_back({color, lenang1});
			color_stops.push_back({color, lenang2});
			return true;
		}
	}
	return false;
}

// <color-stop-list> = <color-stop> , [ <color-hint>? , <color-stop> ]#
template<class T> // T == css_length or float
bool parse_color_stop_list(const vector<css_token_vector>& list, gradient& grad, document_container* container)
{
	if (list.size() < 2) // at least two color-stops must be present
		return false;

	if (!parse_color_stop<T>(list[0], grad.m_colors, container))
		return false;
	
	// [ <color-hint>? , <color-stop> ]#
	for (size_t i = 1; i < list.size(); i++)
	{
		if (parse_color_hint<T>(list[i], grad.m_colors))
		{
			i++;
			if (i == list.size()) return false; // color-hint not followed by color-stop
		}
		if (!parse_color_stop<T>(list[i], grad.m_colors, container))
			return false;
	}
	return true;
}

// https://drafts.csswg.org/css-images-4/#linear-gradients
// [ <angle> | to <side-or-corner> ] || <color-interpolation-method>
bool parse_linear_gradient_direction_and_interpolation(const css_token_vector& tokens, gradient& gradient)
{
	float angle = 180;
	int side = gradient_side_none;
	auto color_space = color_space_oklab;
	auto hue_interpolation = hue_interpolation_shorter;

	int index = 0;
	if (parse_linear_gradient_direction(tokens, index, angle, side))
	{
		parse_color_interpolation_method(tokens, index, color_space, hue_interpolation);
	}
	else if (parse_color_interpolation_method(tokens, index, color_space, hue_interpolation))
	{
		parse_linear_gradient_direction(tokens, index, angle, side);
	}
	else
		return false;
	
	if (index != tokens.size()) return false;

	gradient.angle = angle;
	gradient.m_side = side;
	gradient.color_space = color_space;
	gradient.hue_interpolation = hue_interpolation;
	return true;
}

// https://drafts.csswg.org/css-images-4/#linear-gradients
// <angle> | to <side-or-corner>
// <side-or-corner> = [left | right] || [top | bottom]
bool parse_linear_gradient_direction(const css_token_vector& tokens, int& index, float& angle, int& side)
{
	if (parse_angle(at(tokens, index), angle))
	{
		index++;
		return true;
	}

	if (at(tokens, index).ident() != "to")
		return false;

	string a = at(tokens, index + 1).ident();
	string b = at(tokens, index + 2).ident();

	if (is_one_of(a, "left", "right", "top", "bottom"))
	{
		if (!is_one_of(b, "left", "right", "top", "bottom"))
		{
			switch (_id(a))
			{
			case _top_:    angle = 0; break;
			case _bottom_: angle = 180; break;
			case _left_:   angle = 270; break;
			case _right_:  angle = 90; break;
			default:       return false;
			}
			index += 2;
			return true;
		}
		else
		{
			// fix order
			if (is_one_of(a, "top", "bottom"))
				swap(a, b);

			// check order
			if (!is_one_of(a, "left", "right") || !is_one_of(b, "top", "bottom"))
				return false;

			side  =  a == "left" ? gradient_side_left : gradient_side_right;
			side |=  b == "top" ? gradient_side_top : gradient_side_bottom;
			index += 3;
			return true;
		}
	}
	return false;
}

// https://drafts.csswg.org/css-images-4/#typedef-conic-gradient-syntax
// [ from <angle> ]? [ at <position> ]?
bool parse_conic_angle_position(const css_token_vector& tokens, int& index, gradient& gradient)
{
	if (at(tokens, index).ident() == "from" && parse_angle(at(tokens, index + 1), gradient.conic_from_angle))
		index += 2;

	int i = index;
	if (at(tokens, i).ident() == "at" && parse_gradient_position(tokens, ++i, gradient))
		index = i;

	return true;
}
// [ [ from <angle> ]? [ at <position> ]? ] || <color-interpolation-method>
bool parse_conic_gradient_angle_position_interpolation(const css_token_vector& tokens, gradient& gradient)
{
	if (tokens.empty()) return false;

	auto color_space = color_space_oklab;
	auto hue_interpolation = hue_interpolation_shorter;

	int index = 0;
	// checking color interpolation first because parse_conic_angle_position always succeeds
	if (parse_color_interpolation_method(tokens, index, color_space, hue_interpolation))
	{
		parse_conic_angle_position(tokens, index, gradient);
	}
	else if (parse_conic_angle_position(tokens, index, gradient))
	{
		parse_color_interpolation_method(tokens, index, color_space, hue_interpolation);
	}
	else
		return false;

	if (index != tokens.size()) return false;

	gradient.color_space = color_space;
	gradient.hue_interpolation = hue_interpolation;
	return true;
}

const float π = 3.14159265f;

// https://drafts.csswg.org/css-values-4/#angles
bool parse_angle(const css_token& tok, float& angle, bool percents_allowed)
{
	// The unit identifier may be omitted if the <angle> is zero.  https://drafts.csswg.org/css-images-3/#linear-gradient-syntax
	if (tok.type == NUMBER && tok.n.number == 0)
	{
		angle = 0;
		return true;
	}

	// <angle-percentage> in conic gradient
	if (tok.type == PERCENTAGE && percents_allowed)
	{
		angle = tok.n.number * 360 / 100;
		return true;
	}

	if (tok.type == DIMENSION)
	{
		switch (_id(lowcase(tok.unit)))
		{
		case _deg_:  angle = tok.n.number; break;
		case _grad_: angle = (tok.n.number / 400) * 360; break;
		case _rad_:  angle = (tok.n.number / (2 * π)) * 360; break;
		case _turn_: angle = tok.n.number * 360; break;
		default:     return false;
		}
		return true;
	}

	return false;
}

// https://www.w3.org/TR/css-color-4/#color-interpolation-method
// <rectangular-color-space> = srgb | srgb-linear | display-p3 | a98-rgb | prophoto-rgb | rec2020 | lab | oklab | xyz | xyz-d50 | xyz-d65
// <polar-color-space> = hsl | hwb | lch | oklch
// <hue-interpolation-method> = [ shorter | longer | increasing | decreasing ] hue
// <color-interpolation-method> = in [ <rectangular-color-space> | <polar-color-space> <hue-interpolation-method>? ]
bool parse_color_interpolation_method(const css_token_vector& tokens, int& index,
	color_space_t& color_space, hue_interpolation_t& hue_interpolation)
{
	if (at(tokens, index).ident() == "in" &&
		parse_keyword(at(tokens, index + 1), color_space, color_space_strings, 1))
	{
		index += 2;
	}
	else
		return false;

	if (color_space >= color_space_polar_start &&
		at(tokens, index + 1).ident() == "hue" && // must be checked before parse_keyword, otherwise hue_interpolation may be assigned a value when there is no "hue" keyword
		parse_keyword(at(tokens, index), hue_interpolation, hue_interpolation_strings, 1))
	{
		index += 2;
	}
	return true;
}

// https://www.w3.org/TR/css-images-3/#typedef-radial-size
// <radial-size> = <radial-extent> | <length [0,∞]> | <length-percentage [0,∞]>{2}
// <radial-extent> = closest-corner | closest-side | farthest-corner | farthest-side
// Permitted values also depend on <radial-shape>, see parse_radial_gradient_shape_size_position_interpolation.
// TODO: <radial-size> syntax was extended in https://drafts.csswg.org/css-images-4/#radial-size
bool parse_radial_size(const css_token_vector& tokens, int& index, gradient& gradient)
{
	auto& tok0 = at(tokens, index);
	auto& tok1 = at(tokens, index + 1);

	if (parse_keyword(tok0, gradient.radial_extent, radial_extent_strings, 1))
	{
		index++;
		return true;
	}

	css_length length[2];
	if (length[0].from_token(tok0, f_length_percentage | f_positive) &&
		length[1].from_token(tok1, f_length_percentage | f_positive))
	{
		gradient.radial_extent = radial_extent_none;
		gradient.radial_radius_x = length[0];
		gradient.radial_radius_y = length[1];
		index += 2;
		return true;
	}

	if (length[0].from_token(tok0, f_length | f_positive))
	{
		gradient.radial_extent = radial_extent_none;
		gradient.radial_radius_x = length[0];
		index++;
		return true;
	}

	return false;
}

bool parse_gradient_position(const css_token_vector& tokens, int& index, gradient& gradient)
{
	css_length x, y;
	if (!parse_bg_position(tokens, index, x, y, false))
		return false;

	gradient.m_side = 0;
	if (x.is_predefined())
	{
		if (x.predef() == background_position_center)
			gradient.m_side |= gradient_side_x_center;
		else
			gradient.m_side |= 1 << x.predef();
	}
	else
	{
		gradient.m_side |= gradient_side_x_length;
		gradient.position_x = x;
	}

	if (y.is_predefined())
	{
		if (y.predef() == background_position_center)
			gradient.m_side |= gradient_side_y_center;
		else
			gradient.m_side |= 1 << y.predef();
	}
	else
	{
		gradient.m_side |= gradient_side_y_length;
		gradient.position_y = y;
	}
	return true;
}

// https://drafts.csswg.org/css-images-4/#radial-gradients
// [ [ <radial-shape> || <radial-size> ]? [ at <position> ]? ] || <color-interpolation-method>
bool parse_radial_gradient_shape_size_position_interpolation(const css_token_vector& tokens, gradient& result)
{
	// this check is needed because parse may succeed without consuming any input
	if (tokens.empty()) return false;

	auto shape = radial_shape_none;
	auto radial_shape = [&](const css_token_vector& tokens, int& index)
	{
		if (!parse_keyword(at(tokens, index), shape, "circle;ellipse", 1))
			return false;
		index++;
		return true;
	};
	
	using namespace std::placeholders;
	gradient grad;
	// sets grad.radial_extent or grad.radial_radius_{x,y}
	parse_fn radial_size     = std::bind( parse_radial_size,       _1, _2, std::ref(grad) );
	// sets grad.m_side and grad.radial_position_{x,y}
	parse_fn radial_position = std::bind( parse_gradient_position, _1, _2, std::ref(grad) );

	auto color_space = color_space_oklab;
	auto hue_interpolation = hue_interpolation_shorter;
	auto color_interpolation_method = [&](const css_token_vector& tokens, int& index)
	{
		return parse_color_interpolation_method(tokens, index, color_space, hue_interpolation);
	};

	/////////////////////////////////////////////////////////////////////////////////////////

	auto parse = oror(
		color_interpolation_method, // first trying this because seq(opt,opt) always succeeds
		seq(opt(oror(radial_shape, radial_size)), opt(seq("at"_x, radial_position)))
	);

	int index = 0;
	bool ok = parse(tokens, index) && end(tokens, index);
	if (!ok) return false;

	/////////////////////////////////////////////////////////////////////////////////////////

	// If <radial-shape> is specified as circle or is omitted, the <radial-size> may be given explicitly as <length [0,∞]>
	if (shape == radial_shape_ellipse && 
		// radius_x is specified, but radius_y is not
		!grad.radial_radius_x.is_predefined() && grad.radial_radius_y.is_predefined())
		return false;
	
	// If <radial-shape> is specified as ellipse or is omitted, <radial-size> may instead be given explicitly as <length-percentage [0,∞]>{2}
	if (shape == radial_shape_circle && 
		// both radius_x and radius_y are specified
		!grad.radial_radius_y.is_predefined())
		return false;

	// If <radial-shape> is omitted, the ending shape defaults to a circle if the <radial-size> is a single <length>, and to an ellipse otherwise.
	if (shape == radial_shape_none)
	{
		if (!grad.radial_radius_x.is_predefined() && grad.radial_radius_y.is_predefined())
			shape = radial_shape_circle;
		else
			shape = radial_shape_ellipse;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	result.radial_shape = shape;
	
	result.radial_extent = grad.radial_extent;
	result.radial_radius_x = grad.radial_radius_x;
	result.radial_radius_y = grad.radial_radius_y;
	
	result.m_side = grad.m_side;
	result.position_x = grad.position_x;
	result.position_y = grad.position_y;
	
	result.color_space = color_space;
	result.hue_interpolation = hue_interpolation;
	
	return true;
}

void style::parse_keyword_comma_list(string_id name, const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	int_vector vec;

	for (const auto& layer : layers)
	{
		int idx;
		if (layer.size() != 1) return;
		if (!parse_keyword(layer[0], idx, m_valid_values[name])) return;
		vec.push_back(idx);
	}

	add_parsed_property(name, property_value(vec, important));
}

void style::parse_background_position(const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	length_vector x_positions, y_positions;

	for (const auto& layer : layers)
	{
		css_length x, y;
		int index = 0;
		if (!parse_bg_position(layer, index, x, y, true) || index != layer.size())
			return;

		x_positions.push_back(x);
		y_positions.push_back(y);
	}

	add_parsed_property(_background_position_x_, property_value(x_positions, important));
	add_parsed_property(_background_position_y_, property_value(y_positions, important));
}

void style::parse_background_size(const css_token_vector& tokens, bool important)
{
	auto layers = parse_comma_separated_list(tokens);
	if (layers.empty()) return;

	size_vector sizes;

	for (const auto& layer : layers)
	{
		css_size size;
		int index = 0;
		if (!parse_bg_size(layer, index, size) || index != layer.size())
			return;

		sizes.push_back(size);
	}

	add_parsed_property(_background_size_, property_value(sizes, important));
}

bool parse_font_weight(const css_token& tok, css_length& weight)
{
	if (int idx = value_index(tok.ident(), font_weight_strings); idx >= 0)
	{
		weight.predef(idx);
		return true;
	}

	// https://drafts.csswg.org/css-fonts/#font-weight-absolute-values
	// Note: fractional values are allowed.
	if (tok.type == NUMBER && tok.n.number >= 1 && tok.n.number <= 1000)
	{
		weight.set_value(tok.n.number, css_units_none);
		return true;
	}

	return false;
}

// <font-style> || <font-variant-css2> || <font-weight>
// None of the allowed values intersect with <font-size>, it cannot consume <font-size>.
// Note: <font-weight> can be a number >=1, but <font-size> inside <font> is <length-percentage>,
// so it can be only 0 number. <font-size> as a standalone property does allow <number>s in quirks mode.
bool parse_font_style_variant_weight(const css_token_vector& tokens, int& index,
	int& style, int& variant, css_length& weight)
{
	bool style_found = false;
	bool variant_found = false;
	bool weight_found = false;

	int i = index, count = 0;
	while (i < tokens.size() && count++ < 3)
	{
		const auto& tok = tokens[i++];
		// All three properties can have value "normal", and it changes nothing because initial
		// values of all three properties are "normal".
		if (tok.ident() == "normal")
			continue;
		else if (!style_found && parse_keyword(tok, style, font_style_strings))
			style_found = true;
		else if (!variant_found && parse_keyword(tok, variant, font_variant_strings))
			variant_found = true;
		else if (!weight_found && parse_font_weight(tok, weight))
			weight_found = true;
		else
			return false;
	}
	index = i;
	return true;
}

// https://www.w3.org/TR/css-values-4/#custom-idents
bool is_custom_ident(const css_token& tok)
{
	if (tok.type != IDENT) return false;
	// Custom identifiers are case-sensitive, but they should not case-insensitively match any of
	// CSS-wide keywords or "default".
	return !is_one_of(lowcase(tok.name), "default", "initial", "inherit", "unset");
}

// https://drafts.csswg.org/css-fonts/#propdef-font-family
// font-family = [ <family-name> | <generic-family> ]#
// <family-name> = <string> | <custom-ident>+
// <generic-family> = generic( <custom-ident>+ ) | <string> | <custom-ident>+
bool parse_font_family(const css_token_vector& tokens, string& font_family)
{
	auto list = parse_comma_separated_list(tokens);
	if (list.empty()) return false;

	string result;
	for (const auto& name : list)
	{
		if (name.size() == 1 && name[0].type == STRING)
		{
			//result.push_back(name[0].str);
			result += name[0].str + ',';
			continue;
		}

		// Otherwise: name must be a list of <custom-ident>s
		// Note: generic( <custom-ident>+ ) is not supported
		string str;
		for (const auto& tok : name)
		{
			if (!is_custom_ident(tok)) return false;
			str += tok.name + ' ';
		}
		//result.push_back(trim(str));
		result += trim(str) + ',';
	}
	result.resize(result.size() - 1); // remove last ','
	font_family = result;
	return true;
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/font
// https://drafts.csswg.org/css-fonts/#font-prop
// font = <font-style-weight>? <font-size> [ / <line-height> ]? <font-family>
// font = <system-family-name*>
// <font-style-weight> = <font-style> || <font-variant-css2> || <font-weight> || <font-width-css3*>
// values marked * are not supported
void style::parse_font(css_token_vector tokens, bool important)
{
	// initial values
	int style   = font_style_normal;
	int variant = font_variant_normal;
	css_length weight      = css_length::predef_value(font_weight_normal);
	css_length size        = css_length::predef_value(font_size_medium);
	css_length line_height = css_length::predef_value(line_height_normal);
	string font_family; // this argument is mandatory, no need to set initial value

	int index = 0;
	parse_font_style_variant_weight(tokens, index, style, variant, weight);

	// font-size = <absolute-size> | <relative-size> | <length-percentage [0,∞]> | math
	if (!size.from_token(at(tokens, index), f_length_percentage | f_positive, font_size_strings))
		return;
	index++;

	if (at(tokens, index).ch == '/')
	{
		index++;
		// https://drafts.csswg.org/css2/#propdef-line-height
		// line-height = normal | <number> | <length> | <percentage>
		if (!line_height.from_token(at(tokens, index), f_number | f_length_percentage, line_height_strings))
			return;
		index++;
	}

	remove(tokens, 0, index);
	if (!parse_font_family(tokens, font_family))
		return;

	add_parsed_property(_font_style_,   property_value(style,       important));
	add_parsed_property(_font_variant_, property_value(variant,     important));
	add_parsed_property(_font_weight_,  property_value(weight,      important));
	add_parsed_property(_font_size_,    property_value(size,        important));
	add_parsed_property(_line_height_,  property_value(line_height, important));
	add_parsed_property(_font_family_,  property_value(font_family, important));
}

// https://developer.mozilla.org/en-US/docs/Web/CSS/flex
// https://drafts.csswg.org/css-flexbox/#flex-property
// flex = none | [ <'flex-grow'> <'flex-shrink'>? || <'flex-basis'> ]
void style::parse_flex(const css_token_vector& tokens, bool important)
{
	auto n = tokens.size();
	if (n > 3) return;

	const auto& a = at(tokens, 0);
	const auto& b = at(tokens, 1);
	const auto& c = at(tokens, 2);

	struct flex
	{
		float m_grow = 1; // flex-grow is set to 1 when omitted
		float m_shrink = 1;
		css_length m_basis = 0;

		bool grow(const css_token& tok)
		{
			if (tok.type != NUMBER || tok.n.number < 0) return false;
			m_grow = tok.n.number;
			return true;
		}
		bool shrink(const css_token& tok)
		{
			if (tok.type != NUMBER || tok.n.number < 0) return false;
			m_shrink = tok.n.number;
			return true;
		}
		// unitless_zero_allowed enforces the rule "A unitless zero that is not already preceded by two flex factors must be interpreted as a flex factor."
		bool basis(const css_token& tok, bool unitless_zero_allowed = false)
		{
			if (!unitless_zero_allowed and tok.type == NUMBER and tok.n.number == 0)
				return false;
			return m_basis.from_token(tok, f_length_percentage | f_positive, flex_basis_strings);
		}
	};
	flex flex;

	if (n == 1)
	{
		string_id ident = _id(a.ident());
		if (is_one_of(ident, _initial_, _auto_, _none_))
		{
			css_length _auto = css_length::predef_value(flex_basis_auto);

			switch (ident)
			{
			case _initial_: flex = {0, 1, _auto}; break;
			case _auto_:    flex = {1, 1, _auto}; break; // can be handled by else
			case _none_:    flex = {0, 0, _auto}; break;
			}
		}
		else
		{
			bool ok = flex.grow(a) || flex.basis(a);
			if (!ok) return;
		}
	}
	else if (n == 2)
	{
		// <number> <number>
		// <number> <basis>
		// <basis> <number>
		bool ok =
			flex.grow(a) && (flex.shrink(b) || flex.basis(b)) ||
			flex.basis(a) && flex.grow(b);

		if (!ok) return;
	}
	else // n == 3
	{
		// <number> <number> <basis>
		// <basis> <number> <number>
		bool ok =
			flex.grow(a) && flex.shrink(b) && flex.basis(c, true) ||
			flex.basis(a) && flex.grow(b) && flex.shrink(c);

		if (!ok) return;
	}

	add_parsed_property(_flex_grow_,	property_value(flex.m_grow, important));
	add_parsed_property(_flex_shrink_,	property_value(flex.m_shrink, important));
	add_parsed_property(_flex_basis_,	property_value(flex.m_basis, important));
}

// flex-flow = <'flex-direction'> || <'flex-wrap'>
void style::parse_flex_flow(const css_token_vector& tokens, bool important)
{
	// initial values:  https://developer.mozilla.org/en-US/docs/Web/CSS/flex-flow#formal_definition
	int flex_direction = flex_direction_row;
	int flex_wrap      = flex_wrap_nowrap;

	bool direction_found = false;
	bool wrap_found = false;

	for (const auto& token : tokens)
	{
		if (!direction_found && parse_keyword(token, flex_direction, flex_direction_strings))
			direction_found = true;
		else if (!wrap_found && parse_keyword(token, flex_wrap, flex_wrap_strings))
			wrap_found = true;
		else
			return;
	}

	add_parsed_property(_flex_direction_, property_value(flex_direction, important));
	add_parsed_property(_flex_wrap_,      property_value(flex_wrap,      important));
}

// https://www.w3.org/TR/css-align/#align-self-property
// value = auto | normal | stretch |  [ [first | last]? && baseline ]  |  [safe | unsafe]? <self-position>
// <self-position> = center | start | end | self-start | self-end | flex-start | flex-end
// https://www.w3.org/TR/css-align/#align-items-property
// same as align-self, except auto is not allowed
void style::parse_align_self(string_id name, const css_token_vector& tokens, bool important)
{
	auto n = tokens.size();
	if (n > 2)
		return;
	if (tokens[0].type != IDENT || (n == 2 && tokens[1].type != IDENT))
		return;
	
	string a = tokens[0].ident();
	
	if (name == _align_items_ && a == "auto")
		return;

	if (n == 1)
	{
		int idx = value_index(a, flex_align_items_strings);
		if (idx >= 0)
			add_parsed_property(name, property_value(idx, important));
		return;
	}

	// Otherwise: n == 2

	string b = tokens[1].ident();

	if (a == "baseline") swap(a, b);
	if (b == "baseline" && is_one_of(a, "first", "last"))
	{
		int idx = flex_align_items_baseline | (a == "first" ? flex_align_items_first : flex_align_items_last);
		add_parsed_property(name, property_value(idx, important));
		return;
	}

	// <overflow-position> <self-position>
	int idx = value_index(b, self_position_strings);
	if (idx >= 0 && is_one_of(a, "safe", "unsafe"))
	{
		idx |= (a == "safe" ? flex_align_items_safe : flex_align_items_unsafe);
		add_parsed_property(name, property_value(idx, important));
	}
}

void style::add_parsed_property( string_id name, const property_value& propval )
{
	auto prop = m_properties.find(name);
	if (prop != m_properties.end())
	{
		if (!prop->second.m_important || (propval.m_important && prop->second.m_important))
		{
			prop->second = propval;
		}
	}
	else
	{
		m_properties[name] = propval;
	}
}

void style::remove_property( string_id name, bool important )
{
	auto prop = m_properties.find(name);
	if(prop != m_properties.end())
	{
		if( !prop->second.m_important || (important && prop->second.m_important) )
		{
			m_properties.erase(prop);
		}
	}
}

void style::combine(const style& src)
{
	for (const auto& property : src.m_properties)
	{
		add_parsed_property(property.first, property.second);
	}
}

const property_value& style::get_property(string_id name) const
{
	auto it = m_properties.find(name);
	if (it != m_properties.end())
	{
		return it->second;
	}
	static property_value dummy;
	return dummy;
}

// var( <custom-property-name> , <declaration-value>? )
bool check_var_syntax(const css_token_vector& args)
{
	if (args.empty()) return false;
	
	string name = args[0].ident();
	if (name.substr(0, 2) != "--" || name.size() <= 2)
		return false;
	
	if (args.size() > 1 && args[1].ch != ',')
		return false;
	if (args.size() > 2 && !is_declaration_value(args, 2))
		return false;

	return true;
}

// https://drafts.csswg.org/css-variables/#using-variables
// var( <custom-property-name> , <declaration-value>? )
// returns true if one var() was substituted
// returns true if there was error or var() was not found
bool subst_var(css_token_vector& tokens, const element* el, std::set<string_id>& used_vars)
{
	for (int i = 0; i < tokens.size(); i++)
	{
		auto& tok = tokens[i];
		if (tok.type == CV_FUNCTION && lowcase(tok.name) == "var")
		{
			auto args = tok.value; // copy is intentional
			if (!check_var_syntax(args)) return false;
			
			auto name = _id(args[0].name);
			if (name in used_vars) return false; // dependency cycle  https://drafts.csswg.org/css-variables/#cycles
			used_vars.insert(name);
			
			css_token_vector value;
			if (el->get_custom_property(name, value))
			{
				remove(tokens, i);
				insert(tokens, i, value);
			}
			else // custom property not defined
			{
				if (args.size() == 1) return false; // default value not provided
				remove(args, 0, 2);
				remove(tokens, i);
				insert(tokens, i, args);
			}
			return true;
		}
		if (tok.is_component_value() && subst_var(tok.value, el, used_vars))
			return true;
	}
	return false;
}

void subst_vars_(string_id name, css_token_vector& tokens, const element* el)
{
	std::set<string_id> used_vars = {name};
	while (subst_var(tokens, el, used_vars));
}

void style::subst_vars(const element* el)
{
	for (auto& prop : m_properties)
	{
		if (prop.second.m_type == prop_type_var)
		{
			subst_vars_(prop.first, prop.second.m_token_vector, el);
			// re-adding the same property
			// if it is a custom property it will be readded as a css_token_vector
			// if it is a standard css property it will be parsed and properly added as typed property
			add_property(prop.first, prop.second.m_token_vector, "", prop.second.m_important, el->get_document()->container());
		}
	}
}

} // namespace litehtml