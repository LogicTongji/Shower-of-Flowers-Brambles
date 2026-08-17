#include "gui_host_d3d9.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gdiplus.h>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gui_application_bus.h"
#include "gui_diagnostics.h"
#include "gui_hoi3_lifecycle.h"
#include "gui_indexed_map_d3d9.h"
#include "gui_inprocess_application.h"
#include "gui_localization.h"
#include "gui_lua_bridge.h"
#include "gui_lua_native_binding.h"
#include "gui_marker_layer_d3d9.h"
#include "gui_render_queue.h"
#include "gui_text_renderer_d3d9.h"
#include "gui_texture_loader_d3d9.h"
#include "gui_window_session.h"
#include "gui_window_manager.h"

namespace
{

struct OverlayVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rhw = 1.0f;
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255);
    float u = 0.0f;
    float v = 0.0f;
};

constexpr DWORD OverlayVertexFormat =
    D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr uint64_t Hoi3LifecycleProbeIntervalMilliseconds = 100;

D3DCOLOR ToD3DColor(
    float red,
    float green,
    float blue,
    float alpha = 1.0f
)
{
    const auto channel = [](float value)
    {
        return static_cast<uint8_t>(std::lround(
            std::clamp(value, 0.0f, 1.0f) * 255.0f
        ));
    };
    return D3DCOLOR_ARGB(
        channel(alpha),
        channel(red),
        channel(green),
        channel(blue)
    );
}

bool PointInside(const gui::GuiRect& rect, int x, int y)
{
    return x >= rect.x
        && y >= rect.y
        && x < rect.x + rect.width
        && y < rect.y + rect.height;
}

bool IntersectRects(
    const gui::GuiRect& first,
    const gui::GuiRect& second,
    gui::GuiRect& output
)
{
    const int left = std::max(first.x, second.x);
    const int top = std::max(first.y, second.y);
    const int right = std::min(
        first.x + first.width,
        second.x + second.width
    );
    const int bottom = std::min(
        first.y + first.height,
        second.y + second.height
    );
    output = {
        left,
        top,
        std::max(0, right - left),
        std::max(0, bottom - top)
    };
    return output.width > 0 && output.height > 0;
}

void IncludeRect(
    gui::GuiRect& bounds,
    bool& hasBounds,
    const gui::GuiRect& rect
)
{
    if (rect.width <= 0 || rect.height <= 0)
    {
        return;
    }
    if (!hasBounds)
    {
        bounds = rect;
        hasBounds = true;
        return;
    }
    const int right = std::max(
        bounds.x + bounds.width,
        rect.x + rect.width
    );
    const int bottom = std::max(
        bounds.y + bounds.height,
        rect.y + rect.height
    );
    bounds.x = std::min(bounds.x, rect.x);
    bounds.y = std::min(bounds.y, rect.y);
    bounds.width = right - bounds.x;
    bounds.height = bottom - bounds.y;
}

class GuiD3D9TextureCache
{
public:
    GuiD3D9Texture* ResolveSprite(
        IDirect3DDevice9* device,
        const std::filesystem::path& root,
        const gui::GuiInterpreter& interpreter,
        std::string_view spriteName
    )
    {
        if (spriteName.empty())
        {
            return nullptr;
        }
        const std::string key(spriteName);
        if (failedTextures_.find(key) != failedTextures_.end())
        {
            return nullptr;
        }
        const auto existing = textures_.find(key);
        if (existing != textures_.end())
        {
            return &existing->second;
        }

        const std::filesystem::path path = interpreter.ResolveTexture(
            key,
            root
        );
        if (path.empty())
        {
            return nullptr;
        }
        GuiD3D9Texture texture;
        std::string error;
        if (!LoadGuiD3D9Texture(device, path, texture, error))
        {
            failedTextures_.insert(key);
            WriteGuiDiagnostic(
                "GUI texture load failed: sprite=" + key
                + ", error=" + error
            );
            return nullptr;
        }
        const auto inserted = textures_.emplace(
            key,
            std::move(texture)
        );
        return &inserted.first->second;
    }

    void Clear()
    {
        textures_.clear();
        failedTextures_.clear();
    }

private:
    std::unordered_map<std::string, GuiD3D9Texture> textures_;
    std::unordered_set<std::string> failedTextures_;
};

void ConfigureOverlayState(
    IDirect3DDevice9* device,
    bool premultipliedSource = false
)
{
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(OverlayVertexFormat);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(
        D3DRS_SRCBLEND,
        premultipliedSource ? D3DBLEND_ONE : D3DBLEND_SRCALPHA
    );
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetTextureStageState(
        0,
        D3DTSS_COLOROP,
        D3DTOP_MODULATE
    );
    device->SetTextureStageState(
        0,
        D3DTSS_COLORARG1,
        D3DTA_TEXTURE
    );
    device->SetTextureStageState(
        0,
        D3DTSS_COLORARG2,
        D3DTA_DIFFUSE
    );
    device->SetTextureStageState(
        0,
        D3DTSS_ALPHAOP,
        D3DTOP_MODULATE
    );
    device->SetTextureStageState(
        0,
        D3DTSS_ALPHAARG1,
        D3DTA_TEXTURE
    );
    device->SetTextureStageState(
        0,
        D3DTSS_ALPHAARG2,
        D3DTA_DIFFUSE
    );
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

void DrawQuadRegion(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color,
    float u0,
    float v0,
    float u1,
    float v1
)
{
    if (!device || rect.width <= 0 || rect.height <= 0)
    {
        return;
    }
    const float left = static_cast<float>(rect.x) - 0.5f;
    const float top = static_cast<float>(rect.y) - 0.5f;
    const float right = static_cast<float>(rect.x + rect.width) - 0.5f;
    const float bottom = static_cast<float>(rect.y + rect.height) - 0.5f;
    const OverlayVertex vertices[4] = {
        {left, top, 0.0f, 1.0f, color, u0, v0},
        {right, top, 0.0f, 1.0f, color, u1, v0},
        {left, bottom, 0.0f, 1.0f, color, u0, v1},
        {right, bottom, 0.0f, 1.0f, color, u1, v1}
    };
    device->SetTexture(0, texture);
    if (!texture)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    }
    device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP,
        2,
        vertices,
        sizeof(OverlayVertex)
    );
    if (!texture)
    {
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    }
}

