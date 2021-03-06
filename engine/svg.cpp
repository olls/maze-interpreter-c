struct ParsePathCommandResult
{
  vec2 last_point;
  const u8 *c;
  b32 found_command;
  LineSegment *added_segment;
};

ParsePathCommandResult
parse_path_command(Memory *memory, const u8 command, const u8 *c, const u8 *c_after_command, const u8 *end_f, SVGPath *path, vec2 last_point, vec2 origin)
{
  ParsePathCommandResult result;

  b32 delta = false;
  b32 found_command = true;
  LineSegment *line_seg = 0;

  switch (command)
  {
    case ('m'):
      delta = true;
    case ('M'):
    {
      // Move
      c = c_after_command;

      vec2 d_move;
      consume_until(c, is_num_or_sign, end_f);
      c = get_num(c, end_f, &d_move.x);
      consume_until(c, is_num_or_sign, end_f);
      c = get_num(c, end_f, &d_move.y);

      if (delta)
      {
        last_point += d_move;
      }
      else
      {
        last_point = origin + d_move;
      }
    } break;

    case ('l'):
      delta = true;
    case ('L'):
    {
      // Line
      c = c_after_command;

      ++path->n_segments;
      line_seg = push_struct(memory, LineSegment);
      line_seg->start = last_point;

      consume_until(c, is_num_or_sign, end_f);
      c = get_num(c, end_f, &line_seg->end.x);
      consume_until(c, is_num_or_sign, end_f);
      c = get_num(c, end_f, &line_seg->end.y);

      if (delta)
      {
        line_seg->end += line_seg->start;
      }
      else
      {
        line_seg->start += origin;
        line_seg->end += origin;
      }
    } break;

    case ('h'):
      delta = true;
    case ('H'):
    {
      // Horizontal line
      c = c_after_command;

      ++path->n_segments;
      line_seg = push_struct(memory, LineSegment);
      line_seg->start = last_point;
      line_seg->end.y = last_point.y;

      consume_until(c, is_num_or_sign, end_f);
      c = get_num(c, end_f, &line_seg->end.x);

      if (delta)
      {
        line_seg->end.x += line_seg->start.x;
      }
      else
      {
        line_seg->start += origin;
        line_seg->end += origin;
      }
    } break;

    case ('v'):
      delta = true;
    case ('V'):
    {
      // Vertical line
      c = c_after_command;

      ++path->n_segments;
      line_seg = push_struct(memory, LineSegment);
      line_seg->start = last_point;
      line_seg->end.x = last_point.x;

      consume_until(c, is_num_or_sign, end_f);
      c = get_num(c, end_f, &line_seg->end.y);

      if (delta)
      {
        line_seg->end.y += line_seg->start.y;
      }
      else
      {
        line_seg->start += origin;
        line_seg->end += origin;
      }
    } break;

    case ('Z'):
    case ('z'):
    {
      // Close path
      c = c_after_command;

      ++path->n_segments;
      line_seg = push_struct(memory, LineSegment);
      line_seg->start = last_point;
      line_seg->start = path->segments[0].start;
    } break;

    default:
    {
      found_command = false;
    } break;
  }

  if (line_seg != 0)
  {
    result.last_point = line_seg->end;
  }
  else
  {
    result.last_point = last_point;
  }

  result.c = c;
  result.found_command = found_command;
  result.added_segment = line_seg;

  return result;
}


