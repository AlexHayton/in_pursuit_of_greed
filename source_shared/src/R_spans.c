/***************************************************************************/
/*                                                                         */
/*                                                                         */
/* Raven 3D Engine                                                         */
/* Copyright (C) 1996 by Softdisk Publishing                               */
/*                                                                         */
/* Original Design:                                                        */
/*  John Carmack of id Software                                            */
/*                                                                         */
/* Enhancements by:                                                        */
/*  Robert Morgan of Channel 7............................Main Engine Code */
/*  Todd Lewis of Softdisk Publishing......Tools,Utilities,Special Effects */
/*  John Bianca of Softdisk Publishing..............Low-level Optimization */
/*  Carlos Hasan..........................................Music/Sound Code */
/*                                                                         */
/*                                                                         */
/***************************************************************************/

#include <STDLIB.H>
#include "d_global.h"
#include "r_refdef.h"
#include "d_video.h"
#include "d_misc.h"
#include "r_public.h"

/**** VARIABLES ****/

/*a scaled object is just encoded like a span                                                   */
spantag_t spantags[MAXSPANS];
spantag_t *starttaglist_p, *endtaglist_p;       // set by SortSpans
span_t   spans[MAXSPANS];
int      spansx[MAXSPANS];
int      spanx;
fixed_t  pointz, afrac, hfrac;
int      numspans;
span_t   *span_p;
int      sp_count;
int      sp_call;
int      mr_count;
int      mr_xfrac;
int      mr_yfrac;
int      mr_xstep;
int      mr_ystep;
byte     *sp_dest;          // the bottom most pixel to be drawn (in vie
byte     *sp_source;        // the first pixel in the vertical post (may
byte     *sp_colormap;      // pointer to a 256 byte color number to pal
int	 sp_frac;
int	 sp_fracstep;
byte	 *mr_picture;
int	 sp_loopvalue;
byte	 *mr_dest;
byte	 *mr_colormap;

/**** FUNCTIONS ****/

#define QUICKSORT_CUTOFF 16
#define SWAP(a,b)                \
 {                               \
  temp=a;                        \
  a=b;                           \
  b=temp;                        \
  }


void MedianOfThree(spantag_t *data,unsigned count)
{
 spantag_t temp;

 if (count>=3)
  {
   spantag_t *beg=data;
   spantag_t *mid=data + (count/2);
   spantag_t *end=data + (count-1);
   if (*beg>*mid)
    {
     if (*mid>*end)
      SWAP(*beg,*mid)
     else if (*beg>*end)
      SWAP(*beg,*end)
     }
   else if (*mid>*end)
    {
     if (*beg>*end)
      SWAP(*beg,*end)
     }
   else
    SWAP(*beg,*mid);
   }
 }


int Partition(spantag_t *data,unsigned count)
{
 spantag_t part=data[0];
 int       i=-1;
 int       j=count;
 spantag_t temp;

 while (i<j)
  {
   while (part>data[--j]);
   while (data[++i]>part);
   if (i>=j)
    break;
   SWAP(data[i],data[j]);
   }
 return j+1;
 }


void QuickSortHelper(spantag_t *data,unsigned count)
{
 int left=0;
 int part;

 if (count>QUICKSORT_CUTOFF)
  {
   while (count>1)
    {
     MedianOfThree(data+left,count);
     part=Partition(data+left,count);
     QuickSortHelper(data+left,part);
     left+=part;
     count-=part;
     }
   }
 }


void InsertionSort(spantag_t *data,unsigned count)
{
 int       i, j;
 spantag_t t;

 for (i=1;i<(int)count;i++)
  {
   if (data[i]>data[i-1])
    {
     t=data[i];
     for (j=i;j && t>data[j-1];j--)
      data[j]=data[j-1];
     data[j]=t;
     }
   }
 }


/*************************************************************************/

