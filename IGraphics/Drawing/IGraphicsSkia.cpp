#include <cmath>

#include "IGraphicsSkia.h"

#include "SkDashPathEffect.h"
#include "SkGradientShader.h"
#include "SkFont.h"

#include "gl/GrGLInterface.h"
#include "gl/GrGLUtil.h"
#include "GrContext.h"

#ifdef OS_MAC
#include "SkCGUtils.h"
#endif

#include <OpenGL/gl.h>


SkiaBitmap::SkiaBitmap(const char* path, double sourceScale)
{
  auto data = SkData::MakeFromFileName(path);
  mImage = SkImage::MakeFromEncoded(data);

  SetBitmap(mImage.get(), mImage->width(), mImage->height(), sourceScale, 1.f);
}

#pragma mark -

// Utility conversions
inline SkColor SkiaColor(const IColor& color, const IBlend* pBlend = 0)
{
  return SkColorSetARGB(color.A, color.R, color.G, color.B);
}

inline SkRect SkiaRect(const IRECT& r)
{
  return SkRect::MakeLTRB(r.L, r.T, r.R, r.B);
}

inline SkBlendMode SkiaBlendMode(const IBlend* pBlend)
{
  switch (pBlend->mMethod)
  {
    case EBlend::Default:         // fall through
    case EBlend::Clobber:         // fall through
    case EBlend::SourceOver:      return SkBlendMode::kSrcOver;
    case EBlend::SourceIn:        return SkBlendMode::kSrcIn;
    case EBlend::SourceOut:       return SkBlendMode::kSrcOut;
    case EBlend::SourceAtop:      return SkBlendMode::kSrcATop;
    case EBlend::DestOver:        return SkBlendMode::kDstOver;
    case EBlend::DestIn:          return SkBlendMode::kDstIn;
    case EBlend::DestOut:         return SkBlendMode::kDstOut;
    case EBlend::DestAtop:        return SkBlendMode::kDstATop;
    case EBlend::Add:             return SkBlendMode::kPlus;
    case EBlend::XOR:             return SkBlendMode::kXor;
  }
  
  return SkBlendMode::kClear;
}

SkPaint SkiaPaint(const IPattern& pattern, const IBlend* pBlend)
{
  SkPaint paint;
  paint.setAntiAlias(true);
  
  if(pattern.mType == EPatternType::Solid)
    paint.setColor(SkiaColor(pattern.GetStop(0).mColor, pBlend));
  else
  {
    //TODO: points?
    SkPoint points[2] = {
      SkPoint::Make(0.0f, 0.0f),
      SkPoint::Make(256.0f, 256.0f)
    };
    
    SkColor colors[8];
    
    assert(pattern.NStops() <= 8);
    
    for(int i = 0; i < pattern.NStops(); i++)
      colors[i] = SkiaColor(pattern.GetStop(i).mColor);
   
    if(pattern.mType == EPatternType::Linear)
      paint.setShader(SkGradientShader::MakeLinear(points, colors, nullptr, pattern.NStops(), SkTileMode::kClamp, 0, nullptr));
    else
      paint.setShader(SkGradientShader::MakeRadial(SkPoint::Make(128.0f, 128.0f) /*TODO: points*/, 180.0f, colors, nullptr, pattern.NStops(), SkTileMode::kClamp, 0, nullptr));
  }
  
  return paint;
}

#pragma mark -

IGraphicsSkia::IGraphicsSkia(IGEditorDelegate& dlg, int w, int h, int fps, float scale)
: IGraphicsPathBase(dlg, w, h, fps, scale)
{
  DBGMSG("IGraphics Skia @ %i FPS\n", fps);
}

IGraphicsSkia::~IGraphicsSkia()
{
}

bool IGraphicsSkia::BitmapExtSupported(const char* ext)
{
  char extLower[32];
  ToLower(extLower, ext);
  return (strstr(extLower, "png") != nullptr) /*|| (strstr(extLower, "jpg") != nullptr) || (strstr(extLower, "jpeg") != nullptr)*/;
}

APIBitmap* IGraphicsSkia::LoadAPIBitmap(const char* fileNameOrResID, int scale, EResourceLocation location, const char* ext)
{
  return new SkiaBitmap(fileNameOrResID, scale);
}

void IGraphicsSkia::SetPlatformContext(void* pContext)
{
  mPlatformContext = pContext;
  
#if defined IGRAPHICS_CPU
  mSurface = SkSurface::MakeRasterN32Premul(WindowWidth() * GetDrawScale(), WindowHeight() * GetDrawScale());
  mCanvas = mSurface->getCanvas();
#endif
}

void IGraphicsSkia::OnViewInitialized(void* pContext)
{
#if defined IGRAPHICS_GL
  int fbo = 0, samples = 0, stencilBits = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
  glGetIntegerv(GL_SAMPLES, &samples);
  glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
  
  auto interface = GrGLMakeNativeInterface();
  mGrContext = GrContext::MakeGL(interface);
  
  GrGLFramebufferInfo fbinfo;
  fbinfo.fFBOID = fbo;
  fbinfo.fFormat = 0x8058;
  
  auto backendRenderTarget = GrBackendRenderTarget(WindowWidth() * GetDrawScale(), WindowHeight() * GetDrawScale(), samples, stencilBits, fbinfo);

  mSurface = SkSurface::MakeFromBackendRenderTarget(mGrContext.get(), backendRenderTarget, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, nullptr, nullptr);
  mCanvas = mSurface->getCanvas();
#endif
}