SVGPath
parse_path_d(Memory *memory, XMLTag *path_tag, String d_attr, vec2 origin)
{
  SVGPath path = {.segments = 0, .n_segments = 0};

  log(L_SVG, u8("Parsing d: '%.*s'"), d_attr.length, d_attr.text);
  const u8 *end_f = d_attr.text + d_attr.length;

  vec2 current_point = origin;

  u8 last_command = 0;

  const u8 *c = d_attr.text;
  while (c < end_f)
  {
    consume_whitespace(c, end_f);

    const u8 command = *c;
    const u8 *after_command = c + 1;

    ParsePathCommandResult command_parse_result = parse_path_command(memory, command, c, after_command, end_f, &path, current_point, origin);
    current_point = command_parse_result.last_point;
    c = command_parse_result.c;
    if (command_parse_result.added_segment && path.segments == 0)
    {
      path.segments = command_parse_result.added_segment;
    }

    if (command_parse_result.found_command)
    {
      last_command = command;
    }
    else
    {
      if (last_command != 0)
      {
        // Special case where subsequent coords after a move command, are treated as line commands
        if (last_command == 'm')
        {
          last_command = 'l';
        }
        if (last_command == 'M')
        {
          last_command = 'L';
        }

        command_parse_result = parse_path_command(memory, last_command, c, c, end_f, &path, current_point, origin);
        current_point = command_parse_result.last_point;
        c = command_parse_result.c;
        if (command_parse_result.added_segment && path.segments == 0)
        {
          path.segments = command_parse_result.added_segment;
        }

        assert(command_parse_result.found_command);
      }
      else
      {
        printf("Error: SVG path doesn't contain a command\n");
      }
    }
  }

  return path;
}


b32
is_style_value_char(u8 character)
{
  return is_letter(character) ||
         is_num_or_sign(character) ||
         character == '.' ||
         character == '#';
}


const u8 *
find_svg_style(const u8 *c, const u8 *end, String *label_result, String *value_result)
{
  consume_whitespace_nl(c, end);
  const u8 *start_label = c;

  consume_while_f_or_char(c, is_letter, '-', end);
  const u8 *end_label = c;

  *label_result = (String){
    .text = start_label,
    .length = (u32)(end_label - start_label)
  };

  consume_until_char(c, ':', end);
  ++c;

  consume_whitespace(c, end);
  const u8 *start_value = c;

  consume_while(c, is_style_value_char, end);
  const u8 *end_value = c;

  *value_result = (String){
    .text = start_value,
    .length = (u32)(end_value - start_value)
  };

  consume_until_char(c, ';', end);
  ++c;

  return c;
}


b32
parse_colour_string(String string, vec4 *result)
{
  b32 parsed = true;

  if (string.text[0] == '#')
  {
    // UNIMPLEMENTED: Named colours, eg: red, green, fuchsia

    if (string.length == 4)
    {
      // #rgb
      String channel_str = {.length = 1};

      channel_str.text = string.text + 1;
      result->r = (1.0/15.0) * (r32)hex_string_to_int(channel_str);

      channel_str.text = string.text + 2;
      result->g = (1.0/15.0) * (r32)hex_string_to_int(channel_str);

      channel_str.text = string.text + 3;
      result->b = (1.0/15.0) * (r32)hex_string_to_int(channel_str);
    }
    else if (string.length == 7)
    {
      // #rrggbb
      String channel_str = {.length = 2};

      channel_str.text = string.text + 1;
      result->r = (1.0/255.0) * (r32)hex_string_to_int(channel_str);

      channel_str.text = string.text + 3;
      result->g = (1.0/255.0) * (r32)hex_string_to_int(channel_str);

      channel_str.text = string.text + 5;
      result->b = (1.0/255.0) * (r32)hex_string_to_int(channel_str);
    }
    else
    {
      parsed = false;
    }
  }
  else
  {
    parsed = false;
  }

  return parsed;
}


SVGStrokeLinecap
parse_svg_stroke_linecap(String value)
{
  SVGStrokeLinecap result = LINECAP_BUTT;

  if (str_eq(value, String("butt")))
  {
    result = LINECAP_BUTT;
  }
  else if (str_eq(value, String("round")))
  {
    result = LINECAP_ROUND;
  }
  else if (str_eq(value, String("square")))
  {
    result = LINECAP_SQUARE;
  }

  return result;
}


