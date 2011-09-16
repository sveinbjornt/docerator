// Computes the optical flow between two images, using the Lucas-Kanade method
// on an image pyramid.
//
// Written by nicolasweber@gmx.de, released under MIT license

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>


void gray64fromRgb64(double* dest, const double* src, int w, int h) {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      dest[y*w + x] =
          0.299 * src[3*(y*w + x) + 0]  // red
        + 0.587 * src[3*(y*w + x) + 1]  // green
        + 0.114 * src[3*(y*w + x) + 2]; // blue
    }
  }
}

void rgb64fromRgba32(double* dest, unsigned char* src, int w, int h) {
  int bps = 4 * w;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      dest[(y*w + x)*3 + 0] = src[y*bps + 4*x + 1]/255.0;
      dest[(y*w + x)*3 + 1] = src[y*bps + 4*x + 2]/255.0;
      dest[(y*w + x)*3 + 2] = src[y*bps + 4*x + 3]/255.0;
    }
  }
}

void mask64fromRgba32(double* dest, unsigned char* src, int w, int h) {
  int bps = 4 * w;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      dest[y*w + x] = src[y*bps + 4*x + 0]/255.0;
}


template<class T>
T clamp(T v, T minV, T maxV) {
  if (v < minV) return minV;
  if (v > maxV) return maxV;
  return v;
}

double* allocImage(int w, int h, int c = 1) {
  return (double*)malloc(w * h * c * sizeof(double));
}

class Img {
 public:
  int w, h;
  int c;
  double* pix;

  Img() : w(0), h(0), c(0), pix(NULL) {}

  Img(int iw, int ih, int ic = 1) : w(iw), h(ih), c(ic),
      pix(allocImage(w, h, c))
  { }

  Img(const Img& b) {
    printf("cloning!\n");
    w = b.w;
    h = b.h;
    c = b.c;
    pix = allocImage(w, h, c);
    memcpy(pix, b.pix, w * h * c * sizeof(double));
  }

  ~Img() {
    if (pix)
      free(pix);
    pix = NULL;
  }

  void setSize(int nw, int nh, int nc = 1) {
    if (pix)
      free(pix);
    w = nw;
    h = nh;
    c = nc;
    pix = allocImage(w, h, c);
  }

  void toGray() {
    if (c == 1) return;
    double* grayPix = allocImage(w, h);
    gray64fromRgb64(grayPix, pix, w, h);
    free(pix);
    pix = grayPix;
    c = 1;
  }

 private:
  Img& operator=(const Img& b);
};


#if 1
// Your own image loading/saving functions in here. I use ImageIO, because it's
// preinstalled on OS X and works, even if it's a bit wordy.
// I used OpenCV before, but it seems to be unable to load alpha and didn't
// save the 32x32 variants correctly. DevIL is great, but doesn't compile on
// OS X atm.

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

typedef CGImageSourceRef ImageCollection;

int getWidth(ImageCollection image_source, int idx)
{
  // This is ridiculous.
  int w, h;
  const void* keys[] = {
    kCGImagePropertyPixelWidth, kCGImagePropertyPixelHeight
  };
  const void* values[2] = { kCFBooleanTrue, kCFBooleanTrue };
  CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault,
      keys, values, 2,
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionaryRef size
    = CGImageSourceCopyPropertiesAtIndex(image_source, idx, options);
  CFRelease(options);

  CFNumberRef widthNumber = (CFNumberRef)CFDictionaryGetValue(size,
      kCGImagePropertyPixelWidth);
  CFNumberRef heightNumber = (CFNumberRef)CFDictionaryGetValue(size,
      kCGImagePropertyPixelHeight);

  CFNumberGetValue(widthNumber, kCFNumberIntType, &w);
  CFNumberGetValue(heightNumber, kCFNumberIntType, &h);
  CFRelease(size);
  assert(w == h);
  return w;
}

bool trueColor(ImageCollection image_source, int idx)
{
  const void* keys[] = {
    kCGImagePropertyIsIndexed, kCGImagePropertyColorModel, kCGImagePropertyDepth
  };
  const void* values[3] = { kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue };
  CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault,
      keys, values, 3,
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionaryRef clr
    = CGImageSourceCopyPropertiesAtIndex(image_source, idx, options);
  CFRelease(options);

  CFNumberRef depth = (CFNumberRef)CFDictionaryGetValue(clr,
      kCGImagePropertyDepth);
  CFBooleanRef indexed = (CFBooleanRef)CFDictionaryGetValue(clr,
      kCGImagePropertyIsIndexed);
  CFStringRef clrSpace = (CFStringRef)CFDictionaryGetValue(clr,
      kCGImagePropertyColorModel);

  int d;
  CFNumberGetValue(depth, kCFNumberIntType, &d);
  if (d != 8 || indexed == kCFBooleanTrue) return false;

  bool r = CFStringCompare(clrSpace, CFSTR("RGB"), 0) == kCFCompareEqualTo;
  CFRelease(clr);
  return r;
}