void IGraphicsSkia::OnViewDestroyed()
{
}

void IGraphicsSkia::DrawResize()
{
//  if(mSurface != nullptr);
  
//  mSurface = SkSurface::MakeRasterN32Premul(WindowWidth() * GetDrawScale(), WindowHeight() * GetDrawScale());
}

void IGraphicsSkia::BeginFrame()
{
  mCanvas->clear(SK_ColorWHITE);
}

void IGraphicsSkia::EndFrame()
{
#ifdef IGRAPHICS_CPU
  #ifdef OS_MAC
    SkPixmap pixmap;
    mSurface->peekPixels(&pixmap);
    SkBitmap bmp;
    bmp.installPixels(pixmap);
    SkCGDrawBitmap((CGContextRef) mPlatformContext, bmp, 0, 0);
  #endif
#else
  mCanvas->flush();
#endif
}

void IGraphicsSkia::DrawBitmap(const IBitmap& bitmap, const IRECT& dest, int srcX, int srcY, const IBlend* pBlend)
{
  SkPaint p;
  p.setFilterQuality(kHigh_SkFilterQuality);
  mCanvas->drawImage(static_cast<SkImage*>(bitmap.GetAPIBitmap()->GetBitmap()), dest.L, dest.T, &p);
}

IColor IGraphicsSkia::GetPoint(int x, int y)
{
  return COLOR_BLACK; //TODO:
}

void IGraphicsSkia::DoMeasureText(const IText& text, const char* str, IRECT& bounds) const
{
  
}

void IGraphicsSkia::DoDrawText(const IText& text, const char* str, const IRECT& bounds, const IBlend* pBlend)
{
  SkFont font;
  font.setSubpixel(true);
  font.setSize(text.mSize);
  SkPaint paint;
  paint.setColor(SkiaColor(text.mFGColor, pBlend));
  
  mCanvas->drawSimpleText(str, strlen(str), SkTextEncoding::kUTF8, bounds.L, bounds.T, font, paint);
}

void IGraphicsSkia::PathStroke(const IPattern& pattern, float thickness, const IStrokeOptions& options, const IBlend* pBlend)
{
  float dashArray[8];

  SkPaint paint = SkiaPaint(pattern, pBlend);
  paint.setStyle(SkPaint::kStroke_Style);

  switch (options.mCapOption)
  {
    case ELineCap::Butt:   paint.setStrokeCap(SkPaint::kButt_Cap);     break;
    case ELineCap::Round:  paint.setStrokeCap(SkPaint::kRound_Cap);    break;
    case ELineCap::Square: paint.setStrokeCap(SkPaint::kSquare_Cap);   break;
  }

  switch (options.mJoinOption)
  {
    case ELineJoin::Miter: paint.setStrokeJoin(SkPaint::kMiter_Join);   break;
    case ELineJoin::Round: paint.setStrokeJoin(SkPaint::kRound_Join);   break;
    case ELineJoin::Bevel: paint.setStrokeJoin(SkPaint::kBevel_Join);   break;
  }
  
  if(options.mDash.GetCount())
  {
    for (int i = 0; i < options.mDash.GetCount(); i++)
      dashArray[i] = *(options.mDash.GetArray() + i);
    
    paint.setPathEffect(SkDashPathEffect::Make(dashArray, options.mDash.GetCount(), 0));
  }
  
  paint.setStrokeWidth(thickness);
  paint.setStrokeMiter(options.mMiterLimit);
  
  mCanvas->drawPath(mMainPath, paint);
  
  if (!options.mPreserve)
    mMainPath.reset();
}

void IGraphicsSkia::PathFill(const IPattern& pattern, const IFillOptions& options, const IBlend* pBlend)
{
  SkPaint paint = SkiaPaint(pattern, pBlend);
  paint.setStyle(SkPaint::kFill_Style);
  
  if (options.mFillRule == EFillRule::Winding)
    mMainPath.setFillType(SkPath::kWinding_FillType);
  else
    mMainPath.setFillType(SkPath::kEvenOdd_FillType);
  
  mCanvas->drawPath(mMainPath, paint);
  
  if (!options.mPreserve)
    mMainPath.reset();
}

void IGraphicsSkia::PathTransformSetMatrix(const IMatrix& m)
{
//  SkMatrix::MakeAll(XX, <#SkScalar skewX#>, <#SkScalar transX#>, <#SkScalar skewY#>, <#SkScalar scaleY#>, <#SkScalar transY#>, <#SkScalar pers0#>, <#SkScalar pers1#>, <#SkScalar pers2#>)
//  mCanvas->scale(GetDrawScale(), GetDrawScale());
//  //TODO:
}

void IGraphicsSkia::SetClipRegion(const IRECT& r)
{
  //TODO:

//  SkRect skrect;
//  skrect.set(r.L, r.T, r.R, r.B);
//  mCanvas->clipRect(skrect);
}
