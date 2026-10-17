#include "TestAssert.h"

#include <allheaders.h>
#include <iostream>
#include <string>

int test_leptonica_png_and_tiff_codecs() {
    char* versions = getImagelibVersions();
    CPPTEST_ASSERT(versions != nullptr);
    const std::string versionText(versions);
    lept_free(versions);

    CPPTEST_ASSERT(versionText.find("libpng") != std::string::npos);
    CPPTEST_ASSERT(versionText.find("libtiff") != std::string::npos);

    PIX* source = pixCreate(32, 24, 1);
    CPPTEST_ASSERT(source != nullptr);
    pixSetPixel(source, 7, 9, 1);

    l_uint8* pngData = nullptr;
    size_t pngSize = 0;
    CPPTEST_ASSERT(pixWriteMemPng(&pngData, &pngSize, source, 0.0F) == 0);
    CPPTEST_ASSERT(pngData != nullptr && pngSize > 8);
    PIX* png = pixReadMemPng(pngData, pngSize);
    lept_free(pngData);
    CPPTEST_ASSERT(png != nullptr);

    l_uint8* tiffData = nullptr;
    size_t tiffSize = 0;
    CPPTEST_ASSERT(pixWriteMemTiff(&tiffData, &tiffSize, source, IFF_TIFF_G4) == 0);
    CPPTEST_ASSERT(tiffData != nullptr && tiffSize > 8);
    PIX* tiff = pixReadMemTiff(tiffData, tiffSize, 0);
    lept_free(tiffData);
    CPPTEST_ASSERT(tiff != nullptr);

    l_uint32 pngPixel = 0;
    l_uint32 tiffPixel = 0;
    CPPTEST_ASSERT(pixGetPixel(png, 7, 9, &pngPixel) == 0);
    CPPTEST_ASSERT(pixGetPixel(tiff, 7, 9, &tiffPixel) == 0);
    CPPTEST_ASSERT(pngPixel == 1 && tiffPixel == 1);

    pixDestroy(&tiff);
    pixDestroy(&png);
    pixDestroy(&source);

    return 0;
}

int test_leptonica_bitmap_font() {
    L_BMF* font = bmfCreate(nullptr, 10);
    if (font == nullptr)
        std::cerr << "Leptonica bitmap font creation failed. Rebuild the selected "
                     "Leptonica package with PNG, TIFF, JPEG and zlib support.\n";
    CPPTEST_ASSERT(font != nullptr);
    bmfDestroy(&font);
    CPPTEST_ASSERT(font == nullptr);

    return 0;
}

int main() {
    CPPTEST_RUN(test_leptonica_png_and_tiff_codecs);
    CPPTEST_RUN(test_leptonica_bitmap_font);
    return 0;
}