ImageCollection loadImageSource(const char* name)
{
  CFStringRef pathString = CFStringCreateWithBytes(kCFAllocatorDefault,
      (unsigned char*)name, strlen(name), kCFStringEncodingUTF8, NULL);
  if (!pathString) return NULL;

  CFURLRef texture_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
      pathString, kCFURLPOSIXPathStyle, FALSE);
   CFRelease(pathString);
  if (!texture_url) return NULL;

  CGImageSourceRef image_source = CGImageSourceCreateWithURL(texture_url, NULL);
  CFRelease(texture_url);
  return image_source;
}

int getImageCount(ImageCollection image_source)
{
  return CGImageSourceGetCount(image_source);
}

void freeImageSource(ImageCollection image_source)
{
  CFRelease(image_source);
}

// destroys t
bool imageFromSource(CGImageSourceRef image_source,
    int index, Img& img, Img* mask = NULL)
{
  const int n = 3;
  bool result = true;

  CGImageRef image = NULL;
  CGColorSpaceRef color_space = NULL;
  CGContextRef context = NULL;
  void* data = NULL;

  image = CGImageSourceCreateImageAtIndex(
      image_source, index, NULL);
  if (!image) 
  { 
	  result = false;
	  if (context) CFRelease(context);
	  if (color_space) CFRelease(color_space);
	  if (data) free(data);
	  if (image) CFRelease(image);
	  return result;
  }

  unsigned width = CGImageGetWidth(image);
  unsigned height = CGImageGetHeight(image);

  data = malloc(width * height * 4);
  memset(data, 0, width * height * 4);

  color_space = CGColorSpaceCreateDeviceRGB();

  // ImageIO is not happy if the context is not alpha-premultiplied
  // ( http://developer.apple.com/qa/qa2001/qa1037.html )
  context = CGBitmapContextCreate(data, width, height,
      8, width * 4, color_space, kCGImageAlphaPremultipliedFirst);
  if (!context) 
  { 
	result = false; 
	  if (context) CFRelease(context);
	  if (color_space) CFRelease(color_space);
	  if (data) free(data);
	  if (image) CFRelease(image);
	  return result;
  }

  CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);

  img.setSize(width, height, n);
  rgb64fromRgba32(img.pix, (unsigned char*)data, width, height);

  if (mask != NULL) {
    mask->setSize(width, height);
    mask64fromRgba32(mask->pix, (unsigned char*)data, width, height);

    // undo alpha-premultiplication *curse apple-apis*
    for (unsigned y = 0; y < height; ++y)
      for (unsigned x = 0; x < width; ++x)
        if (mask->pix[y*width + x] != 0.0)
          for (unsigned c = 0; c < n; ++c)
            img.pix[(y*width + x)*n + c] /= mask->pix[y*width + x];
  }

  return result;
}

bool LoadImage(const char* name, Img& img, Img* mask = NULL)
{
  ImageCollection image_source = loadImageSource(name);
  if (!image_source) return false;
  bool r = imageFromSource(image_source, 0, img, mask);
  freeImageSource(image_source);
  return r;
}

