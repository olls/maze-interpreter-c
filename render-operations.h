const u32 MAX_RENDER_OPS = 512;


enum RenderOperationType
{
  RENDER_OP_CIRCLE,
  RENDER_OP_BOX,
  RENDER_OP_FAST_BOX,
  RENDER_OP_LINE,
  RENDER_OP_BITMAP
};


struct RenderOperation
{
  RenderOperationType type;
  RenderBasis render_basis;

  union
  {
    struct
    {
      V2 world_pos;
      r32 world_radius;
      V4 color;
    } circle;
    struct
    {
      Rectangle box;
      V4 color;
    } box;
    struct
    {
      Rectangle box;
      PixelColor color;
    } fast_box;
    struct
    {
      V2 world_start;
      V2 world_end;
      V4 color;
      r32 width;
    } line;
    struct
    {
      Bitmap *bitmap;
      V2 pos;
      BlitBitmapOptions opts;
    } bitmap;
  };
};


struct RenderOperations
{
  RenderOperation ops[MAX_RENDER_OPS];
  u32 next_free;
};