SVGStrokeLinejoin
parse_svg_stroke_linejoin(String value)
{
  SVGStrokeLinejoin result = LINEJOIN_MITER;

  if (str_eq(value, String("miter")))
  {
    result = LINEJOIN_MITER;
  }
  else if (str_eq(value, String("round")))
  {
    result = LINEJOIN_ROUND;
  }
  else if (str_eq(value, String("bevel")))
  {
    result = LINEJOIN_BEVEL;
  }

  return result;
}


void
parse_svg_style(String label, String value, SVGStyle *result)
{
  if (str_eq(label, String("fill")))
  {
    log(L_SVG, u8("Style label: fill"));

    result->filled = parse_colour_string(value, &result->fill_colour);
  }
  else if (str_eq(label, String("fill-opacity")))
  {
    log(L_SVG, u8("Style label: fill-opacity"));

    get_num(value.text, value.text + value.length, &result->fill_colour.a);
  }
  else if (str_eq(label, String("stroke")))
  {
    log(L_SVG, u8("Style label: stroke"));

    parse_colour_string(value, &result->stroke_colour);
  }
  else if (str_eq(label, String("stroke-opacity")))
  {
    log(L_SVG, u8("Style label: stroke-opacity"));

    get_num(value.text, value.text + value.length, &result->stroke_colour.a);
  }
  else if (str_eq(label, String("stroke-width")))
  {
    log(L_SVG, u8("Style label: stroke-width"));

    get_num(value.text, value.text + value.length, &result->stroke_width);
  }
  else if (str_eq(label, String("stroke-linecap")))
  {
    log(L_SVG, u8("Style label: stroke-linecap"));

    result->stroke_linecap = parse_svg_stroke_linecap(value);
  }
  else if (str_eq(label, String("stroke-linejoin")))
  {
    log(L_SVG, u8("Style label: stroke-linejoin"));

    result->stroke_linejoin = parse_svg_stroke_linejoin(value);
  }
}


void
get_svg_styles_from_attrs(XMLTag *tag, SVGStyle *result)
{
  String fill = get_attr_value(tag, String("fill"));
  if (fill.text != 0)
  {
    log(L_SVG, u8("Style label: fill"));

    result->filled = parse_colour_string(fill, &result->fill_colour);
  }

  String fill_opacity = get_attr_value(tag, String("fill-opacity"));
  if (fill_opacity.text != 0)
  {
    log(L_SVG, u8("Style label: fill-opacity"));

    get_num(fill_opacity.text, fill_opacity.text + fill_opacity.length, &result->fill_colour.a);
  }

  String stroke = get_attr_value(tag, String("stroke"));
  if (stroke.text != 0)
  {
    log(L_SVG, u8("Style label: stroke"));

    parse_colour_string(stroke, &result->stroke_colour);
  }

  String stroke_opacity = get_attr_value(tag, String("stroke-opacity"));
  if (stroke_opacity.text != 0)
  {
    log(L_SVG, u8("Style label: stroke-opacity"));

    get_num(stroke_opacity.text, stroke_opacity.text + stroke_opacity.length, &result->stroke_colour.a);
  }

  String stroke_width = get_attr_value(tag, String("stroke-width"));
  if (stroke_width.text != 0)
  {
    log(L_SVG, u8("Style label: stroke-width"));

    get_num(stroke_width.text, stroke_width.text + stroke_width.length, &result->stroke_width);
  }

  String stroke_linecap = get_attr_value(tag, String("stroke-linecap"));
  if (stroke_linecap.text != 0)
  {
    log(L_SVG, u8("Style label: stroke-linecap"));

    result->stroke_linecap = parse_svg_stroke_linecap(stroke_linecap);
  }

  String stroke_linejoin = get_attr_value(tag, String("stroke-linejoin"));
  if (stroke_linejoin.text != 0)
  {
    log(L_SVG, u8("Style label: stroke-linejoin"));

    result->stroke_linejoin = parse_svg_stroke_linejoin(stroke_linejoin);
  }
}