bool SaveImage(const char* name, const Img& img)
{
  bool result = true;

  CFStringRef pathString = NULL;
  CFURLRef texture_url = NULL;
  CGImageDestinationRef image_dest = NULL;
  CGImageRef image = NULL;
  CGColorSpaceRef color_space = NULL;
  CGContextRef context = NULL;
  unsigned char* dst = NULL;

  pathString = CFStringCreateWithBytes(kCFAllocatorDefault,
      (unsigned char*)name, strlen(name), kCFStringEncodingUTF8, NULL);
  texture_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
      pathString, kCFURLPOSIXPathStyle, FALSE);
  if (!texture_url) { result = false; goto cleanup; }

  dst = (unsigned char*)malloc(img.w * img.h * 4);
  if (img.c == 1)
    for (int y = 0; y < img.h; ++y) {
      for (int x = 0; x < img.w; ++x) {
        dst[y*img.w + x] = int(255*img.pix[y*img.w + x] + 0.5);
      }
    }
  else
    for (int y = 0; y < img.h; ++y) {
      for (int x = 0; x < img.w; ++x) {
        dst[4*(y*img.w + x) + 1] = int(255*img.pix[3*(y*img.w + x) + 0] + 0.5);
        dst[4*(y*img.w + x) + 2] = int(255*img.pix[3*(y*img.w + x) + 1] + 0.5);
        dst[4*(y*img.w + x) + 3] = int(255*img.pix[3*(y*img.w + x) + 2] + 0.5);
      }
    }

  if (img.c == 1)
    color_space = CGColorSpaceCreateDeviceGray();
  else
    color_space = CGColorSpaceCreateDeviceRGB();

  // Since icns files are always a power of two, bytes per row are always a
  // multiple of 8, as apple requires it
  context = CGBitmapContextCreate(dst, img.w, img.h,
      8, img.w * (img.c == 1 ? 1 : 4), color_space,
      img.c == 1 ? kCGImageAlphaNone : kCGImageAlphaNoneSkipFirst);

  image = CGBitmapContextCreateImage(context);
  if (!image) { result = false; goto cleanup; }

  image_dest
    = CGImageDestinationCreateWithURL(texture_url, kUTTypePNG, 1, NULL);
  if (!image_dest) { result = false; goto cleanup; }


  CGImageDestinationAddImage(image_dest, image, NULL);
  if (!CGImageDestinationFinalize(image_dest))
    result = false;

cleanup:
  if (pathString) CFRelease(pathString);
  if (context) CFRelease(context);
  if (color_space) CFRelease(color_space);
  if (image) CFRelease(image);
  if (image_dest) CFRelease(image_dest);
  if (texture_url) CFRelease(texture_url);
  if (dst) free(dst);

  assert(result);
  return result;
}

std::string format(const char* s, va_list argList)
{
#ifdef _MSC_VER
  #define vsnprintf _vsnprintf
#endif
  std::string str;
  const int startSize = 161;
  int currSize, maxSize = 1000000;
  char buff1[startSize];
  currSize = startSize - 1;
  int ret = vsnprintf(buff1, currSize, s, argList);
  if(ret >= 0 && ret <= startSize)
    str = std::string(buff1);
  else
  {
    char* buff2 = NULL;
    while((ret < 0 || ret > currSize + 1)&& currSize < maxSize)
    {
      currSize *= 2;
      buff2 = (char*)realloc(buff2, currSize + 1);
      ret = vsnprintf(buff2, currSize, s, argList);
    }
    str = std::string(buff2);
  }
  return str;
}

bool SaveImage(const Img& img, const char* name, ...)
{

  va_list argList;
  va_start(argList, name);
  std::string str = format(name, argList);
  va_end(argList);

  return SaveImage(str.c_str(), img);
}
  
#endif


// Creates a gaussian filter of size w x h. Both must be odd.
void gauss(double* dest, int w, int h, double sigma) {
  assert(w%2 != 0 && h%2 != 0);
  double sum = 0.0;
  for (int dy = -(h - 1)/2, idy = 0; idy < h; ++dy, ++idy) {
    for (int dx = -(w - 1)/2, idx = 0; idx < w; ++dx, ++idx) {
      dest[idy * w + idx] = exp(- (dx*dx + dy*dy) / (2.0 * sigma * sigma));
      sum += dest[idy * w + idx];
    }
  }

  // normalize
  for (int idy = 0; idy < h; ++idy)
    for (int idx = 0; idx < w; ++idx)
      dest[idy * w + idx] /= sum;
}


// Filters an image with an fw x fw filter.
// Pixels outside of the error are assumed to have the value of the nearest
// border pixel.
// fw must be odd.
void filter(double* dst, const double* src, int w, int h,
    double* filter, int fw, int nChans = 1) {
  assert(fw%2 != 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < nChans; ++c) {
        *dst = 0.0;
        for (int dy = -(fw - 1)/2, idy = 0; idy < fw; ++dy, ++idy) {
          for (int dx = -(fw - 1)/2, idx = 0; idx < fw; ++dx, ++idx) {
            *dst +=
              src[(clamp(y + dy, 0, h-1)*w + clamp(x + dx, 0, w-1))*nChans + c]
              * filter[idy*fw + idx];
          }
        }
        ++dst;
      }
    }
  }
}

