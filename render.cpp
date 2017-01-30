void
set_pixel(Renderer *renderer, u32 pixel_x, u32 pixel_y, V4 color)
{
  assert(pixel_x >= 0 && pixel_x < renderer->frame_buffer.width);
  assert(pixel_y >= 0 && pixel_y < renderer->frame_buffer.height);

  u32 pixel_pos = (pixel_y * renderer->frame_buffer.width) + pixel_x;

  V3 prev_color = pixel_color_to_V3(renderer->frame_buffer.buffer[pixel_pos]);
  V3 new_color = remove_alpha(color) * 255.0;

  PixelColor alpha_blended = to_color((color.a * new_color) + ((1.0f - color.a) * prev_color));
  renderer->frame_buffer.buffer[pixel_pos] = alpha_blended;
}


void
render_circle(Renderer *renderer,
              RenderBasis *render_basis,
              V2 world_pos,
              r32 world_radius,
              V4 color)
{
  // TODO: There seems to be some off-by-one bug in here, the right /
  //       bottom side of the circle seems to be clipped slightly
  //       sometimes.

  V2 fract_pixel_pos = transform_coord(render_basis, world_pos);

  r32 radius = (world_radius / render_basis->world_per_pixel) * render_basis->scale;
  r32 radius_sq = squared(radius);
  r32 radius_minus_one_sq = squared(radius - 1);

  Rectangle window_region = (Rectangle){(V2){0, 0}, (V2){renderer->frame_buffer.width, renderer->frame_buffer.height}};
  Rectangle render_region = render_basis->clip_region / render_basis->world_per_pixel;
  render_region = get_overlap(render_region, window_region);

  Rectangle fract_pixels_circle_region = {fract_pixel_pos - radius,
                                          fract_pixel_pos + radius + 1};
  fract_pixels_circle_region = crop_to(fract_pixels_circle_region, render_region);

  Rectangle pixels_circle_region = round_down(fract_pixels_circle_region);

  for (u32 pixel_y = pixels_circle_region.start.y;
       pixel_y < pixels_circle_region.end.y;
       pixel_y++)
  {
    for (u32 pixel_x = pixels_circle_region.start.x;
         pixel_x < pixels_circle_region.end.x;
         pixel_x++)
    {
      V2 d_center = (V2){pixel_x, pixel_y} - fract_pixel_pos;
      r32 dist_sq = length_sq(d_center);

      if (dist_sq < radius_sq)
      {
        V4 this_color = color;

        if (dist_sq > radius_minus_one_sq)
        {
          r32 diff = radius - sqrt(dist_sq);
          this_color.a *= diff;
        }

        set_pixel(renderer, pixel_x, pixel_y, this_color);
      }
    }
  }
}


void
render_box(Renderer *renderer, RenderBasis *render_basis, Rectangle box, V4 color)
{
  Rectangle window_region = (Rectangle){(V2){0, 0}, (V2){renderer->frame_buffer.width, renderer->frame_buffer.height}};
  Rectangle render_region = render_basis->clip_region / render_basis->world_per_pixel;
  render_region = get_overlap(render_region, window_region);

  Rectangle fract_pixel_space = transform_coord_rect(render_basis, box);

  Rectangle pixel_space = round_down(fract_pixel_space);
  pixel_space = (Rectangle){pixel_space.start, pixel_space.end + 1};

  pixel_space = crop_to(pixel_space, render_region);

  for (u32 pixel_y = pixel_space.start.y;
       pixel_y < pixel_space.end.y;
       pixel_y++)
  {
    for (u32 pixel_x = pixel_space.start.x;
         pixel_x < pixel_space.end.x;
         pixel_x++)
    {
      V4 this_color = color;

      if (pixel_x == pixel_space.start.x)
      {
        this_color.a *= (pixel_space.start.x + 1) - fract_pixel_space.start.x;
      }
      if (pixel_x == pixel_space.end.x-1)
      {
        this_color.a *= fract_pixel_space.end.x - (pixel_space.end.x - 1);
      }
      if (pixel_y == pixel_space.start.y)
      {
        this_color.a *= (pixel_space.start.y + 1) - fract_pixel_space.start.y;
      }
      if (pixel_y == pixel_space.end.y-1)
      {
        this_color.a *= fract_pixel_space.end.y - (pixel_space.end.y - 1);
      }

      set_pixel(renderer, pixel_x, pixel_y, this_color);
    }
  }
}