void DrawQuad(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255)
)
{
    DrawQuadRegion(
        device,
        rect,
        texture,
        color,
        0.0f,
        0.0f,
        1.0f,
        1.0f
    );
}

void DrawTextureQuad(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255)
)
{
    if (texture)
    {
        DrawQuad(device, rect, texture, color);
    }
}

void DrawTextureRegion(
    IDirect3DDevice9* device,
    const gui::GuiRect& destination,
    IDirect3DTexture9* texture,
    const gui::GuiRect& source,
    int textureWidth,
    int textureHeight
)
{
    if (!texture
        || source.width <= 0
        || source.height <= 0
        || textureWidth <= 0
        || textureHeight <= 0)
    {
        return;
    }
    DrawQuadRegion(
        device,
        destination,
        texture,
        D3DCOLOR_ARGB(255, 255, 255, 255),
        static_cast<float>(source.x) / textureWidth,
        static_cast<float>(source.y) / textureHeight,
        static_cast<float>(source.x + source.width) / textureWidth,
        static_cast<float>(source.y + source.height) / textureHeight
    );
}

gui::GuiRect CalculateScrollbarThumb(
    const GuiListRuntimeLayout& layout
)
{
    if (layout.scrollbar.height <= 0
        || layout.contentHeight <= layout.viewport.height)
    {
        return {};
    }
    const int thumbHeight = std::max(
        18,
        layout.scrollbar.height * layout.viewport.height
            / std::max(1, layout.contentHeight)
    );
    const int travel = std::max(
        0,
        layout.scrollbar.height - thumbHeight
    );
    const int y = layout.scrollbar.y
        + travel * layout.scrollOffset
            / std::max(1, layout.maximumScroll);
    return {
        layout.scrollbar.x,
        y,
        layout.scrollbar.width,
        thumbHeight
    };
}

bool IsInteractiveTarget(const gui::GuiResolvedWidget* widget)
{
    if (!widget || !widget->definition || !widget->enabled)
    {
        return false;
    }
    const gui::WidgetDefinition& definition = *widget->definition;
    const gui::GuiActionBinding& actions = definition.actions;
    return definition.draggable
        || definition.type == gui::WidgetType::Button
        || definition.type == gui::WidgetType::IndexedMap
        || definition.type == gui::WidgetType::MarkerLayer
        || definition.type == gui::WidgetType::Custom
        || !actions.onClick.empty()
        || !actions.onPress.empty()
        || !actions.onRelease.empty();
}

}

struct GuiD3D9Host::Impl
{
    struct SessionView
    {
        std::unique_ptr<GuiWindowSessionController> controller;
        GuiIndexedMapD3D9Runtime indexedMaps;
        GuiMarkerLayerD3D9Runtime markerLayers;
        gui::GuiRect sourceRect;
        gui::GuiRect presentationRect;
        int windowOffsetX = 0;
        int windowOffsetY = 0;
        int initialWindowOffsetX = 0;
        int initialWindowOffsetY = 0;
        bool draggingWindow = false;
        int dragMouseX = 0;
        int dragMouseY = 0;
        int dragOffsetX = 0;
        int dragOffsetY = 0;
        bool expanded = false;
        bool visibilityObserved = false;
        bool lastVisible = false;
        bool firstDrawLogged = false;
        bool effectiveVisible = false;
        bool awaitingGameplaySnapshot = false;
        uint64_t lifecycleGeneration = 0;
        std::string persistenceSignature;

        std::vector<gui::GuiResolvedWidget> InteractiveWidgets() const
        {
            return controller->ResolveInteractiveWidgets();
        }
    };

    GuiInProcessApplication application;
    GuiApplicationActionBus actionBus;
    GuiLocalizationRegistry localization;
    GuiD3D9TextureCache textures;
    GuiTextRendererD3D9 textRenderer;
    GuiWindowManager windowManager;
    std::vector<std::unique_ptr<SessionView>> sessions;
    IDirect3DDevice9* device = nullptr;
    HWND targetWindow = nullptr;
    IDirect3DTexture9* canvasTexture = nullptr;
    IDirect3DSurface9* canvasSurface = nullptr;
    int canvasWidth = 0;
    int canvasHeight = 0;
    ULONG_PTR gdiplusToken = 0;
    bool initialized = false;
    uint64_t nextHoi3LifecycleProbeMilliseconds = 0;
    bool hoi3LifecycleUnsupportedLogged = false;

    void RefreshHoi3Lifecycle(uint64_t now)
    {
        if (now < nextHoi3LifecycleProbeMilliseconds)
        {
            return;
        }
        nextHoi3LifecycleProbeMilliseconds = now
            + Hoi3LifecycleProbeIntervalMilliseconds;

        const GuiHoi3LifecycleProbeResult probe =
            ProbeGuiHoi3Lifecycle();
        if (probe.status
            == GuiHoi3LifecycleProbeStatus::UnsupportedExecutable)
        {
            if (!hoi3LifecycleUnsupportedLogged)
            {
                hoi3LifecycleUnsupportedLogged = true;
                WriteGuiDiagnostic(
                    "HOI3 read-only lifecycle probe unavailable: "
                    "unsupported executable"
                );
            }
            return;
        }
        if (probe.status == GuiHoi3LifecycleProbeStatus::Unavailable)
        {
            return;
        }

        GuiLuaBridgeService& service = GetGuiLuaBridgeService();
        const GuiGameplayLifecycleSnapshot previous =
            service.GameplayLifecycle();
        if (probe.status == GuiHoi3LifecycleProbeStatus::Frontend
            && previous.state != GuiGameplayLifecycleState::Frontend)
        {
            GetGuiLuaNativeBinding().ResetChannelOwnership();
        }
        if (!service.ReportGameplayPlayerTag(probe.playerTag))
        {
            return;
        }
        const GuiGameplayLifecycleSnapshot lifecycle =
            service.GameplayLifecycle();
        WriteGuiDiagnostic(
            "HOI3 lifecycle changed by read-only player tag: player="
            + lifecycle.playerTag
            + ", state="
            + (lifecycle.state == GuiGameplayLifecycleState::Gameplay
                ? "gameplay" : "frontend")
            + ", generation="
            + std::to_string(lifecycle.generation)
        );
    }

