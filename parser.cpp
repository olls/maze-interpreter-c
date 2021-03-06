const u8 *
get_direction(const u8 *ptr, vec2 *result)
{
  *result = STATIONARY;

  if (ptr[0] == '%')
  {
    switch (ptr[1])
    {
      case 'U':
      case 'u':
      {
        *result = UP;
        ptr += 2;
      } break;
      case 'D':
      case 'd':
      {
        *result = DOWN;
        ptr += 2;
      } break;
      case 'L':
      case 'l':
      {
        *result = LEFT;
        ptr += 2;
      } break;
      case 'R':
      case 'r':
      {
        *result = RIGHT;
        ptr += 2;
      } break;
    }
  }

  return ptr;
}


const u8 *
parse_function_definition(Functions *functions, const u8 cell_str[2], const u8 *f_ptr, const u8 *f_end, Cell *cell)
{
  u32 function_index = get_function_index(cell_str);

  const u8 *start_f_ptr = f_ptr;

  consume_whitespace(f_ptr, f_end);

  if (str_eq(f_ptr, u8("->"), 2))
  {
    // Function Definition
    log(L_Parser, u8("Parsing function definition,"));

    f_ptr += 2;

    Function *function = functions->hash_table + function_index;

    if (function->type != FUNCTION_NULL)
    {
      log(L_Parser, u8("Function already defined, ignore this definition."));
    }
    else
    {
      function->name[0] = cell_str[0];
      function->name[1] = cell_str[1];

      consume_whitespace(f_ptr, f_end);

      if (*f_ptr == '=')
      {
        log(L_Parser, u8("  Type: Assignment,"));

        ++f_ptr;
        consume_whitespace(f_ptr, f_end);

        u32 assignment_value;
        const u8 *end_num_f_ptr = get_num(f_ptr, f_end, &assignment_value);

        if (end_num_f_ptr == f_ptr)
        {
          function->type = FUNCTION_NULL;
          log(L_Parser, u8("  Function invalid: Assignment value missing."));
        }
        else
        {
          log(L_Parser, u8("  Value: %d"), assignment_value);
          f_ptr = end_num_f_ptr;

          function->value = assignment_value;
          function->type = FUNCTION_ASSIGNMENT;
        }
      }
      else if ((f_ptr[0] == '+' ||
                f_ptr[0] == '-' ||
                f_ptr[0] == '*' ||
                f_ptr[0] == '/') && f_ptr[1] == '=')
      {
        log(L_Parser, u8("  Type: Operator,"));

        u8 sign = f_ptr[0];

        f_ptr += 2;
        consume_whitespace(f_ptr, f_end);

        u32 assignment_value;
        const u8 *end_num_f_ptr = get_num(f_ptr, f_end, &assignment_value);
        if (end_num_f_ptr == f_ptr)
        {
          function->type = FUNCTION_NULL;
          log(L_Parser, u8("  Invalid function: Operator value missing."));
        }
        else
        {
          log(L_Parser, u8("  Value: %d"), assignment_value);
          f_ptr = end_num_f_ptr;

          function->value = assignment_value;

          switch (sign)
          {
            case '+':
            {
              function->type = FUNCTION_INCREMENT;
            } break;
            case '-':
            {
              function->type = FUNCTION_DECREMENT;
            } break;
            case '*':
            {
              function->type = FUNCTION_MULTIPLY;
            } break;
            case '/':
            {
              function->type = FUNCTION_DIVIDE;
            } break;
          }
        }
      }
      else if (str_eq(f_ptr, u8("IF"), 2) || str_eq(f_ptr, u8("if"), 2))
      {
        log(L_Parser, u8("  Type: Conditional,"));

        f_ptr += 2;
        consume_whitespace(f_ptr, f_end);

        FunctionType conditional_func_type;
        b32 valid_condition = true;

        if (str_eq(f_ptr, u8("<="), 2))
        {
          f_ptr += 2;
          conditional_func_type = FUNCTION_LESS_EQUAL;
        }
        else if (*f_ptr == '<')
        {
          f_ptr += 1;
          conditional_func_type = FUNCTION_LESS;
        }
        else if (str_eq(f_ptr, u8(">="), 2))
        {
          f_ptr += 2;
          conditional_func_type = FUNCTION_GREATER_EQUAL;
        }
        else if (*f_ptr == '>')
        {
          f_ptr += 1;
          conditional_func_type = FUNCTION_GREATER;
        }
        else if (str_eq(f_ptr, u8("=="), 2))
        {
          f_ptr += 2;
          conditional_func_type = FUNCTION_EQUAL;
        }
        else if (str_eq(f_ptr, u8("!="), 2))
        {
          f_ptr += 2;
          conditional_func_type = FUNCTION_NOT_EQUAL;
        }
        else
        {
          log(L_Parser, u8("  Invalid function: Conditional operator missing."));
          function->type = FUNCTION_NULL;
          valid_condition = false;
        }

        if (valid_condition)
        {
          consume_whitespace(f_ptr, f_end);

          u32 condition_value;
          const u8 *end_num_f_ptr = get_num(f_ptr, f_end, &condition_value);
          if (end_num_f_ptr == f_ptr)
          {
            log(L_Parser, u8("  Invalid function: Missing condition value."));
            function->type = FUNCTION_NULL;
          }
          else
          {
            log(L_Parser, u8("  Condition value: %d"), condition_value);
            f_ptr = end_num_f_ptr;

            consume_whitespace(f_ptr, f_end);

            if (!(str_eq(f_ptr, u8("THEN"), 4) || str_eq(f_ptr, u8("then"), 4)))
            {
              log(L_Parser, u8("  Invalid function: Missing 'THEN' keyword."));
              function->type = FUNCTION_NULL;
            }
            else
            {
              f_ptr += 4;
              consume_whitespace(f_ptr, f_end);

              vec2 true_direction;
              const u8 *end_true_direction_f_ptr = get_direction(f_ptr, &true_direction);
              if (end_true_direction_f_ptr == f_ptr)
              {
                log(L_Parser, u8("  Invalid function: Missing 'THEN' direction."));
                function->type = FUNCTION_NULL;
              }
              else
              {
                log(L_Parser, u8("  THEN direction: (%f, %f)"), true_direction.x, true_direction.y);
                f_ptr = end_true_direction_f_ptr;

                consume_whitespace(f_ptr, f_end);

                b32 valid_conditional_function = true;
                b32 else_exists = false;
                vec2 false_direction;

                // ELSE is optional
                if (str_eq(f_ptr, u8("ELSE"), 4) || str_eq(f_ptr, u8("else"), 4))
                {
                  else_exists = true;

                  f_ptr += 4;
                  consume_whitespace(f_ptr, f_end);

                  const u8 *end_false_direction_f_ptr = get_direction(f_ptr, &false_direction);
                  if (end_false_direction_f_ptr == f_ptr)
                  {
                    log(L_Parser, u8("Invalid function: Missing 'ELSE' direction."));
                    valid_conditional_function = false;
                    function->type = FUNCTION_NULL;
                  }
                  else
                  {
                    log(L_Parser, u8("  ELSE direction: (%f, %f)"), false_direction.x, false_direction.y);
                    f_ptr = end_false_direction_f_ptr;

                    valid_conditional_function = true;
                  }
                }

                if (valid_conditional_function)
                {
                  function->type = conditional_func_type;
                  function->conditional.value = condition_value;
                  function->conditional.true_direction = true_direction;

                  function->conditional.else_exists = else_exists;
                  if (else_exists)
                  {
                    function->conditional.false_direction = false_direction;
                  }
                }

              }
            }
          }
        }
      }
    }

    if (function->type != FUNCTION_NULL)
    {
      ++functions->n_functions;
    }

    consume_until_newline(f_ptr, f_end);
  }
  else
  {
    f_ptr = start_f_ptr;
  }

  return f_ptr;
}


