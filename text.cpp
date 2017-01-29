#define consume_while(ptr, func, end) while (func(*(ptr)) && (ptr) < (end)) ++(ptr)
#define consume_whitespace(ptr, end) consume_while(ptr, is_whitespace, end)
#define consume_whitespace_nl(ptr, end) consume_while(ptr, is_whitespace_nl, end)
#define consume_until(ptr, func, end) while (!func(*(ptr)) && (ptr) < (end)) ++(ptr)
#define consume_until_char(ptr, c, end) while (*(ptr) != (c) && (ptr) < (end)) ++(ptr)
#define consume_until_newline(ptr, end) consume_until(ptr, is_newline, end)


bool
str_eq(const char *a, const char *b, u32 n)
{
  for (u32 i = 0; i < n; ++i)
  {
    if (a[i] != b[i])
    {
      return false;;
    }
  }

  return true;
}


b32
is_num(char character)
{
  b32 result = (character >= '0') && (character <= '9');
  return result;
}


b32
is_num_or_sign(char character)
{
  b32 result = is_num(character) || character == '-';
  return result;
}


b32
is_letter(char character)
{
  b32 result = (((character >= 'a') && (character <= 'z')) ||
                ((character >= 'A') && (character <= 'Z')));
  return result;
}


b32
is_newline(char character)
{
  b32 result = (character == '\n') || (character == '\r');
  return result;
}


b32
is_whitespace(char character)
{
  b32 result = (character == ' ') || (character == '\t');
  return result;
}


b32
is_whitespace_nl(char character)
{
  b32 result = is_whitespace(character) || is_newline(character);
  return result;
}


char *
get_num(char *ptr, char *f_end, u32 *result)
{
  char *num_start = ptr;

  consume_while(ptr, is_num, f_end);

  *result = 0;
  u32 num_length = ptr - num_start;

  for (u32 num_pos = 0;
       num_pos < num_length;
       ++num_pos)
  {
    u32 digit = *(num_start + num_pos) - '0';
    *result += digit * pow(10, (num_length - num_pos - 1));
  }

  return ptr;
}


char *
get_num(char *ptr, char *f_end, s32 *result)
{
  b32 coef = 1;
  if (*ptr == '-')
  {
    ++ptr;
    coef = -1;
  }

  char *num_start = ptr;

  consume_while(ptr, is_num, f_end);

  *result = 0;
  u32 num_length = ptr - num_start;

  for (u32 num_pos = 0;
       num_pos < num_length;
       ++num_pos)
  {
    u32 digit = *(num_start + num_pos) - '0';
    *result += digit * pow(10, (num_length - num_pos - 1));
  }

  *result *= coef;

  return ptr;
}


char*
get_num(char *ptr, char *f_end, r32 *result)
{
  s32 whole_num;
  ptr = get_num(ptr, f_end, &whole_num);

  *result = whole_num;

  if (*ptr == '.')
  {
    ++ptr;

    u32 frac_num;
    ptr = get_num(ptr, f_end, &frac_num);

    r32 frac_part = frac_num;
    while (frac_part > 1)
    {
      frac_part /= 10;
    }

    if (whole_num < 0)
    {
      frac_part *= -1;
    }

    *result += frac_part;
  }

  return ptr;
}