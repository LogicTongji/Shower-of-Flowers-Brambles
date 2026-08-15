#include "gui_host_d3d9.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gdiplus.h>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gui_application_bus.h"
#include "gui_diagnostics.h"
#include "gui_indexed_map_d3d9.h"
#include "gui_inprocess_application.h"
#include "gui_localization.h"
#include "gui_marker_layer_d3d9.h"
#include "gui_render_queue.h"
#include "gui_text_renderer_d3d9.h"
#include "gui_texture_loader_d3d9.h"
#include "gui_window_session.h"

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

void OffsetRect(gui::GuiRect& rect, int x, int y)
{
    rect.x += x;
    rect.y += y;
}

void OffsetWidgets(
    std::vector<gui::GuiResolvedWidget>& widgets,
    int x,
    int y
)
{
    for (gui::GuiResolvedWidget& widget : widgets)
    {
        OffsetRect(widget.rect, x, y);
    }
}

bool PointInside(const gui::GuiRect& rect, int x, int y)
{
    return x >= rect.x
        && y >= rect.y
        && x < rect.x + rect.width
        && y < rect.y + rect.height;
}

bool RectanglesIntersect(
    const gui::GuiRect& first,
    const gui::GuiRect& second
)
{
    return first.x < second.x + second.width
        && first.x + first.width > second.x
        && first.y < second.y + second.height
        && first.y + first.height > second.y;
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
    }

private:
    std::unordered_map<std::string, GuiD3D9Texture> textures_;
};

