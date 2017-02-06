#pragma once
#ifndef GG_RENDERING_H
#define GG_RENDERING_H

#include "Table.h"
#include "ResourcePool.h"
#include "RenderTypes.h"

namespace gg {

class Rendering {

public:

    class Hub;

    typedef void* WindowHandle;

    struct BlueprintId { unsigned value; };
    struct FramebufferId { unsigned value; };
    struct PipelineId { unsigned value; };
    struct ImageId { unsigned value; };
    struct TilesetId { unsigned value; };
    struct SpriteId { unsigned value; };

    struct Blueprint;
    struct Framebuffer;
    struct Pipeline;
    struct Image;
    struct Tileset;
    struct Sprite;

    void addSprite(float x, float y, Sprite const& sprite);
    void addImage(float x, float y, Image const& image);

private:

    explicit Rendering(Hub* hub, PipelineId pipelineId)
        : hub_(hub)
        , pipelineId_(pipelineId) {
    }

    template<class T_Id, void (Hub::*T_DestroyFunc)(T_Id)> class IdOwner;

    struct SpritePrim {
        float x;
        float y;
        SpriteId spriteId;
    };

    struct ImagePrim {
        float x;
        float y;
        ImageId imageId;
    };

    Rendering& reset() {
        spritePrims_.removeAll();
        imagePrims_.removeAll();
        antecedents_.removeAll();
        return *this;
    }

    Hub* const hub_;
    PipelineId pipelineId_;
    Array<SpritePrim> spritePrims_;
    Array<ImagePrim> imagePrims_;
    Array<Rendering*> antecedents_;
};


class Rendering::Hub {

public:
    Hub(char const** errorMsg);
    ~Hub();

    void submitQueuedUploads();

    Rendering startRendering(PipelineId pipelineId);
    void submitRendering(Rendering&& rendering);

    PipelineId createPipeline(WindowHandle displayWindow);
    ImageId createImage(Span<uint8_t> const& data, RenderFormat const& format, unsigned width, unsigned height);
    TilesetId createTileset(Span<uint8_t> const& data, RenderFormat const& format, unsigned width, unsigned height, unsigned tileWidth, unsigned tileHeight);

    void destroyPipeline(PipelineId id);
    void destroyImage(ImageId id);
    void destroyTileset(TilesetId id);

private:
    struct Platform;
    struct PresentationSurface;
    struct PipelineResource;
    struct ImageResource;
    struct TilesetResource;

    PresentationSurface* maintainPresentationSurface(WindowHandle displayWindow) const;

    std::unique_ptr<Platform> platform_;
    Table<WindowHandle, PresentationSurface> presentationSurfaces_;
    ResourcePool<PipelineId, PipelineResource> pipelineResources_;
    ResourcePool<ImageId, ImageResource> imageResources_;
    ResourcePool<TilesetId, TilesetResource> tilesetResources_;

    Array<Rendering> renderingFreeList_;
};

template<class T_Id, void (Rendering::Hub::*T_DestroyFunc)(T_Id)>
class Rendering::IdOwner {
public:
    operator T_Id() const {
        return id_;
    }
protected:
    IdOwner(Rendering::Hub* hub, T_Id&& id)
        : hub_(hub)
        , id_(id) {
    }
    IdOwner(IdOwner&& source)
        : hub_(std::exchange(source.hub_, nullptr))
        , id_(std::exchange(source.id_, {})) {
    }
    IdOwner(IdOwner const&) = delete;
    GG_NO_INLINE ~IdOwner() {
        if (hub_) {
            (hub_->*T_DestroyFunc)(id_);
        }
    }

    Rendering::Hub* hub_;
    T_Id id_;
};

struct Rendering::Pipeline : Rendering::IdOwner<PipelineId, &Hub::destroyPipeline> {
    Pipeline(Hub* hub, void* displayWindow)
        : IdOwner(hub, hub->createPipeline(displayWindow)) {
    }
};

struct Rendering::Image : Rendering::IdOwner<ImageId, &Hub::destroyImage> {
    Image(Hub* hub, Span<uint8_t> const& data, RenderFormat const& format, unsigned width, unsigned height)
        : IdOwner(hub, hub->createImage(data, format, width, height)) {
    }
};

struct Rendering::Tileset : Rendering::IdOwner<TilesetId, &Hub::destroyTileset> {
    Tileset(Hub* hub, Span<uint8_t> const& data, RenderFormat const& format, unsigned width, unsigned height, unsigned tileWidth, unsigned tileHeight)
        : IdOwner(hub, hub->createTileset(data, format, width, height, tileWidth, tileHeight)) {
    }
};

}

#endif