void DrawDoorPost(void)
{
 fixed_t top, bottom;    // precise y coordinates for post
 int     topy, bottomy;  // pixel y coordinates for post
 fixed_t fracadjust;     // the amount to prestep for the top pixel
 fixed_t scale;
 int     light;

 scale=TEXELSTEP(pointz);
 sp_source=span_p->picture;

 if (span_p->shadow==0)
  {
   light=(pointz>>FRACBITS)+span_p->light;
   if (light>MAXZLIGHT) return;
    else if (light<0) light=0;
   sp_colormap=zcolormap[light];
   }
 else if (span_p->shadow==1) sp_colormap=colormaps+(wallglow<<8);
 else if (span_p->shadow==2) sp_colormap=colormaps+(wallflicker1<<8);
 else if (span_p->shadow==3) sp_colormap=colormaps+(wallflicker2<<8);
 else if (span_p->shadow==4) sp_colormap=colormaps+(wallflicker3<<8);
 else if (span_p->shadow>=5 && span_p->shadow<=8)
  {
   if (wallcycle==span_p->shadow-5) sp_colormap=colormaps;
   else
    {
     light=(pointz>>FRACBITS)+span_p->light;
     if (light>MAXZLIGHT) light=MAXZLIGHT;
      else if (light<0) light=0;
     sp_colormap=zcolormap[light];
     }
   }
 else if (span_p->shadow==9)
  {
   light=(pointz>>FRACBITS)+span_p->light+wallflicker4;
   if (light>MAXZLIGHT) light=MAXZLIGHT;
    else if (light<0) light=0;
   sp_colormap=zcolormap[light];
   }

 /* `scale` above is the geometry step and projects the post; this is the
    texture step.  Identical while one texel was one world unit; see DrawWall. */
 sp_fracstep=TEXELSTEP(pointz)<<texscaleshift;
 top=FIXEDDIV(span_p->y,scale);
 topy=top>>FRACBITS;
 fracadjust=top&(FRACUNIT-1);
 sp_frac=FIXEDMUL(fracadjust,sp_fracstep);
 topy=CENTERY-topy;
 /* Doors are ordinary wall lumps, so the true wrap is the lump's own height
    (128 rows for every door in the archive) and this fixed 256 already lets a
    tall enough post read into the next column.  Left as it was rather than
    fixed here, so original mode stays byte-identical; scaled so a 4x pack
    wraps at the same *place*, just with four times the texels. */
 sp_loopvalue=(256<<texscaleshift)<<FRACBITS;
 if (topy<scrollmin)
  {
   sp_frac+=(scrollmin-topy)*sp_fracstep;
   if (sp_loopvalue>0)
        while (sp_frac>sp_loopvalue) sp_frac-=sp_loopvalue;
   topy=scrollmin;
   }
 bottom=FIXEDDIV(span_p->yh,scale);
 bottomy= bottom>=((CENTERY+scrollmin)<<FRACBITS) ?
  scrollmax-1 : CENTERY+(bottom>>FRACBITS);
 if (bottomy<=scrollmin || topy>=scrollmax) return;
 sp_count=bottomy-topy+1;
 sp_dest=viewylookup[bottomy-scrollmin]+spanx;

 if (span_p->spantype==sp_maskeddoor) ScaleMaskedPost();
  else ScalePost();
 }


void ScaleTransPost()
{
 pixel_t color;

 sp_dest-=windowWidth*(sp_count-1);     // go to the top
 --sp_loopvalue;
 while (--sp_count)
  {
   color=sp_source[sp_frac>>FRACBITS];
   if (color)
    *sp_dest=*(translookup[sp_colormap[color]-1]+*sp_dest);
   sp_dest+=windowWidth;
   sp_frac+=sp_fracstep;
   sp_frac&=sp_loopvalue;
   }
 }