    SessionView* FindSession(std::string_view id) const
    {
        const auto found = std::find_if(
            sessions.begin(),
            sessions.end(),
            [id](const std::unique_ptr<SessionView>& session)
            {
                return session->controller->PluginId() == id;
            }
        );
        return found == sessions.end() ? nullptr : found->get();
    }

    std::vector<SessionView*> OrderedSessions(bool forInput) const
    {
        const std::vector<std::string> order = forInput
            ? windowManager.InputOrder()
            : windowManager.RenderOrder();
        std::vector<SessionView*> result;
        result.reserve(order.size());
        for (const std::string& id : order)
        {
            if (SessionView* session = FindSession(id))
            {
                result.push_back(session);
            }
        }
        return result;
    }

    void ReleaseCanvas()
    {
        if (canvasSurface)
        {
            canvasSurface->Release();
            canvasSurface = nullptr;
        }
        if (canvasTexture)
        {
            canvasTexture->Release();
            canvasTexture = nullptr;
        }
    }

    bool CreateCanvas(std::string& error)
    {
        ReleaseCanvas();
        if (!device || canvasWidth <= 0 || canvasHeight <= 0)
        {
            error = "D3D9 GUI canvas dimensions are invalid";
            return false;
        }
        const HRESULT textureResult = device->CreateTexture(
            static_cast<UINT>(canvasWidth),
            static_cast<UINT>(canvasHeight),
            1,
            D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &canvasTexture,
            nullptr
        );
        if (FAILED(textureResult) || !canvasTexture)
        {
            error = "Failed to create D3D9 GUI render canvas";
            ReleaseCanvas();
            return false;
        }
        if (FAILED(canvasTexture->GetSurfaceLevel(0, &canvasSurface))
            || !canvasSurface)
        {
            error = "Failed to resolve D3D9 GUI canvas surface";
            ReleaseCanvas();
            return false;
        }
        error.clear();
        return true;
    }

    double CanvasScale(const D3DVIEWPORT9& viewport) const
    {
        if (canvasWidth <= 0
            || canvasHeight <= 0
            || viewport.Width == 0
            || viewport.Height == 0)
        {
            return 0.0;
        }
        const double maximumWidth = viewport.Width * 0.92;
        const double maximumHeight = viewport.Height * 0.90;
        return std::min({
            1.0,
            maximumWidth / canvasWidth,
            maximumHeight / canvasHeight
        });
    }

    gui::GuiRect CalculateSourceRect(SessionView& view)
    {
        const std::vector<gui::GuiResolvedWidget> widgets =
            view.controller->ResolveSceneWidgets();
        const std::vector<GuiRenderCommand> queue =
            BuildGuiRenderQueue(
                widgets,
                view.controller->ListTemplateNames()
            );
        gui::GuiRect bounds;
        bool hasBounds = false;
        view.expanded = false;
        for (const gui::GuiResolvedWidget& widget : widgets)
        {
            if (widget.visible
                && widget.definition
                && widget.definition->type == gui::WidgetType::Window
                && widget.definition->moveable)
            {
                view.expanded = true;
                break;
            }
        }
        for (const GuiRenderCommand& command : queue)
        {
            if (!command.widget
                || command.type == GuiRenderCommandType::Custom)
            {
                continue;
            }
            gui::GuiRect visibleRect = command.widget->rect;
            if (command.widget->hasClipRect
                && !IntersectRects(
                    visibleRect,
                    command.widget->clipRect,
                    visibleRect
                ))
            {
                continue;
            }
            IncludeRect(bounds, hasBounds, visibleRect);
        }
        if (!hasBounds)
        {
            return {};
        }
        const int left = std::clamp(bounds.x, 0, canvasWidth);
        const int top = std::clamp(bounds.y, 0, canvasHeight);
        const int right = std::clamp(
            bounds.x + bounds.width,
            0,
            canvasWidth
        );
        const int bottom = std::clamp(
            bounds.y + bounds.height,
            0,
            canvasHeight
        );
        return {
            left,
            top,
            std::max(0, right - left),
            std::max(0, bottom - top)
        };
    }

    void UpdatePresentationRect(
        SessionView& view,
        const D3DVIEWPORT9& viewport
    )
    {
        view.sourceRect = CalculateSourceRect(view);
        const double scale = CanvasScale(viewport);
        if (view.sourceRect.width <= 0
            || view.sourceRect.height <= 0
            || scale <= 0.0)
        {
            view.presentationRect = {};
            return;
        }
        const int rootWidth = static_cast<int>(std::lround(
            canvasWidth * scale
        ));
        const int rootHeight = static_cast<int>(std::lround(
            canvasHeight * scale
        ));
        const int rootX = static_cast<int>(viewport.X)
            + (static_cast<int>(viewport.Width) - rootWidth) / 2;
        const int rootY = static_cast<int>(viewport.Y)
            + (static_cast<int>(viewport.Height) - rootHeight) / 2;
        view.presentationRect = {
            rootX + static_cast<int>(std::lround(
                view.sourceRect.x * scale
            )) + (view.expanded ? view.windowOffsetX : 0),
            rootY + static_cast<int>(std::lround(
                view.sourceRect.y * scale
            )) + (view.expanded ? view.windowOffsetY : 0),
            std::max(1, static_cast<int>(std::lround(
                view.sourceRect.width * scale
            ))),
            std::max(1, static_cast<int>(std::lround(
                view.sourceRect.height * scale
            )))
        };
    }