void separableFilter33(double* dst, const double* src, int w, int h,
    double fx[3], double fy[3], int nChans = 1) {
  double f[3 * 3];
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x)
      f[y*3 + x] = fy[y] * fx[x];
  filter(dst, src, w, h, f, 3, nChans);
}

void calcDx(double* dst, const double* src, int w, int h, int nChans = 1) {
  double xfilter[] = { -0.5, 0, 0.5 };
  double yfilter[3]; gauss(yfilter, 3, 1, 0.8);
  separableFilter33(dst, src, w, h, xfilter, yfilter, nChans);
}

void calcDy(double* dst, const double* src, int w, int h, int nChans = 1) {
  double xfilter[3]; gauss(xfilter, 3, 1, 0.8);
  double yfilter[] = { -0.5, 0, 0.5 };
  separableFilter33(dst, src, w, h, xfilter, yfilter, nChans);
}

void calcDt(double* dst, const double* src0, const double* src1, int w, int h,
    int nChans = 1, const double* mask = NULL) {
  double f[3 * 3]; gauss(f, 3, 3, 1.0);

  Img tmp(w, h, nChans);
  filter(tmp.pix, src0, w, h, f, 3, nChans);
  filter(dst, src1, w, h, f, 3, nChans);

  // filter mask as well
  Img filteredMask;
  if (mask) {
    filteredMask.setSize(w, h);
    filter(filteredMask.pix, mask, w, h, f, 3);

    // is premul followed by filtering the same as filtering mask and image
    // and the premul'ing? -> No. damn. ((x1 + x2) * (a1 + a2) != a1*x1 + a2*x2)
    //
    // So the formula below is not 100% correct.
  }

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < nChans; ++c) {
        if (!mask)
          dst[(y*w + x)*nChans + c] -= tmp.pix[(y*w + x)*nChans + c];
        // d - (1-a)*d + a*s = a*(s - d) = premul - a*d
        else
          dst[(y*w + x)*nChans + c]
            = filteredMask.pix[y*w + x]*dst[(y*w + x)*nChans + c]
            - tmp.pix[(y*w + x)*nChans + c];
      }
    }
  }
}

void printMatrix(const double* m, int w, int h, const char* fmt = "%.4f") {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      printf(fmt, m[y*w + x]);
      printf(" ");
    }
    printf("\n");
  }
}


double lerp(double s, double x0, double x1) {
  return x0 + s*(x1 - x0);
}

double sample(const double* src, int w, int h, int nc,
    double x, double y, int c) {
#if 1
  // extend border
  int x0 = clamp((int)x, 0, w-1);
  int x1 = clamp(x0 + 1, 0, w-1);
  double fx = x - (int)x;

  int y0 = clamp((int)y, 0, h-1);
  int y1 = clamp(y0 + 1, 0, h-1);
  double fy = y - (int)y;

  double s0 = lerp(fx, src[(y0*w + x0)*nc + c], src[(y0*w + x1)*nc + c]);
  double s1 = lerp(fx, src[(y1*w + x0)*nc + c], src[(y1*w + x1)*nc + c]);

  return lerp(fy, s0, s1);
#else
  // outside is white
  int x0 = (int)x;
  int x1 = x0 + 1;
  double fx = x - (int)x;

  int y0 = (int)y;
  int y1 = y0 + 1;
  double fy = y - (int)y;

  if (y0 < 0 || x0 < 0 || y1 >= h || x1 >= w) return 1.0;

  double s0 = lerp(fx, src[(y0*w + x0)*nc + c], src[(y0*w + x1)*nc + c]);
  double s1 = lerp(fx, src[(y1*w + x0)*nc + c], src[(y1*w + x1)*nc + c]);

  return lerp(fy, s0, s1);
#endif
}

// Does an affine map using parameters a thusly:
//   x' = a[0] + a[1] * x + a[2] * y
//   y' = a[3] + a[4] * x + a[5] * y
// Uses an inverse mapping and linear interpolation to fight aliasing
void interp2(double* dest, const double* src, int w, int h, double a[6],
    int nc = 1) {
  // Found by Cramer's rule applied to homogenous coordinates
  double det = a[1]*a[5] - a[2]*a[4];
  double aInv[6] = { (a[2]*a[3] - a[0]*a[5])/det,  a[5]/det, -a[2]/det,
                     (a[0]*a[4] - a[1]*a[3])/det, -a[4]/det,  a[1]/det };
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {

      double dx = x - (w - 1)/2.0;
      double dy = y - (h - 1)/2.0;

      double sx = aInv[0] + aInv[1] * dx + aInv[2] * dy;
      double sy = aInv[3] + aInv[4] * dx + aInv[5] * dy;

      sx = sx + (w - 1)/2.0;
      sy = sy + (h - 1)/2.0;

      for (int c = 0; c < nc; ++c) {
        dest[(w*y + x)*nc + c] = sample(src, w, h, nc, sx, sy, c);
      }
    }
  }
}

