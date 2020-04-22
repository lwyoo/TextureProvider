// polyspace-begin MISRA-CPP:2-10-1 [Justified:Low] "[Maintainability] Applied rule exception - Different identifiers shall be typographically unambiguous"
// polyspace-begin MISRA-CPP:2-10-2 [Justified:Low] "[Maintainability] Applied rule exception - Identifiers declared in an inner scope shall not hide an identifier declared in an outer scope."
// polyspace-begin MISRA-CPP:5-2-1 [Justified:Low] "[Maintainability] Applied rule exception - Each operand of a logical  or || shall be a postfix expression."
// polyspace-begin MISRA-CPP:6-6-5 [Justified:Low] "[Maintainability] Applied rule exception - A function shall have a single point of exit at the end of the function."
// polyspace-begin MISRA-CPP:16-2-1 [Justified:Low] "[Maintainability] Applied rule exception - The pre-processor shall only be used for file inclusion and include guards."
// polyspace-begin MISRA-CPP:18-4-1 [Justified:Low] "[Maintainability] Applied rule exception - Dynamic heap memory allocation shall not be used."

#include "TextureProvider.h"
#include <QDebug>
#include <QFile>
#include <QLatin1String>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QThread>
#include <QtQuick/QQuickTextureFactory>
#include <QtQuick/QSGTexture>
#include <fcntl.h>
#include <qopengl.h>

#include "TextureProviderPrivate.h"
#include <zlib.h>

static TextureProviderPrivate* g_textureProvider;

int TextureProviderPrivate::inf(int source, size_t nLen, void** pOut, size_t nSize, qint64 offset)
{
    int ret;
    z_stream strm;

    strm.zalloc = nullptr;
    strm.zfree = nullptr;
    strm.opaque = nullptr;
    strm.avail_in = 0;
    strm.next_in = nullptr;
    ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return ret;
    }
    //    unsigned char* out = static_cast<unsigned char*>(malloc(nSize));
    unsigned char* out = new unsigned char[nSize];
    *pOut = out;
    strm.avail_out = static_cast<uInt>(nSize);
    strm.next_out = out;
    unsigned char* pData = new unsigned char[nLen];
    strm.avail_in = static_cast<uInt>(nLen);
    strm.next_in = pData;
    pread(source, pData, nLen, offset);

    do {
        ret = inflate(&strm, Z_NO_FLUSH);
        //assert(ret != Z_STREAM_ERROR);   state not clobbered
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR; /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_BUF_ERROR:
            (void)inflateEnd(&strm);
            free(out);
            delete[] pData;
            *pOut = nullptr;
            return ret;
        default:
            // Intentionally left as blank for Misra rule 6-4-6
            break;
        }
    } while ((ret != Z_STREAM_END) && (strm.avail_out == 0));

    delete[] pData;
    /* clean up and return */
    inflateEnd(&strm);
    return (ret == static_cast<int>(Z_STREAM_END)) ? static_cast<int>(Z_OK) : static_cast<int>(Z_DATA_ERROR);
}

TextureImage::TextureImage(const char* id, int size, void* param)
    : ImageBase(id, size)
    , m_texture(QOpenGLTexture::Target2D)
{
    CPUImage* pImg = static_cast<CPUImage*>(param);
    effectiveSize = pImg->effectiveSize;
    format = pImg->format;
    m_texture.setSize(effectiveSize.width(), effectiveSize.height());
    m_texture.setFormat(static_cast<QOpenGLTexture::TextureFormat>(pImg->format));
    m_texture.setMipLevels(1);
    m_texture.setMipLevelRange(0, 0);
    m_texture.allocateStorage();
    if (pImg->format == QOpenGLTexture::RGBA8_ETC2_EAC) {
        m_texture.setCompressedData(pImg->memSizeInBytes, pImg->data);
    } else if (pImg->format == QOpenGLTexture::RGBAFormat) {
        m_texture.setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, pImg->data);
    } else if (pImg->format == QOpenGLTexture::RGBFormat) {
        m_texture.setData(0, QOpenGLTexture::RGB, QOpenGLTexture::UInt8, pImg->data);
    } else {
    }
    m_texture.setWrapMode(QOpenGLTexture::ClampToEdge);
    m_texture.setMinificationFilter(QOpenGLTexture::Linear);
    m_texture.setMagnificationFilter(QOpenGLTexture::Linear);
}

TextureImage::~TextureImage()
{
}

int TextureImage::textureId()
{
    GLuint idVal = m_texture.textureId();
    return static_cast<int>(idVal);
}