    bool MapWindowPoint(
        const SessionView& view,
        int windowX,
        int windowY,
        int& canvasX,
        int& canvasY
    ) const
    {
        if (view.presentationRect.width <= 0
            || view.presentationRect.height <= 0
            || view.sourceRect.width <= 0
            || view.sourceRect.height <= 0
            || !PointInside(view.presentationRect, windowX, windowY))
        {
            return false;
        }
        canvasX = view.sourceRect.x
            + static_cast<int>(std::floor(
                static_cast<double>(windowX - view.presentationRect.x)
                    * view.sourceRect.width
                    / view.presentationRect.width
            ));
        canvasY = view.sourceRect.y
            + static_cast<int>(std::floor(
                static_cast<double>(windowY - view.presentationRect.y)
                    * view.sourceRect.height
                    / view.presentationRect.height
            ));
        return true;
    }

    bool Initialize(
        const std::filesystem::path& root,
        IDirect3DDevice9* nextDevice,
        std::string& error
    )
    {
        if (initialized)
        {
            return device == nextDevice;
        }
        if (!nextDevice)
        {
            error = "D3D9 device is missing";
            return false;
        }

        Gdiplus::GdiplusStartupInput gdiplusInput;
        if (Gdiplus::GdiplusStartup(
                &gdiplusToken,
                &gdiplusInput,
                nullptr
            ) != Gdiplus::Ok)
        {
            error = "Failed to initialize GDI+";
            return false;
        }

        device = nextDevice;
        D3DDEVICE_CREATION_PARAMETERS parameters{};
        if (SUCCEEDED(device->GetCreationParameters(&parameters)))
        {
            targetWindow = parameters.hFocusWindow;
        }
        if (!application.Initialize(root, error))
        {
            Shutdown();
            return false;
        }
        for (const GuiConfigurationIssue& issue : application.Issues())
        {
            WriteGuiDiagnostic(
                "GUI configuration issue: plugin="
                + (issue.pluginId.empty()
                    ? std::string("<global>")
                    : issue.pluginId)
                + ", stage=" + issue.stage
                + ", error=" + issue.message
            );
        }

        std::string localizationError;
        localization.LoadDirectory(
            application.Root() / "localisation",
            localizationError
        );
        if (!textRenderer.Initialize(
                application.Root() / "font",
                device,
                error
            ))
        {
            Shutdown();
            return false;
        }

        int cascade = 0;
        for (const GuiPluginLaunch& launch : application.Launches())
        {
            auto view = std::make_unique<SessionView>();
            view->windowOffsetX = cascade * 24;
            view->windowOffsetY = cascade * 24;
            view->initialWindowOffsetX = view->windowOffsetX;
            view->initialWindowOffsetY = view->windowOffsetY;
            view->controller =
                std::make_unique<GuiWindowSessionController>(
                    application.Root(),
                    launch,
                    application.Interpreter(),
                    application.Behaviors()
                );
            SessionView* viewPointer = view.get();
            view->markerLayers.Initialize(
                device,
                textRenderer,
                localization,
                [this](std::string_view name)
                {
                    GuiD3D9Texture* texture = ResolveSprite(name);
                    return texture ? texture->texture : nullptr;
                }
            );
            view->controller->SetLocalizationResolver(
                [this](std::string_view key)
                {
                    return localization.Resolve(key);
                }
            );
            view->controller->SetPersistenceStore(
                application.PersistenceStore()
            );
            view->controller->SetApplicationActionInvoker(
                [this](
                    std::string_view sourcePluginId,
                    const GuiActionContext& context
                )
                {
                    return actionBus.Dispatch(
                        sourcePluginId,
                        context
                    );
                }
            );
            view->controller->SetDataChangedCallback(
                [viewPointer]()
                {
                    viewPointer->markerLayers.SetData(
                        viewPointer->controller->DataRegistry()
                    );
                    viewPointer->indexedMaps.Refresh(
                        viewPointer->controller->LayoutContext()
                    );
                }
            );
            view->controller->SetSessionChangedCallback(
                [viewPointer](std::string_view, std::string_view)
                {
                    viewPointer->windowOffsetX =
                        viewPointer->initialWindowOffsetX;
                    viewPointer->windowOffsetY =
                        viewPointer->initialWindowOffsetY;
                    viewPointer->draggingWindow = false;
                    viewPointer->dragMouseX = 0;
                    viewPointer->dragMouseY = 0;
                    viewPointer->dragOffsetX = 0;
                    viewPointer->dragOffsetY = 0;
                }
            );
            view->controller->SetEventResolver(
                [viewPointer](std::vector<GuiActionEvent>& events)
                {
                    viewPointer->indexedMaps.AttachItemIds(events);
                }
            );
            std::string sessionError;
            if (!view->controller->Bind(sessionError))
            {
                WriteGuiDiagnostic(
                    "GUI plugin disabled during bind: id="
                    + launch.id + ", error=" + sessionError
                );
                continue;
            }
            const gui::WindowDefinition* definition =
                view->controller->Runtime().Definition();
            if (!definition
                || !view->indexedMaps.Initialize(
                    application.Root(),
                    device,
                    application.Interpreter(),
                    *definition,
                    sessionError
                )
                || !view->controller->Initialize(
                    device,
                    sessionError
                ))
            {
                view->controller->Shutdown();
                view->indexedMaps.Shutdown();
                view->markerLayers.Shutdown();
                WriteGuiDiagnostic(
                    "GUI plugin disabled during initialization: id="
                    + launch.id + ", error=" + sessionError
                );
                continue;
            }
            canvasWidth = std::max(
                canvasWidth,
                definition->rect.x + definition->rect.width
            );
            canvasHeight = std::max(
                canvasHeight,
                definition->rect.y + definition->rect.height
            );
            view->markerLayers.SetData(
                view->controller->DataRegistry()
            );
            view->indexedMaps.Refresh(
                view->controller->LayoutContext()
            );
            if (!windowManager.Register({
                    launch.id,
                    launch.windowZOrder,
                    launch.modal
                }))
            {
                view->controller->Shutdown();
                view->indexedMaps.Shutdown();
                view->markerLayers.Shutdown();
                WriteGuiDiagnostic(
                    "GUI plugin disabled during window registration: id="
                    + launch.id
                );
                continue;
            }
            windowManager.SetState(
                launch.id,
                view->controller->IsOpen(),
                GetGuiLuaBridgeService().GameplayLifecycle().state
                        != GuiGameplayLifecycleState::Frontend
                    && view->controller->IsVisible()
            );
            sessions.push_back(std::move(view));
            ++cascade;
        }

        if (sessions.empty())
        {
            error = "No valid GUI sessions could be initialized";
            Shutdown();
            return false;
        }

        if (!CreateCanvas(error))
        {
            Shutdown();
            return false;
        }

        RebuildActionBus();
        initialized = true;
        return true;
    }