// Uses only translation/scaling
void interp2Scale(double* dest, const double* src, int w, int h, double a[4],
    int nc = 1) {
  double aFull[] = { a[0], a[1], 0, a[2], 0, a[3] };
  interp2(dest, src, w, h, aFull, nc);
}

template <class Num>
bool gaussJordan(Num* matrix, Num* rhs, int n)
{
  int rows = n;
  int i; //loop counter

  std::vector<int> pivotRows(n);
  std::vector<int> pivotColumns(n);
  std::vector<bool> wasColumnUsed(n, false);

  for(int rowCount = 0; rowCount < rows; ++rowCount)
  {
    Num maxVal = (Num)0;
    int currPivotRow = -1, currPivotColumn = -1;
    //search for biggest number in matrix, use it as pivot
    for(i = 0; i < rows; ++i) //loop thru rows
    {
      if(wasColumnUsed[i]) //use only one pivot from each row
        continue;

      for(int j = 0; j < rows; ++j) //loop thru columns
      {
        if(wasColumnUsed[j]) //use only one pivot from each column
          continue;

        Num curr = (Num)fabs(matrix[j*n + i]);
        if(curr > maxVal)
        {
          maxVal = curr;
          currPivotRow = j;
          currPivotColumn = i;
        }
      }
    }

    assert (currPivotRow != -1 && currPivotColumn != -1);
    if(wasColumnUsed[currPivotColumn]) {
      assert(false);
      return false;
    }
    wasColumnUsed[currPivotColumn] = true;

    //store which pivot was chosen in the rowCount'th run
    //to reswap inverse matrix afterwards
    pivotRows[rowCount] = currPivotRow;
    pivotColumns[rowCount] = currPivotColumn;


    //swap rows to bring pivot on diagonal
    if(currPivotRow != currPivotColumn)
    {
      for(i = 0; i < rows; ++i)
        std::swap(matrix[currPivotRow*n + i], matrix[currPivotColumn*n + i]);
      std::swap(rhs[currPivotRow], rhs[currPivotColumn]);
      currPivotRow = currPivotColumn;
    }

    if(matrix[currPivotRow*n + currPivotColumn] == (Num)0) {
      assert(false);
      return false;
    }

    //start eliminating the pivot's column in each row
    Num inversePivot = ((Num)1)/matrix[currPivotRow*n + currPivotColumn];

    matrix[currPivotRow*n + currPivotColumn] = (Num)1;
    for(i = 0; i < rows; ++i)
      matrix[currPivotRow*n + i] *= inversePivot;
    rhs[currPivotRow] *= inversePivot;

    //reduce non-pivot rows
    for(i = 0; i < rows; ++i)
    {
      if(i == currPivotRow) //pivot row is already reduced
        continue;

      Num temp = matrix[i*n + currPivotRow];
      matrix[i*n + currPivotRow] = (Num)0;
      for(int j = 0; j < rows; ++j)
        matrix[i*n + j] -= temp*matrix[currPivotRow*n + j];
      rhs[i] -= rhs[currPivotRow] * temp;
    }
  }

#if 0  // we don't need the inverse, only the solution
  //unscramble inverse matrix
  for(i = rows - 1; i >= 0; --i)
  {
    if(pivotRows[i] != pivotColumns[i])
    {
      for(int j = 0; j < rows; ++j)
        std::swap(matrix[j*n + pivotRows[i]], matrix[j*n + pivotColumns[i]]);
    }
  }
#endif

  //done :-)!!

  return true;
}



