/** The rasterizer for the 'dense' renderer */

#include <stdio.h>
#undef FT_COMPONENT
#define FT_COMPONENT dense

#include <freetype/ftoutln.h>
#include <freetype/internal/ftcalc.h>
#include <freetype/internal/ftdebug.h>
#include <freetype/internal/ftobjs.h>
#include "ftdense.h"

#include <math.h>
#include <tmmintrin.h>
#include "ftdenseerrs.h"

#define PIXEL_BITS 8

#define ONE_PIXEL  ( 1 << PIXEL_BITS )
#define TRUNC( x ) (int)( ( x ) >> PIXEL_BITS )

#define UPSCALE( x )   ( ( x ) * ( ONE_PIXEL >> 6 ) )
#define DOWNSCALE( x ) ( ( x ) >> ( PIXEL_BITS - 6 ) )

typedef struct dense_TRaster_
{
  void* memory;

} dense_TRaster, *dense_PRaster;

static FT_Vector
Lerp( float aT, FT_Vector aP0, FT_Vector aP1 )
{
  FT_Vector p;
  p.x = aP0.x + aT * ( aP1.x - aP0.x );
  p.y = aP0.y + aT * ( aP1.y - aP0.y );
  return p;
}

static int
dense_move_to( const FT_Vector* to, dense_worker* worker )
{
  TPos x, y;

  x              = UPSCALE( to->x );
  y              = UPSCALE( to->y );
  worker->prev_x = x;
  worker->prev_y = y;
  // printf( "last point is {%f, %f}", lp.m_x, lp.m_y );
  return 0;
}

static int
dense_line_to( const FT_Vector* to, dense_worker* worker )
{
  //printf( "dense_line_to: %d, %d\n", to->x, to->y );
  dense_render_line( worker, UPSCALE( to->x ), UPSCALE( to->y ) );
  dense_move_to( to, worker );
  return 0;
}

FT26D6 max(FT26D6 x, FT26D6 y){
  if(x > y)
    return x;
  return y;
}

FT26D6 min(FT26D6 x, FT26D6 y){
  if(x < y)
    return x;
  return y;
}

void
swap( long int* a, long int* b )
{
  long int temp = *a;
  *a            = *b;
  *b            = temp;
}


