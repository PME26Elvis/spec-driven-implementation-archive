#include "mdedit/image.h"

#include <jpeglib.h>
#include <png.h>

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void image_error(char *error,size_t cap,const char *message) {
    if (error!=NULL&&cap!=0U) (void)snprintf(error,cap,"%s",message);
}

void md_image_init(MdImage *image) { memset(image,0,sizeof(*image)); }

void md_image_free(MdImage *image) { free(image->rgba); md_image_init(image); }

MdImageFormat md_image_detect(const uint8_t *data,size_t len) {
    static const uint8_t png_sig[8]={137U,80U,78U,71U,13U,10U,26U,10U};
    if (len>=8U&&memcmp(data,png_sig,8U)==0) return MD_IMAGE_PNG;
    if (len>=3U&&data[0]==0xffU&&data[1]==0xd8U&&data[2]==0xffU) return MD_IMAGE_JPEG;
    if (len>=2U&&data[0]=='B'&&data[1]=='M') return MD_IMAGE_BMP;
    return MD_IMAGE_UNKNOWN;
}

const char *md_image_mime(MdImageFormat format) {
    if (format==MD_IMAGE_PNG) return "image/png";
    if (format==MD_IMAGE_JPEG) return "image/jpeg";
    if (format==MD_IMAGE_BMP) return "image/bmp";
    return "application/octet-stream";
}

const char *md_image_extension(MdImageFormat format) {
    if (format==MD_IMAGE_PNG) return ".png";
    if (format==MD_IMAGE_JPEG) return ".jpg";
    if (format==MD_IMAGE_BMP) return ".bmp";
    return ".bin";
}

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0]|((uint16_t)p[1]<<8U);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);
}

static bool decode_bmp(const uint8_t *data,size_t len,MdImage *image,char *error,size_t error_cap) {
    if (len<54U||read_le32(data+14U)<40U) { image_error(error,error_cap,"BMP header is truncated"); return false; }
    uint32_t offset=read_le32(data+10U),width=read_le32(data+18U);
    int32_t signed_height=(int32_t)read_le32(data+22U);
    uint16_t planes=read_le16(data+26U),bits=read_le16(data+28U);
    uint32_t compression=read_le32(data+30U);
    if (width==0U||signed_height==0||planes!=1U||(bits!=24U&&bits!=32U)||compression!=0U) {
        image_error(error,error_cap,"Unsupported BMP layout (requires uncompressed 24/32-bit)"); return false;
    }
    uint32_t height=signed_height<0?(uint32_t)(-(int64_t)signed_height):(uint32_t)signed_height;
    size_t row_bits=0U,row_bytes=0U,pixels=0U,rgba_bytes=0U;
    if (!md_size_mul((size_t)width,(size_t)bits,&row_bits)) goto huge;
    row_bytes=((row_bits+31U)/32U)*4U;
    if (!md_size_mul(row_bytes,(size_t)height,&pixels)||offset>len||pixels>len-offset||
        !md_size_mul((size_t)width,(size_t)height,&pixels)||!md_size_mul(pixels,4U,&rgba_bytes)) goto huge;
    uint8_t *rgba=malloc(rgba_bytes);
    if (rgba==NULL) { image_error(error,error_cap,"Out of memory decoding BMP"); return false; }
    size_t bytes_per_pixel=bits/8U;
    for (uint32_t y=0U;y<height;++y) {
        uint32_t sy=signed_height<0?y:height-1U-y;
        const uint8_t *row=data+offset+(size_t)sy*row_bytes;
        for (uint32_t x=0U;x<width;++x) {
            const uint8_t *src=row+(size_t)x*bytes_per_pixel;
            uint8_t *dst=rgba+((size_t)y*width+x)*4U;
            dst[0]=src[2]; dst[1]=src[1]; dst[2]=src[0]; dst[3]=bits==32U?src[3]:255U;
        }
    }
    image->rgba=rgba; image->width=width; image->height=height; image->format=MD_IMAGE_BMP;
    return true;
huge:
    image_error(error,error_cap,"BMP dimensions or payload are invalid"); return false;
}

static bool decode_png(const uint8_t *data,size_t len,MdImage *image,char *error,size_t error_cap) {
    png_image png;
    memset(&png,0,sizeof(png)); png.version=PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&png,data,len)) {
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"PNG decode failed: %s",png.message);
        return false;
    }
    png.format=PNG_FORMAT_RGBA;
    if (png.width==0U||png.height==0U||PNG_IMAGE_SIZE(png)>SIZE_MAX) {
        png_image_free(&png); image_error(error,error_cap,"PNG dimensions are invalid"); return false;
    }
    uint8_t *rgba=malloc(PNG_IMAGE_SIZE(png));
    if (rgba==NULL) { png_image_free(&png); image_error(error,error_cap,"Out of memory decoding PNG"); return false; }
    if (!png_image_finish_read(&png,NULL,rgba,0,NULL)) {
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"PNG decode failed: %s",png.message);
        free(rgba); png_image_free(&png); return false;
    }
    image->rgba=rgba; image->width=png.width; image->height=png.height; image->format=MD_IMAGE_PNG;
    png_image_free(&png); return true;
}

typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
    char message[JMSG_LENGTH_MAX];
} JpegError;

static void jpeg_error_exit(j_common_ptr common) {
    JpegError *err=(JpegError *)common->err;
    (*common->err->format_message)(common,err->message);
    longjmp(err->jump,1);
}

static void jpeg_output_message(j_common_ptr common) {
    JpegError *err=(JpegError *)common->err;
    (*common->err->format_message)(common,err->message);
}

static bool decode_jpeg(const uint8_t *data,size_t len,MdImage *image,char *error,size_t error_cap) {
    struct jpeg_decompress_struct cinfo;
    JpegError jerr;
    memset(&cinfo,0,sizeof(cinfo)); memset(&jerr,0,sizeof(jerr));
    cinfo.err=jpeg_std_error(&jerr.pub); jerr.pub.error_exit=jpeg_error_exit; jerr.pub.output_message=jpeg_output_message;
    if (setjmp(jerr.jump)!=0) {
        jpeg_destroy_decompress(&cinfo);
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"JPEG decode failed: %s",jerr.message);
        return false;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo,data,(unsigned long)len);
    (void)jpeg_read_header(&cinfo,TRUE);
    cinfo.out_color_space=JCS_RGB;
    (void)jpeg_start_decompress(&cinfo);
    uint32_t width=cinfo.output_width,height=cinfo.output_height;
    size_t pixels=0U,bytes=0U,row_bytes=0U;
    if (width==0U||height==0U||!md_size_mul(width,height,&pixels)||
        !md_size_mul(pixels,4U,&bytes)||!md_size_mul(width,3U,&row_bytes)) {
        jpeg_destroy_decompress(&cinfo); image_error(error,error_cap,"JPEG dimensions are invalid"); return false;
    }
    uint8_t *rgba=malloc(bytes); uint8_t *row=malloc(row_bytes);
    if (rgba==NULL||row==NULL) {
        free(rgba); free(row); jpeg_destroy_decompress(&cinfo); image_error(error,error_cap,"Out of memory decoding JPEG"); return false;
    }
    while (cinfo.output_scanline<cinfo.output_height) {
        JSAMPROW rows[1]={row}; JDIMENSION got=jpeg_read_scanlines(&cinfo,rows,1U);
        if (got!=1U) { free(rgba); free(row); jpeg_destroy_decompress(&cinfo); image_error(error,error_cap,"JPEG scanline read failed"); return false; }
        size_t y=(size_t)cinfo.output_scanline-1U;
        for (uint32_t x=0U;x<width;++x) {
            uint8_t *dst=rgba+(y*width+x)*4U;
            dst[0]=row[(size_t)x*3U]; dst[1]=row[(size_t)x*3U+1U]; dst[2]=row[(size_t)x*3U+2U]; dst[3]=255U;
        }
    }
    (void)jpeg_finish_decompress(&cinfo); jpeg_destroy_decompress(&cinfo); free(row);
    image->rgba=rgba; image->width=width; image->height=height; image->format=MD_IMAGE_JPEG; return true;
}

bool md_image_decode(const uint8_t *data,size_t len,MdImage *image,char *error,size_t error_cap) {
    md_image_free(image);
    MdImageFormat format=md_image_detect(data,len);
    if (format==MD_IMAGE_PNG) return decode_png(data,len,image,error,error_cap);
    if (format==MD_IMAGE_JPEG) return decode_jpeg(data,len,image,error,error_cap);
    if (format==MD_IMAGE_BMP) return decode_bmp(data,len,image,error,error_cap);
    image_error(error,error_cap,"Unsupported image format"); return false;
}

bool md_image_load(const char *path,MdImage *image,MdBytes *original,char *error,size_t error_cap) {
    original->len=0U;
    if (!md_read_file(path,original,error,error_cap)) return false;
    if (!md_image_decode(original->data,original->len,image,error,error_cap)) { original->len=0U; return false; }
    return true;
}

bool md_image_write_png(const char *path,const uint8_t *rgba,uint32_t width,uint32_t height,
                        char *error,size_t error_cap) {
    png_image png; memset(&png,0,sizeof(png)); png.version=PNG_IMAGE_VERSION;
    png.width=width; png.height=height; png.format=PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&png,path,0,rgba,0,NULL)) {
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"PNG write failed: %s",png.message);
        return false;
    }
    return true;
}