    void RebuildActionBus()
    {
        std::vector<IGuiApplicationEndpoint*> endpoints;
        endpoints.reserve(sessions.size());
        for (const auto& session : sessions)
        {
            endpoints.push_back(session->controller.get());
        }
        actionBus.SetEndpoints(std::move(endpoints));
    }

    void Shutdown()
    {
        actionBus.SetEndpoints({});
        for (const auto& session : sessions)
        {
            session->controller->Shutdown();
            session->markerLayers.Shutdown();
            session->indexedMaps.Shutdown();
        }
        sessions.clear();
        windowManager.Clear();
        textRenderer.Shutdown();
        textures.Clear();
        application.Shutdown();
        ReleaseCanvas();
        canvasWidth = 0;
        canvasHeight = 0;
        device = nullptr;
        targetWindow = nullptr;
        initialized = false;
        nextHoi3LifecycleProbeMilliseconds = 0;
        hoi3LifecycleUnsupportedLogged = false;
        if (gdiplusToken != 0)
        {
            Gdiplus::GdiplusShutdown(gdiplusToken);
            gdiplusToken = 0;
        }
    }

    GuiD3D9Texture* ResolveSprite(std::string_view name)
    {
        return textures.ResolveSprite(
            device,
            application.Root(),
            application.Interpreter(),
            name
        );
    }