// Computes the flow from i0 to i1, stores results in a. a must contain a
// valid close starting value (e.g. { 0, 1, 0, 1 })
void basicFlow(const double* i0, const double* i1, int w, int h, double* a,
    int nc, int iters, const double* mask, int index) {
  // XXX: add ROI
  const double EPS = 1e-4;

  Img warped(w, h, nc);

  Img dx(w, h, nc);
  Img dy(w, h, nc);
  Img dt(w, h, nc);

  for (int i = 0; i < iters; ++i) {

    printf("Iter %d: %f %f %f %f\n", i, a[0], a[1], a[2], a[3]);

    double aInv[4] = { -a[0]/a[1], 1.0/a[1], -a[2]/a[3], 1.0/a[3] };
    interp2Scale(warped.pix, i1, w, h, aInv, nc);
#if 0
    Img warpedMask(w, h);  // mask is always just one channel
    // Turns out applying the mask to the background image confuses the
    // algorithm.

    if (mask) {
      // XXX: do i really need to transform the mask? i doubt it.
      interp2Scale(warpedMask.pix, mask, w, h, aInv);

      // Leaving this out doens't seem to cause much harm, putting it in
      // correctly (untransformed mask) does.

      // premultiply into warped image so that calcDx/Dy use the premultiplied
      // data
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          for (int c = 0; c < nc; ++c)
            //warped.pix[nc*(y*w + x) + c] *= warpedMask.pix[y*w + x];
            warped.pix[nc*(y*w + x) + c] *= mask[y*w + x];
    }
#endif


    SaveImage(warped, "%d_warped_%03d_%03d.png", index, w, i);

    // XXX: these need to compute norm or rgb vectors
    calcDx(dx.pix, warped.pix, w, h, nc);
    calcDy(dy.pix, warped.pix, w, h, nc);
    calcDt(dt.pix, i0, warped.pix, w, h, nc, mask);

    // Makes only a difference of 20 seconds when running this on 14 inputs!
    //SaveImage("dx.png", dx);
    //SaveImage("dy.png", dy);
    //SaveImage("dt.png", dt);

    double* fImg[] = { dx.pix, dx.pix, dy.pix, dy.pix };
    double fPos[] = { 1.0, 0.0, 1.0, 0.0 };

    double structureTensor[16] = { 0.0 }, rhs[4] = { 0.0 };
    for (int y = 0; y < h; ++y) {
      fPos[3] = y - (h - 1)/2.0 ;
      for (int x = 0; x < w; ++x) {
        fPos[1] = x - (w - 1)/2.0 ;

        for (int j = 0; j < 4; ++j) {
          for (int k = 0; k < 4; ++k) {
            for (int c = 0; c < nc; ++c)
              structureTensor[j*4 + k] += fImg[j][(y*w + x)*nc + c]
                * fImg[k][(y*w + x)*nc + c]
                * fPos[j]
                * fPos[k];
          }
          for (int c = 0; c < nc; ++c)
            rhs[j] -=
              fImg[j][(y*w + x)*nc + c]
              * dt.pix[(y*w + x)*nc + c]
              * fPos[j];
        }
      }
    }

    // Solve linear equation
    //printMatrix(structureTensor, 4, 4); printf("\n");
    //printMatrix(rhs, 4, 1); printf("\n");
    gaussJordan(structureTensor, rhs, 4);
    //printMatrix(rhs, 4, 1); printf("\n");

    double norm = 0.0;
    for (int j = 0; j < 4; ++j)
      norm += rhs[j] * rhs[j];
    if (sqrt(norm) < EPS) return;

    float damp = 0.8;
    for (int j = 0; j < 4; ++j)
      a[j] += damp*rhs[j];
  }
}

void downsample2(double* dst, const double* src, int w, int h, int nc = 1) {
  for (int y = 0; y < h/2; ++y) {
    for (int x = 0; x < w/2; ++x) {
      for (int c = 0; c < nc; ++c) {
        int di = (y*(w/2) + x)*nc;
        int si = (2*y*w + 2*x)*nc;
        for (int c = 0; c < nc; ++c) {
          dst[di + c] = src[si + c];
          if (x + 1 < w) dst[di + c] += src[si + nc + c];
          if (y + 1 < h) dst[di + c] += src[si + w*nc + c];
          if (x + 1 < w && y + 1 < h) dst[di + c] += src[si + (w + 1)*nc + c];
          dst[di + c] /= 4.0;  // XXX: darkens edge too much
        }
      }
    }
  }
}

Img** gaussianPyramid(const double* src, int w, int h,
    double sigma, int levels, int nc = 1) {
  Img** pyr = new Img*[levels];
  pyr[0] = new Img(w, h, nc);
  memcpy(pyr[0]->pix, src, w * h * nc * sizeof(double));

  double f[5 * 5]; gauss(f, 5, 5, sigma);
  for (int i = 1; i < levels; ++i) {
    double* tmp = allocImage(w, h, nc);
    filter(tmp, pyr[i - 1]->pix, w, h, f, 5, nc);//XXX: use "0.0" outside image?
    pyr[i] = new Img(w/2, h/2, nc);
    downsample2(pyr[i]->pix, tmp, w, h, nc);
    free(tmp);
    w /= 2; h /= 2;
  }
  return pyr;
}