bool md_image_write_jpeg(const char *path,const uint8_t *rgba,uint32_t width,uint32_t height,
                         int quality,char *error,size_t error_cap) {
    if (rgba==NULL||width==0U||height==0U||quality<1||quality>100) {
        image_error(error,error_cap,"JPEG write arguments are invalid"); return false;
    }
    size_t row_bytes=0U;
    if (!md_size_mul((size_t)width,3U,&row_bytes)) { image_error(error,error_cap,"JPEG row is too large"); return false; }
    uint8_t *row=malloc(row_bytes);
    if (row==NULL) { image_error(error,error_cap,"Out of memory writing JPEG"); return false; }
    FILE *file=fopen(path,"wb");
    if (file==NULL) { free(row); image_error(error,error_cap,"Cannot create JPEG output file"); return false; }
    struct jpeg_compress_struct cinfo; JpegError jerr;
    memset(&cinfo,0,sizeof(cinfo)); memset(&jerr,0,sizeof(jerr));
    cinfo.err=jpeg_std_error(&jerr.pub); jerr.pub.error_exit=jpeg_error_exit; jerr.pub.output_message=jpeg_output_message;
    if (setjmp(jerr.jump)!=0) {
        jpeg_destroy_compress(&cinfo); (void)fclose(file); free(row);
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"JPEG write failed: %s",jerr.message);
        return false;
    }
    jpeg_create_compress(&cinfo); jpeg_stdio_dest(&cinfo,file);
    cinfo.image_width=width; cinfo.image_height=height; cinfo.input_components=3; cinfo.in_color_space=JCS_RGB;
    jpeg_set_defaults(&cinfo); jpeg_set_quality(&cinfo,quality,TRUE); jpeg_start_compress(&cinfo,TRUE);
    while (cinfo.next_scanline<cinfo.image_height) {
        size_t y=(size_t)cinfo.next_scanline;
        for (uint32_t x=0U;x<width;++x) {
            const uint8_t *pixel=rgba+(y*(size_t)width+x)*4U;
            row[(size_t)x*3U]=pixel[0]; row[(size_t)x*3U+1U]=pixel[1]; row[(size_t)x*3U+2U]=pixel[2];
        }
        JSAMPROW rows[1]={row}; if (jpeg_write_scanlines(&cinfo,rows,1U)!=1U) { image_error(error,error_cap,"JPEG scanline write failed"); jpeg_destroy_compress(&cinfo); (void)fclose(file); free(row); return false; }
    }
    jpeg_finish_compress(&cinfo); jpeg_destroy_compress(&cinfo);
    bool ok=fclose(file)==0; free(row);
    if (!ok) image_error(error,error_cap,"Cannot close JPEG output file");
    return ok;
}

bool md_image_parse_data_uri(const char *uri,size_t len,MdImageFormat *format,MdBytes *bytes,
                             char *error,size_t error_cap) {
    const char *prefix=NULL;
    if (len>=22U&&memcmp(uri,"data:image/png;base64,",22U)==0) { *format=MD_IMAGE_PNG; prefix=uri+22U; }
    else if (len>=23U&&memcmp(uri,"data:image/jpeg;base64,",23U)==0) { *format=MD_IMAGE_JPEG; prefix=uri+23U; }
    else if (len>=22U&&memcmp(uri,"data:image/bmp;base64,",22U)==0) { *format=MD_IMAGE_BMP; prefix=uri+22U; }
    else { image_error(error,error_cap,"Unsupported or malformed image data URI"); return false; }
    size_t prefix_len=(size_t)(prefix-uri);
    if (!md_base64_decode(prefix,len-prefix_len,bytes,error,error_cap)) return false;
    if (md_image_detect(bytes->data,bytes->len)!=*format) {
        bytes->len=0U; image_error(error,error_cap,"Data URI media type does not match decoded bytes"); return false;
    }
    MdImage check; md_image_init(&check);
    bool ok=md_image_decode(bytes->data,bytes->len,&check,error,error_cap);
    md_image_free(&check); if (!ok) bytes->len=0U; return ok;
}

bool md_image_make_data_uri(MdImageFormat format,const uint8_t *data,size_t len,MdBuf *out) {
    if (format==MD_IMAGE_UNKNOWN||md_image_detect(data,len)!=format) return false;
    out->len=0U;
    if (!md_buf_reserve(out,0U)) return false;
    out->data[0]='\0';
    if (!md_buf_appendf(out,"data:%s;base64,",md_image_mime(format))) return false;
    MdBuf encoded; md_buf_init(&encoded);
    bool ok=md_base64_encode(data,len,&encoded)&&md_buf_append(out,encoded.data,encoded.len);
    md_buf_free(&encoded); return ok;
}