void ScalePost()
{
	sp_dest -= windowWidth * (sp_count - 1);     // go to the top
	--sp_loopvalue;
	while ( sp_count > 0 )
	{
		*sp_dest = sp_source[sp_frac>>FRACBITS];
		sp_dest += windowWidth;
		sp_frac += sp_fracstep;
		sp_frac &= sp_loopvalue;
		sp_count--;
	}
}


void ScaleMaskedPost()
{
	pixel_t color;

	sp_dest -= windowWidth * (sp_count - 1);     // go to the top
	--sp_loopvalue;
	while ( sp_count > 0 )
	{
		color = sp_source[sp_frac>>FRACBITS];
		if ( color )
			*sp_dest = color;
		sp_dest += windowWidth;
		sp_frac += sp_fracstep;
		sp_frac &= sp_loopvalue;
		sp_count--;
	}
}


void MapRow()
{
	/* mr_xfrac/mr_yfrac are 16.16 *world* coordinates, and the shifts turn them
	   into a texel index.  Originally that was one texel per world unit and a
	   64x64 flat: >>(FRACBITS-6) & 63*64 for the row, >>FRACBITS & 63 for the
	   column.  With flatscaleshift texels per unit and a (1<<flatshift)-square
	   flat both shifts lose that many more bits and both masks widen, which for
	   a 4x pack is >>6 & 0xFF00 and >>14 & 0xFF.

	   Everything is hoisted into locals because these are globals and the
	   compiler cannot prove `*dest++` does not alias them -- without this it
	   reloads all four every pixel.  The write-back at the end is load-bearing:
	   DrawSpans skips the recompute when the next span is contiguous and relies
	   on this having advanced mr_xfrac/mr_yfrac. */
	int   ysh = FRACBITS - flatscaleshift - flatshift, xsh = FRACBITS - flatscaleshift;
	int   ymask = flatmask << flatshift, xmask = flatmask;
	byte *pic = mr_picture, *cmap = mr_colormap, *dest = mr_dest;
	fixed_t xf = mr_xfrac, yf = mr_yfrac, xs = mr_xstep, ys = mr_ystep;
	int   n = mr_count;

	while ( n > 0 )
	{
		*dest++ = cmap[pic[((yf >> ysh) & ymask) + ((xf >> xsh) & xmask)]];
		xf += xs;
		yf += ys;
		n--;
	}

	mr_xfrac = xf;
	mr_yfrac = yf;
	mr_dest  = dest;
	mr_count = 0;
}


