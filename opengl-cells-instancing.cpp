// Cells storage in instance VBO:
//  - Allocate block of X bytes
//  - Use block for contiguous array of CellInstances
//  - Record CellInstance positions in QuadTree Cells


// TODO: Separate instance types for the different cell types.


void
setup_cell_vertex_vbo_and_ibo(CellInstancingVBOs *cell_instancing_vbos, const vec2 *vertices, u32 n_vertices,
                                                                        const GLushort *indices, u32 n_indices)
{
  // Vertex VBO

  glGenBuffers(1, &cell_instancing_vbos->cell_vertex_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, cell_instancing_vbos->cell_vertex_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * n_vertices, vertices, GL_STATIC_DRAW);

  GLuint attribute_vertex = 8;
  glVertexAttribPointer(attribute_vertex, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), 0);
  glEnableVertexAttribArray(attribute_vertex);

  // Vertex IBO

  glGenBuffers(1, &cell_instancing_vbos->cell_vertex_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cell_instancing_vbos->cell_vertex_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * n_indices, indices, GL_STATIC_DRAW);
  cell_instancing_vbos->n_cell_indices = n_indices;
}


void
setup_cell_instances_vbo(CellInstancingVBOs *cell_instancing_vbos)
{
  glBindBuffer(GL_ARRAY_BUFFER, cell_instancing_vbos->cell_instances_vbo);

  GLuint attribute_world_cell_position_x = 0;
  glEnableVertexAttribArray(attribute_world_cell_position_x);
  glVertexAttribIPointer(attribute_world_cell_position_x, 1, GL_INT, sizeof(CellInstance), (void *)offsetof(CellInstance, world_cell_position_x));
  glVertexAttribDivisor(attribute_world_cell_position_x, 1);

  GLuint attribute_world_cell_position_y = 1;
  glEnableVertexAttribArray(attribute_world_cell_position_y);
  glVertexAttribIPointer(attribute_world_cell_position_y, 1, GL_INT, sizeof(CellInstance), (void *)offsetof(CellInstance, world_cell_position_y));
  glVertexAttribDivisor(attribute_world_cell_position_y, 1);

  GLuint attribute_world_cell_offset = 2;
  glEnableVertexAttribArray(attribute_world_cell_offset);
  glVertexAttribPointer(attribute_world_cell_offset, sizeof(vec2)/sizeof(r32), GL_FLOAT, GL_FALSE, sizeof(CellInstance), (void *)offsetof(CellInstance, world_cell_offset));
  glVertexAttribDivisor(attribute_world_cell_offset, 1);

  GLuint attribute_colour = 4;
  glEnableVertexAttribArray(attribute_colour);
  glVertexAttribPointer(attribute_colour, sizeof(vec4)/sizeof(r32), GL_FLOAT, GL_FALSE, sizeof(CellInstance), (void *)offsetof(CellInstance, colour));
  glVertexAttribDivisor(attribute_colour, 1);
}


void
allocate_cell_instances_block(CellInstancingVBOs *cell_instancing_vbos, u32 n_cell_instances)
{
  glBindBuffer(GL_ARRAY_BUFFER, cell_instancing_vbos->cell_instances_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(CellInstance) * n_cell_instances, NULL, GL_STATIC_DRAW);
  cell_instancing_vbos->n_cell_instances = 0;
  cell_instancing_vbos->cell_instances_vbo_size = n_cell_instances;
}


void
extend_cell_instance_vbo(CellInstancingVBOs *cell_instancing_vbos)
{
  if (cell_instancing_vbos->cell_instances_vbo_size == 0)
  {
    log(L_CellInstancing, "No buffer allocated yet, allocating new buffer.");
    allocate_cell_instances_block(cell_instancing_vbos, INITIAL_CELL_INSTANCE_VBO_SIZE);
  }
  else
  {
    log(L_CellInstancing, "Out of space, allocating larger buffer.");

    assert(cell_instancing_vbos->cell_instances_vbo_size < MAX_U32 / 2);
    u32 new_buffer_size = cell_instancing_vbos->cell_instances_vbo_size * 2;

    GLuint new_buffer = create_buffer();
    glBindBuffer(GL_ARRAY_BUFFER, new_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CellInstance) * new_buffer_size, NULL, GL_STATIC_DRAW);

    // Copy data into new VBO
    // Bind buffers to special READ/WRITE copying buffers
    glBindBuffer(GL_COPY_READ_BUFFER, cell_instancing_vbos->cell_instances_vbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, new_buffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(CellInstance) * cell_instancing_vbos->n_cell_instances);

    glDeleteBuffers(1, &cell_instancing_vbos->cell_instances_vbo);

    cell_instancing_vbos->cell_instances_vbo = new_buffer;
    cell_instancing_vbos->cell_instances_vbo_size = new_buffer_size;

    // TODO: Because we overwrite the old buffer with a new buffer, and the
    //       attributes are linked to the old buffer, we must re-set-them-up.
    //       If we kept the same buffer name, but still expanded it without
    //       losing the data, this would prevent us having to do this.
    setup_cell_instances_vbo(cell_instancing_vbos);
  }

  print_gl_errors();
  log(L_CellInstancing, "Reallocated VBO.");
}