void freePyr(Img** pyr, int levels) {
  for (int i = 0; i < levels; ++i)
    delete pyr[i];
  delete [] pyr;
}

void pyramidFlow(const double* i0, const double* i1, int w, int h, double* a,
    int levels, const double* mask, int index) {
  Img** pyr0 = gaussianPyramid(i0, w, h, 0.8, levels, 3);
  Img** pyr1 = gaussianPyramid(i1, w, h, 0.8, levels, 3);
  Img** pyrMask = gaussianPyramid(mask, w, h, 0.8, levels, 1);

  // transform the larger pyramid levels to grayscale for speed
  // Still use color in the small pyramid levels. This makes a difference for
  // the firefox icon for example.
  for (int i = 0; i < levels; ++i) {
    if (pyr0[i]->w >= 128)
      pyr0[i]->toGray();
    if (pyr1[i]->w >= 128)
      pyr1[i]->toGray();
  }

  //a[0] = 0.0; a[1] = 1.0;
  //a[2] = 0.0; a[3] = 1.0;

  // Somewhat reasonable first guess for doc icons
  // 11 successes with this
  a[0] = 0.0; a[1] = 0.5;
  a[2] = 0.0; a[3] = 0.5;

for (int i = levels - 1; i >= 0; --i) {

    SaveImage(*pyr0[i], "%d_pyr0_%d.png", index, i);
    SaveImage(*pyr1[i], "%d_pyr1_%d.png", index, i);
    SaveImage(*pyrMask[i], "%d_pyrmask_%d.png", index, i);

    a[0] *= 2.0;
    a[2] *= 2.0;

    // damp 0.8, 30 iters: http client works
    // damp 0.9, 50 iters: acorn works
    // damp 0.8, 50 iters: http client works, http works (12 working, only Ff,
    //                     chaching missing)
    int iters = 50;
    if (pyr0[i]->w <= 32) iters = 100;

    printf("Pyr level %d\n", i);
    basicFlow(pyr0[i]->pix, pyr1[i]->pix, pyr0[i]->w, pyr0[i]->h, a,
        pyr0[i]->c, iters, pyrMask[i]->pix, index);
  }

  freePyr(pyr0, levels);
  freePyr(pyr1, levels);
  freePyr(pyrMask, levels);
}


int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Expected two arguments\n");
    return 1;
  }

#if 0  // test all the stuff above
  double m[5 * 5];
  gauss(m, 5, 5, 0.8);
  //printMatrix(m, 5, 5);

  // test solver
  double mat[] = {
    0.5, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 1.0,
    0.0, 0.0, 1.0, 0.0,
    };
  double rhs[] = { 1.0, 2.0, 3.0, 4.0 };
  gaussJordan(mat, rhs, 4);
  printMatrix(rhs, 4, 1);

  Img i0(argv[1]);
  Img i1(i0.w, i0.h);

  // fixed transform, use and test if basicFlow correctly recovers it.
  double a[] = {
    15.0, 0.5,
    0.0, 0.5
  };
  interp2Scale(i1.pix, i0.pix, i0.w, i0.h, a);

  i0.save("in.png");
  i1.save("out.png");

  // Starting guess
  a[0] = 0.0; a[1] = 1.0;
  a[2] = 0.0; a[3] = 1.0;

  // Try to reconstruct original distortion
  //basicFlow(i0.pix, i1.pix, i0.w, i0.h, a);
  pyramidFlow(i0.pix, i1.pix, i0.w, i0.h, a, 4, 20);

  printf("Result:\n");
  printMatrix(a, 4, 1);

  interp2Scale(i1.pix, i0.pix, i0.w, i0.h, a);
  i1.save("out_estimated.png");

