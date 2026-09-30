#include "tile_render.h"
#include "fonts/font8x8_basic.h"
#include "fonts/font8x8_ext_latin.h"
static void pixel(uint8_t *b, int x, int y, uint32_t c)
{
    if (x < 0 || x >= 80 || y < 0 || y >= 80) return;
    unsigned p = 54 + ((79 - x) * 80 + y) * 3;
    b[p] = c; b[p+1] = c >> 8; b[p+2] = c >> 16;
}
void tile_rect(uint8_t *b, int x, int y, int width, int height, uint32_t rgb)
{
    for(int yy=y;yy<y+height;yy++) for(int xx=x;xx<x+width;xx++) pixel(b,xx,yy,rgb);
}
void tile_background(uint8_t *b, uint32_t rgb)
{
    mini_demo_bmp(0, false, b);
    for (int y=0;y<80;y++) for (int x=0;x<80;x++) pixel(b,x,y,rgb);
}
void tile_rgba_at(uint8_t *b, const uint8_t *rgba, unsigned size, unsigned top)
{
    if (!size || size > 80 || top > 80-size) return;
    unsigned margin = (80-size)/2;
    for (unsigned y=0;y<size;y++) for(unsigned x=0;x<size;x++) {
        const uint8_t *s = rgba + ((y*80/size)*80+x*80/size)*4;
        unsigned p=54+((79-x-margin)*80+y+top)*3, a=s[3];
        for(unsigned c=0;c<3;c++) b[p+c]=(s[2-c]*a+b[p+c]*(255-a)+127)/255;
    }
}
void tile_rgba(uint8_t *b, const uint8_t *rgba, unsigned size)
{
    if (size <= 80) tile_rgba_at(b, rgba, size, (80-size)/2);
}
static unsigned next_char(const unsigned char **p)
{
    unsigned c=*(*p)++;
    if ((c == 0xc2 || c == 0xc3) && **p >= 0x80 && **p <= 0xbf) c=((c&31)<<6)|(*(*p)++&63);
    else if(c>=128) { while ((**p & 0xc0)==0x80) ++*p; c='?'; }
    return c;
}
static const unsigned char *glyph(unsigned c)
{
    return c<128 ? font8x8_basic[c] : c>=160 && c<=255 ? font8x8_ext_latin[c-160] : font8x8_basic['?'];
}
static unsigned glyph_bits(unsigned c)
{
    const unsigned char *g=glyph(c);unsigned bits=0;
    for(unsigned y=0;y<8;y++) bits|=g[y];return bits;
}
static unsigned glyph_left(unsigned c)
{
    unsigned bits=glyph_bits(c),left=0;
    if(bits) while(!(bits&1)) {++left;bits>>=1;}return left;
}
static unsigned width(unsigned c)
{
    if(c==' ') return 4;
    unsigned bits=glyph_bits(c)>>glyph_left(c),w=0;
    while(bits) {++w;bits>>=1;}return w+1;
}
void tile_text(uint8_t *b, const char *text, int y, unsigned scale, uint32_t rgb)
{
    if (!text || (scale!=1 && scale!=2)) return;
    unsigned chars[64], n=0,total=0;
    const unsigned char *p=(const unsigned char *)text;
    while (*p && n<64) {chars[n]=next_char(&p);total+=width(chars[n++]);}
    if(total*scale>80) scale=1;
    if(total>80) {
        while(n && total+width('~')>80) total-=width(chars[--n]);
        if(n<64) {chars[n++]='~';total+=width('~');}
    }
    int left=(80-(int)total*(int)scale)/2;
    for(unsigned i=0;i<n;i++) {
        const unsigned char *g=glyph(chars[i]);
        for(unsigned yy=0;yy<8;yy++) for(unsigned xx=0;xx<8;xx++) if(g[yy]&(1<<xx))
            for(unsigned dy=0;dy<scale;dy++) for(unsigned dx=0;dx<scale;dx++)
                pixel(b,left+(xx-glyph_left(chars[i]))*scale+dx,y+yy*scale+dy,rgb);
        left+=width(chars[i])*scale;
    }
}
void tile_arrow(uint8_t *b, bool next)
{
    for(int y=22;y<54;y++) for(int x=23;x<57;x++) {
        int yy=next ? y : 75-y;
        if ((y<39 && x>=40-(y-22) && x<=40+(y-22)) || (y>=39 && x>=35 && x<=45))
            pixel(b,x,yy,0x55acee);
    }
}