void
update_cell_instance(CellInstancingVBOs *cell_instancing_vbos, u32 cell_instance_position, CellInstance *cell_instance)
{
  // Overwrite cell_instance_position with cell_instance
  if (cell_instance_position < cell_instancing_vbos->n_cell_instances)
  {
    glBindBuffer(GL_ARRAY_BUFFER, cell_instancing_vbos->cell_instances_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(CellInstance) * cell_instance_position, sizeof(CellInstance), cell_instance);
  }
}


void
add_cell_instance(CellInstancingVBOs *cell_instancing_vbos, Cell *cell)
{
  log(L_CellInstancing, "Adding cell instance.");

  if (cell_instancing_vbos->n_cell_instances >= cell_instancing_vbos->cell_instances_vbo_size)
  {
    extend_cell_instance_vbo(cell_instancing_vbos);
  }

  CellInstance cell_instance = {
    .world_cell_position_x = cell->x,
    .world_cell_position_y = cell->y,
    .world_cell_offset = {0, 0},

    // TODO: This is temporary until we have the different cell types in different instance arrays.
    .colour = get_cell_color(cell->type)
  };

  // Write cell_instance to the end of the cell instances array.

  cell->opengl_instance_position = cell_instancing_vbos->n_cell_instances;
  ++cell_instancing_vbos->n_cell_instances;

  update_cell_instance(cell_instancing_vbos, cell->opengl_instance_position, &cell_instance);

  log(L_CellInstancing, "n_cell_instances: %u, out of cell_instances_vbo_size: %u", cell_instancing_vbos->n_cell_instances, cell_instancing_vbos->cell_instances_vbo_size);
}


void
recurse_adding_all_cell_instances(CellInstancingVBOs *cell_instancing_vbos, QuadTree *tree)
{
  if (tree)
  {
    for (u32 cell_index = 0;
         cell_index < tree->used;
         ++cell_index)
    {
      Cell *cell = tree->cells + cell_index;
      add_cell_instance(cell_instancing_vbos, cell);
    }

    recurse_adding_all_cell_instances(cell_instancing_vbos, tree->top_right);
    recurse_adding_all_cell_instances(cell_instancing_vbos, tree->top_left);
    recurse_adding_all_cell_instances(cell_instancing_vbos, tree->bottom_right);
    recurse_adding_all_cell_instances(cell_instancing_vbos, tree->bottom_left);
  }
}


void
add_all_cell_instances(CellInstancingVBOs *cell_instancing_vbos, QuadTree *tree)
{
  recurse_adding_all_cell_instances(cell_instancing_vbos, tree);
  print_gl_errors();
  log(L_CellInstancing, "Added all cell instances.");
}