void TextureImage::Bind()
{
    m_texture.bind();
}

CPUImage::CPUImage(const char* id, int size, void* param)
    : ImageBase(id, size)
    , data(param)
{
    //    data = malloc(size);
    //    data = param;
}

CPUImage::~CPUImage()
{
    if (data != nullptr) {
        free(const_cast<void*>(data));
    }
}

MSGTexture::MSGTexture(CPUImage* image)
{
    m_image = static_cast<TextureImage*>(g_textureProvider->gpuImage.Search(image->m_id));
    if (m_image == nullptr) {
        m_image = static_cast<TextureImage*>(g_textureProvider->gpuImage.InsertIntoHead(image->m_id, image->memSizeInBytes, image));
    }
}

MSGTexture::~MSGTexture()
{
    g_textureProvider->gpuImage.Remove(m_image);
}

void MSGTexture::bind()
{
    if (m_image == nullptr) {
        return;
    }

    m_image->Bind();
}

bool MSGTexture::hasAlphaChannel() const
{
    bool ret = true;

    if (m_image != nullptr) {
        ret = (m_image->format == QOpenGLTexture::RGBAFormat) || (m_image->format == QOpenGLTexture::RGBA8_ETC2_EAC);
    } else {
    }
    return ret;
}

TextureFactory::TextureFactory(CPUImage* image)
    : m_image(image)
{
}

TextureFactory::~TextureFactory()
{
    g_textureProvider->cpuImage.Remove(m_image);
}

QSGTexture* TextureFactory::createTexture(QQuickWindow* window) const
{
    Q_UNUSED(window)

    return new MSGTexture(m_image);
}

void Cache::RemoveLocal(ImageBase* pImg)
{
    if (pImg->refCount == 1) {
        if (pImg == m_pHead) {
            m_pHead = pImg->pNext;
        } else {
            pImg->pPrev->pNext = pImg->pNext;
        }

        if (pImg == m_pTail) {
            m_pTail = pImg->pPrev;
        } else {
            pImg->pNext->pPrev = pImg->pPrev;
        }

        pImg->pPrev = m_pTailFree;
        pImg->pNext = nullptr;

        if (m_pHeadFree == nullptr) {
            m_pHeadFree = pImg;
        } else {
            m_pTailFree->pNext = pImg;
        }
        m_pTailFree = pImg;
        m_nImgCount--;
        m_nImgFreeCount++;

        pImg->Delete();
    } else if (pImg->refCount > 1) {
        pImg->Delete();
    } else {
        //qDebug() << "Delete File 2 " << pImg->id << pImg->refCount;
    }
}

bool Cache::MakeRoomForNewEntry(long nSize)
{
    ImageBase* pImg = m_pHeadFree;

    pImg = m_pHeadFree;
    while ((pImg != nullptr) && (m_sizeMemFree < nSize)) {
        if (pImg == m_pTailFree) {
            m_pTailFree = nullptr;
        }
        m_pHeadFree = pImg->pNext;
        m_hash.remove(pImg->m_id);
        m_sizeMemFree += pImg->memSizeInBytes;
        m_nImgFreeCount--;
        delete pImg;
        pImg = m_pHeadFree;
    }

    qDebug() << (isGPUMem == true ? "[GPU]" : "[CPU]") << "free: " << m_sizeMemFree << "/" << m_initsize;

    return (m_sizeMemFree >= nSize);
}

ImageBase* Cache::Search(const char* id)
{
    QMutexLocker lock(&m_mutex);
    QHash<const char*, ImageBase*>::const_iterator i = m_hash.find(id);
    ImageBase* pImg = nullptr;

    if (i != m_hash.end()) {
        qDebug() << __PRETTY_FUNCTION__ << "something found";
        pImg = i.value();
        if (pImg != nullptr) {
            m_nHitCount++;
            if (pImg->refCount > 0) {
                if (m_pHead == pImg) {
                    m_pHead = pImg->pNext;
                } else {
                    pImg->pPrev->pNext = pImg->pNext;
                }

                if (m_pTail == pImg) {
                    m_pTail = pImg->pPrev;
                } else {
                    pImg->pNext->pPrev = pImg->pPrev;
                }
            } else {
                if (m_pHeadFree == pImg) {
                    m_pHeadFree = pImg->pNext;
                } else {
                    pImg->pPrev->pNext = pImg->pNext;
                }

                if (m_pTailFree == pImg) {
                    m_pTailFree = pImg->pPrev;
                } else {
                    pImg->pNext->pPrev = pImg->pPrev;
                }
                m_nImgFreeCount--;
                m_nImgCount++;
            }
            pImg->pPrev = nullptr;
            pImg->pNext = m_pHead;

            if (m_pTail == nullptr) {
                m_pTail = pImg;
            } else {
                m_pHead->pPrev = pImg;
            }
            m_pHead = pImg;

            pImg->AddRef();
        }
    } else {

        qDebug() << __PRETTY_FUNCTION__ << "nothing found";
    }

    return pImg;
}