    void DrawSprite(
        std::string_view name,
        gui::GuiRect rect,
        D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255)
    )
    {
        GuiD3D9Texture* texture = ResolveSprite(name);
        if (!texture)
        {
            return;
        }
        if (rect.width <= 0)
        {
            rect.width = texture->width;
        }
        if (rect.height <= 0)
        {
            rect.height = texture->height;
        }
        DrawQuad(device, rect, texture->texture, color);
    }

    bool ApplyWidgetClip(const gui::GuiResolvedWidget& widget)
    {
        if (!widget.hasClipRect)
        {
            device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            return true;
        }
        const int left = std::clamp(
            widget.clipRect.x,
            0,
            canvasWidth
        );
        const int top = std::clamp(
            widget.clipRect.y,
            0,
            canvasHeight
        );
        const int right = std::clamp(
            widget.clipRect.x + widget.clipRect.width,
            0,
            canvasWidth
        );
        const int bottom = std::clamp(
            widget.clipRect.y + widget.clipRect.height,
            0,
            canvasHeight
        );
        if (right <= left || bottom <= top)
        {
            return false;
        }
        const RECT rect{left, top, right, bottom};
        if (FAILED(device->SetScissorRect(&rect)))
        {
            return false;
        }
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        return true;
    }

    void DrawScrollbar(
        SessionView& view,
        std::string_view scrollbarName
    )
    {
        for (const std::string& listName
            : view.controller->ListNames())
        {
            gui::GuiListBinding binding;
            if (!view.controller->Runtime().ResolveListBinding(
                    listName,
                    binding,
                    view.controller->LayoutContext()
                )
                || binding.scrollbarName != scrollbarName)
            {
                continue;
            }
            const GuiListRuntimeLayout layout =
                view.controller->BuildListRuntimeLayout(listName);
            if (layout.maximumScroll > 0)
            {
                DrawSprite(
                    layout.scrollbarTrackSprite,
                    layout.scrollbar
                );
                DrawSprite(
                    layout.scrollbarThumbSprite,
                    CalculateScrollbarThumb(layout)
                );
            }
            return;
        }
    }

    void DrawSession(SessionView& view)
    {
        std::vector<gui::GuiResolvedWidget> widgets =
            view.controller->ResolveSceneWidgets();
        const std::vector<GuiRenderCommand> queue =
            BuildGuiRenderQueue(
                widgets,
                view.controller->ListTemplateNames()
            );
        if (!view.firstDrawLogged)
        {
            WriteGuiDiagnostic(
                "First GUI session draw: id="
                + std::string(view.controller->PluginId())
                + ", widgets="
                + std::to_string(widgets.size())
                + ", commands="
                + std::to_string(queue.size())
            );
            view.firstDrawLogged = true;
        }
        for (const GuiRenderCommand& command : queue)
        {
            if (!command.widget || !command.widget->definition)
            {
                continue;
            }
            if (!ApplyWidgetClip(*command.widget))
            {
                continue;
            }
            const gui::WidgetDefinition& definition =
                *command.widget->definition;
            switch (command.type)
            {
            case GuiRenderCommandType::WindowFrame:
                DrawSprite(definition.frameSpriteName, command.widget->rect);
                break;
            case GuiRenderCommandType::Image:
                DrawSprite(
                    view.controller->ResolveWidgetSprite(
                        *command.widget
                    ),
                    command.widget->rect
                );
                break;
            case GuiRenderCommandType::Button:
            {
                const bool pressed = view.controller->IsWidgetPressed(
                    *command.widget
                );
                DrawSprite(
                    view.controller->ResolveWidgetSprite(
                        *command.widget,
                        pressed
                    ),
                    command.widget->rect,
                    command.widget->enabled
                        ? D3DCOLOR_ARGB(255, 255, 255, 255)
                        : D3DCOLOR_ARGB(150, 150, 150, 150)
                );
                break;
            }
            case GuiRenderCommandType::ColorBox:
                DrawQuad(
                    device,
                    command.widget->rect,
                    nullptr,
                    ToD3DColor(
                        definition.textColor[0],
                        definition.textColor[1],
                        definition.textColor[2]
                    )
                );
                break;
            case GuiRenderCommandType::ProgressBar:
            {
                const gui::ProgressBarResource* resource =
                    application.Interpreter().FindProgressBar(
                        definition.progressResourceName
                    );
                if (!resource)
                {
                    break;
                }
                const float value = std::clamp(
                    definition.valueSource.empty()
                        ? definition.value
                        : static_cast<float>(
                            view.controller->LayoutContext().valueResolver
                            ? view.controller->LayoutContext().valueResolver(
                                definition.valueSource
                            )
                            : 0.0
                        ),
                    0.0f,
                    1.0f
                );
                gui::GuiRect fill = command.widget->rect;
                if (resource->horizontal)
                {
                    fill.width = static_cast<int>(fill.width * value);
                    if (definition.fillFromEnd)
                    {
                        fill.x = command.widget->rect.x
                            + command.widget->rect.width - fill.width;
                    }
                }
                else
                {
                    fill.height = static_cast<int>(fill.height * value);
                    if (definition.fillFromEnd)
                    {
                        fill.y = command.widget->rect.y
                            + command.widget->rect.height - fill.height;
                    }
                }
                const float* color = definition.progressColorIndex == 1
                    ? resource->secondColor
                    : resource->color;
                DrawQuad(
                    device,
                    fill,
                    nullptr,
                    ToD3DColor(color[0], color[1], color[2])
                );
                break;
            }
            case GuiRenderCommandType::ScrollBar:
                DrawScrollbar(view, definition.name);
                break;
            case GuiRenderCommandType::IndexedMap:
            {
                GuiIndexedMapD3D9DrawLayers layers;
                if (view.indexedMaps.ResolveDrawLayers(
                        *command.widget,
                        layers
                    ))
                {
                    DrawTextureQuad(device, layers.rect, layers.base);
                    DrawTextureQuad(device, layers.rect, layers.overlay);
                    DrawTextureQuad(device, layers.rect, layers.boundary);
                    DrawTextureQuad(device, layers.rect, layers.hover);
                }
                break;
            }
            case GuiRenderCommandType::Text:
            {
                gui::GuiTextCommand text;
                if (!view.controller->ResolveWidgetText(
                        *command.widget,
                        text
                    ))
                {
                    break;
                }
                const std::string slot = std::string(
                    view.controller->PluginId()
                ) + ":text:" + std::to_string(
                    reinterpret_cast<std::uintptr_t>(&definition)
                ) + ":" + command.widget->listName
                    + ":" + std::to_string(
                        command.widget->listIndex
                    );
                DrawTextureQuad(
                    device,
                    text.rect,
                    textRenderer.Resolve(slot, text)
                );
                break;
            }
            case GuiRenderCommandType::MarkerLayer:
                view.markerLayers.DrawWidget(
                    *command.widget,
                    widgets,
                    view.indexedMaps
                );
                break;
            case GuiRenderCommandType::Custom:
                break;
            }
        }
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    }

    void TickAndRender(IDirect3DDevice9* nextDevice)
    {
        if (!initialized || !nextDevice || nextDevice != device)
        {
            return;
        }
        const uint64_t now = GetTickCount64();
        RefreshHoi3Lifecycle(now);
        const GuiGameplayLifecycleSnapshot lifecycle =
            GetGuiLuaBridgeService().GameplayLifecycle();
        for (const auto& session : sessions)
        {
            const bool dataChanged = lifecycle.state
                    == GuiGameplayLifecycleState::Frontend
                ? false : session->controller->Tick(now);
            if (session->lifecycleGeneration != lifecycle.generation)
            {
                session->lifecycleGeneration = lifecycle.generation;
                session->awaitingGameplaySnapshot =
                    lifecycle.state
                    == GuiGameplayLifecycleState::Gameplay;
                session->draggingWindow = false;
            }
            if (lifecycle.state == GuiGameplayLifecycleState::Gameplay
                && dataChanged)
            {
                session->awaitingGameplaySnapshot = false;
            }
            if (lifecycle.state == GuiGameplayLifecycleState::Unknown)
            {
                session->awaitingGameplaySnapshot = false;
            }
            const bool visible = session->controller->IsVisible()
                && lifecycle.state
                    != GuiGameplayLifecycleState::Frontend
                && !session->awaitingGameplaySnapshot;
            session->effectiveVisible = visible;
            windowManager.SetState(
                session->controller->PluginId(),
                session->controller->IsOpen(),
                visible
            );
            const std::shared_ptr<GuiDataRegistry>& data =
                session->controller->DataRegistry();
            const std::string persistenceSignature = data
                ? data->ResolveText("state.sessionid") + "|"
                    + data->ResolveText("state.persistencekey") + "|"
                    + data->ResolveText("state.persistencerevision") + "|"
                    + data->ResolveText("state.persistedrevision") + "|"
                    + data->ResolveText(
                        "state.persistencependingrevision"
                    ) + "|"
                    + data->ResolveText("state.persistencependingticks")
                    + "|"
                    + data->ResolveText("state.persistenceobservedday")
                    + "|" + data->ResolveText("state.leader1region")
                    + "|" + data->ResolveText("state.leader2region")
                : std::string();
            if (!session->visibilityObserved
                || session->lastVisible != visible
                || session->persistenceSignature
                    != persistenceSignature)
            {
                WriteGuiDiagnostic(
                    "GUI session visibility: id="
                    + std::string(session->controller->PluginId())
                    + ", open="
                    + (session->controller->IsOpen() ? "true" : "false")
                    + ", visible="
                    + (visible ? "true" : "false")
                    + ", state.visible="
                    + (data && data->ResolveBool("state.visible")
                        ? "true" : "false")
                    + ", state.active="
                    + (data && data->ResolveBool("state.active")
                        ? "true" : "false")
                    + ", viewer="
                    + (data
                        ? data->ResolveText("state.viewertag")
                        : std::string())
                    + ", persistence="
                    + (data
                        && data->ResolveBool("state.persistenceavailable")
                        ? "available" : "unavailable")
                    + ", persistence_key="
                    + (data
                        ? data->ResolveText("state.persistencekey")
                        : std::string())
                    + ", persistence_error="
                    + (data
                        ? data->ResolveText("state.persistenceerror")
                        : std::string())
                    + ", session="
                    + (data
                        ? data->ResolveText("state.sessionid")
                        : std::string())
                    + ", memory_revision="
                    + (data
                        ? data->ResolveText(
                            "state.persistencerevision"
                        ) : std::string())
                    + ", stored_revision="
                    + (data
                        ? data->ResolveText("state.persistedrevision")
                        : std::string())
                    + ", pending_revision="
                    + (data
                        ? data->ResolveText(
                            "state.persistencependingrevision"
                        ) : std::string())
                    + ", pending_ticks="
                    + (data
                        ? data->ResolveText(
                            "state.persistencependingticks"
                        ) : std::string())
                    + ", observed_day="
                    + (data
                        ? data->ResolveText(
                            "state.persistenceobservedday"
                        ) : std::string())
                    + ", leader1_region="
                    + (data
                        ? data->ResolveText("state.leader1region")
                        : std::string())
                    + ", leader2_region="
                    + (data
                        ? data->ResolveText("state.leader2region")
                        : std::string())
                    + ", lifecycle="
                    + (lifecycle.state
                            == GuiGameplayLifecycleState::Gameplay
                        ? "gameplay"
                        : lifecycle.state
                                == GuiGameplayLifecycleState::Frontend
                            ? "frontend" : "unknown")
                    + ", lifecycle_generation="
                    + std::to_string(lifecycle.generation)
                    + ", lifecycle_player="
                    + lifecycle.playerTag
                );
                session->visibilityObserved = true;
                session->lastVisible = visible;
                session->persistenceSignature =
                    persistenceSignature;
            }
        }

        IDirect3DStateBlock9* stateBlock = nullptr;
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock))
            || !stateBlock)
        {
            return;
        }
        if (FAILED(stateBlock->Capture())
            || !canvasTexture
            || !canvasSurface)
        {
            stateBlock->Release();
            return;
        }

        IDirect3DSurface9* previousRenderTarget = nullptr;
        IDirect3DSurface9* previousDepthStencil = nullptr;
        D3DVIEWPORT9 previousViewport{};
        const bool hasRenderTarget = SUCCEEDED(
            device->GetRenderTarget(0, &previousRenderTarget)
        ) && previousRenderTarget;
        device->GetDepthStencilSurface(&previousDepthStencil);
        const bool hasViewport = SUCCEEDED(
            device->GetViewport(&previousViewport)
        );
        if (!hasRenderTarget || !hasViewport)
        {
            if (previousDepthStencil)
            {
                previousDepthStencil->Release();
            }
            if (previousRenderTarget)
            {
                previousRenderTarget->Release();
            }
            stateBlock->Release();
            return;
        }

        const D3DVIEWPORT9 canvasViewport{
            0,
            0,
            static_cast<DWORD>(canvasWidth),
            static_cast<DWORD>(canvasHeight),
            0.0f,
            1.0f
        };
        textRenderer.BeginFrame();
        for (const auto& session : sessions)
        {
            if (!session->controller->IsOpen()
                || !session->effectiveVisible)
            {
                session->sourceRect = {};
                session->presentationRect = {};
            }
        }
        for (SessionView* session : OrderedSessions(false))
        {
            UpdatePresentationRect(*session, previousViewport);
            if (session->presentationRect.width <= 0
                || session->presentationRect.height <= 0)
            {
                continue;
            }

            device->SetDepthStencilSurface(nullptr);
            if (FAILED(device->SetRenderTarget(0, canvasSurface)))
            {
                device->SetRenderTarget(0, previousRenderTarget);
                device->SetDepthStencilSurface(previousDepthStencil);
                device->SetViewport(&previousViewport);
                continue;
            }
            device->SetViewport(&canvasViewport);
            device->Clear(
                0,
                nullptr,
                D3DCLEAR_TARGET,
                D3DCOLOR_ARGB(0, 0, 0, 0),
                1.0f,
                0
            );
            ConfigureOverlayState(device);
            DrawSession(*session);

            device->SetRenderTarget(0, previousRenderTarget);
            device->SetDepthStencilSurface(previousDepthStencil);
            device->SetViewport(&previousViewport);
            ConfigureOverlayState(device, true);
            DrawTextureRegion(
                device,
                session->presentationRect,
                canvasTexture,
                session->sourceRect,
                canvasWidth,
                canvasHeight
            );
        }
        textRenderer.EndFrame();

        if (previousDepthStencil)
        {
            previousDepthStencil->Release();
        }
        previousRenderTarget->Release();
        stateBlock->Apply();
        stateBlock->Release();
    }

    bool HandleWindowMessage(
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    )
    {
        if (!initialized)
        {
            return false;
        }
        if (GetGuiLuaBridgeService().GameplayLifecycle().state
            == GuiGameplayLifecycleState::Frontend)
        {
            return false;
        }
        int mouseX = GET_X_LPARAM(lParam);
        int mouseY = GET_Y_LPARAM(lParam);
        if (message == WM_MOUSEWHEEL)
        {
            POINT point{mouseX, mouseY};
            ScreenToClient(targetWindow, &point);
            mouseX = point.x;
            mouseY = point.y;
        }
        const int windowMouseX = mouseX;
        const int windowMouseY = mouseY;

        for (SessionView* session : OrderedSessions(true))
        {
            SessionView& view = *session;
            if (!view.controller->IsOpen()
                || !view.effectiveVisible)
            {
                continue;
            }
            if (message == WM_MOUSEMOVE && view.draggingWindow)
            {
                view.windowOffsetX = view.dragOffsetX
                    + windowMouseX - view.dragMouseX;
                view.windowOffsetY = view.dragOffsetY
                    + windowMouseY - view.dragMouseY;
                return true;
            }
            if (message == WM_LBUTTONUP && view.draggingWindow)
            {
                view.draggingWindow = false;
                return true;
            }

            int canvasMouseX = 0;
            int canvasMouseY = 0;
            if (!MapWindowPoint(
                    view,
                    windowMouseX,
                    windowMouseY,
                    canvasMouseX,
                    canvasMouseY
                ))
            {
                if (message == WM_MOUSEMOVE)
                {
                    std::vector<gui::GuiResolvedWidget> widgets =
                        view.InteractiveWidgets();
                    view.markerLayers.HandleMove(
                        widgets,
                        view.indexedMaps,
                        -1,
                        -1
                    );
                    view.indexedMaps.HandleMove(widgets, -1, -1);
                    view.controller->DispatchMove(widgets, -1, -1);
                }
                continue;
            }
            if (message == WM_LBUTTONDOWN)
            {
                windowManager.Focus(view.controller->PluginId());
            }

            std::vector<gui::GuiResolvedWidget> widgets =
                view.InteractiveWidgets();
            const gui::GuiResolvedWidget* target =
                gui::HitTestGuiWidgets(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
            if (message == WM_MOUSEMOVE)
            {
                GuiMarkerLayerD3D9InputResult markerResult =
                    view.markerLayers.HandleMove(
                        widgets,
                        view.indexedMaps,
                        canvasMouseX,
                        canvasMouseY
                    );
                if (!markerResult.events.empty())
                {
                    view.controller->DispatchEvents(
                        markerResult.events,
                        canvasMouseX,
                        canvasMouseY
                    );
                }
                if (markerResult.consumed)
                {
                    view.indexedMaps.HandleMove(widgets, -1, -1);
                    view.controller->DispatchMove(widgets, -1, -1);
                    return true;
                }
                view.indexedMaps.HandleMove(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                view.controller->DispatchMove(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                continue;
            }
            if (message == WM_MOUSEWHEEL)
            {
                if (view.controller->ScrollListAt(
                        canvasMouseX,
                        canvasMouseY,
                        -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA
                    ))
                {
                    return true;
                }
                continue;
            }
            if (message == WM_LBUTTONDOWN)
            {
                WriteGuiDiagnostic(
                    "GUI pointer down: plugin="
                    + std::string(view.controller->PluginId())
                    + ", window=("
                    + std::to_string(windowMouseX) + ","
                    + std::to_string(windowMouseY) + ")"
                    + ", canvas=("
                    + std::to_string(canvasMouseX) + ","
                    + std::to_string(canvasMouseY) + ")"
                    + ", target="
                    + (target && target->definition
                        ? target->definition->name
                        : std::string("none"))
                );
                GuiMarkerLayerD3D9InputResult markerResult =
                    view.markerLayers.HandlePress(
                        widgets,
                        view.indexedMaps,
                        canvasMouseX,
                        canvasMouseY
                    );
                if (!markerResult.events.empty())
                {
                    view.controller->DispatchEvents(
                        markerResult.events,
                        canvasMouseX,
                        canvasMouseY
                    );
                }
                if (markerResult.consumed)
                {
                    return true;
                }
                if (view.controller->IsWindowDragRegion(
                        canvasMouseX,
                        canvasMouseY
                    ))
                {
                    view.draggingWindow = true;
                    view.dragMouseX = windowMouseX;
                    view.dragMouseY = windowMouseY;
                    view.dragOffsetX = view.windowOffsetX;
                    view.dragOffsetY = view.windowOffsetY;
                    return true;
                }
                view.indexedMaps.HandlePress(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                view.controller->DispatchPress(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                if (IsInteractiveTarget(target))
                {
                    return true;
                }
            }
            if (message == WM_LBUTTONUP)
            {
                GuiMarkerLayerD3D9InputResult markerResult =
                    view.markerLayers.HandleRelease(
                        widgets,
                        view.indexedMaps,
                        canvasMouseX,
                        canvasMouseY
                    );
                if (!markerResult.events.empty())
                {
                    view.controller->DispatchEvents(
                        markerResult.events,
                        canvasMouseX,
                        canvasMouseY
                    );
                }
                if (markerResult.consumed)
                {
                    return true;
                }
                view.indexedMaps.HandleRelease(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                const bool interactive = IsInteractiveTarget(target)
                    || !view.controller->InputState().pressedKey.empty();
                view.controller->DispatchRelease(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                if (interactive)
                {
                    return true;
                }
            }
        }
        if (windowManager.HasActiveModal()
            && (message == WM_LBUTTONDOWN
                || message == WM_LBUTTONUP
                || message == WM_RBUTTONDOWN
                || message == WM_RBUTTONUP
                || message == WM_MOUSEWHEEL))
        {
            return true;
        }
        return false;
    }
};

GuiD3D9Host::GuiD3D9Host()
    : impl_(std::make_unique<Impl>())
{
}

GuiD3D9Host::~GuiD3D9Host()
{
    Shutdown();
}

bool GuiD3D9Host::Initialize(
    const std::filesystem::path& root,
    IDirect3DDevice9* device,
    std::string& error
)
{
    return impl_->Initialize(root, device, error);
}

void GuiD3D9Host::Shutdown()
{
    impl_->Shutdown();
}

void GuiD3D9Host::TickAndRender(IDirect3DDevice9* device)
{
    impl_->TickAndRender(device);
}

void GuiD3D9Host::BeforeDeviceReset()
{
    impl_->ReleaseCanvas();
}

bool GuiD3D9Host::AfterDeviceReset(
    IDirect3DDevice9* device,
    std::string& error
)
{
    if (!impl_->initialized)
    {
        error = "D3D9 GUI host is not initialized";
        return false;
    }
    if (!device)
    {
        error = "Reset D3D9 device is missing";
        return false;
    }
    if (impl_->device != device)
    {
        error = "Reset D3D9 device does not own the GUI host";
        return false;
    }
    return impl_->CreateCanvas(error);
}

bool GuiD3D9Host::HandleWindowMessage(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (window != impl_->targetWindow)
    {
        return false;
    }
    return impl_->HandleWindowMessage(message, wParam, lParam);
}

bool GuiD3D9Host::IsInitialized() const
{
    return impl_->initialized;
}

bool GuiD3D9Host::UsesDevice(IDirect3DDevice9* device) const
{
    return impl_->initialized && impl_->device == device;
}

HWND GuiD3D9Host::TargetWindow() const
{
    return impl_->targetWindow;
}
