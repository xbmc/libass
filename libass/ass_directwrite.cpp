/*
 * Copyright (C) 2015 Stephan Vedder <stephan.vedder@gmail.com>
 *
 * This file is part of libass.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "ass_compat.h"

#include <initguid.h>
#include <ole2.h>
#include <shobjidl.h>
#include <dwrite.h>

#include "ass_directwrite.h"
#include "ass_utils.h"
#include "ass_fontselect.h"
#include "ass_library.h"
#include <minwinbase.h>

#define NAME_MAX_LENGTH 256
#define FALLBACK_DEFAULT_FONT L"Arial"

static const ASS_FontMapping font_substitutions[] = {
    {"sans-serif", "Arial"},
    {"serif", "Times New Roman"},
    {"monospace", "Courier New"}
};

/*
 * The private data stored for every font, detected by this backend.
 */
typedef struct
{
    struct IDWriteFont* font;
    struct IDWriteFontFace* face;
    struct IDWriteFontFileStream* stream;
} FontPrivate;

interface LocalFontEnumerator : IDWriteFontFileEnumerator
{
    HRESULT __stdcall QueryInterface(const IID& riid, void** ppvObject) override
    {
        if (IsEqualGUID(riid, __uuidof(IDWriteFontFileEnumerator))
            || IsEqualGUID(riid, __uuidof(IUnknown)))
        {
            *ppvObject = this;
        }
        else
        {
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    ULONG __stdcall AddRef() override
    {
        return InterlockedIncrement(&ref_count);
    }

    ULONG __stdcall Release() override
    {
        auto count = InterlockedDecrement(&ref_count);
        if (count == 0)
            delete this;

        return count;
    }

    HRESULT __stdcall MoveNext(BOOL* hasCurrentFile) override
    {
        if (hFind == INVALID_HANDLE_VALUE)
        {
            hFind = FindFirstFileW(dirPath, &ffd);
            if (hFind == INVALID_HANDLE_VALUE)
            {
                *hasCurrentFile = FALSE;
                return E_INVALIDARG;
            }

            while (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                auto result = FindNextFileW(hFind, &ffd);
                if (result == 0)
                {
                    FindClose(hFind);
                    hFind = INVALID_HANDLE_VALUE;
                    *hasCurrentFile = FALSE;
                    return S_OK;
                }
            }

            FileNameToPath(ffd.cFileName);
            *hasCurrentFile = TRUE;
            return S_OK;
        }

        auto result = FindNextFileW(hFind, &ffd);
        if (result == 0)
        {
            FindClose(hFind);
            hFind = INVALID_HANDLE_VALUE;
            *hasCurrentFile = FALSE;
            return S_OK;
        }

        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            FileNameToPath(ffd.cFileName);

        *hasCurrentFile = TRUE;
        return S_OK;
    }

    HRESULT __stdcall GetCurrentFontFile(IDWriteFontFile** fontFile) override
    {
        auto hr = factory->CreateFontFileReference(filePath, &ffd.ftLastWriteTime, fontFile);
        return hr;
    }

    LocalFontEnumerator(IDWriteFactory* f, char* dir)
        : ref_count(0)
        , dirPathLength(0)
        , dirPath(nullptr)
        , filePath(nullptr)
        , hFind(INVALID_HANDLE_VALUE)
        , factory(f)
    {
        LocalFontEnumerator::AddRef();
        factory->AddRef();
        int len = strlen(dir);
        dirPathLength = len + 2; //add an extra for null and another for a * at the end
        if (len > 32000) //simple sanity checking, around max path length
            return;
        //if (!(dir[0] == '\\' && dir[1] == '\\' && dir[2] == '?' && dir[3] == '\\'))
        //  dirPathLength += 4;

        char* newDir = static_cast<char*>(malloc(dirPathLength));
        //strcpy_s(newDir, dirPathLength, "\\\\?\\");
        strcpy_s(newDir, dirPathLength, dir);
        strcat_s(newDir, dirPathLength, "*");

        dirPath = to_utf16(newDir, dirPathLength);
        free(newDir);
        filePath = static_cast<wchar_t*>(malloc(32000 * sizeof(wchar_t)));
    }

    virtual ~LocalFontEnumerator()
    {
        factory->Release();
        if (dirPath)
            free(dirPath);
        if (filePath)
            free(filePath);
        if (hFind != INVALID_HANDLE_VALUE)
            FindClose(hFind);
    }

private:
    void FileNameToPath(wchar_t* filename) const
    {
        if (dirPath[dirPathLength - 2] == '*')
            dirPath[dirPathLength - 2] = '\0';

        memset(filePath, 0, 32000 * sizeof(wchar_t));
        wcscpy_s(filePath, 32000, dirPath);
        //wcscat_s(filePath, 32000, L"\\");
        wcscat_s(filePath, 32000, filename);
    }

    DWORD ref_count;
    size_t dirPathLength;
    wchar_t* dirPath;
    wchar_t* filePath;
    WIN32_FIND_DATAW ffd;
    HANDLE hFind;
    IDWriteFactory* factory;
};


interface LocalFontLoader : IDWriteFontCollectionLoader
{
    HRESULT __stdcall QueryInterface(const IID& riid, void** ppvObject) override
    {
        if (IsEqualGUID(riid, __uuidof(IDWriteFontCollectionLoader))
            || IsEqualGUID(riid, __uuidof(IUnknown)))
        {
            *ppvObject = this;
        }
        else
        {
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    ULONG __stdcall AddRef() override
    {
        return InterlockedIncrement(&ref_count);
    }

    ULONG __stdcall Release() override
    {
        auto count = InterlockedDecrement(&ref_count);
        if (count == 0)
            delete this;

        return count;
    }

    LocalFontLoader()
        : ref_count(0)
    {
        LocalFontLoader::AddRef();
    }

    virtual ~LocalFontLoader() = default;

    HRESULT __stdcall CreateEnumeratorFromKey(IDWriteFactory* factory, void const* collectionKey, UINT32 collectionKeySize, IDWriteFontFileEnumerator** fontFileEnumerator) override;
private:
    DWORD ref_count;
};

/**
 * Custom text renderer class for logging the fonts used. It does not
 * actually render anything or do anything apart from that.
 */

interface FallbackLogTextRenderer : IDWriteTextRenderer
{
    HRESULT __stdcall QueryInterface(const IID& riid, void** ppvObject) override;
    ULONG __stdcall AddRef() override;
    ULONG __stdcall Release() override;
    HRESULT __stdcall IsPixelSnappingDisabled(void* clientDrawingContext, BOOL* isDisabled) override;
    HRESULT __stdcall GetCurrentTransform(void* clientDrawingContext, DWRITE_MATRIX* transform) override;
    HRESULT __stdcall GetPixelsPerDip(void* clientDrawingContext, FLOAT* pixelsPerDip) override;
    HRESULT __stdcall DrawGlyphRun(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun, DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription, IUnknown* clientDrawingEffect) override;
    HRESULT __stdcall DrawUnderline(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE const* underline, IUnknown* clientDrawingEffect) override;
    HRESULT __stdcall DrawStrikethrough(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const* strikethrough, IUnknown* clientDrawingEffect) override;
    HRESULT __stdcall DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect) override;
    IDWriteFactory* dw_factory;
    virtual ~FallbackLogTextRenderer() = default;

private:
    static DWORD ref_count;
};

DWORD FallbackLogTextRenderer::ref_count = 0;

typedef struct
{
    struct IDWriteFactory* factory;
    struct LocalFontLoader* loader;
    struct LocalFontEnumerator* enumerator;
    char* dirPath;
} ProviderPrivate;

HRESULT FallbackLogTextRenderer::QueryInterface(const IID& riid, void** ppvObject)
{
    if (IsEqualGUID(riid, __uuidof(IDWriteTextRenderer))
        || IsEqualGUID(riid, __uuidof(IDWritePixelSnapping))
        || IsEqualGUID(riid, __uuidof(IUnknown)))
    {
        *ppvObject = this;
    }
    else
    {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG FallbackLogTextRenderer::AddRef()
{
    return InterlockedIncrement(&ref_count);
}

ULONG FallbackLogTextRenderer::Release()
{
    auto new_count = InterlockedDecrement(&ref_count);
    if (new_count == 0)
    {
        delete this;
        return 0;
    }

    return new_count;
}

long FallbackLogTextRenderer::IsPixelSnappingDisabled(void* clientDrawingContext, BOOL* isDisabled)
{
    *isDisabled = true;
    return S_OK;
}

HRESULT FallbackLogTextRenderer::GetCurrentTransform(void* clientDrawingContext, DWRITE_MATRIX* transform)
{
    return E_NOTIMPL;
}

HRESULT FallbackLogTextRenderer::GetPixelsPerDip(void* clientDrawingContext, FLOAT* pixelsPerDip)
{
    return E_NOTIMPL;
}

HRESULT FallbackLogTextRenderer::DrawGlyphRun(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun, DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription, IUnknown* clientDrawingEffect)
{
    HRESULT hr;
    struct IDWriteFontCollection* font_coll = nullptr;
    auto font = static_cast<struct IDWriteFont **>(clientDrawingContext);

    hr = dw_factory->GetSystemFontCollection(&font_coll, FALSE);
    if (FAILED(hr))
        return E_FAIL;

    hr = font_coll->GetFontFromFontFace(glyphRun->fontFace, font);
    if (FAILED(hr))
        return E_FAIL;

    return S_OK;
}

HRESULT FallbackLogTextRenderer::DrawUnderline(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE const* underline, IUnknown* clientDrawingEffect)
{
    return S_OK;
}

HRESULT FallbackLogTextRenderer::DrawStrikethrough(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const* strikethrough, IUnknown* clientDrawingEffect)
{
    return S_OK;
}

HRESULT FallbackLogTextRenderer::DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect)
{
    return S_OK;
}

static void init_FallbackLogTextRenderer(FallbackLogTextRenderer** r,
                                         struct IDWriteFactory* factory)
{
    *r = new FallbackLogTextRenderer();
    (*r)->dw_factory = factory;
    (*r)->AddRef();
}

HRESULT LocalFontLoader::CreateEnumeratorFromKey(IDWriteFactory* factory, void const* collectionKey, UINT32 collectionKeySize, IDWriteFontFileEnumerator** fontFileEnumerator)
{
    auto priv = static_cast<ProviderPrivate*>(const_cast<void*>(collectionKey));
    priv->enumerator = new LocalFontEnumerator(factory, priv->dirPath);
    *fontFileEnumerator = priv->enumerator;
    return S_OK;
}

/*
 * This function is called whenever a font is accessed for the
 * first time. It will create a FontFace for metadata access and
 * memory reading, which will be stored within the private data.
 */
static bool init_font_private_face(FontPrivate* priv)
{
    HRESULT hr;
    struct IDWriteFontFace* face;

    if (priv->face != nullptr)
        return true;

    hr = priv->font->CreateFontFace(&face);
    if (FAILED(hr) || !face)
        return false;

    priv->face = face;
    return true;
}

/*
 * This function is called whenever a font is used for the first
 * time. It will create a FontStream for memory reading, which
 * will be stored within the private data.
 */
static bool init_font_private_stream(FontPrivate* priv)
{
    struct IDWriteFontFile* file = nullptr;
    struct IDWriteFontFileStream* stream = nullptr;
    struct IDWriteFontFileLoader* loader = nullptr;
    UINT32 n_files = 1;
    const void* refKey = nullptr;
    UINT32 keySize = 0;

    if (priv->stream != nullptr)
        return true;

    if (!init_font_private_face(priv))
        return false;

    /* DirectWrite only supports one file per face */
    auto hr = priv->face->GetFiles(&n_files, &file);
    if (FAILED(hr) || !file)
        return false;

    hr = file->GetReferenceKey(&refKey, &keySize);
    if (FAILED(hr))
    {
        file->Release();
        return false;
    }

    hr = file->GetLoader(&loader);
    if (FAILED(hr) || !loader)
    {
        file->Release();
        return false;
    }

    hr = loader->CreateStreamFromKey(refKey, keySize, &stream);
    if (FAILED(hr) || !stream)
    {
        file->Release();
        return false;
    }

    priv->stream = stream;
    file->Release();

    return true;
}

/*
 * Read a specified part of a fontfile into memory.
 * If the font wasn't used before first creates a
 * FontStream and save it into the private data for later usage.
 * If the parameter "buf" is NULL libass wants to know the
 * size of the Fontfile
 */
static size_t get_data(void* data, unsigned char* buf, size_t offset,
                       size_t length)
{
    HRESULT hr;
    auto priv = static_cast<FontPrivate *>(data);
    const void* fileBuf = nullptr;
    void* fragContext = nullptr;

    if (!init_font_private_stream(priv))
        return 0;

    if (buf == nullptr)
    {
        UINT64 fileSize;
        hr = priv->stream->GetFileSize(&fileSize);
        if (FAILED(hr))
            return 0;

        return fileSize;
    }

    hr = priv->stream->ReadFileFragment(&fileBuf, offset,
                                        length, &fragContext);

    if (FAILED(hr) || !fileBuf)
        return 0;

    memcpy(buf, fileBuf, length);

    priv->stream->ReleaseFileFragment(fragContext);

    return length;
}

/*
 * Check whether the font contains PostScript outlines.
 */
static bool check_postscript(void* data)
{
    auto priv = static_cast<FontPrivate *>(data);

    if (!init_font_private_face(priv))
        return false;

    auto type = priv->face->GetType();
    return type == DWRITE_FONT_FACE_TYPE_CFF ||
        type == DWRITE_FONT_FACE_TYPE_RAW_CFF ||
        type == DWRITE_FONT_FACE_TYPE_TYPE1;
}

/*
 * Check if the passed font has a specific unicode character.
 */
static bool check_glyph(void* data, uint32_t code)
{
    auto priv = static_cast<FontPrivate *>(data);
    auto exists = FALSE;

    if (code == 0)
        return true;

    auto hr = priv->font->HasCharacter(code, &exists);
    if (FAILED(hr))
        return false;

    return exists;
}

/*
 * This will release the directwrite backend
 */
static void destroy_provider(void* priv)
{
    auto provider_priv = static_cast<ProviderPrivate *>(priv);
    provider_priv->factory->Release();
    free(provider_priv);
}

/*
 * This will destroy a specific font and it's
 * Fontstream (in case it does exist)
 */

static void destroy_font(void* data)
{
    auto priv = static_cast<FontPrivate *>(data);

    priv->font->Release();
    if (priv->face != nullptr)
        priv->face->Release();
    if (priv->stream != nullptr)
        priv->stream->Release();

    free(priv);
}

static int encode_utf16(wchar_t* chars, uint32_t codepoint)
{
    if (codepoint < 0x10000)
    {
        chars[0] = codepoint;
        return 1;
    }
    else
    {
        chars[0] = (codepoint >> 10) + 0xD7C0;
        chars[1] = (codepoint & 0x3FF) + 0xDC00;
        return 2;
    }
}

static char* get_fallback(void* priv, const char* base, uint32_t codepoint)
{
    HRESULT hr;
    auto provider_priv = static_cast<ProviderPrivate *>(priv);
    auto dw_factory = provider_priv->factory;
    struct IDWriteTextFormat* text_format = nullptr;
    struct IDWriteTextLayout* text_layout = nullptr;
    FallbackLogTextRenderer* renderer;

    init_FallbackLogTextRenderer(&renderer, dw_factory);

    auto requested_font = to_utf16(base, 0);

    hr = dw_factory->CreateTextFormat(requested_font ? requested_font : FALLBACK_DEFAULT_FONT,
                                   nullptr,
                                   DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
                                   DWRITE_FONT_STRETCH_NORMAL, 1.0f, L"", &text_format);
    free(requested_font);
    if (FAILED(hr))
        return nullptr;

    // Encode codepoint as UTF-16
    wchar_t char_string[2];
    auto char_len = encode_utf16(char_string, codepoint);

    // Create a text_layout, a high-level text rendering facility, using
    // the given codepoint and dummy format.
    hr = dw_factory->CreateTextLayout(char_string, char_len, text_format,
                                      0.0f, 0.0f, &text_layout);
    if (FAILED(hr))
    {
        text_format->Release();
        return nullptr;
    }

    // Draw the layout with a dummy renderer, which logs the
    // font used and stores it.
    struct IDWriteFont* font = nullptr;
    hr = text_layout->Draw(&font, renderer, 0.0f, 0.0f);
    // We're done with these now
    text_layout->Release();
    text_format->Release();
    renderer->Release();
    if (FAILED(hr) || font == nullptr)
    {
        return nullptr;
    }


    // Now, just extract the first family name
    auto exists = FALSE;
    struct IDWriteLocalizedStrings* familyNames = nullptr;
    hr = font->GetInformationalStrings(
        DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
        &familyNames, &exists);
    if (FAILED(hr) || !exists)
    {
        font->Release();
        return nullptr;
    }

    wchar_t temp_name[NAME_MAX_LENGTH];
    hr = familyNames->GetString(0, temp_name, NAME_MAX_LENGTH);
    if (FAILED(hr))
    {
        familyNames->Release();
        font->Release();
        return nullptr;
    }
    temp_name[NAME_MAX_LENGTH - 1] = 0;

    // DirectWrite may not have found a valid fallback, so check that
    // the selected font actually has the requested glyph.
    if (codepoint > 0)
    {
        hr = font->HasCharacter(codepoint, &exists);
        if (FAILED(hr) || !exists)
        {
            familyNames->Release();
            font->Release();
            return nullptr;
        }
    }

    auto family = to_utf8(temp_name, 0);

    familyNames->Release();
    font->Release();
    return family;
}

static int map_width(enum DWRITE_FONT_STRETCH stretch)
{
    switch (stretch)
    {
    case DWRITE_FONT_STRETCH_ULTRA_CONDENSED: return 50;
    case DWRITE_FONT_STRETCH_EXTRA_CONDENSED: return 63;
    case DWRITE_FONT_STRETCH_CONDENSED: return FONT_WIDTH_CONDENSED;
    case DWRITE_FONT_STRETCH_SEMI_CONDENSED: return 88;
    case DWRITE_FONT_STRETCH_MEDIUM: return FONT_WIDTH_NORMAL;
    case DWRITE_FONT_STRETCH_SEMI_EXPANDED: return 113;
    case DWRITE_FONT_STRETCH_EXPANDED: return FONT_WIDTH_EXPANDED;
    case DWRITE_FONT_STRETCH_EXTRA_EXPANDED: return 150;
    case DWRITE_FONT_STRETCH_ULTRA_EXPANDED: return 200;
    default:
        return FONT_WIDTH_NORMAL;
    }
}

static char* get_font_path(IDWriteFont* font)
{
    IDWriteFontFace* fontFace;
    auto hr = font->CreateFontFace(&fontFace);
    if (FAILED(hr))
        return nullptr;

    IDWriteFontFile* fontFiles[1];
    UINT32 files = 1;
    hr = fontFace->GetFiles(&files, fontFiles);
    if (FAILED(hr))
    {
        fontFace->Release();
        return nullptr;
    }

    const wchar_t* refKey = nullptr;
    hr = fontFiles[0]->GetReferenceKey(reinterpret_cast<void const **>(&refKey), &files);
    if (FAILED(hr))
    {
        fontFace->Release();
        for (auto f : fontFiles)
            f->Release();
        return nullptr;
    }

    // This must be before we release the reference because the key is
    // only guaranteed to be valid until release
    char* path = nullptr;
    auto start = wcschr(refKey, L':');
    if (start)
    {
        auto diff = start - refKey - 1;
        auto length = files / 2 - diff;
        path = to_utf8(start - 1, length);
    }

    fontFace->Release();
    for (auto f : fontFiles)
        f->Release();

    return path;
}

static void add_font(struct IDWriteFont* font, struct IDWriteFontFamily* fontFamily,
                     ASS_FontProvider* provider)
{
    HRESULT hr;
    BOOL exists;
    wchar_t temp_name[NAME_MAX_LENGTH];
    ASS_FontProviderMetaData meta = {nullptr};

    meta.weight = font->GetWeight();
    meta.width = map_width(font->GetStretch());

    auto style = font->GetStyle();
    meta.slant = (style == DWRITE_FONT_STYLE_NORMAL) ? FONT_SLANT_NONE :
                     (style == DWRITE_FONT_STYLE_OBLIQUE) ? FONT_SLANT_OBLIQUE :
                     (style == DWRITE_FONT_STYLE_ITALIC) ? FONT_SLANT_ITALIC : FONT_SLANT_NONE;

    struct IDWriteLocalizedStrings* psNames;
    hr = font->GetInformationalStrings(
        DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &psNames, &exists);
    if (FAILED(hr))
        goto cleanup;

    if (exists)
    {
        hr = psNames->GetString(0, temp_name, NAME_MAX_LENGTH);
        if (FAILED(hr))
        {
            psNames->Release();
            goto cleanup;
        }

        temp_name[NAME_MAX_LENGTH - 1] = 0;
        auto mbName = to_utf8(temp_name, 0);
        if (!mbName)
        {
            psNames->Release();
            goto cleanup;
        }
        meta.postscript_name = mbName;

        psNames->Release();
    }

    struct IDWriteLocalizedStrings* fontNames;
    hr = font->GetInformationalStrings(
        DWRITE_INFORMATIONAL_STRING_FULL_NAME, &fontNames, &exists);
    if (FAILED(hr))
        goto cleanup;

    if (exists)
    {
        meta.n_fullname = fontNames->GetCount();
        meta.fullnames = static_cast<char **>(calloc(meta.n_fullname, sizeof(char *)));
        if (!meta.fullnames)
        {
            fontNames->Release();
            goto cleanup;
        }
        for (int k = 0; k < meta.n_fullname; k++)
        {
            hr = fontNames->GetString(k, temp_name, NAME_MAX_LENGTH);
            if (FAILED(hr))
            {
                fontNames->Release();
                goto cleanup;
            }

            temp_name[NAME_MAX_LENGTH - 1] = 0;
            char* mbName = to_utf8(temp_name, 0);
            if (!mbName)
            {
                fontNames->Release();
                goto cleanup;
            }
            meta.fullnames[k] = mbName;
        }
        fontNames->Release();
    }

    struct IDWriteLocalizedStrings* familyNames;
    hr = font->GetInformationalStrings(
        DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &familyNames, &exists);
    if (FAILED(hr) || !exists)
        hr = fontFamily->GetFamilyNames(&familyNames);
    if (FAILED(hr))
        goto cleanup;

    meta.n_family = familyNames->GetCount();
    meta.families = static_cast<char **>(calloc(meta.n_family, sizeof(char *)));
    if (!meta.families)
    {
        familyNames->Release();
        goto cleanup;
    }
    for (auto k = 0; k < meta.n_family; k++)
    {
        hr = familyNames->GetString(k, temp_name, NAME_MAX_LENGTH);
        if (FAILED(hr))
        {
            familyNames->Release();
            goto cleanup;
        }

        temp_name[NAME_MAX_LENGTH - 1] = 0;
        auto mbName = to_utf8(temp_name, 0);
        if (!mbName)
        {
            familyNames->Release();
            goto cleanup;
        }
        meta.families[k] = mbName;
    }
    familyNames->Release();

    auto path = get_font_path(font);

    FontPrivate* font_priv = static_cast<FontPrivate *>(calloc(1, sizeof(*font_priv)));
    if (!font_priv)
        goto cleanup;
    font_priv->font = font;
    font = nullptr;

    ass_font_provider_add_font(provider, &meta, path, 0, font_priv);

cleanup:
    if (meta.families)
    {
        for (int k = 0; k < meta.n_family; k++)
            free(meta.families[k]);
        free(meta.families);
    }

    if (meta.fullnames)
    {
        for (int k = 0; k < meta.n_fullname; k++)
            free(meta.fullnames[k]);
        free(meta.fullnames);
    }

    free(meta.postscript_name);

    if (font)
        font->Release();
}

/*
 * Scan every system font on the current machine and add it
 * to the libass lookup. Stores the FontPrivate as private data
 * for later memory reading
 */
static void scan_fonts(struct IDWriteFactory* factory,
                       struct IDWriteFontCollection* fontCollection,
                       ASS_FontProvider* provider)
{
    struct IDWriteFont* font = nullptr;
    HRESULT hr;

    UINT32 familyCount = fontCollection->GetFontFamilyCount();

    for (UINT32 i = 0; i < familyCount; ++i)
    {
        struct IDWriteFontFamily* fontFamily = nullptr;

        hr = fontCollection->GetFontFamily(i, &fontFamily);
        if (FAILED(hr))
            continue;

        UINT32 fontCount = fontFamily->GetFontCount();
        for (UINT32 j = 0; j < fontCount; ++j)
        {
            hr = fontFamily->GetFont(j, &font);
            if (FAILED(hr))
                continue;

            // Simulations for bold or oblique are sometimes synthesized by
            // DirectWrite. We are only interested in physical fonts.
            if (font->GetSimulations() != 0)
            {
                font->Release();
                continue;
            }

            add_font(font, fontFamily, provider);
        }

        fontFamily->Release();
    }

    fontCollection->Release();
}

static void get_substitutions(void* priv, const char* name,
                              ASS_FontProviderMetaData* meta)
{
    const int n = sizeof(font_substitutions) / sizeof(font_substitutions[0]);
    ass_map_font(font_substitutions, n, name, meta);
}

/*
 * Called by libass when the provider should perform the
 * specified task
 */
static ASS_FontProviderFuncs directwrite_callbacks = {
    get_data,
    check_postscript,
    check_glyph,
    destroy_font,
    destroy_provider,
    nullptr,
    get_substitutions,
    get_fallback,
};

/*
 * Register the directwrite provider. Upon registering
 * scans all system fonts. The private data for this
 * provider is IDWriteFactory
 * On failure returns NULL
 */
extern "C"
{
    ASS_FontProvider* ass_directwrite_add_provider(ASS_Library* lib,
                                                   ASS_FontSelector* selector,
                                                   const char* config)
    {
        struct IDWriteFactory* dwFactory = nullptr;
        ASS_FontProvider* provider;
        ProviderPrivate* priv = nullptr;

        auto hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                      __uuidof(IDWriteFactory),
                                      reinterpret_cast<IUnknown **>(&dwFactory));
        if (FAILED(hr) || !dwFactory)
        {
            ass_msg(lib, MSGL_WARN, "Failed to initialize directwrite.");
            dwFactory = nullptr;
            goto cleanup;
        }

        priv = static_cast<ProviderPrivate *>(calloc(sizeof(*priv), 1));
        if (!priv)
            goto cleanup;

        priv->dirPath = lib->fonts_dir;
        priv->factory = dwFactory;
        priv->loader = new LocalFontLoader();
        hr = dwFactory->RegisterFontCollectionLoader(priv->loader);
        if (FAILED(hr))
            goto cleanup;

        IDWriteFontCollection* collection;
        hr = dwFactory->CreateCustomFontCollection(priv->loader, static_cast<void*>(priv), sizeof(ProviderPrivate), &collection);
        if (FAILED(hr))
            goto cleanup;

        provider = ass_font_provider_new(selector, &directwrite_callbacks, priv);
        if (!provider)
            goto cleanup;

        IDWriteFontCollection* systemCollection;
        hr = dwFactory->GetSystemFontCollection(&systemCollection, FALSE);
        if (FAILED(hr))
            goto cleanup;

        scan_fonts(dwFactory, systemCollection, provider);
        scan_fonts(dwFactory, collection, provider);

        return provider;

    cleanup:
        if (priv->enumerator)
            priv->enumerator->Release();
        if (priv->loader)
            priv->loader->Release();

        free(priv);
        if (dwFactory)
            dwFactory->Release();

        return nullptr;
    }
}