void
remove_cell_instance(CellInstancingVBOs *cell_instancing_vbos, Maze *maze, Cell *cell)
{
  // Move last CellInstance in VBO into the removed CellInstance.

  u32 cell_instance_number_to_remove = cell->opengl_instance_position;
  cell->opengl_instance_position = INVALID_CELL_INSTANCE_POSITION;
  log(L_CellInstancing, "Attempting to remove cell instance %u.", cell_instance_number_to_remove);

  if (cell_instance_number_to_remove < cell_instancing_vbos->n_cell_instances &&
      cell_instance_number_to_remove != INVALID_CELL_INSTANCE_POSITION)
  {
    glBindBuffer(GL_ARRAY_BUFFER, cell_instancing_vbos->cell_instances_vbo);

    // Reduce size of cell instances by one, this is also now indicating the position of the cell we need to move into the removed cell's slot.
    --cell_instancing_vbos->n_cell_instances;
    u32 cell_to_move = cell_instancing_vbos->n_cell_instances;

    if (cell_to_move == 0 ||
        cell_instance_number_to_remove == cell_to_move)
    {
      // All cell instances have been removed, or the cell we were removing was at the end of the array, hence no more action is needed.
    }
    else
    {
      u32 cell_to_remove_position = cell_instance_number_to_remove * sizeof(CellInstance);
      u32 cell_to_move_position = cell_to_move * sizeof(CellInstance);

      glCopyBufferSubData(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, cell_to_move_position, cell_to_remove_position, sizeof(CellInstance));

      // Need to update the main Cell record with the moved instance's new position.
      // TODO: If we could figure out a way around this it would be nice...
      u32 moved_cell_position = cell_to_remove_position;

      CellInstance moved_cell_instance;
      glGetBufferSubData(GL_ARRAY_BUFFER, moved_cell_position, sizeof(CellInstance), &moved_cell_instance);

      Cell *moved_cell = get_cell(maze, moved_cell_instance.world_cell_position_x, moved_cell_instance.world_cell_position_y);
      if (moved_cell)
      {
        moved_cell->opengl_instance_position = moved_cell_position;
      }
      else
      {
        log(L_CellInstancing, "Could not get cell being moved from the Maze to update it's instance position");
      }
    }
  }

  log(L_CellInstancing, "n_cell_instances: %u, out of cell_instances_vbo_size: %u", cell_instancing_vbos->n_cell_instances, cell_instancing_vbos->cell_instances_vbo_size);
}


void
draw_instanced_cells(CellInstancingVBOs *cell_instancing_vbos, Panning *panning, mat4 projection_matrix)
{
  Uniforms *uniforms = &cell_instancing_vbos->uniforms;

  glUniformMatrix4fv(uniforms->mat4_projection_matrix.location, 1, GL_TRUE, projection_matrix.es);

  glUniform1i(uniforms->int_render_origin_cell_x.location, panning->world_maze_pos.cell_x);
  glUniform1i(uniforms->int_render_origin_cell_y.location, panning->world_maze_pos.cell_y);
  glUniform2f(uniforms->vec2_render_origin_offset.location, panning->world_maze_pos.offset.x, panning->world_maze_pos.offset.y);
  glUniform1f(uniforms->float_scale.location, panning->zoom);

  glDrawElementsInstanced(GL_TRIANGLES, cell_instancing_vbos->n_cell_indices, GL_UNSIGNED_SHORT, 0, cell_instancing_vbos->n_cell_instances);
}


b32
setup_cell_instancing(CellInstancingVBOs *cell_instancing_vbos)
{
  b32 success = true;

  const char *filenames[] = {"vertex-shader.glvs", "fragment-shader.glfs"};
  GLenum shader_types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};

  success &= create_shader_program(filenames, shader_types, array_count(shader_types), &cell_instancing_vbos->shader_program);
  glUseProgram(cell_instancing_vbos->shader_program);

  setup_vao();
  setup_cell_vertex_vbo_and_ibo(cell_instancing_vbos, CELL_VERTICES, array_count(CELL_VERTICES),
                                                      CELL_TRIANGLE_INDICES, array_count(CELL_TRIANGLE_INDICES));

  cell_instancing_vbos->cell_instances_vbo = create_buffer();
  setup_cell_instances_vbo(cell_instancing_vbos);
  cell_instancing_vbos->cell_instances_vbo_size = 0;

  Uniforms *uniforms = &cell_instancing_vbos->uniforms;
  uniforms->mat4_projection_matrix.name = "projection_matrix";
  uniforms->int_render_origin_cell_x.name = "render_origin_cell_x";
  uniforms->int_render_origin_cell_y.name = "render_origin_cell_y";
  uniforms->vec2_render_origin_offset.name = "render_origin_offset";
  uniforms->float_scale.name = "scale";
  success &= get_uniform_locations(cell_instancing_vbos->shader_program, uniforms->array, array_count(uniforms->array));

  success &= print_gl_errors();
  log(L_CellInstancing, "OpenGL VBOs setup finished.");

  return success;
}