const u8 *
parse_cell(Maze *maze, Functions *functions, const u8 cell_str[2], const u8 *f_ptr, const u8 *f_end, Cell *cell)
{
  if (str_eq(cell_str, u8("^^"), 2))
  {
    cell->type = CELL_START;
  }
  else if (str_eq(cell_str, u8(".."), 2))
  {
    cell->type = CELL_PATH;
  }
  else if (str_eq(cell_str, u8("##"), 2) ||
           str_eq(cell_str, u8("  "), 2) ||
           str_eq(cell_str, u8("``"), 2))
  {
    cell->type = CELL_WALL;
  }
  else if (str_eq(cell_str, u8("()"), 2))
  {
    cell->type = CELL_HOLE;
  }
  else if (str_eq(cell_str, u8("<>"), 2))
  {
    cell->type = CELL_SPLITTER;
  }
  else if (is_letter(cell_str[0]) && (is_letter(cell_str[1]) || is_num(cell_str[1])))
  {
    const u8 *end_function_name_f_ptr = f_ptr + 2;
    const u8 *end_function_definition_f_ptr = parse_function_definition(functions, cell_str, end_function_name_f_ptr, f_end, cell);
    if (end_function_definition_f_ptr != end_function_name_f_ptr)
    {
      f_ptr = end_function_definition_f_ptr;
    }
    else
    {
      cell->type = CELL_FUNCTION;
      cell->function_index = get_function_index(cell_str);
    }

  }
  else if (str_eq(cell_str, u8("--"), 2))
  {
    cell->type = CELL_ONCE;
  }
  else if ((cell_str[0] == '*') && ((cell_str[1] == 'U') ||
                                    (cell_str[1] == 'u')))
  {
    cell->type = CELL_UP_UNLESS_DETECT;
  }
  else if ((cell_str[0] == '*') && ((cell_str[1] == 'D') ||
                                    (cell_str[1] == 'd')))
  {
    cell->type = CELL_DOWN_UNLESS_DETECT;
  }
  else if ((cell_str[0] == '*') && ((cell_str[1] == 'L') ||
                                    (cell_str[1] == 'l')))
  {
    cell->type = CELL_LEFT_UNLESS_DETECT;
  }
  else if ((cell_str[0] == '*') && ((cell_str[1] == 'R') ||
                                    (cell_str[1] == 'r')))
  {
    cell->type = CELL_RIGHT_UNLESS_DETECT;
  }
  else if (str_eq(cell_str, u8(">>"), 2))
  {
    cell->type = CELL_OUT;
  }
  else if (str_eq(cell_str, u8("<<"), 2))
  {
    cell->type = CELL_INP;
  }
  else if ((cell_str[0] == '%') && ((cell_str[1] == 'U') ||
                                    (cell_str[1] == 'u')))
  {
    cell->type = CELL_UP;
  }
  else if ((cell_str[0] == '%') && ((cell_str[1] == 'D') ||
                                    (cell_str[1] == 'd')))
  {
    cell->type = CELL_DOWN;
  }
  else if ((cell_str[0] == '%') && ((cell_str[1] == 'L') ||
                                    (cell_str[1] == 'l')))
  {
    cell->type = CELL_LEFT;
  }
  else if ((cell_str[0] == '%') && ((cell_str[1] == 'R') ||
                                    (cell_str[1] == 'r')))
  {
    cell->type = CELL_RIGHT;
  }
  else if (is_num(cell_str[0]) && is_num(cell_str[1]))
  {
    cell->type = CELL_PAUSE;

    u32 digit0 = cell_str[0] - '0';
    u32 digit1 = cell_str[1] - '0';
    cell->pause = (10 * digit0) + digit1;
  }

  if (cell->type != CELL_NULL)
  {
    f_ptr += 2;
  }

  return f_ptr;
}


