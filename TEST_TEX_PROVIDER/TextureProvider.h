#pragma once

#include <QtQuick/QQuickImageProvider>

class TextureProviderPrivate;
class QQuickTextureFactory;

class TextureProvider : public QQuickImageProvider
{
public:
    virtual QQuickTextureFactory* requestTexture(const QString& id, QSize* size, const QSize& requestedSize) Q_DECL_OVERRIDE;
    virtual QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) Q_DECL_OVERRIDE;
    virtual QPixmap requestPixmap(const QString& /* id */, QSize* /* size */, const QSize& /* requestedSize */) Q_DECL_OVERRIDE;

public:
    static bool CreateInstance(const QString& path, int imgCacheSize, int textureCacheSize);
    static TextureProvider& instance();

private:
    explicit TextureProvider();
    virtual ~TextureProvider() override;
    TextureProviderPrivate* pData;
    TextureProvider(const TextureProvider& rhs);
    TextureProvider& operator=(const TextureProvider& rhs);

private:
    static TextureProvider* m_theInstance;
};