void
fast_render_box(Renderer *renderer, RenderBasis *render_basis, Rectangle box, PixelColor color)
{
  Rectangle window_region = (Rectangle){(V2){0, 0}, (V2){renderer->frame_buffer.width, renderer->frame_buffer.height}};
  Rectangle render_region = render_basis->clip_region / render_basis->world_per_pixel;
  render_region = get_overlap(render_region, window_region);

  Rectangle fract_pixel_space = transform_coord_rect(render_basis, box);
  fract_pixel_space = crop_to(fract_pixel_space, render_region);

  Rectangle pixel_space = round_down(fract_pixel_space);

  for (u32 pixel_y = pixel_space.start.y;
       pixel_y < pixel_space.end.y;
       pixel_y++)
  {
    for (u32 pixel_x = pixel_space.start.x;
         pixel_x < pixel_space.end.x;
         pixel_x++)
    {
      renderer->frame_buffer.buffer[pixel_x + pixel_y * renderer->frame_buffer.width] = color;
    }
  }
}


void
render_line(Renderer *renderer, RenderBasis *render_basis, V2 world_start, V2 world_end, V4 color, r32 world_width)
{
  V2 start = transform_coord(render_basis, world_start);
  V2 end = transform_coord(render_basis, world_end);
  r32 width = render_basis->scale * world_width / render_basis->world_per_pixel;

  if (start.x > end.x)
  {
    V2 tmp = start;
    start = end;
    end = tmp;
  }

  Rectangle window_region = (Rectangle){(V2){0, 0}, (V2){renderer->frame_buffer.width, renderer->frame_buffer.height}};
  Rectangle render_region = render_basis->clip_region / render_basis->world_per_pixel;
  render_region = crop_to(render_region, window_region);

  // Clip line endpoints to screen

  r32 x_gradient = (end.y - start.y) / (end.x - start.x);
  r32 y_gradient = (end.x - start.x) / (end.y - start.y);

  if (!in_rectangle(start, render_region))
  {
    if (start.x < render_region.start.x)
    {
      start.y += (render_region.start.x - start.x) * x_gradient;
      start.x = render_region.start.x;
    }
    else if (start.x >= render_region.end.x)
    {
      start.y += (render_region.end.x - start.x) * x_gradient;
      start.x = render_region.end.x - 1;
    }
    if (start.y < render_region.start.y)
    {
      start.x += (render_region.start.y - start.y) * y_gradient;
      start.y = render_region.start.y;
    }
    else if (start.y >= render_region.end.y)
    {
      start.x += (render_region.end.y - start.y) * y_gradient;
      start.y = render_region.end.y - 1;
    }
  }
  if (!in_rectangle(end, render_region))
  {
    if (end.x < render_region.start.x)
    {
      end.y += (render_region.start.x - end.x) * x_gradient;
      end.x = render_region.start.x;
    }
    else if (end.x >= render_region.end.x)
    {
      end.y += (render_region.end.x - end.x) * x_gradient;
      end.x = render_region.end.x - 1;
    }
    if (end.y < render_region.start.y)
    {
      end.x += (render_region.start.y - end.y) * y_gradient;
      end.y = render_region.start.y;
    }
    else if (end.y >= render_region.end.y)
    {
      end.x += (render_region.end.y - end.y) * y_gradient;
      end.y = render_region.end.y - 1;
    }
  }

  // Calculate line width stuff

  V2 length_components = {(end.x - start.x),
                          (end.y - start.y)};

  r32 x_length = abs(length_components.x);
  r32 y_length = abs(length_components.y);

  r32 theta = atan2(length_components.y, length_components.x);

  r32 num_steps;
  r32 width_comp;
  V2 axis;

  if (x_length > y_length)
  {
    r32 width_y_component = abs(width / (r32)sin((M_PI*.5)-theta));

    num_steps = x_length;
    axis = (V2){0, 1};
    width_comp = width_y_component;
  }
  else
  {
    r32 width_x_component = abs(width / (r32)sin(theta));

    num_steps = y_length;
    axis = (V2){1, 0};
    width_comp = width_x_component;
  }

  // TODO: Sub-pixel rendering
  // TODO: End-caps

  if (num_steps)
  {
    V2 step = length_components / num_steps;

    V2 line_center_fract = start;
    for (u32 pixel_n = 0;
         pixel_n < num_steps;
         ++pixel_n)
    {

      V2 pixel_pos_fract = line_center_fract - (axis * width_comp * 0.5);
      for (u32 offset = 0;
           offset < width_comp;
           ++offset)
      {
        V2 pixel_pos = round_down(pixel_pos_fract);
        set_pixel(renderer, pixel_pos.x, pixel_pos.y, color);

        pixel_pos_fract += axis;
      }

      // set_pixel(renderer, line_center_fract.x, line_center_fract.y, (V4){1, 1, 0, 0});
      line_center_fract += step;
    }
  }
}