void
dense_render_line( dense_worker* worker, TPos tox, TPos toy )
{
  // printf("line from: %d, %d to %d, %d\n", worker->prev_x, worker->prev_y,
  // to_x, to_y);


  FT26D6 fx = worker->prev_x>>2;
  FT26D6 fy = worker->prev_y>>2;


  // float from_x = fx;
  // float from_y = fy;
    
  FT26D6 from_x = fx;
  FT26D6 from_y = fy;


  // from_x /= 64.0;
  // from_y /= 64.0;

  FT26D6 tx = tox>>2;
  FT26D6 ty = toy>>2;

  if ( fy == ty )
    return;

  //  float to_x = tx / 64.0;
  //  float to_y = ty / 64.0;
  FT26D6 to_x = tx;
  FT26D6 to_y = ty;



  //printf("line from: %f, %f to %f, %f\n", from_x, from_y, to_x, to_y);

  int dir = 1;
  if ( from_y >= to_y )
  {
    dir = -1;
    swap( &from_x, &to_x );
    swap( &from_y, &to_y );
  }

  // Clip to the height.
  if ( from_y >= worker->m_h<<6 || to_y <= 0 )
    return;

  //float dxdy = ( to_x - from_x ) / (float)( to_y - from_y );
  FT26D6 deltax,deltay;
  deltax = to_x - from_x;
  deltay = to_y - from_y;

  if ( from_y < 0 )
  {
    from_x -= from_y * deltax/deltay;
    from_y = 0;
  }

  // TODO: test, imp line
  if ( to_y > worker->m_h<<6 )
  {
    to_x -= (( to_y - worker->m_h<<6 ) * deltax/deltay);
    to_y = worker->m_h<<6;
  }

  int  x       = from_x;
  int    y0      = from_y>>6;
  int    y_limit = (to_y + 0x3f)>>6;
  FT20D12* m_a     = worker->m_a;

  for ( int y = y0; y < y_limit; y++ )
  {
   // printf("y is %d\n", y);
    int   linestart = y * worker->m_w;
    FT26D6 dy        = min( (y + 1)<<6, to_y ) - max( y<<6, from_y );
    FT26D6 xnext     = x + dy * deltax/deltay;
    FT26D6 d         = dy * dir;

    FT26D6 x0, x1;
    if ( x < xnext )
    {
      x0 = x;
      x1 = xnext;
    }
    else
    {
      x0 = xnext;
      x1 = x;
    }

    /*
    It's possible for x0 to be negative on the last scanline because of
    floating-point inaccuracy That would cause an out-of-bounds array access at
    index -1.
    */
    // float x0floor = x0 <= 0.0f ? 0.0f : (float)floor( x0 );

    int   x0i    = x0>>6;
    FT26D6 x0floor = x0i<<6;


    // float x1ceil = (float)ceil( x1 );
    int   x1i    = (x1+0x3f)>>6;
    FT26D6 x1ceil =  x1i <<6;

    if ( x1i <= x0i + 1 )
    {
      //printf("Hit condition 1\n");
      FT26D6 xmf = ( ( x + xnext )>>1) - x0floor;
      m_a[linestart + x0i] += d * ((1<<6) - xmf);
      m_a[linestart + ( x0i + 1 )] += d * xmf;
    }
    else
    {
      //printf("Hit condition 2\n");
      // float s   = 1.0f / ( x1 - x0 );
      // float x0f = x0 - x0floor;
      // float a0  = 0.5f * s * ( 1.0f - x0f ) * ( 1.0f - x0f );
      // float x1f = x1 - x1ceil + 1.0f;
      // float am  = 0.5f * s * x1f * x1f;

      FT26D6 oneOverS = x1 - x0;
      FT26D6 x0f = x0 - x0floor;


      FT26D6 oneMinusX0f = (1<<6) - x0f;
			FT26D6 a0 = ((oneMinusX0f * oneMinusX0f) >> 1) / oneOverS;
			FT26D6 x1f = x1 - x1ceil + 1<<6;
			FT26D6 am = ((x1f * x1f) >> 1) / oneOverS;


      m_a[linestart + x0i] += d * a0;

      if ( x1i == x0i + 2 )
        m_a[linestart + ( x0i + 1 )] += d * ( (1<<6) - a0 - am );
      else
      {
        //printf("Hit condition 3\n");
        FT26D6 a1 = (((1<<6) + (1<<5) - x0f) << 6) / oneOverS;
        m_a[linestart + ( x0i + 1 )] += d * ( a1 - a0 );

        FT26D6 dTimesS = (d << 12) / oneOverS;
        
        for ( FT26D6 xi = x0i + 2; xi < x1i - 1; xi++ )
          m_a[linestart + xi] += dTimesS;

        FT26D6 a2 = a1 + (( x1i - x0i - 3 )<<12)/oneOverS;
        m_a[linestart + ( x1i - 1 )] += d * ( (1<<6) - a2 - am );
      }
      m_a[linestart + x1i] += d * am;
    }
    x = xnext;
  }
}

static int
dense_conic_to( const FT_Vector* control,
                const FT_Vector* to,
                dense_worker*    worker )
{
  //printf( "dense_conic_to: %d, %d\n", to->x, to->y );
  dense_render_quadratic( worker, control, to );
  return 0;
}

void
dense_render_quadratic( dense_worker*    worker,
                        const FT_Vector* control,
                        const FT_Vector* to )
{
  /*
  Calculate devsq as the square of four times the
  distance from the control point to the midpoint of the curve.
  This is the place at which the curve is furthest from the
  line joining the control points.

  4 x point on curve = p0 + 2p1 + p2
  4 x midpoint = 4p1

  The division by four is omitted to save time.
  */

  FT_Vector aP0 = { DOWNSCALE( worker->prev_x ), DOWNSCALE( worker->prev_y ) };
  FT_Vector aP1 = { control->x, control->y };
  FT_Vector aP2 = { to->x, to->y };

  float devx  = aP0.x - aP1.x - aP1.x + aP2.x;
  float devy  = aP0.y - aP1.y - aP1.y + aP2.y;
  float devsq = devx * devx + devy * devy;

  if ( devsq < 0.333f )
  {
    dense_line_to( &aP2, worker );
    return;
  }

  /*
  According to Raph Levien, the reason for the subdivision by n (instead of
  recursive division by the Casteljau system) is that "I expect the flatness
  computation to be semi-expensive (it's done once rather than on each potential
  subdivision) and also because you'll often get fewer subdivisions. Taking a
  circular arc as a simplifying assumption, where I get n, a recursive approach
  would get 2^ceil(lg n), which, if I haven't made any horrible mistakes, is
  expected to be 33% more in the limit".
  */

  const float tol = 3.0f;
  int         n   = (int)floor( sqrt( sqrt( tol * devsq ) ) )/8;
  //printf( "n is %d\n", n );
  FT_Vector p      = aP0;
  float     nrecip = 1.0f / ( n + 1.0f );
  float     t      = 0.0f;
  for ( int i = 0; i < n; i++ )
  {
    t += nrecip;
    FT_Vector next = Lerp( t, Lerp( t, aP0, aP1 ), Lerp( t, aP1, aP2 ) );
    dense_line_to(&next, worker );
    p              = next;
  }

  dense_line_to( &aP2, worker );
  // worker->prev_x = aP2.x;
  // worker->prev_y = aP2.y;
}