// TODO: Parse comments!


bool
parse(Maze *maze, Functions *functions, Memory *memory, const u8 *filename)
{
  bool success = true;

  clear_maze(maze);
  zero(functions, Functions);

  File file;
  success &= open_file(filename, &file);
  if (success)
  {
    maze->tree.bounds = (Rectangle){(vec2){0, 0}, (vec2){MAX_MAZE_SIZE, MAX_MAZE_SIZE}};

    u32 x = 0;
    u32 y = 0;

    u8 cell_str[2] = {};
    const u8 *f_ptr = file.text;
    const u8 *f_end = f_ptr + file.size;
    while (f_ptr < f_end)
    {
      cell_str[0] = f_ptr[0];
      cell_str[1] = f_ptr[1];

      Cell new_cell = {};
      new_cell.type = CELL_NULL;
      f_ptr = parse_cell(maze, functions, cell_str, f_ptr, f_end, &new_cell);

      if (new_cell.type != CELL_NULL)
      {
        Cell *cell = create_new_cell(maze, x, y, memory);

        cell->type = new_cell.type;
        cell->pause = new_cell.pause;
        cell->function_index = new_cell.function_index;

        cell->name[0] = cell_str[0];
        cell->name[1] = cell_str[1];

        log_s(L_Parser, u8("%.2s "), cell_str);
        ++x;
      }
      else
      {
        if (cell_str[0] == '\n')
        {
          x = 0;
          ++y;
          log_s(L_Parser, u8("\n"));
        }
        f_ptr += 1;
      }
    }

    log_s(L_Parser, u8("\n"));

    close_file(&file);
  }

  return success;
}