void ConfigureOverlayState(IDirect3DDevice9* device)
{
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetFVF(OverlayVertexFormat);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
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

void DrawQuad(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255)
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
        {left, top, 0.0f, 1.0f, color, 0.0f, 0.0f},
        {right, top, 0.0f, 1.0f, color, 1.0f, 0.0f},
        {left, bottom, 0.0f, 1.0f, color, 0.0f, 1.0f},
        {right, bottom, 0.0f, 1.0f, color, 1.0f, 1.0f}
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
        int offsetX = 0;
        int offsetY = 0;
        bool draggingWindow = false;
        int dragMouseX = 0;
        int dragMouseY = 0;
        int dragOffsetX = 0;
        int dragOffsetY = 0;
        bool visibilityObserved = false;
        bool lastVisible = false;
        bool firstDrawLogged = false;

        std::vector<gui::GuiResolvedWidget> InteractiveWidgets() const
        {
            std::vector<gui::GuiResolvedWidget> widgets =
                controller->ResolveInteractiveWidgets();
            OffsetWidgets(widgets, offsetX, offsetY);
            return widgets;
        }
    };

    GuiInProcessApplication application;
    GuiApplicationActionBus actionBus;
    GuiLocalizationRegistry localization;
    GuiD3D9TextureCache textures;
    GuiTextRendererD3D9 textRenderer;
    std::vector<std::unique_ptr<SessionView>> sessions;
    IDirect3DDevice9* device = nullptr;
    HWND targetWindow = nullptr;
    ULONG_PTR gdiplusToken = 0;
    bool initialized = false;

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
            view->offsetX = cascade * 24;
            view->offsetY = cascade * 24;
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
            view->controller->SetEventResolver(
                [viewPointer](std::vector<GuiActionEvent>& events)
                {
                    viewPointer->indexedMaps.AttachItemIds(events);
                }
            );
            if (!view->controller->Bind(error))
            {
                Shutdown();
                return false;
            }
            const gui::WindowDefinition* definition =
                view->controller->Runtime().Definition();
            if (!definition
                || !view->indexedMaps.Initialize(
                    application.Root(),
                    device,
                    application.Interpreter(),
                    *definition,
                    error
                )
                || !view->controller->Initialize(device, error))
            {
                Shutdown();
                return false;
            }
            view->markerLayers.SetData(
                view->controller->DataRegistry()
            );
            view->indexedMaps.Refresh(
                view->controller->LayoutContext()
            );
            sessions.push_back(std::move(view));
            ++cascade;
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
        textRenderer.Shutdown();
        textures.Clear();
        application.Shutdown();
        device = nullptr;
        targetWindow = nullptr;
        initialized = false;
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

    void DrawList(SessionView& view, std::string_view listName)
    {
        GuiListRuntimeLayout layout =
            view.controller->BuildListRuntimeLayout(listName);
        OffsetRect(layout.viewport, view.offsetX, view.offsetY);
        OffsetRect(layout.scrollbar, view.offsetX, view.offsetY);
        for (GuiListItemRuntimeLayout& item : layout.items)
        {
            OffsetRect(item.rect, view.offsetX, view.offsetY);
            item.rect.y -= layout.scrollOffset;
            if (!item.visible
                || !RectanglesIntersect(item.rect, layout.viewport))
            {
                continue;
            }
            DrawSprite(
                item.pressed
                    ? item.pressedSpriteName
                    : item.normalSpriteName,
                item.rect,
                item.enabled
                    ? D3DCOLOR_ARGB(255, 255, 255, 255)
                    : D3DCOLOR_ARGB(150, 150, 150, 150)
            );
        }
        if (layout.maximumScroll > 0)
        {
            DrawSprite(layout.scrollbarTrackSprite, layout.scrollbar);
            DrawSprite(
                layout.scrollbarThumbSprite,
                CalculateScrollbarThumb(layout)
            );
        }

        std::vector<gui::GuiTextCommand> textCommands =
            view.controller->Runtime().BuildListTextCommands(
                listName,
                view.controller->LayoutContext()
            );
        for (std::size_t index = 0;
            index < textCommands.size();
            ++index)
        {
            gui::GuiTextCommand& command = textCommands[index];
            OffsetRect(command.rect, view.offsetX, view.offsetY);
            command.rect.y -= layout.scrollOffset;
            if (!RectanglesIntersect(command.rect, layout.viewport))
            {
                continue;
            }
            const std::string slot = std::string(
                view.controller->PluginId()
            ) + ":list:" + std::string(listName)
                + ":" + std::to_string(index);
            DrawTextureQuad(
                device,
                command.rect,
                textRenderer.Resolve(std::move(slot), command)
            );
        }
    }

    void DrawSession(SessionView& view)
    {
        std::vector<gui::GuiResolvedWidget> widgets =
            view.controller->Runtime().ResolveLayout(
                view.controller->LayoutContext()
            );
        OffsetWidgets(widgets, view.offsetX, view.offsetY);
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
        std::unordered_map<
            const gui::WidgetDefinition*,
            gui::GuiTextCommand
        > textCommands;
        for (gui::GuiTextCommand command
            : view.controller->Runtime().BuildTextCommands(
                view.controller->LayoutContext()
            ))
        {
            OffsetRect(command.rect, view.offsetX, view.offsetY);
            textCommands[command.definition] = std::move(command);
        }

        for (const GuiRenderCommand& command : queue)
        {
            if (!command.widget || !command.widget->definition)
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
                DrawSprite(definition.spriteName, command.widget->rect);
                break;
            case GuiRenderCommandType::Button:
            {
                const bool pressed =
                    !view.controller->InputState().pressedKey.empty()
                    && view.controller->InputState()
                        .pressedSnapshot.definition == &definition;
                DrawSprite(
                    pressed && !definition.pressedSpriteName.empty()
                        ? definition.pressedSpriteName
                        : definition.spriteName,
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
            case GuiRenderCommandType::List:
                DrawList(view, definition.name);
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
                const auto text = textCommands.find(&definition);
                if (text == textCommands.end())
                {
                    break;
                }
                const std::string slot = std::string(
                    view.controller->PluginId()
                ) + ":text:" + std::to_string(
                    reinterpret_cast<std::uintptr_t>(&definition)
                );
                DrawTextureQuad(
                    device,
                    text->second.rect,
                    textRenderer.Resolve(slot, text->second)
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
    }

    void TickAndRender(IDirect3DDevice9* nextDevice)
    {
        if (!initialized || !nextDevice || nextDevice != device)
        {
            return;
        }
        const uint64_t now = GetTickCount64();
        for (const auto& session : sessions)
        {
            session->controller->Tick(now);
            const bool visible = session->controller->IsVisible();
            if (!session->visibilityObserved
                || session->lastVisible != visible)
            {
                const std::shared_ptr<GuiDataRegistry>& data =
                    session->controller->DataRegistry();
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
                );
                session->visibilityObserved = true;
                session->lastVisible = visible;
            }
        }

        IDirect3DStateBlock9* stateBlock = nullptr;
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock))
            || !stateBlock)
        {
            return;
        }
        stateBlock->Capture();
        ConfigureOverlayState(device);
        textRenderer.BeginFrame();
        for (const auto& session : sessions)
        {
            if (session->controller->IsOpen()
                && session->controller->IsVisible())
            {
                DrawSession(*session);
            }
        }
        textRenderer.EndFrame();
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
        int mouseX = GET_X_LPARAM(lParam);
        int mouseY = GET_Y_LPARAM(lParam);
        if (message == WM_MOUSEWHEEL)
        {
            POINT point{mouseX, mouseY};
            ScreenToClient(targetWindow, &point);
            mouseX = point.x;
            mouseY = point.y;
        }

        for (auto iterator = sessions.rbegin();
            iterator != sessions.rend();
            ++iterator)
        {
            SessionView& view = **iterator;
            if (!view.controller->IsOpen()
                || !view.controller->IsVisible())
            {
                continue;
            }
            if (message == WM_MOUSEMOVE && view.draggingWindow)
            {
                view.offsetX = view.dragOffsetX
                    + mouseX - view.dragMouseX;
                view.offsetY = view.dragOffsetY
                    + mouseY - view.dragMouseY;
                return true;
            }

            std::vector<gui::GuiResolvedWidget> widgets =
                view.InteractiveWidgets();
            const gui::GuiResolvedWidget* target =
                gui::HitTestGuiWidgets(widgets, mouseX, mouseY);
            if (message == WM_MOUSEMOVE)
            {
                GuiMarkerLayerD3D9InputResult markerResult =
                    view.markerLayers.HandleMove(
                        widgets,
                        view.indexedMaps,
                        mouseX,
                        mouseY
                    );
                if (!markerResult.events.empty())
                {
                    view.controller->DispatchEvents(
                        markerResult.events,
                        mouseX,
                        mouseY
                    );
                }
                if (markerResult.consumed)
                {
                    view.indexedMaps.HandleMove(widgets, -1, -1);
                    view.controller->DispatchMove(widgets, -1, -1);
                    return true;
                }
                view.indexedMaps.HandleMove(widgets, mouseX, mouseY);
                view.controller->DispatchMove(widgets, mouseX, mouseY);
                continue;
            }
            if (message == WM_MOUSEWHEEL)
            {
                if (view.controller->ScrollListAt(
                        mouseX - view.offsetX,
                        mouseY - view.offsetY,
                        -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA
                    ))
                {
                    return true;
                }
                continue;
            }
            if (message == WM_LBUTTONDOWN)
            {
                GuiMarkerLayerD3D9InputResult markerResult =
                    view.markerLayers.HandlePress(
                        widgets,
                        view.indexedMaps,
                        mouseX,
                        mouseY
                    );
                if (!markerResult.events.empty())
                {
                    view.controller->DispatchEvents(
                        markerResult.events,
                        mouseX,
                        mouseY
                    );
                }
                if (markerResult.consumed)
                {
                    return true;
                }
                if (view.controller->IsWindowDragRegion(
                        mouseX - view.offsetX,
                        mouseY - view.offsetY
                    ))
                {
                    view.draggingWindow = true;
                    view.dragMouseX = mouseX;
                    view.dragMouseY = mouseY;
                    view.dragOffsetX = view.offsetX;
                    view.dragOffsetY = view.offsetY;
                    return true;
                }
                view.indexedMaps.HandlePress(widgets, mouseX, mouseY);
                view.controller->DispatchPress(widgets, mouseX, mouseY);
                if (IsInteractiveTarget(target))
                {
                    return true;
                }
            }
            if (message == WM_LBUTTONUP)
            {
                if (view.draggingWindow)
                {
                    view.draggingWindow = false;
                    return true;
                }
                GuiMarkerLayerD3D9InputResult markerResult =
                    view.markerLayers.HandleRelease(
                        widgets,
                        view.indexedMaps,
                        mouseX,
                        mouseY
                    );
                if (!markerResult.events.empty())
                {
                    view.controller->DispatchEvents(
                        markerResult.events,
                        mouseX,
                        mouseY
                    );
                }
                if (markerResult.consumed)
                {
                    return true;
                }
                view.indexedMaps.HandleRelease(widgets, mouseX, mouseY);
                const bool interactive = IsInteractiveTarget(target)
                    || !view.controller->InputState().pressedKey.empty();
                view.controller->DispatchRelease(
                    widgets,
                    mouseX,
                    mouseY
                );
                if (interactive)
                {
                    return true;
                }
            }
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
    error.clear();
    return true;
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