void DrawSprite(void)
{
 fixed_t    leftx, scale, xfrac, fracstep;
 fixed_t    shapebottom, topheight, bottomheight;
 int        post, x, topy, bottomy, light, shadow, bitshift, shift;
 special_t  specialtype;
 scalepic_t *pic;
 byte       *collumn;
 scaleobj_t *sp;

 /********* floor shadows ***********/
 specialtype=(special_t)(span_p->shadow>>8);
 shadow=span_p->shadow&255;

 if (specialtype==st_maxlight) sp_colormap=colormaps;
 else if (specialtype==st_transparent) sp_colormap=colormaps;
 else if (shadow==0)
  {
   light=(pointz>>FRACBITS)+span_p->light;
   if (light>MAXZLIGHT) return;
    else if (light<0) light=0;
   sp_colormap=zcolormap[light];
   }
 else if (span_p->shadow==1) sp_colormap=colormaps+(wallglow<<8);
 else if (span_p->shadow==2) sp_colormap=colormaps+(wallflicker1<<8);
 else if (span_p->shadow==3) sp_colormap=colormaps+(wallflicker2<<8);
 else if (span_p->shadow==4) sp_colormap=colormaps+(wallflicker3<<8);
 else if (span_p->shadow>=5 && span_p->shadow<=8)
  {
   if (wallcycle==span_p->shadow-5) sp_colormap=colormaps;
   else
    {
     light=(pointz>>FRACBITS)+span_p->light;
     if (light>MAXZLIGHT) light=MAXZLIGHT;
      else if (light<0) light=0;
     sp_colormap=zcolormap[light];
     }
    }
 else if (shadow==9)
  {
   light=(pointz>>FRACBITS)+span_p->light+wallflicker4;
   if (light>MAXZLIGHT) light=MAXZLIGHT;
    else if (light<0) light=0;
   sp_colormap=zcolormap[light];
   }

 pic=(scalepic_t *)span_p->picture;
 sp=(scaleobj_t *)span_p->structure;

 /* scaleobj_t.scale is already a texel-density exponent: 1 for almost every
    sprite (art at 2 texels per world unit), 2 for a few.  A 4x sprite pack is
    two more doublings on top, applied here rather than by editing the ~40
    sprite_p->scale assignments in Spawn.c -- which keeps midgetmode and
    player.holoscale working, and keeps savegames valid, since sprites are
    persisted by type and respawned rather than restored field by field. */
 shift=sp->scale+spriteshift;
 bitshift=FRACBITS-shift;

 shapebottom=span_p->y;
 // project the x and height
 scale=FIXEDDIV(SCALE,pointz);
 fracstep=TEXELSTEP(pointz)<<shift;
 sp_fracstep=fracstep;
 leftx=span_p->x2;
 leftx-=pic->leftoffset<<bitshift;
 x=CENTERX+(FIXEDMUL(leftx,scale)>>FRACBITS);
 // step through the shape, drawing posts where visible
 xfrac=0;
 if (x<0)
  {
   xfrac-=fracstep * x;
   x=0;
   }
 sp_loopvalue=(256<<spriteshift)<<FRACBITS;

 for (; x<windowWidth; x++)
  {
   post=xfrac>>FRACBITS;
   if (post>=pic->width)
    return;   // shape finished drawing
   xfrac+=fracstep;
   if (pointz>=wallz[x] && (pointz>=wallz[x]+TILEUNIT || (specialtype!=st_noclip && specialtype!=st_transparent)))
    continue;
   // If the offset of the columns is zero then there is no data for the post
   if (SP_COLOFS(pic,post)==0)
    continue;
   collumn=(byte *)pic+SP_COLOFS(pic,post);
   topheight=shapebottom+(SP_TOP(collumn)<<bitshift);
   bottomheight=shapebottom+(SP_BOTTOM(collumn)<<bitshift);
   collumn+=SP_COLHDR;
   // scale a post

   bottomy=CENTERY - (FIXEDMUL(bottomheight,scale)>>FRACBITS);
   if (bottomy<scrollmin)
    continue;
   if (bottomy>=scrollmax)
    bottomy=scrollmax-1;

   topy=CENTERY-(FIXEDMUL(topheight,scale)>>FRACBITS);
   if (topy<scrollmin)
    {
     sp_frac=(scrollmin-topy)*sp_fracstep;
     topy=scrollmin;
     }
   else
    sp_frac=0;

   if (topy>=scrollmax)
    continue;

   sp_count=bottomy-topy+1;

   sp_dest=viewylookup[bottomy-scrollmin]+x;
   sp_source=collumn;
   if (specialtype==st_transparent)
    ScaleTransPost();
   else
    ScaleMaskedPost();
   }
 }