static int
dense_cubic_to( const FT_Vector* control1,
                const FT_Vector* control2,
                const FT_Vector* to,
                dense_worker*    worker )
{
  dense_render_cubic( worker, control1, control2, to );
  return 0;
}

void
dense_render_cubic( dense_worker* worker,
                    FT_Vector*    aP1,
                    FT_Vector*    aP2,
                    FT_Vector*    aP3 )
{
  // assert( worker );
  FT_Vector aP0    = { worker->prev_x, worker->prev_y };
  float     devx   = aP0.x - aP1->x - aP1->x + aP2->x;
  float     devy   = aP0.y - aP1->y - aP1->y + aP2->y;
  float     devsq0 = devx * devx + devy * devy;
  devx             = aP1->x - aP2->x - aP2->x + aP3->x;
  devy             = aP1->y - aP2->y - aP2->y + aP3->y;
  float devsq1     = devx * devx + devy * devy;
  float devsq      = fmax( devsq0, devsq1 );

  if ( devsq < 0.333f )
  {
    dense_render_line( worker, aP3->x, aP3->y );
    return;
  }

  const float tol    = 3.0f;
  int         n      = (int)floor( sqrt( sqrt( tol * devsq ) ) );
  FT_Vector   p      = aP0;
  float       nrecip = 1.0f / ( n + 1.0f );
  float       t      = 0.0f;
  for ( int i = 0; i < n; i++ )
  {
    t += nrecip;
    FT_Vector a    = Lerp( t, Lerp( t, aP0, *aP1 ), Lerp( t, *aP1, *aP2 ) );
    FT_Vector b    = Lerp( t, Lerp( t, *aP1, *aP2 ), Lerp( t, *aP2, *aP3 ) );
    FT_Vector next = Lerp( t, a, b );
    dense_render_line( worker, next.x, next.y );
    worker->prev_x = next.x;
    worker->prev_y = next.y;
    p              = next;
  }

  dense_render_line( worker, aP3->x, aP3->y );
  worker->prev_x = aP3->x;
  worker->prev_y = aP3->y;
}

static int
dense_raster_new( FT_Memory memory, dense_PRaster* araster )
{
  FT_Error      error;
  dense_PRaster raster;

  if ( !FT_NEW( raster ) )
    raster->memory = memory;

  *araster = raster;
  return error;
}

static void
dense_raster_done( FT_Raster raster )
{
  FT_Memory memory = (FT_Memory)( (dense_PRaster)raster )->memory;

  FT_FREE( raster );
}

static void
dense_raster_reset( FT_Raster      raster,
                    unsigned char* pool_base,
                    unsigned long  pool_size )
{
  FT_UNUSED( raster );
  FT_UNUSED( pool_base );
  FT_UNUSED( pool_size );
}

static int
dense_raster_set_mode( FT_Raster raster, unsigned long mode, void* args )
{
  FT_UNUSED( raster );
  FT_UNUSED( mode );
  FT_UNUSED( args );

  return 0; /* nothing to do */
}

FT_DEFINE_OUTLINE_FUNCS( dense_decompose_funcs,

                         (FT_Outline_MoveTo_Func)dense_move_to,   /* move_to  */
                         (FT_Outline_LineTo_Func)dense_line_to,   /* line_to  */
                         (FT_Outline_ConicTo_Func)dense_conic_to, /* conic_to */
                         (FT_Outline_CubicTo_Func)dense_cubic_to, /* cubic_to */

                         0, /* shift    */
                         0  /* delta    */
)