void
parse_svg_styles(XMLTag *from_tag, SVGStyle *result)
{
  // Get styles from style attribute

  String style = get_attr_value(from_tag, String("style"));
  if (style.text != 0)
  {
    log(L_SVG, u8("Parsing style: %.*s"), style.length, style.text);

    const u8 *c = style.text;
    const u8 *end = style.text + style.length;

    result->stroke_width = 1;
    result->stroke_colour = (vec4){1, 0, 0, 0};
    result->stroke_linecap = LINECAP_BUTT;
    result->stroke_linejoin = LINEJOIN_MITER;
    result->filled = false;
    result->fill_colour = (vec4){1, 0, 0, 0};

    while (c < end)
    {
      String label, value;
      c = find_svg_style(c, end, &label, &value);

      parse_svg_style(label, value, result);
    }
  }

  // Get styles from individual attributes (overriding previous styles)
  get_svg_styles_from_attrs(from_tag, result);

  log(L_SVG, u8("Styles:"));

  log(L_SVG, u8("stroke_width: %d"), result->stroke_width);
  log(L_SVG, u8("stroke_colour: (%f,%f,%f,%f)"), result->stroke_colour.a, result->stroke_colour.r, result->stroke_colour.g, result->stroke_colour.b);
  log(L_SVG, u8("stroke_linecap: %d"), result->stroke_linecap);
  log(L_SVG, u8("stroke_linejoin: %d"), result->stroke_linejoin);
  log(L_SVG, u8("filled: %d"), result->filled);
  log(L_SVG, u8("fill_colour: (%f,%f,%f,%f)"), result->fill_colour.a, result->fill_colour.r, result->fill_colour.g, result->fill_colour.b);
}


SVGPath
parse_path(Memory *memory, XMLTag *path_tag, vec2 origin)
{
  SVGPath result;

  String d_attr = get_attr_value(path_tag, String("d"));
  if (d_attr.text != 0)
  {
    result = parse_path_d(memory, path_tag, d_attr, origin);
  }

  return result;
}


// NOTE: Only implements translate()
vec2
parse_svg_transform(String transform_attr)
{
  vec2 result = {0, 0};

  const u8 *c = transform_attr.text;
  const u8 *end_f = transform_attr.text + transform_attr.length;

  consume_whitespace(c, end_f);
  if (str_eq(c, String("translate")))
  {
    consume_until_char(c, '(', end_f);
    const u8 *start_translate = c;

    consume_until_char(c, ')', end_f);
    const u8 *end_translate = c;

    if (start_translate == end_f || end_translate == end_f)
    {
      log(L_SVG, u8("Invalid translate() in transform"));
    }
    else
    {
      c = start_translate + 1;

      consume_until(c, is_num_or_sign, end_translate);
      c = get_num(c, end_translate, &result.x);

      if (c == end_translate)
      {
        log(L_SVG, u8("Invalid translate: No dx provided"));
      }
      else
      {
        consume_until(c, is_num_or_sign, end_translate);
        c = get_num(c, end_translate, &result.y);
      }
    }
  }

  return result;
}


SVGRect
parse_rect(Memory *memory, XMLTag *rect_tag, vec2 origin)
{
  SVGRect result;

  String transform_attr = get_attr_value(rect_tag, String("transform"));
  if (transform_attr.text != 0)
  {
    origin += parse_svg_transform(transform_attr);
  }

  result.rect = (Rectangle){ .start = origin, .end = origin};

  String x_str = get_attr_value(rect_tag, String("x"));
  String y_str = get_attr_value(rect_tag, String("y"));
  String width_str  = get_attr_value(rect_tag, String("width"));
  String height_str = get_attr_value(rect_tag, String("height"));
  String r_str = get_attr_value(rect_tag, String("ry"));

  vec2 pos = {0, 0};
  vec2 size = {0, 0};
  r32 radius = 0;

  get_num(x_str.text, x_str.text + x_str.length, &pos.x);
  get_num(y_str.text, y_str.text + y_str.length, &pos.y);
  get_num( width_str.text,  width_str.text +  width_str.length, &size.x);
  get_num(height_str.text, height_str.text + height_str.length, &size.y);
  get_num(r_str.text, r_str.text + r_str.length, &radius);

  result.rect.start = origin + pos;
  result.rect.end = result.rect.start + size;
  result.radius = radius;

  return result;
}