#endif

  ImageCollection docIcons = loadImageSource(argv[1]);
  ImageCollection appIcons = loadImageSource(argv[2]);

  int numDocs = getImageCount(docIcons);
  int numApps = getImageCount(appIcons);
  if (numDocs != numApps)
    printf("Image counts do not match (%d != %d)\n", numDocs, numApps);

  std::map<int, std::vector<double> > foundRects;
  for (int docIndex = 0, appIndex = 0;
      docIndex < numDocs && appIndex < numApps;) {


    bool downsampleAppIcon = false;

    // We assume that sizes get smaller with larger indices (which is true
    // for most icns files: they contain all truecolor variants in falling
    // sizes, sometimes followed by low-color larger icons). Good enough,
    // I only care about truecolor icons.
    if (getWidth(docIcons, docIndex) < getWidth(appIcons, appIndex)) {
      ++appIndex; continue;
    } else if (getWidth(docIcons, docIndex) > getWidth(appIcons, appIndex)) {
      if (2*getWidth(docIcons, docIndex) == getWidth(appIcons, appIndex - 1)
          && appIndex > 0) {
        // if the docicon has a 256x256 icon, but the app icon has only
        // 512x512 and 128x128, use downsampled 512 variant for detection
        downsampleAppIcon = true;
      } else {
        ++docIndex; continue;
      }
    }

    if (
        // flow can only deal with power-of-two images (wouldn't take too much
        // work, but I'm too lazy).
        getWidth(docIcons, docIndex) == 48

        // The 1-bit variants are useless, the indexed variants not interesting.
        || !trueColor(docIcons, docIndex) || !trueColor(appIcons, appIndex)
       ) {
      printf("Skipping doc variant %d, app variant %d\n", docIndex, appIndex);
      ++appIndex; ++docIndex; continue;
    }



    printf("Collection index %d\n", docIndex);

    Img docIcon, appIcon, appIconMask;

    if (!imageFromSource(docIcons, docIndex, docIcon)) {
      printf("Failed to load %d %s, exiting.\n", docIndex, argv[1]);
      return -1;
    }

    if (!downsampleAppIcon) {
      if (!imageFromSource(appIcons, appIndex, appIcon, &appIconMask)) {
        printf("Failed to load %d %s, exiting.\n", appIndex, argv[2]);
        return -1;
      }
    } else {
      Img tmp, tmpMask;
      appIndex--;  // this is incremented again later on
      if (!imageFromSource(appIcons, appIndex, tmp, &tmpMask)) {
        printf("Failed to load %d %s, exiting.\n", appIndex, argv[2]);
        return -1;
      }

      appIcon.setSize(tmp.w/2, tmp.h/2, tmp.c);
      downsample2(appIcon.pix, tmp.pix, tmp.w, tmp.h, tmp.c);
      appIconMask.setSize(tmp.w/2, tmp.h/2);
      downsample2(appIconMask.pix, tmpMask.pix, tmpMask.w, tmpMask.h, 1);
    }

    if (docIcon.w != appIcon.w || docIcon.h != appIcon.h) {
      printf("Image dimensions do not match, exiting.\n");
      return -1;
    }

    printf("%dx%d\n", appIcon.w, appIcon.h);

    //double f[5 * 5]; gauss(f, 5, 5, 0.8);
    //filter(i0.pix, i1.pix, i0.w, i0.h, f, 5, n);

    SaveImage(docIcon, "%d_in.png", docIndex);
    SaveImage(appIconMask, "%d_out_mask.png", docIndex);
    SaveImage(appIcon, "%d_out.png", docIndex);

    int levels = 0;
    while ((1 << levels) < docIcon.w) ++levels;
    //levels -= 4; // smallest size is 32x32 (for Preview.app) (3 successes, few close)
    levels -= 3; // smallest size is 16x16 (for Terminal.app) (5 successes)
    if (levels < 1) levels = 1;  // don't ignore 16x16 version

    double a[4];
    pyramidFlow(appIcon.pix, docIcon.pix, docIcon.w, docIcon.h, a, levels,
        appIconMask.pix, docIndex);

    printMatrix(a, 4, 1);
    interp2Scale(docIcon.pix, appIcon.pix, docIcon.w, docIcon.h, a, 3);
    SaveImage(docIcon, "%d_out_estimated.png", docIndex);

    for (int t = 0; t < 4; ++t)
      foundRects[docIcon.w].push_back(a[t]);

    ++docIndex, ++appIndex;
  }

  std::map<int, std::vector<double> >::iterator it, end = foundRects.end();
  printf("\nrects = {\n");
  for (it = foundRects.begin(); it != end; ++it) {
    printf("    %3d: (", it->first);
    for (int t = 0; t < 4; ++t)
      printf("%8.4f%s", it->second[t], t == 3 ? "" : ", ");
    printf("),\n");
  }
  printf("}\n");

  freeImageSource(docIcons);
  freeImageSource(appIcons);
}