ImageBase* Cache::InsertIntoHead(const char* id, int size, void* param)
{
    QMutexLocker lock(&m_mutex);
    ImageBase* pImg = nullptr;
    bool hasSpace = MakeRoomForNewEntry(size);

    if (hasSpace) {
        pImg = CreateNewInstance(id, size, param);
        if (pImg != nullptr) {
            pImg->pPrev = nullptr;
            pImg->pNext = m_pHead;

            if (m_pTail == nullptr) {
                m_pTail = pImg;
            } else {
                m_pHead->pPrev = pImg;
            }
            m_pHead = pImg;

            m_nImgCount++;
            m_hash.insert(pImg->m_id, pImg);
            if (m_sizeMemFree >= pImg->memSizeInBytes) {
                m_sizeMemFree -= pImg->memSizeInBytes;
            }
            pImg->AddRef();
        }
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "space full !!! ";
    }
    return pImg;
}

QImage TextureFactory::image() const
{
    return QImage(static_cast<const uchar*>(m_image->data), m_image->effectiveSize.width(), m_image->effectiveSize.height(),
        QImage::Format_RGBA8888_Premultiplied);
}

int TextureFactory::textureByteCount() const { return m_image->memSizeInBytes; }
QSize TextureFactory::textureSize() const { return m_image->effectiveSize; }

ImageBase* CPUCache::CreateNewInstance(const char* id, int size, void* param)
{
    QString tempID(id);
    qDebug() << __PRETTY_FUNCTION__ << "tempID : " << tempID;
    CPUImage* pImg = new CPUImage(id, size, param);
    return pImg;
}

ImageBase* GPUCache::CreateNewInstance(const char* id, int size, void* param)
{
    TextureImage* pImg = new TextureImage(id, (m_nFactor * size) + m_nOffset, param);
    return pImg;
}

TextureProviderPrivate::TextureProviderPrivate(const QString& path, int imgCacheSize, int textureCacheSize)
    : cpuImage(imgCacheSize)
    , gpuImage(textureCacheSize)
{
    qDebug() << __PRETTY_FUNCTION__ << path.toLatin1();

    fd = open(path.toLatin1(), O_RDONLY);

    int i = 0;
    while (tableImg[i].nameImage != nullptr) {
        m_hash.insert(QString(tableImg[i].nameImage), &tableImg[i]);
        i++;
    }
    g_textureProvider = this;
}

QQuickTextureFactory* TextureProviderPrivate::requestTexture(const QString& id, QSize* size, const QSize& /*requestedSize*/)
{
    //    qDebug() << __PRETTY_FUNCTION__;
    qDebug() << Q_FUNC_INFO;
    const imgToRscEntry* pEntry = nullptr;
    QHash<QString, imgToRscEntry*>::const_iterator i = m_hash.find(id);
    {
        if (i != m_hash.end()) {
            pEntry = i.value();
        } else {
            qDebug() << __PRETTY_FUNCTION__ << "not found for :" << i.value() << ", " << i.key();
            return nullptr;
        }
    }

    CPUImage* image = static_cast<CPUImage*>(cpuImage.Search(pEntry->nameImage));
    if (image != nullptr) {
        return new TextureFactory(image);
    }

    char sig[5];
    sig[4] = '\0';

    void* pBuf;
    pread(fd, sig, 4, pEntry->offsetImg);
    if (QString(sig) != "rscx") {
        return nullptr;
    }
    inf(fd, static_cast<size_t>(pEntry->lengthImg), &pBuf, static_cast<size_t>(pEntry->imgSize), pEntry->offsetImg + static_cast<qint64>(4));
    if (pBuf == nullptr) {
        qDebug() << "File error : " << pEntry->nameImage << "offsetImg = " << pEntry->offsetImg << "lengthImg = " << pEntry->lengthImg;
        return nullptr;
    }
    CPUImage* pPKMImg = dynamic_cast<CPUImage*>(cpuImage.InsertIntoHead(pEntry->nameImage, pEntry->imgSize, pBuf));
    if (pPKMImg != nullptr) {
        pPKMImg->format = pEntry->formatImg;
        pPKMImg->effectiveSize.setWidth(pEntry->widthImg);
        pPKMImg->effectiveSize.setHeight(pEntry->heightImg);

        if (size != nullptr) {
            *size = pPKMImg->effectiveSize;
        }
        return new TextureFactory(pPKMImg);
    } else {
        qDebug() << " * ??";
    }
    return nullptr;
}