static int
dense_render_glyph( dense_worker* worker, const FT_Bitmap* target )
{
  FT_Error error = FT_Outline_Decompose( &( worker->outline ),
                                         &dense_decompose_funcs, worker );
  // Render into bitmap
  const FT20D12* source = worker->m_a;

  unsigned char* dest     = target->buffer;
  unsigned char* dest_end = target->buffer + worker->m_w * worker->m_h;

  // __m128 offset = _mm_setzero_ps();
  // __m128i mask = _mm_set1_epi32(0x0c080400);
  // __m128 sign_mask = _mm_set1_ps(-0.f);
  // for (int i = 0; i < worker->m_h*worker->m_w; i += 4) {
  //   __m128 x = _mm_load_ps(&source[i]);
  //   x = _mm_add_ps(x, _mm_castsi128_ps(_mm_slli_si128(_mm_castps_si128(x), 4)));
  //   x = _mm_add_ps(x, _mm_shuffle_ps(_mm_setzero_ps(), x, 0x40));
  //   x = _mm_add_ps(x, offset);
  //   __m128 y = _mm_andnot_ps(sign_mask, x);  // fabs(x)
  //   y = _mm_min_ps(y, _mm_set1_ps(1.0f));
  //   y = _mm_mul_ps(y, _mm_set1_ps(255.0f));
  //   __m128i z = _mm_cvtps_epi32(y);
  //   z = _mm_shuffle_epi8(z, mask);
  //   _mm_store_ss((float *)&dest[i], (__m128)z);
  //   offset = _mm_shuffle_ps(x, x, _MM_SHUFFLE(3, 3, 3, 3));
  // }

  FT20D12 valnew = 0;
  //float          value    = 0.0f;
  while ( dest < dest_end )
  {
    valnew += *source++;

   // printf("%d\n", *source);

    if(valnew > 0){
      int nnew = valnew >>2;

      if(nnew>255)nnew=255;
      *dest = (unsigned char)nnew;
    }else{
      *dest = 0;
    }
    dest++;
  }

  // float          value    = 0.0f;
  // while ( dest < dest_end )
  // {
  //   value += *source++;
  //   if ( value > 0.0f )
  //   {
  //     int n = (int)( fabs( value ) * 255.0f + 0.5f );
  //     if ( n > 255 )
  //       n = 255;
  //     *dest = (unsigned char)n;
  //   }
  //   else
  //     *dest = 0;
  //   dest++;
  // }

  free(worker->m_a);
  return error;
}

static int
dense_raster_render( FT_Raster raster, const FT_Raster_Params* params )
{
  const FT_Outline* outline    = (const FT_Outline*)params->source;
  FT_Bitmap*  target_map = params->target;

  // dense_worker* worker = malloc( sizeof( dense_worker ) );
  dense_worker worker[1];

  if ( !raster )
    return FT_THROW( Invalid_Argument );

  if ( !outline )
    return FT_THROW( Invalid_Outline );

  worker->outline = *outline;

  if ( !target_map )
    return FT_THROW( Invalid_Argument );

  /* nothing to do */
  if ( !target_map->width || !target_map->rows )
    return 0;

  if ( !target_map->buffer )
    return FT_THROW( Invalid_Argument );

  worker->m_origin_x = 0;
  worker->m_origin_y = 0;
  worker->m_w = target_map->pitch;
  worker->m_h = target_map->rows;

  int size = worker->m_w * worker->m_h + 4;

  worker->m_a      = malloc( sizeof( FT20D12 ) * size );
  worker->m_a_size = size;

  memset( worker->m_a, 0, ( sizeof( FT20D12 ) * size ) );
  /* exit if nothing to do */
  if ( worker->m_w <= worker->m_origin_x || worker->m_h <= worker->m_origin_y )
  {
    return 0;
  }

  // Invert the pitch to account for different +ve y-axis direction in dense array
  // (maybe temporary solution)
  target_map->pitch *= -1;
  return dense_render_glyph( worker, target_map );
}

FT_DEFINE_RASTER_FUNCS(
    ft_dense_raster,

    FT_GLYPH_FORMAT_OUTLINE,

    (FT_Raster_New_Func)dense_raster_new,           /* raster_new      */
    (FT_Raster_Reset_Func)dense_raster_reset,       /* raster_reset    */
    (FT_Raster_Set_Mode_Func)dense_raster_set_mode, /* raster_set_mode */
    (FT_Raster_Render_Func)dense_raster_render,     /* raster_render   */
    (FT_Raster_Done_Func)dense_raster_done          /* raster_done     */
)

/* END */