void DrawSpans(void)
/* Spans farther than MAXZ away should NOT have been entered into the list */
{
 spantag_t *spantag_p, tag;
 int      spannum;
 int      x2;
 fixed_t  lastz;                  // the pointz for which xystep is valid
 fixed_t  length;
 fixed_t  zerocosine, zerosine;
 fixed_t  zeroxfrac, zeroyfrac;
 fixed_t  xf2, yf2;               // endpoint texture for sloping spans
 int      angle;
 int      light;
 int      px, py, h1, x1, center, y1, x;
 fixed_t  a, w;
 pixel_t  color;

 // set up backdrop stuff
 w=windowWidth/2;
 center=viewangle&255;

 // set up for drawing
 starttaglist_p=spantags;
 if (numspans)
  {
   QuickSortHelper(starttaglist_p,numspans);
   InsertionSort(starttaglist_p,numspans);
   }
 endtaglist_p=starttaglist_p+numspans;
 spantag_p=starttaglist_p;

 angle=viewfineangle+pixelangle[0];
 angle&=TANANGLES *4-1;
 zerocosine=cosines[angle];
 zerosine=sines[angle];
 // draw from back to front
 x2=-1;
 lastz=-1;
 // draw everything else
 while (spantag_p!=endtaglist_p)
  {
   tag=*spantag_p++;
   /* Mask before shifting.  The old 32-bit layout let the top bits of the
      12-bit index fall into the low bits of the recovered depth; masking
      keeps the index out of it entirely. */
   pointz=(fixed_t)((tag&ZMASK)>>ZTOFRAC);
   spannum=(int)(tag&SPANMASK);
   span_p=&spans[spannum];
   spanx=spansx[spannum];
   switch (span_p->spantype)
    {
     case sp_flat:
     case sp_flatsky:
     // floor / ceiling span
      if (pointz!=lastz)
       {
	lastz=pointz;
	/* The exact form is algebraically FIXEDMUL(pointz,xscale) with
	   xscale=FIXEDDIV(viewsin,SCALE), minus the intermediate truncation.
	   SCALE is proj<<FRACBITS, so that FIXEDDIV collapses to an integer
	   divide and xscale keeps only log2(65536/proj) bits -- 409 max at the
	   original projection of 160, but just 99 at an HD projection of 659.
	   The lost ulp is pointz/65536 world units of texture error *per column*
	   and it accumulates across the row, because the contiguous-span path
	   below deliberately never resets mr_xfrac.  At 1574 columns and z=200
	   that is ~5 texels of floor shear, and 4x art makes it four times as
	   visible.  Two 64-bit multiplies per distinct z, not per pixel.

	   Gated the same way TEXELSTEP is (R_public.h): original mode keeps the
	   historical expression so its output stays byte-identical, which is the
	   regression check this port has leaned on throughout. */
	if (hdmode)
	 {
	  mr_xstep=(fixed_t)(((int64_t)pointz*(int64_t)viewsin)/SCALE);
	  mr_ystep=(fixed_t)(((int64_t)pointz*(int64_t)viewcos)/SCALE);
	  }
	else
	 {
	  mr_xstep=FIXEDMUL(pointz, xscale);
	  mr_ystep=FIXEDMUL(pointz, yscale);
	  }
	// calculate starting texture point
	length=FIXEDDIV(pointz, pixelcosine[0]);
	zeroxfrac=mr_xfrac=viewx+FIXEDMUL(length, zerocosine);
	zeroyfrac=mr_yfrac=viewy-FIXEDMUL(length, zerosine);
	x2=0;
	}
      if (spanx!=x2)
       {
	mr_xfrac=zeroxfrac+mr_xstep *spanx;
	mr_yfrac=zeroyfrac+mr_ystep *spanx;
	}

      /* floor shadows */
      if (span_p->shadow==0)
       {
	light=(pointz>>FRACBITS)+span_p->light;
	if (light>MAXZLIGHT) break;
	 else if (light<0) light=0;
	mr_colormap=zcolormap[light];
	}
      else if (span_p->shadow==9)
       {
	light=(pointz>>FRACBITS)+span_p->light+wallflicker4;
	if (light>MAXZLIGHT) break;
	 else if (light<0) light=0;
	mr_colormap=zcolormap[light];
	}
      else mr_colormap=(byte *)span_p->shadow;

      y1=span_p->y-scrollmin;

      if ((unsigned)y1>=(unsigned)windowHeight) break;

      mr_dest=viewylookup[y1]+spanx;
      mr_picture=span_p->picture;
      x2=span_p->x2;

      if ((unsigned)x2>(unsigned)windowWidth) break;

      mr_count=x2-spanx;
      MapRow();

      if (span_p->spantype==sp_flatsky)
       {
	py=span_p->y-scrollmin;
	px=spanx;
	mr_count=span_p->x2-spanx;
	mr_dest=viewylookup[py]+px;
	if (windowHeight!=64) py=span_p->y+64;
	h1=(hfrac*py)>>FRACBITS;
	if (px<=w)
	 {
	  a=((TANANGLES/2)<<FRACBITS) + afrac*(w-px);
	  while (px<=w && mr_count>0)
	   {
	    x=backtangents[a>>FRACBITS];
	    x2=(center<<skyshift) - x + 2*(viewproj<<skyshift) - 1;
	    x2&=skymask;
	    if (*mr_dest==255) *mr_dest=*(backdroplookup[h1]+x2);
	    a-=afrac;
	    px++;
	    mr_count--;
	    mr_dest++;
	    }
	  }
	if (px>w)
	 {
	  a=((TANANGLES/2)<<FRACBITS) + afrac*(px-w);
	  while (mr_count>0)
	   {
	    x1=(center<<skyshift) + backtangents[a>>FRACBITS];
	    x1&=skymask;
	    if (*mr_dest==255) *mr_dest=*(backdroplookup[h1]+x1);
	    a+=afrac;
	    px++;
	    mr_count--;
	    mr_dest++;
	    }
	  }
	}

      break;
     case sp_sky:
      py=span_p->y-scrollmin;
      if ((unsigned)py>=(unsigned)windowHeight) break;
      px=spanx;

      if ((unsigned)span_p->x2>(unsigned)windowWidth) break;

      mr_count=span_p->x2-spanx;
      mr_dest=viewylookup[py]+px;
      if (windowHeight!=64) py=span_p->y+64;
      h1=(hfrac*py)>>FRACBITS;
      if (px<=w)
       {
	a=((TANANGLES/2)<<FRACBITS) + afrac*(w-px);
	while (px<=w && mr_count>0)
	 {
	  x=backtangents[a>>FRACBITS];
	  x2=(center<<skyshift) - x + 2*(viewproj<<skyshift) - 1;
	  x2&=skymask;
	  *mr_dest=*(backdroplookup[h1]+x2);
	  a-=afrac;
	  px++;
	  mr_count--;
	  mr_dest++;
	  }
	}
      if (px>w)
       {
	a=((TANANGLES/2)<<FRACBITS) + afrac*(px-w);
	while (mr_count>0)
	 {
	  x1=(center<<skyshift) + backtangents[a>>FRACBITS];
	  x1&=skymask;
	  *mr_dest=*(backdroplookup[h1]+x1);
	  a+=afrac;
	  px++;
	  mr_count--;
	  mr_dest++;
	  }
	}
      break;
     case sp_step:
      x=span_p->x2;
      sp_dest=tpwalls_dest[x];
      sp_source=span_p->picture;
      sp_colormap=tpwalls_colormap[x];
      sp_frac=span_p->y;
      sp_fracstep=span_p->yh;
      sp_count=tpwalls_count[x];
      sp_loopvalue=(fixed_t)span_p->light<<FRACBITS;
      ScalePost();
      break;
     case sp_shape:
      DrawSprite();
      break;
     case sp_slope:
     case sp_slopesky:
      // sloping floor / ceiling span
      lastz=-1;  // we are going to get out of order here, so

      if (span_p->shadow==0)
       {
	light=(pointz>>FRACBITS)+span_p->light;
	if (light>MAXZLIGHT) break;
	 else if (light<0) light=0;
	mr_colormap=zcolormap[light];
	}
      else if (span_p->shadow==9)
       {
	light=(pointz>>FRACBITS)+span_p->light+wallflicker4;
	if (light>MAXZLIGHT) break;
	 else if (light<0) light=0;
	mr_colormap=zcolormap[light];
	}
      else mr_colormap=(byte *)span_p->shadow;

      x2=span_p->x2;
      y1=span_p->y-scrollmin;

      if ((unsigned)y1>=(unsigned)windowHeight) break;
      if ((unsigned)x2>(unsigned)windowWidth) break;

      mr_dest=viewylookup[y1]+spanx;
      mr_picture=span_p->picture;
      mr_count=x2-spanx;
      // calculate starting texture point
      length=FIXEDDIV(pointz, pixelcosine[spanx]);
      angle=viewfineangle+pixelangle[spanx];
      angle&=TANANGLES *4-1;
      mr_xfrac=viewx+FIXEDMUL(length, cosines[angle]);
      mr_yfrac=viewy-FIXEDMUL(length, sines[angle]);
      // calculate ending texture point
      //  (yh is pointz2 for ending point)
      length=FIXEDDIV(span_p->yh, pixelcosine[x2]);
      angle=viewfineangle+pixelangle[x2];
      angle&=TANANGLES *4-1;
      xf2=viewx+FIXEDMUL(length, cosines[angle]);
      yf2=viewy-FIXEDMUL(length, sines[angle]);
      mr_xstep=(xf2-mr_xfrac)/mr_count;
      mr_ystep=(yf2-mr_yfrac)/mr_count;
      MapRow();

      if (span_p->spantype==sp_slopesky)
       {
	py=span_p->y-scrollmin;
	px=spanx;
	mr_count=span_p->x2-spanx;
	mr_dest=viewylookup[py]+px;
	if (windowHeight!=64) py=span_p->y+64;
	h1=(hfrac*py)>>FRACBITS;
	if (px<=w)
	 {
	  a=((TANANGLES/2)<<FRACBITS) + afrac*(w-px);
	  while (px<=w && mr_count>0)
	   {
	    x=backtangents[a>>FRACBITS];
	    x2=(center<<skyshift) - x + 2*(viewproj<<skyshift) - 1;
	    x2&=skymask;
	    if (*mr_dest==255) *mr_dest=*(backdroplookup[h1]+x2);
	    a-=afrac;
	    px++;
	    mr_count--;
	    mr_dest++;
	    }
	  }
	if (px>w)
	 {
	  a=((TANANGLES/2)<<FRACBITS) + afrac*(px-w);
	  while (mr_count>0)
	   {
	    x1=(center<<skyshift) + backtangents[a>>FRACBITS];
	    x1&=skymask;
	    if (*mr_dest==255) *mr_dest=*(backdroplookup[h1]+x1);
	    a+=afrac;
	    px++;
	    mr_count--;
	    mr_dest++;
	    }
	  }
	}
      break;
     case sp_door:
     case sp_maskeddoor:
      DrawDoorPost();
      break;
     case sp_transparentwall:
      x=span_p->x2;
      sp_dest=tpwalls_dest[x];
      sp_source=span_p->picture;
      sp_colormap=tpwalls_colormap[x];
      sp_frac=span_p->y;
      sp_fracstep=span_p->yh;
      sp_count=tpwalls_count[x];
      sp_loopvalue=(fixed_t)span_p->light<<FRACBITS;
      ScaleMaskedPost();
      break;
     case sp_inviswall:
      x=span_p->x2;
      sp_dest=tpwalls_dest[x];
      sp_source=span_p->picture;
      sp_colormap=tpwalls_colormap[x];
      sp_frac=span_p->y;
      sp_fracstep=span_p->yh;
      sp_count=tpwalls_count[x];
      sp_loopvalue=(fixed_t)span_p->light<<FRACBITS;
      sp_dest-=windowWidth*(sp_count-1);     // go to the top
      --sp_loopvalue;
      while (sp_count--)
       {
	color=sp_source[sp_frac>>FRACBITS];
	if (color)
	 *sp_dest=*(translookup[sp_colormap[color]-1]+*sp_dest);
	sp_dest+=windowWidth;
	sp_frac+=sp_fracstep;
	sp_frac&=sp_loopvalue;
	}
      break;
     }
   }
 }