QImage TextureProviderPrivate::requestImage(const QString& id, QSize* /*size*/, const QSize& /*requestedSize*/)
{
    qDebug() << __PRETTY_FUNCTION__;
    QImage img;

    QHash<QString, imgToRscEntry*>::const_iterator i = m_hash.find(id);

    if (i != m_hash.end()) {
        const imgToRscEntry* pEntry = i.value();
        CPUImage* image = static_cast<CPUImage*>(cpuImage.Search(pEntry->nameImage));
        if (image != nullptr) {
            img = QImage(static_cast<const uchar*>(image->data), image->effectiveSize.width(),
                image->effectiveSize.height(), image->effectiveSize.width() * 4, QImage::Format_RGBA8888_Premultiplied);
            cpuImage.Remove(image);
        } else {
            char sig[5];
            sig[4] = '\0';

            void* pBuf;
            pread(fd, sig, 4, pEntry->offsetImg);
            if (QString(sig) == "rscx") {
                inf(fd, static_cast<size_t>(pEntry->lengthImg), &pBuf, static_cast<size_t>(pEntry->imgSize), pEntry->offsetImg + static_cast<qint64>(4));
                if (pBuf == nullptr) {
                    qDebug() << "File error : " << pEntry->nameImage << "offsetImg = " << pEntry->offsetImg << "lengthImg = " << pEntry->lengthImg;
                } else {
                    img = QImage(static_cast<uchar*>(pBuf), pEntry->widthImg,
                        pEntry->heightImg, pEntry->widthImg * 4, QImage::Format_RGBA8888_Premultiplied);
                    //free(pBuf);
                }
            }
        }
    } else {
    }

    return img;
}

TextureProvider::~TextureProvider()
{
}

TextureProvider::TextureProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Texture)
    , pData(nullptr)
{
}

bool TextureProvider::CreateInstance(const QString& path, int imgCacheSize, int textureCacheSize)
{
    qDebug() << __PRETTY_FUNCTION__;
    if (instance().pData == nullptr) {
        instance().pData = new TextureProviderPrivate(path, imgCacheSize, textureCacheSize);
    }
    return instance().pData == nullptr;
}

TextureProvider* TextureProvider::m_theInstance = nullptr;

TextureProvider& TextureProvider::instance()
{
    if (m_theInstance == nullptr) {
        m_theInstance = new TextureProvider;
    }

    return *m_theInstance;
}

QQuickTextureFactory* TextureProvider::requestTexture(const QString& id, QSize* size, const QSize& requestedSize)
{
    qDebug() << Q_FUNC_INFO;
    return pData->requestTexture(id, size, requestedSize);
}

QImage TextureProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    return pData->requestImage(id, size, requestedSize);
}

QPixmap TextureProvider::requestPixmap(const QString& /* id */, QSize* /* size */, const QSize& /* requestedSize */)
{
    return QPixmap();
}

ImageBase::~ImageBase()
{
}

Cache::~Cache()
{
}

// polyspace-end MISRA-CPP:2-10-1 [Justified:Low] "[Maintainability] Applied rule exception - Different identifiers shall be typographically unambiguous"
// polyspace-end MISRA-CPP:2-10-2 [Justified:Low] "[Maintainability] Applied rule exception - Identifiers declared in an inner scope shall not hide an identifier declared in an outer scope."
// polyspace-end MISRA-CPP:5-2-1 [Justified:Low] "[Maintainability] Applied rule exception - Each operand of a logical  or || shall be a postfix expression."
// polyspace-end MISRA-CPP:6-6-5 [Justified:Low] "[Maintainability] Applied rule exception - A function shall have a single point of exit at the end of the function."
// polyspace-end MISRA-CPP:16-2-1 [Justified:Low] "[Maintainability] Applied rule exception - The pre-processor shall only be used for file inclusion and include guards."
// polyspace-end MISRA-CPP:18-4-1 [Justified:Low] "[Maintainability] Applied rule exception - Dynamic heap memory allocation shall not be used."
