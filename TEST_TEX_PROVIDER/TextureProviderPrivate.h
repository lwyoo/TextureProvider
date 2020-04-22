#pragma once

#include <QMutex>
#include <QOpenGLTexture>
#include <QtQuick/QQuickTextureFactory>
#include <QtQuick/QSGTexture>

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QString>

#include "ImageRsc.h"

class QQuickTextureFactory;
class QQuickView;

struct PKMImage;
struct ASSAImage;
class TextureProvider;
class Cache;

class ImageBase
{
    friend Cache;

public:
    ImageBase(const char* id, int size)
        : m_id(id)
        , memSizeInBytes(size)
    {
        pPrev = nullptr;
        pNext = nullptr;
        refCount = 0;
        format = 0;
    }
    virtual ~ImageBase();
    const char* m_id;
    int memSizeInBytes;

    int refCount;
    int format;

private:
    ImageBase* pPrev;
    ImageBase* pNext;

    void AddRef() { refCount++; }
    void Delete() { refCount--; }
};

struct CPUImage : public ImageBase
{
    QSize effectiveSize;
    QSize originalSize;
    const void* data;

    CPUImage(const char* id, int size, void* param);
    virtual ~CPUImage() override;
};

class TextureFactory : public QQuickTextureFactory
{
public:
    TextureFactory(CPUImage* image);
    virtual ~TextureFactory() override;

    virtual QSGTexture* createTexture(QQuickWindow* window) const Q_DECL_OVERRIDE;
    virtual QImage image() const Q_DECL_OVERRIDE;
    virtual int textureByteCount() const Q_DECL_OVERRIDE;
    virtual QSize textureSize() const Q_DECL_OVERRIDE;

private:
public:
    CPUImage* m_image;
};

class Cache
{

public:
    Cache(int size)
        : m_pHead(nullptr)
        , m_pTail(nullptr)
        , m_pHeadFree(nullptr)
        , m_pTailFree(nullptr)
        , m_initsize(size)
        , m_sizeMemFree(size)
        , m_nImgCount(0)
        , m_nImgFreeCount(0)
        , m_nHitCount(0)
        , m_nMissCount(0)
        , m_nFactor(1)
        , m_nOffset(0)
    {
    }

public:
    virtual ~Cache();
    virtual ImageBase* CreateNewInstance(const char* id, int size, void* param) = 0;
    bool isGPUMem = false;

private:
    void RemoveLocal(ImageBase* pImg);
    bool MakeRoomForNewEntry(long nSize);

private:
    void CheckImgFree();
    void CheckImg();

public:
    void Remove(ImageBase* pImg)
    {
        QMutexLocker lock(&m_mutex);
        RemoveLocal(pImg);
    }
    ImageBase* Search(const char* id);
    ImageBase* InsertIntoHead(const char* id, int size, void* param);

private:
    ImageBase* m_pHead;
    ImageBase* m_pTail;
    ImageBase* m_pHeadFree;
    ImageBase* m_pTailFree;
    QHash<const char*, ImageBase*> m_hash;
    QMutex m_mutex;

public:
    int m_initsize;
    int m_sizeMemFree;
    int m_nImgCount;
    int m_nImgFreeCount;
    int m_nHitCount;
    int m_nMissCount;
    int m_nFactor;
    int m_nOffset;
};

class CPUCache : public Cache
{
public:
    CPUCache(int size)
        : Cache(size)
    {
    }
    virtual ImageBase* CreateNewInstance(const char* id, int size, void* param) override;
};

class GPUCache : public Cache
{
public:
    GPUCache(int size)
        : Cache(size)
    {
        m_nFactor = 1;
        m_nOffset = 0;
        isGPUMem = true;
    }
    virtual ImageBase* CreateNewInstance(const char* id, int size, void* param) override;
};

struct TextureImage : public ImageBase
{
    QOpenGLTexture m_texture;
    QSize effectiveSize;

    TextureImage(const char* id, int size, void* param);
    virtual ~TextureImage() override;

    int textureId();
    void Bind();
};

class MSGTexture : public QSGTexture
{
public:
    MSGTexture(CPUImage* image);
    virtual ~MSGTexture() override;
    virtual void bind() Q_DECL_OVERRIDE;
    virtual bool hasAlphaChannel() const Q_DECL_OVERRIDE;
    virtual bool hasMipmaps() const Q_DECL_OVERRIDE { return false; }

    virtual int textureId() const Q_DECL_OVERRIDE { return m_image ? m_image->textureId() : 0; }
    virtual QSize textureSize() const Q_DECL_OVERRIDE { return m_image->effectiveSize; }
    virtual bool isAtlasTexture() const override { return false; }

private:
public:
    TextureImage* m_image;
};

class TextureProviderPrivate
{
public:
    QQuickTextureFactory* requestTexture(const QString& id, QSize* size, const QSize& /*requestedSize*/);
    QImage requestImage(const QString& id, QSize* /*size*/, const QSize& /*requestedSize*/);
    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize);
    static int inf(int source, size_t nLen, void** pOut, size_t nSize, qint64 offset);

public:
    TextureProviderPrivate(const QString& path, int imgCacheSize, int textureCacheSize);

public:
    QHash<QString, imgToRscEntry*> m_hash;
    CPUCache cpuImage;
    GPUCache gpuImage;
    int fd;
};
