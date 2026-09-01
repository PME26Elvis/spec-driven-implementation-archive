/* imgcodec.c - WIC-based PNG/JPEG/BMP decode/encode. */
#include "imgcodec.h"
#include "ce_common.h"
#include <windows.h>
#include <wincodec.h>
#include <objbase.h>

static IWICImagingFactory *factory(void){
    static IWICImagingFactory *f = NULL;
    if(f) return f;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    (void)hr;
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void**)&f);
    if(FAILED(hr)) return NULL;
    return f;
}

const char *img_mime(int fmt){
    switch(fmt){
        case IMG_FMT_PNG: return "png";
        case IMG_FMT_JPEG: return "jpeg";
        case IMG_FMT_BMP: return "bmp";
    }
    return "png";
}

static const GUID *container_for(int fmt){
    switch(fmt){
        case IMG_FMT_PNG: return &GUID_ContainerFormatPng;
        case IMG_FMT_JPEG: return &GUID_ContainerFormatJpeg;
        case IMG_FMT_BMP: return &GUID_ContainerFormatBmp;
    }
    return &GUID_ContainerFormatPng;
}

uint8_t *img_decode(const unsigned char *data, size_t len, int *w, int *h){
    IWICImagingFactory *f = factory();
    if(!f) return NULL;
    IWICStream *stream = NULL;
    if(FAILED(f->lpVtbl->CreateStream(f, &stream))) return NULL;
    if(FAILED(stream->lpVtbl->InitializeFromMemory(stream, (BYTE*)data, (DWORD)len))){ stream->lpVtbl->Release(stream); return NULL; }
    IWICBitmapDecoder *dec = NULL;
    HRESULT hr = f->lpVtbl->CreateDecoderFromStream(f, (IStream*)stream, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    stream->lpVtbl->Release(stream);
    if(FAILED(hr) || !dec) return NULL;
    IWICBitmapFrameDecode *frame = NULL;
    hr = dec->lpVtbl->GetFrame(dec, 0, &frame);
    dec->lpVtbl->Release(dec);
    if(FAILED(hr) || !frame) return NULL;
    UINT fw = 0, fh = 0;
    frame->lpVtbl->GetSize(frame, &fw, &fh);
    if(fw == 0 || fh == 0 || fw > 65536 || fh > 65536){ frame->lpVtbl->Release(frame); return NULL; }

    WICPixelFormatGUID fmt;
    frame->lpVtbl->GetPixelFormat(frame, &fmt);
    IWICBitmapSource *src = (IWICBitmapSource*)frame;
    IWICFormatConverter *conv = NULL;
    if(IsEqualGUID(&fmt, &GUID_WICPixelFormat32bppRGBA)){
        /* already RGBA */
    } else {
        hr = f->lpVtbl->CreateFormatConverter(f, &conv);
        if(FAILED(hr)){ frame->lpVtbl->Release(frame); return NULL; }
        hr = conv->lpVtbl->Initialize(conv, (IWICBitmapSource*)frame, &GUID_WICPixelFormat32bppRGBA,
                                      WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
        if(FAILED(hr)){ conv->lpVtbl->Release(conv); frame->lpVtbl->Release(frame); return NULL; }
        src = (IWICBitmapSource*)conv;
    }
    uint8_t *out = ce_malloc((size_t)fw * fh * 4);
    HRESULT cp = src->lpVtbl->CopyPixels(src, NULL, fw * 4, fw * fh * 4, out);
    if(conv) conv->lpVtbl->Release(conv);
    frame->lpVtbl->Release(frame);
    if(FAILED(cp)){ ce_free(out); return NULL; }
    *w = (int)fw; *h = (int)fh;
    return out;
}

unsigned char *img_encode(const uint8_t *rgba, int w, int h, int fmt, size_t *out_len){
    IWICImagingFactory *f = factory();
    if(!f) return NULL;
    IStream *stream = NULL;
    if(FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream))) return NULL;
    IWICBitmapEncoder *enc = NULL;
    HRESULT hr = f->lpVtbl->CreateEncoder(f, container_for(fmt), NULL, &enc);
    if(FAILED(hr)){ stream->lpVtbl->Release(stream); return NULL; }
    hr = enc->lpVtbl->Initialize(enc, stream, WICBitmapEncoderNoCache);
    if(FAILED(hr)){ enc->lpVtbl->Release(enc); stream->lpVtbl->Release(stream); return NULL; }

    IWICBitmapFrameEncode *frame = NULL;
    IPropertyBag2 *bag = NULL;
    hr = enc->lpVtbl->CreateNewFrame(enc, &frame, &bag);
    if(FAILED(hr)){ enc->lpVtbl->Release(enc); stream->lpVtbl->Release(stream); return NULL; }
    if(bag){ bag->lpVtbl->Release(bag); bag = NULL; }
    hr = frame->lpVtbl->Initialize(frame, NULL);
    if(FAILED(hr)){ frame->lpVtbl->Release(frame); enc->lpVtbl->Release(enc); stream->lpVtbl->Release(stream); return NULL; }
    frame->lpVtbl->SetSize(frame, (UINT)w, (UINT)h);

    WICPixelFormatGUID pf_target;
    uint8_t *pixels;
    UINT stride;
    if(fmt == IMG_FMT_JPEG){
        pf_target = GUID_WICPixelFormat24bppBGR;
        stride = (UINT)w * 3;
        pixels = ce_malloc((size_t)w * h * 3);
        for(int i = 0; i < w * h; i++){
            pixels[i*3+0] = rgba[i*4+2];
            pixels[i*3+1] = rgba[i*4+1];
            pixels[i*3+2] = rgba[i*4+0];
        }
    } else {
        pf_target = GUID_WICPixelFormat32bppBGRA;
        stride = (UINT)w * 4;
        pixels = ce_malloc((size_t)w * h * 4);
        for(int i = 0; i < w * h; i++){
            pixels[i*4+0] = rgba[i*4+2];
            pixels[i*4+1] = rgba[i*4+1];
            pixels[i*4+2] = rgba[i*4+0];
            pixels[i*4+3] = rgba[i*4+3];
        }
    }
    frame->lpVtbl->SetPixelFormat(frame, &pf_target);
    hr = frame->lpVtbl->WritePixels(frame, (UINT)h, stride, (UINT)stride * h, pixels);
    ce_free(pixels);
    if(FAILED(hr)){ frame->lpVtbl->Release(frame); enc->lpVtbl->Release(enc); stream->lpVtbl->Release(stream); return NULL; }
    hr = frame->lpVtbl->Commit(frame);
    frame->lpVtbl->Release(frame);
    if(FAILED(hr)){ enc->lpVtbl->Release(enc); stream->lpVtbl->Release(stream); return NULL; }
    hr = enc->lpVtbl->Commit(enc);
    enc->lpVtbl->Release(enc);
    if(FAILED(hr)){ stream->lpVtbl->Release(stream); return NULL; }

    /* read stream back via the HGLOBAL */
    HGLOBAL hg = NULL;
    unsigned char *buf = NULL;
    if(SUCCEEDED(GetHGlobalFromStream(stream, &hg))){
        SIZE_T total = GlobalSize(hg);
        void *p = GlobalLock(hg);
        if(p){
            buf = ce_malloc(total ? total : 1);
            memcpy(buf, p, total);
            GlobalUnlock(hg);
            *out_len = total;
        }
    }
    stream->lpVtbl->Release(stream);
    if(!buf){ buf = ce_malloc(1); *out_len = 0; }
    return buf;
}