SVGCircle
parse_circle(Memory *memory, XMLTag *circle_tag, vec2 origin)
{
  SVGCircle result;

  String transform_attr = get_attr_value(circle_tag, String("transform"));
  if (transform_attr.text != 0)
  {
    origin += parse_svg_transform(transform_attr);
  }

  String x_str = get_attr_value(circle_tag, String("cx"));
  String y_str = get_attr_value(circle_tag, String("cy"));
  String radius_str  = get_attr_value(circle_tag, String("r"));

  vec2 pos;
  r32 radius;
  get_num(x_str.text, x_str.text + x_str.length, &pos.x);
  get_num(y_str.text, y_str.text + y_str.length, &pos.y);
  get_num(radius_str.text, radius_str.text + radius_str.length, &radius);

  result.position = origin + pos;
  result.radius = radius;

  return result;
}


SVGLine
parse_line(Memory *memory, XMLTag *line_tag, vec2 origin)
{
  SVGLine result;

  String transform_attr = get_attr_value(line_tag, String("transform"));
  if (transform_attr.text != 0)
  {
    origin += parse_svg_transform(transform_attr);
  }

  String x1_str = get_attr_value(line_tag, String("x1"));
  String y1_str = get_attr_value(line_tag, String("y1"));
  String x2_str = get_attr_value(line_tag, String("x2"));
  String y2_str = get_attr_value(line_tag, String("y2"));

  vec2 start, end;
  get_num(x1_str.text, x1_str.text + x1_str.length, &start.x);
  get_num(y1_str.text, y1_str.text + y1_str.length, &start.y);
  get_num(x2_str.text, x2_str.text + x2_str.length,   &end.x);
  get_num(y2_str.text, y2_str.text + y2_str.length,   &end.y);

  result.line.start = origin + start;
  result.line.end = origin + end;

  return result;
}


vec2
parse_svg_group(XMLTag *g_tag)
{
  vec2 result = {0, 0};

  String transform_attr = get_attr_value(g_tag, String("transform"));
  if (transform_attr.text != 0)
  {
    result = parse_svg_transform(transform_attr);
  }

  return result;
}


SVGRect
get_svg_root_rect(XMLTag *root, vec2 origin)
{
  SVGRect result;
  result.rect = (Rectangle){ .start = origin, .end = origin};

  String width_str  = get_attr_value(root, String("width"));
  String height_str = get_attr_value(root, String("height"));

  vec2 size = {0, 0};
  get_num( width_str.text,  width_str.text +  width_str.length, &size.x);
  get_num(height_str.text, height_str.text + height_str.length, &size.y);

  result.rect.end += size;
  result.radius = 0;

  return result;
}


void
get_svg_operations(Memory *memory, XMLTag *tag, SVGOperation **result, vec2 delta = (vec2){0, 0})
{
  XMLTag *child = tag->first_child;
  while (child)
  {
    if (str_eq(child->name, String("path")))
    {
      log(L_SVG, u8("Found: path"));

      SVGOperation *old_start = *result;
      *result = push_struct(memory, SVGOperation);

      (*result)->type = SVG_OP_PATH;
      (*result)->next = old_start;

      (*result)->path = parse_path(memory, child, delta);
      parse_svg_styles(child, &((*result)->style));
    }
    else if (str_eq(child->name, String("rect")))
    {
      log(L_SVG, u8("Found: rect"));

      SVGOperation *old_start = *result;
      *result = push_struct(memory, SVGOperation);

      (*result)->type = SVG_OP_RECT;
      (*result)->next = old_start;

      (*result)->rect = parse_rect(memory, child, delta);
      parse_svg_styles(child, &((*result)->style));
    }
    else if (str_eq(child->name, String("circle")))
    {
      log(L_SVG, u8("Found: circle"));

      SVGOperation *old_start = *result;
      *result = push_struct(memory, SVGOperation);

      (*result)->type = SVG_OP_CIRCLE;
      (*result)->next = old_start;

      (*result)->circle = parse_circle(memory, child, delta);
      parse_svg_styles(child, &((*result)->style));
    }
    else if (str_eq(child->name, String("line")))
    {
      log(L_SVG, u8("Found: line"));

      SVGOperation *old_start = *result;
      *result = push_struct(memory, SVGOperation);

      (*result)->type = SVG_OP_LINE;
      (*result)->next = old_start;

      (*result)->line = parse_line(memory, child, delta);
      parse_svg_styles(child, &((*result)->style));
    }
    else if (str_eq(child->name, String("g")))
    {
      log(L_SVG, u8("Found: g"));

      delta += parse_svg_group(child);

      get_svg_operations(memory, child, result, delta);
    }
    else if (str_eq(child->name, String("svg")))
    {
      log(L_SVG, u8("Found: svg"));

      SVGOperation *old_start = *result;
      *result = push_struct(memory, SVGOperation);

      (*result)->type = SVG_OP_RECT;
      (*result)->next = old_start;

      (*result)->rect = get_svg_root_rect(child, delta);

      // TODO: Temporary hack for displaying rectangle around SVGs
      (*result)->style = (SVGStyle){
        .stroke_width = 1,
        .stroke_colour = (vec4){0.2, 1, 1, 0},
        .stroke_linecap = LINECAP_SQUARE,
        .stroke_linejoin = LINEJOIN_MITER,
        .filled = false
      };

      get_svg_operations(memory, child, result, delta);
    }

    child = child->next_sibling;
  }
}


void
test_log_svg_operations(SVGOperation *svg_operations)
{
  SVGOperation *current = svg_operations;
  while (current)
  {
    if (current->path.segments)
    {
      LineSegment *line_segments = current->path.segments;

      for (u32 i = 0; i < current->path.n_segments; ++i)
      {
        LineSegment *line_seg = line_segments + i;
        // log(L_SVG, "  segment:");
        // log(L_SVG, "    (%f,%f)", line_seg->start.x, line_seg->start.y);
        // log(L_SVG, "    (%f,%f)", line_seg->end.x, line_seg->end.y);
      }
    }

    current = current->next;
  }
}


LineEndStyle
svg_linecap_to_line_end(SVGStrokeLinecap linecap)
{
  LineEndStyle result;

  switch (linecap)
  {
    case (LINECAP_BUTT):
      result = LINE_END_BUTT;
      break;
    case (LINECAP_SQUARE):
      result = LINE_END_SQUARE;
      break;
    case (LINECAP_ROUND):
      result = LINE_END_ROUND;
      break;
    default:
      result = LINE_END_NULL;
      break;
  }

  return result;
}


LineEndStyle
svg_linejoin_to_line_end(SVGStrokeLinejoin linejoin)
{
  LineEndStyle result;

  switch (linejoin)
  {
    case (LINEJOIN_MITER):
      result = LINE_END_MITER;
      break;
    case (LINEJOIN_ROUND):
      result = LINE_END_ROUND;
      break;
    case (LINEJOIN_BEVEL):
      result = LINE_END_BEVEL;
      break;
    default:
      result = LINE_END_NULL;
      break;
  }

  return result;
}