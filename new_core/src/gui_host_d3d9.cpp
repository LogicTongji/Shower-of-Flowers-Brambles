#include "gui_host_d3d9.h"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gdiplus.h>
#include <memory>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cctype>
#include <string>

#include "gui_application_bus.h"
#include "gui_custom_widget_d3d9.h"
#include "gui_diagnostics.h"
#include "gui_indexed_map_d3d9.h"
#include "gui_inprocess_application.h"
#include "gui_localization.h"
#include "gui_lua_bridge.h"
#include "gui_render_queue.h"
#include "gui_text_renderer_d3d9.h"
#include "gui_texture_loader_d3d9.h"
#include "gui_window_session.h"
#include "gui_window_manager.h"

namespace
{

std::string Lower(std::string value)
{
	std::transform(
		value.begin(),
		value.end(),
		value.begin(),
		[](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		}
	);
	return value;
}

enum class GuiImageScaleMode
{
    Stretch,
    Contain,
    Center
};

GuiImageScaleMode ResolveImageScaleMode(
    std::string_view value
)
{
    std::string normalized(value);

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    if (
        normalized == "center"
        || normalized == "none"
    )
    {
        return GuiImageScaleMode::Center;
    }

    if (
        normalized == "contain"
        || normalized == "preserve"
        || normalized == "preserveaspect"
        || normalized == "aspect"
    )
    {
        return GuiImageScaleMode::Contain;
    }

    return GuiImageScaleMode::Stretch;
}
gui::GuiRect CalculateContainRect(
    const gui::GuiRect& target,
    int sourceWidth,
    int sourceHeight
)
{
    if (
        target.width <= 0
        || target.height <= 0
        || sourceWidth <= 0
        || sourceHeight <= 0
    )
    {
        return target;
    }

    const double scale = std::min(
        static_cast<double>(target.width)
            / static_cast<double>(sourceWidth),
        static_cast<double>(target.height)
            / static_cast<double>(sourceHeight)
    );

    const int width = std::max(
        1,
        static_cast<int>(
            std::lround(sourceWidth * scale)
        )
    );

    const int height = std::max(
        1,
        static_cast<int>(
            std::lround(sourceHeight * scale)
        )
    );

    return {
        target.x + (target.width - width) / 2,
        target.y + (target.height - height) / 2,
        width,
        height
    };
}

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

D3DCOLOR ModulateColor(
	D3DCOLOR color,
	const gui::GuiRgbaColor& multiplier
)
{
	const auto channel = [](uint32_t value, float scale)
	{
		return static_cast<uint8_t>(std::lround(std::clamp(
			static_cast<float>(value) * scale,
			0.0f,
			255.0f
		)));
	};
	return D3DCOLOR_ARGB(
		channel((color >> 24) & 0xff, multiplier.a),
		channel((color >> 16) & 0xff, multiplier.r),
		channel((color >> 8) & 0xff, multiplier.g),
		channel(color & 0xff, multiplier.b)
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

    GuiD3D9Texture* ResolveAsset(
        IDirect3DDevice9* device,
        const std::filesystem::path& root,
        const gui::GuiInterpreter& interpreter,
        const std::filesystem::path& asset
    )
    {
        if (asset.empty())
        {
            return nullptr;
        }
        const std::filesystem::path path = interpreter.ResolveAssetPath(
            asset,
            root
        );
        if (path.empty())
        {
            return nullptr;
        }
        const std::string key = "asset:" + path.lexically_normal().string();
        if (failedTextures_.find(key) != failedTextures_.end())
        {
            return nullptr;
        }
        const auto existing = textures_.find(key);
        if (existing != textures_.end())
        {
            return &existing->second;
        }
        GuiD3D9Texture texture;
        std::string error;
        if (!LoadGuiD3D9Texture(device, path, texture, error))
        {
            failedTextures_.insert(key);
            WriteGuiDiagnostic(
                "GUI texture load failed: asset=" + path.string()
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
    float v1,
	const gui::GuiTransform2D* transform = nullptr,
	const gui::GuiRect* transformBounds = nullptr
)
{
    if (!device || rect.width <= 0 || rect.height <= 0)
    {
        return;
    }
	float leftTopX = static_cast<float>(rect.x);
	float leftTopY = static_cast<float>(rect.y);
	float rightTopX = static_cast<float>(rect.x + rect.width);
	float rightTopY = static_cast<float>(rect.y);
	float leftBottomX = static_cast<float>(rect.x);
	float leftBottomY = static_cast<float>(rect.y + rect.height);
	float rightBottomX = static_cast<float>(rect.x + rect.width);
	float rightBottomY = static_cast<float>(rect.y + rect.height);
	const bool transformed = transform
		&& (std::abs(transform->rotationDegrees) > 0.000001f
			|| std::abs(transform->scaleX - 1.0f) > 0.000001f
			|| std::abs(transform->scaleY - 1.0f) > 0.000001f
			|| transform->flipX
			|| transform->flipY);
	if (transformed)
	{
		const gui::GuiRect& bounds = transformBounds
			? *transformBounds
			: rect;
		const float pivotX = static_cast<float>(bounds.x)
			+ static_cast<float>(bounds.width) * transform->pivotX;
		const float pivotY = static_cast<float>(bounds.y)
			+ static_cast<float>(bounds.height) * transform->pivotY;
		const float scaleX = transform->scaleX
			* (transform->flipX ? -1.0f : 1.0f);
		const float scaleY = transform->scaleY
			* (transform->flipY ? -1.0f : 1.0f);
		const float radians = transform->rotationDegrees
			* 3.14159265358979323846f / 180.0f;
		const float cosine = std::cos(radians);
		const float sine = std::sin(radians);
		const auto apply = [pivotX, pivotY, scaleX, scaleY, cosine, sine](
			float& x,
			float& y
		)
		{
			const float localX = (x - pivotX) * scaleX;
			const float localY = (y - pivotY) * scaleY;
			x = pivotX + cosine * localX - sine * localY;
			y = pivotY + sine * localX + cosine * localY;
		};
		apply(leftTopX, leftTopY);
		apply(rightTopX, rightTopY);
		apply(leftBottomX, leftBottomY);
		apply(rightBottomX, rightBottomY);
	}
    const OverlayVertex vertices[4] = {
		{leftTopX - 0.5f, leftTopY - 0.5f, 0.0f, 1.0f, color, u0, v0},
		{rightTopX - 0.5f, rightTopY - 0.5f, 0.0f, 1.0f, color, u1, v0},
		{leftBottomX - 0.5f, leftBottomY - 0.5f, 0.0f, 1.0f, color, u0, v1},
		{rightBottomX - 0.5f, rightBottomY - 0.5f, 0.0f, 1.0f, color, u1, v1}
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
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255),
	const gui::GuiTransform2D* transform = nullptr,
	const gui::GuiRect* transformBounds = nullptr
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
        1.0f,
		transform,
		transformBounds
    );
}

void DrawTextureQuad(
    IDirect3DDevice9* device,
    const gui::GuiRect& rect,
    IDirect3DTexture9* texture,
    D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255),
	const gui::GuiTransform2D* transform = nullptr,
	const gui::GuiRect* transformBounds = nullptr
)
{
    if (texture)
    {
        DrawQuad(device, rect, texture, color, transform, transformBounds);
    }
}

void DrawTextureRegion(
    IDirect3DDevice9* device,
    const gui::GuiRect& destination,
    IDirect3DTexture9* texture,
    const gui::GuiRect& source,
    int textureWidth,
    int textureHeight,
    D3DCOLOR color = D3DCOLOR_ARGB(255,255,255,255),
	const gui::GuiTransform2D* transform = nullptr,
	const gui::GuiRect* transformBounds = nullptr
)
{
    if (!texture
        || destination.width <= 0
        || destination.height <= 0
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
        color,
        static_cast<float>(source.x) / textureWidth,
        static_cast<float>(source.y) / textureHeight,
        static_cast<float>(source.x + source.width) / textureWidth,
		static_cast<float>(source.y + source.height) / textureHeight,
		transform,
		transformBounds
    );
}

void FitSlicePair(
    int total,
    int requestedFirst,
    int requestedSecond,
    int& first,
    int& second
)
{
    requestedFirst =
        std::max(
            0,
            requestedFirst
        );

    requestedSecond =
        std::max(
            0,
            requestedSecond
        );

    if (total <= 0)
    {
        first = 0;
        second = 0;
        return;
    }

    const int requestedTotal =
        requestedFirst
        + requestedSecond;
    /*
        两侧边距没有超过可用尺寸：
        直接保持原尺寸。
    */
    if (requestedTotal <= total)
    {
        first =
            requestedFirst;

        second =
            requestedSecond;

        return;
    }
    /*
        两侧边距超过目标尺寸。
    */
    if (requestedTotal <= 0)
    {
        first = 0;
        second = 0;
        return;
    }

    const double scale =
        static_cast<double>(
            total
        )
        /
        static_cast<double>(
            requestedTotal
        );

    first =
        static_cast<int>(
            std::lround(
                requestedFirst
                * scale
            )
        );

    first =
        std::clamp(
            first,
            0,
            total
        );
    /*
    不会因为浮点取整产生 1 像素缺口。
    */
    second =
        total - first;
}

void DrawNineSlice(
    IDirect3DDevice9* device,
    const GuiD3D9Texture& texture,
	const gui::GuiRect& sourceFrame,
    const gui::GuiRect& target,
    const gui::GuiNineSliceInsets& requestedInsets,
    D3DCOLOR color,
	const gui::GuiTransform2D* transform = nullptr,
	const gui::GuiRect* transformBounds = nullptr
)
{
    if (
        !device
        || !texture.texture
        || texture.width <= 0
        || texture.height <= 0
		|| sourceFrame.width <= 0
		|| sourceFrame.height <= 0
        || target.width <= 0
        || target.height <= 0
    )
    {
        return;
    }
    /*
    ===========================================================================
    1. 计算 Source 九宫格边距
    ===========================================================================
    */
    int sourceLeft = 0;
    int sourceRight = 0;

    int sourceTop = 0;
    int sourceBottom = 0;

    FitSlicePair(
        sourceFrame.width,
        requestedInsets.left,
        requestedInsets.right,
        sourceLeft,
        sourceRight
    );

    FitSlicePair(
        sourceFrame.height,
        requestedInsets.top,
        requestedInsets.bottom,
        sourceTop,
        sourceBottom
    );

    /*
    ===========================================================================
    2. 计算 Destination 九宫格边距
    ===========================================================================
    */
    int destinationLeft = 0;
    int destinationRight = 0;

    int destinationTop = 0;
    int destinationBottom = 0;

    FitSlicePair(
        target.width,
        sourceLeft,
        sourceRight,
        destinationLeft,
        destinationRight
    );

    FitSlicePair(
        target.height,
        sourceTop,
        sourceBottom,
        destinationTop,
        destinationBottom
    );
    /*
    ===========================================================================
    3. Source 四条 X/Y 分割线
    ===========================================================================

              sourceLeft            sourceRight
                ↓                       ↓

        x0        x1            x2       x3
        │         │             │        │
        0      left       width-right   width
    */
    const int sourceX[4] =
    {
        sourceFrame.x,

        sourceFrame.x + sourceLeft,

        sourceFrame.x + sourceFrame.width
            - sourceRight,

        sourceFrame.x + sourceFrame.width
    };

    const int sourceY[4] =
    {
        sourceFrame.y,

        sourceFrame.y + sourceTop,

        sourceFrame.y + sourceFrame.height
            - sourceBottom,

        sourceFrame.y + sourceFrame.height
    };
    /*
    ===========================================================================
    4. Destination 四条 X/Y 分割线
    ===========================================================================
    */
    const int destinationX[4] =
    {
        target.x,

        target.x
            + destinationLeft,

        target.x
            + target.width
            - destinationRight,

        target.x
            + target.width
    };

    const int destinationY[4] =
    {
        target.y,

        target.y
            + destinationTop,

        target.y
            + target.height
            - destinationBottom,

        target.y
            + target.height
    };
    /*
    ===========================================================================
    5. 绘制 3 × 3 共九个区域
    ===========================================================================
        ┌─────────┬─────────────┬─────────┐
        │   TL    │     TOP     │   TR    │
        ├─────────┼─────────────┼─────────┤
        │  LEFT   │   CENTER    │  RIGHT  │
        ├─────────┼─────────────┼─────────┤
        │   BL    │   BOTTOM    │   BR    │
        └─────────┴─────────────┴─────────┘
    */
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

    for (
        int row = 0;
        row < 3;
        ++row
    )
    {
        for (
            int column = 0;
            column < 3;
            ++column
        )
        {
            const gui::GuiRect source
            {
                sourceX[column],
                sourceY[row],

                sourceX[column + 1]
                    - sourceX[column],

                sourceY[row + 1]
                    - sourceY[row]
            };

            const gui::GuiRect destination
            {
                destinationX[column],
                destinationY[row],

                destinationX[column + 1]
                    - destinationX[column],

                destinationY[row + 1]
                    - destinationY[row]
            };
            /*
                某些极小尺寸情况下：直接跳过即可。
            */
            if (
                source.width <= 0
                || source.height <= 0
                || destination.width <= 0
                || destination.height <= 0
            )
            {
                continue;
            }

            DrawTextureRegion(
                device,
                destination,
                texture.texture,
                source,
                texture.width,
                texture.height,
				color,
				transform,
				transformBounds ? transformBounds : &target
            );
        }
    }

    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
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
        layout.minimumScrollbarThumbSize,
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
        || !actions.onClick.empty()
        || !actions.onPress.empty()
        || !actions.onRelease.empty();
}

bool SameWidgetIdentity(
    const gui::GuiResolvedWidget& first,
    const gui::GuiResolvedWidget& second
)
{
    return first.definition == second.definition
        && first.listName == second.listName
        && first.listIndex == second.listIndex
        && first.listItemId == second.listItemId;
}

uint32_t ResolveInputModifiers(WPARAM wParam = 0)
{
    uint32_t modifiers = gui::GuiCustomModifierNone;
    if ((wParam & MK_SHIFT) != 0
        || (GetKeyState(VK_SHIFT) & 0x8000) != 0)
    {
        modifiers |= gui::GuiCustomModifierShift;
    }
    if ((wParam & MK_CONTROL) != 0
        || (GetKeyState(VK_CONTROL) & 0x8000) != 0)
    {
        modifiers |= gui::GuiCustomModifierControl;
    }
    if ((GetKeyState(VK_MENU) & 0x8000) != 0)
    {
        modifiers |= gui::GuiCustomModifierAlt;
    }
    if ((wParam & MK_LBUTTON) != 0)
    {
        modifiers |= gui::GuiCustomModifierLeftButton;
    }
    if ((wParam & MK_RBUTTON) != 0)
    {
        modifiers |= gui::GuiCustomModifierRightButton;
    }
    if ((wParam & MK_MBUTTON) != 0)
    {
        modifiers |= gui::GuiCustomModifierMiddleButton;
    }
    if ((wParam & MK_XBUTTON1) != 0)
    {
        modifiers |= gui::GuiCustomModifierX1Button;
    }
    if ((wParam & MK_XBUTTON2) != 0)
    {
        modifiers |= gui::GuiCustomModifierX2Button;
    }
    return modifiers;
}

gui::GuiCustomPointerButton ResolvePointerButton(
    UINT message,
    WPARAM wParam
)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        return gui::GuiCustomPointerButton::Left;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        return gui::GuiCustomPointerButton::Right;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        return gui::GuiCustomPointerButton::Middle;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        return GET_XBUTTON_WPARAM(wParam) == XBUTTON1
            ? gui::GuiCustomPointerButton::X1
            : gui::GuiCustomPointerButton::X2;
    default:
        return gui::GuiCustomPointerButton::None;
    }
}

bool IsPointerDownMessage(UINT message)
{
    return message == WM_LBUTTONDOWN
        || message == WM_RBUTTONDOWN
        || message == WM_MBUTTONDOWN
        || message == WM_XBUTTONDOWN;
}

bool IsPointerUpMessage(UINT message)
{
    return message == WM_LBUTTONUP
        || message == WM_RBUTTONUP
        || message == WM_MBUTTONUP
        || message == WM_XBUTTONUP;
}

}

struct GuiD3D9Host::Impl
{
    struct SessionView
    {
        std::unique_ptr<GuiWindowSessionController> controller;
        GuiIndexedMapD3D9Runtime indexedMaps;
        GuiCustomWidgetD3D9Runtime customWidgetRuntime;
        gui::GuiRect sourceRect;
        gui::GuiRect presentationRect;
		gui::GuiRect rootCanvasRect;
		gui::GuiRect viewportRect;
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
        gui::GuiResolvedWidget hoveredCustom;
        gui::GuiResolvedWidget focusedCustom;
        gui::GuiResolvedWidget capturedCustom;
        gui::GuiCustomPointerButton capturedButton =
            gui::GuiCustomPointerButton::None;
        bool hasHoveredCustom = false;
        bool hasFocusedCustom = false;
        bool hasCapturedCustom = false;
        int lastCanvasMouseX = -1;
        int lastCanvasMouseY = -1;
        std::string tooltipHoverKey;
        uint64_t tooltipHoverStartedMilliseconds = 0;
		uint64_t animationEpochMilliseconds = 0;
		bool animationClockStarted = false;
        double maxViewportWidthRatio = 1.0;
        double maxViewportHeightRatio = 1.0;

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

	bool EnsureCanvasSize(
		int requiredWidth,
		int requiredHeight,
		std::string& error
	)
	{
		requiredWidth = std::max(requiredWidth, 1);
		requiredHeight = std::max(requiredHeight, 1);
		if (canvasTexture && canvasSurface
			&& canvasWidth >= requiredWidth
			&& canvasHeight >= requiredHeight)
		{
			return true;
		}
		canvasWidth = std::max(canvasWidth, requiredWidth);
		canvasHeight = std::max(canvasHeight, requiredHeight);
		return CreateCanvas(error);
	}

	bool IsFullScreen(const SessionView& view) const
	{
		const gui::WindowDefinition* definition =
			view.controller->Runtime().Definition();
		return definition && definition->fullScreen;
	}

	void UpdateRootClientLayout(
		SessionView& view,
		const D3DVIEWPORT9& viewport
	)
	{
		gui::GuiLayoutContext& context =
			view.controller->LayoutContext();
		context.hasRootClientRect = IsFullScreen(view);
		context.rootClientRect = context.hasRootClientRect
			? gui::GuiRect{
				0,
				0,
				static_cast<int>(viewport.Width),
				static_cast<int>(viewport.Height)
			}
			: gui::GuiRect{};
		view.rootCanvasRect =
			view.controller->Interpreter().ResolveRootRect(
				std::string(view.controller->WindowName()),
				context
			);
	}

    double CanvasScale(
        const SessionView& view,
        const D3DVIEWPORT9& viewport
    ) const
    {
		if (view.rootCanvasRect.width <= 0
			|| view.rootCanvasRect.height <= 0
            || viewport.Width == 0
            || viewport.Height == 0)
        {
            return 0.0;
        }
		if (IsFullScreen(view))
		{
			return 1.0;
		}
		const double maximumWidth = viewport.Width * std::clamp(
			view.maxViewportWidthRatio,
			0.0,
			1.0
		);
		const double maximumHeight = viewport.Height * std::clamp(
			view.maxViewportHeightRatio,
			0.0,
			1.0
		);
        return std::min({
            1.0,
			maximumWidth / view.rootCanvasRect.width,
			maximumHeight / view.rootCanvasRect.height
        });
    }

    gui::GuiRect CalculateSourceRect(SessionView& view)
    {
        const std::vector<gui::GuiResolvedWidget> widgets =
            view.controller->ResolveSceneWidgets();
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
		const int left = std::clamp(
			view.rootCanvasRect.x,
			0,
			canvasWidth
		);
		const int top = std::clamp(
			view.rootCanvasRect.y,
			0,
			canvasHeight
		);
        const int right = std::clamp(
			view.rootCanvasRect.x + view.rootCanvasRect.width,
            0,
            canvasWidth
        );
        const int bottom = std::clamp(
			view.rootCanvasRect.y + view.rootCanvasRect.height,
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
		view.viewportRect = {
			static_cast<int>(viewport.X),
			static_cast<int>(viewport.Y),
			static_cast<int>(viewport.Width),
			static_cast<int>(viewport.Height)
		};
		if (IsFullScreen(view))
		{
			view.expanded = false;
			view.windowOffsetX = 0;
			view.windowOffsetY = 0;
			view.presentationRect = view.sourceRect.width > 0
				&& view.sourceRect.height > 0
				? view.viewportRect
				: gui::GuiRect{};
			return;
		}
        const double scale = CanvasScale(view, viewport);
        if (view.sourceRect.width <= 0
            || view.sourceRect.height <= 0
            || scale <= 0.0)
        {
            view.presentationRect = {};
            return;
        }
		const int rootWidth = std::max(
			1,
			static_cast<int>(std::lround(
				view.sourceRect.width * scale
			))
		);
		const int rootHeight = std::max(
			1,
			static_cast<int>(std::lround(
				view.sourceRect.height * scale
			))
		);
		const gui::WindowDefinition* definition =
			view.controller->Runtime().Definition();
		const std::string orientation = definition
			? Lower(definition->orientation)
			: std::string{};
		const int offsetX = static_cast<int>(std::lround(
			view.rootCanvasRect.x * scale
		));
		const int offsetY = static_cast<int>(std::lround(
			view.rootCanvasRect.y * scale
		));
		int rootX = static_cast<int>(viewport.X)
			+ (static_cast<int>(viewport.Width) - rootWidth) / 2
			+ offsetX;
		int rootY = static_cast<int>(viewport.Y)
			+ (static_cast<int>(viewport.Height) - rootHeight) / 2
			+ offsetY;
		if (orientation == "upper_left")
		{
			rootX = static_cast<int>(viewport.X) + offsetX;
			rootY = static_cast<int>(viewport.Y) + offsetY;
		}
		else if (orientation == "upper_right")
		{
			rootX = static_cast<int>(viewport.X + viewport.Width)
				- rootWidth - offsetX;
			rootY = static_cast<int>(viewport.Y) + offsetY;
		}
		else if (orientation == "lower_left")
		{
			rootX = static_cast<int>(viewport.X) + offsetX;
			rootY = static_cast<int>(viewport.Y + viewport.Height)
				- rootHeight - offsetY;
		}
		else if (orientation == "lower_right")
		{
			rootX = static_cast<int>(viewport.X + viewport.Width)
				- rootWidth - offsetX;
			rootY = static_cast<int>(viewport.Y + viewport.Height)
				- rootHeight - offsetY;
		}
		else if (orientation == "center_up" || orientation == "center_top")
		{
			rootY = static_cast<int>(viewport.Y) + offsetY;
		}
		else if (orientation == "center_down"
			|| orientation == "center_bottom")
		{
			rootY = static_cast<int>(viewport.Y + viewport.Height)
				- rootHeight - offsetY;
		}
		else if (orientation == "center_left")
		{
			rootX = static_cast<int>(viewport.X) + offsetX;
		}
		else if (orientation == "center_right")
		{
			rootX = static_cast<int>(viewport.X + viewport.Width)
				- rootWidth - offsetX;
		}
		if (view.expanded)
		{
			view.windowOffsetX = std::clamp(
				view.windowOffsetX,
				static_cast<int>(viewport.X) - rootX,
				static_cast<int>(viewport.X + viewport.Width)
					- rootX - rootWidth
			);
			view.windowOffsetY = std::clamp(
				view.windowOffsetY,
				static_cast<int>(viewport.Y) - rootY,
				static_cast<int>(viewport.Y + viewport.Height)
					- rootY - rootHeight
			);
		}
        view.presentationRect = {
			rootX + (view.expanded ? view.windowOffsetX : 0),
			rootY + (view.expanded ? view.windowOffsetY : 0),
			rootWidth,
			rootHeight
        };
    }

	void SetWindowOffset(
		SessionView& view,
		int desiredX,
		int desiredY
	) const
	{
		if (!view.expanded
			|| view.presentationRect.width <= 0
			|| view.presentationRect.height <= 0
			|| view.viewportRect.width <= 0
			|| view.viewportRect.height <= 0)
		{
			view.windowOffsetX = desiredX;
			view.windowOffsetY = desiredY;
			return;
		}
		const int baseX = view.presentationRect.x
			- view.windowOffsetX;
		const int baseY = view.presentationRect.y
			- view.windowOffsetY;
		view.windowOffsetX = std::clamp(
			desiredX,
			view.viewportRect.x - baseX,
			view.viewportRect.x + view.viewportRect.width
				- baseX - view.presentationRect.width
		);
		view.windowOffsetY = std::clamp(
			desiredY,
			view.viewportRect.y - baseY,
			view.viewportRect.y + view.viewportRect.height
				- baseY - view.presentationRect.height
		);
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
		D3DVIEWPORT9 initialViewport{};
		if (FAILED(device->GetViewport(&initialViewport))
			|| initialViewport.Width == 0
			|| initialViewport.Height == 0)
		{
			error = "Failed to resolve the D3D9 client viewport";
			Shutdown();
			return false;
		}
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
            view->windowOffsetX = cascade * launch.cascadeOffsetX;
            view->windowOffsetY = cascade * launch.cascadeOffsetY;
            view->initialWindowOffsetX = view->windowOffsetX;
            view->initialWindowOffsetY = view->windowOffsetY;
            view->maxViewportWidthRatio =
                launch.maxViewportWidthRatio;
            view->maxViewportHeightRatio =
                launch.maxViewportHeightRatio;
            view->controller =
                std::make_unique<GuiWindowSessionController>(
                    application.Root(),
                    launch,
                    application.Interpreter(),
                    application.Behaviors()
                );
            SessionView* viewPointer = view.get();
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
                    viewPointer->customWidgetRuntime.SetData(
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
                    viewPointer->tooltipHoverKey.clear();
                    viewPointer->tooltipHoverStartedMilliseconds = 0;
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
            if (!view->customWidgetRuntime.Initialize(
                    device,
                    textRenderer,
                    localization,
                    [this](std::string_view name)
                    {
                        GuiD3D9Texture* texture = ResolveSprite(name);
                        return texture ? texture->texture : nullptr;
                    },
                    view->controller->CustomWidgets(),
                    view->indexedMaps,
                    sessionError
                ))
            {
                view->controller->Shutdown();
                WriteGuiDiagnostic(
                    "GUI plugin disabled during custom widget initialization: id="
                    + launch.id + ", error=" + sessionError
                );
                continue;
            }
            const gui::WindowDefinition* definition =
                view->controller->Runtime().Definition();
			if (!definition
				|| (!definition->fullScreen
					&& (definition->rect.x < 0
						|| definition->rect.y < 0
						|| definition->rect.width <= 0
						|| definition->rect.height <= 0)))
			{
				sessionError = "root_window_canvas_rect_invalid";
			}
			if (!definition
				|| !sessionError.empty()
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
                view->customWidgetRuntime.Shutdown();
                WriteGuiDiagnostic(
                    "GUI plugin disabled during initialization: id="
                    + launch.id + ", error=" + sessionError
                );
                continue;
            }
			UpdateRootClientLayout(*view, initialViewport);
			if (view->rootCanvasRect.x < 0
				|| view->rootCanvasRect.y < 0
				|| view->rootCanvasRect.width <= 0
				|| view->rootCanvasRect.height <= 0)
			{
				view->controller->Shutdown();
				view->indexedMaps.Shutdown();
				view->customWidgetRuntime.Shutdown();
				WriteGuiDiagnostic(
					"GUI plugin disabled during root layout: id="
					+ launch.id + ", error=root_window_canvas_rect_invalid"
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
            view->customWidgetRuntime.SetData(
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
                view->customWidgetRuntime.Shutdown();
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
        if (targetWindow && GetCapture() == targetWindow)
        {
            ReleaseCapture();
        }
        actionBus.SetEndpoints({});
        for (const auto& session : sessions)
        {
            session->controller->Shutdown();
            session->customWidgetRuntime.Shutdown();
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

	int ResolveWidgetFrame(
		const SessionView& view,
		const gui::GuiResolvedWidget& widget,
		std::string_view spriteName,
		uint64_t nowMilliseconds
	) const
	{
		if (!widget.definition)
		{
			return 1;
		}
		const gui::SpriteResource* resource =
			application.Interpreter().FindSprite(std::string(spriteName));
		if (!resource)
		{
			return 1;
		}
		const gui::WidgetDefinition& definition = *widget.definition;
		const bool hasSourcedFrame = !definition.frameSource.empty();
		double sourcedFrameValue = definition.frame;
		if (hasSourcedFrame)
		{
			sourcedFrameValue = view.controller->ResolveWidgetNumber(
					widget,
					definition.frameSource,
					definition.frame
				);
		}
		const int sourcedFrame = static_cast<int>(std::lround(std::clamp(
			sourcedFrameValue,
			static_cast<double>(std::numeric_limits<int>::min()),
			static_cast<double>(std::numeric_limits<int>::max())
		)));
		uint64_t animationTime = nowMilliseconds
			>= view.animationEpochMilliseconds
			? nowMilliseconds - view.animationEpochMilliseconds
			: 0;
		if (!definition.animationTimeSource.empty())
		{
			const double sourceTime = std::max(
				0.0,
				view.controller->ResolveWidgetNumber(
					widget,
					definition.animationTimeSource,
					0.0
				)
			);
			animationTime = sourceTime >= static_cast<double>(
				std::numeric_limits<uint64_t>::max()
			)
				? std::numeric_limits<uint64_t>::max()
				: static_cast<uint64_t>(sourceTime);
		}
		return gui::ResolveSpriteFrameIndex(
			*resource,
			definition,
			animationTime,
			sourcedFrame,
			hasSourcedFrame
		);
	}

	gui::GuiRgbaColor ResolveWidgetEffectMultiplier(
		const SessionView& view,
		const gui::GuiResolvedWidget& widget,
		uint64_t nowMilliseconds
	) const
	{
		const std::string effectName =
			view.controller->ResolveWidgetEffect(widget);
		const gui::GuiEffectResource* resource =
			application.Interpreter().FindEffect(effectName);
		if (!resource)
		{
			return {};
		}
		uint64_t effectTime = nowMilliseconds
			>= view.animationEpochMilliseconds
			? nowMilliseconds - view.animationEpochMilliseconds
			: 0;
		if (widget.definition
			&& !widget.definition->effectTimeSource.empty())
		{
			const double sourceTime = std::max(
				0.0,
				view.controller->ResolveWidgetNumber(
					widget,
					widget.definition->effectTimeSource,
					0.0
				)
			);
			effectTime = sourceTime >= static_cast<double>(
				std::numeric_limits<uint64_t>::max()
			)
				? std::numeric_limits<uint64_t>::max()
				: static_cast<uint64_t>(sourceTime);
		}
		return gui::SampleGuiEffect(*resource, effectTime);
	}

    void DrawSprite(
        std::string_view name,
        gui::GuiRect rect,
        D3DCOLOR color = D3DCOLOR_ARGB(255, 255, 255, 255),
        GuiImageScaleMode scaleMode = GuiImageScaleMode::Stretch,
        const gui::GuiNineSliceInsets* nineSlice = nullptr,
		int frame = 1,
		const gui::GuiTransform2D* transform = nullptr,
		const gui::GuiRect* transformBounds = nullptr
    )
    {
        GuiD3D9Texture* texture = ResolveSprite(name);
        if (!texture)
        {
            return;
        }
		const gui::SpriteResource* resource =
			application.Interpreter().FindSprite(std::string(name));
		const gui::GuiRect sourceFrame = resource
			? gui::ResolveSpriteFrameSourceRect(
				*resource,
				texture->width,
				texture->height,
				frame
			)
			: gui::GuiRect{0, 0, texture->width, texture->height};
		if (sourceFrame.width <= 0 || sourceFrame.height <= 0)
		{
			return;
		}
        if (rect.width <= 0)
        {
            rect.width = sourceFrame.width;
        }
        if (rect.height <= 0)
        {
            rect.height = sourceFrame.height;
        }
        /*
    nineSlice 优先于 scaleMode。
    只要定义了有效的 nineSlice，
    就进入九宫格绘制。
    scaleMode 此时不再参与。
       */
        if (
            nineSlice
            && nineSlice->Enabled()
           )
        {
            DrawNineSlice(
				device,
				*texture,
				sourceFrame,
				rect,
				*nineSlice,
				color,
				transform,
				transformBounds ? transformBounds : &rect
			);
            return;
        }
        switch (scaleMode)
        {
    case GuiImageScaleMode::Contain:
    {
        const gui::GuiRect destination =
             CalculateContainRect(
				 rect,
				 sourceFrame.width,
				 sourceFrame.height
			 );
		DrawTextureRegion(
			device,
			destination,
			texture->texture,
			sourceFrame,
			texture->width,
			texture->height,
			color,
			transform,
			transformBounds ? transformBounds : &rect
		);
        break;
    }
    case GuiImageScaleMode::Center:
    {
    const int visibleWidth = std::min(rect.width,sourceFrame.width);
    const int visibleHeight = std::min(rect.height,sourceFrame.height);
    if (
        visibleWidth <= 0
        || visibleHeight <= 0
    )
    {
        return;
    }
    // 纹理比控件大时，从纹理中央裁切。
    const gui::GuiRect source{
        sourceFrame.x + std::max(
            0,
            (sourceFrame.width - visibleWidth) / 2
        ),
        sourceFrame.y + std::max(
            0,
            (sourceFrame.height - visibleHeight) / 2
        ),
        visibleWidth,
        visibleHeight
    };
    // 纹理比控件小时，在控件中居中。
    const gui::GuiRect destination{
        rect.x
            + std::max(
                0,
                (rect.width - visibleWidth) / 2
            ),
        rect.y
            + std::max(
                0,
                (rect.height - visibleHeight) / 2
            ),
        visibleWidth,
        visibleHeight
    };
    DrawTextureRegion(
		device,
		destination,
		texture->texture,
		source,
		texture->width,
		texture->height,
		color,
		transform,
		transformBounds ? transformBounds : &rect
	);
    break;
    }
     case GuiImageScaleMode::Stretch:
          default:
        {
		   DrawTextureRegion(
			   device,
			   rect,
			   texture->texture,
			   sourceFrame,
			   texture->width,
			   texture->height,
			   color,
			   transform,
			   transformBounds ? transformBounds : &rect
		   );
        break;
        }
        }
    }
    bool ApplyClipRect(gui::GuiRect clipRect)
    {
        const int left = std::clamp(
			clipRect.x,
            0,
            canvasWidth
        );
        const int top = std::clamp(
			clipRect.y,
            0,
            canvasHeight
        );
        const int right = std::clamp(
			clipRect.x + clipRect.width,
            0,
            canvasWidth
        );
        const int bottom = std::clamp(
			clipRect.y + clipRect.height,
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

	bool ApplyWidgetClip(
		const SessionView& view,
		const gui::GuiResolvedWidget& widget
	)
    {
		gui::GuiRect clipRect = view.rootCanvasRect;
		if (widget.hasClipRect
			&& !IntersectRects(
				clipRect,
				widget.clipRect,
				clipRect
			))
		{
			return false;
		}
        return ApplyClipRect(clipRect);
    }

    void DrawGenericTooltip(SessionView& view, uint64_t nowMilliseconds)
    {
        const GuiRuntimeInputState& input =
            view.controller->InputState();
        if (view.lastCanvasMouseX < 0
            || view.lastCanvasMouseY < 0
            || input.hoveredKey.empty()
            || !input.hoveredSnapshot.definition)
        {
            view.tooltipHoverKey.clear();
            view.tooltipHoverStartedMilliseconds = 0;
            return;
        }

        gui::GuiTextCommand text;
        if (!view.controller->ResolveWidgetTooltip(
                input.hoveredSnapshot,
                text
            ))
        {
            view.tooltipHoverKey.clear();
            view.tooltipHoverStartedMilliseconds = 0;
            return;
        }

        if (view.tooltipHoverKey != input.hoveredKey)
        {
            view.tooltipHoverKey = input.hoveredKey;
            view.tooltipHoverStartedMilliseconds = nowMilliseconds;
        }
        const gui::WidgetDefinition& definition =
            *input.hoveredSnapshot.definition;
        const uint64_t elapsed = nowMilliseconds
            - view.tooltipHoverStartedMilliseconds;
        if (elapsed < static_cast<uint64_t>(
                definition.tooltipDelayMilliseconds
            ))
        {
            return;
        }

        std::string placement = definition.tooltipPlacement;
        std::transform(
            placement.begin(),
            placement.end(),
            placement.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            }
        );
        gui::GuiRect rect{
            view.lastCanvasMouseX + definition.tooltipRect.x,
            view.lastCanvasMouseY + definition.tooltipRect.y,
            text.rect.width,
            text.rect.height
        };
        const gui::GuiRect& source = input.hoveredSnapshot.rect;
        if (placement == "right")
        {
            rect.x = source.x + source.width + definition.tooltipRect.x;
            rect.y = source.y + definition.tooltipRect.y;
        }
        else if (placement == "left")
        {
            rect.x = source.x - rect.width + definition.tooltipRect.x;
            rect.y = source.y + definition.tooltipRect.y;
        }
        else if (placement == "top")
        {
            rect.x = source.x + definition.tooltipRect.x;
            rect.y = source.y - rect.height + definition.tooltipRect.y;
        }
        else if (placement == "bottom")
        {
            rect.x = source.x + definition.tooltipRect.x;
            rect.y = source.y + source.height + definition.tooltipRect.y;
        }

        const int maximumX = view.rootCanvasRect.x + std::max(
            0,
            view.rootCanvasRect.width - rect.width
        );
        const int maximumY = view.rootCanvasRect.y + std::max(
            0,
            view.rootCanvasRect.height - rect.height
        );
        rect.x = std::clamp(
            rect.x,
            view.rootCanvasRect.x,
            maximumX
        );
        rect.y = std::clamp(
            rect.y,
            view.rootCanvasRect.y,
            maximumY
        );
        if (!ApplyClipRect(view.rootCanvasRect))
        {
            return;
        }

        const float opacity = input.hoveredSnapshot.opacity;
        const D3DCOLOR backgroundColor = ToD3DColor(
            definition.tooltipColor.r,
            definition.tooltipColor.g,
            definition.tooltipColor.b,
            definition.tooltipColor.a * opacity
        );
        if (!definition.tooltipSpriteName.empty())
        {
            DrawSprite(
                definition.tooltipSpriteName,
                rect,
                backgroundColor,
                ResolveImageScaleMode(definition.tooltipScaleMode),
                &definition.tooltipNineSlice
            );
        }
        else
        {
            DrawQuad(device, rect, nullptr, backgroundColor);
        }

        const int padding = std::max(0, definition.tooltipPadding);
        text.rect = {
            rect.x + padding,
            rect.y + padding,
            std::max(0, rect.width - padding * 2),
            std::max(0, rect.height - padding * 2)
        };
        if (text.rect.width <= 0 || text.rect.height <= 0)
        {
            return;
        }
        const std::string slot = std::string(
            view.controller->PluginId()
        ) + ":tooltip:" + std::to_string(
            reinterpret_cast<std::uintptr_t>(&definition)
        ) + ":" + input.hoveredSnapshot.listName
            + ":" + std::to_string(input.hoveredSnapshot.listIndex)
            + ":" + std::to_string(input.hoveredSnapshot.listItemId);
        DrawTextureQuad(
            device,
            text.rect,
            textRenderer.Resolve(slot, text),
            ToD3DColor(1.0f, 1.0f, 1.0f, opacity)
        );
    }

    void DrawScrollbar(
        SessionView& view,
		const gui::GuiResolvedWidget& widget,
		D3DCOLOR color
    )
    {
		if (!widget.definition)
		{
			return;
		}
		const gui::WidgetDefinition& definition = *widget.definition;
        const std::string_view scrollbarName = definition.name;
        const GuiImageScaleMode scaleMode = ResolveImageScaleMode(
            definition.scaleMode
        );
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
                    layout.scrollbar,
                    color,
                    scaleMode,
					&definition.nineSlice,
					1,
					&widget.transform,
					&widget.rect
                );
                DrawSprite(
                    layout.scrollbarThumbSprite,
                    CalculateScrollbarThumb(layout),
                    color,
                    scaleMode,
					&definition.nineSlice,
					1,
					&widget.transform,
					&widget.rect
                );
            }
            return;
        }
    }

    void DrawSession(SessionView& view, uint64_t nowMilliseconds)
    {
		if (!view.animationClockStarted)
		{
			view.animationEpochMilliseconds = nowMilliseconds;
			view.animationClockStarted = true;
		}
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
        const gui::GuiCustomWidgetContext customContext{
            device,
            view.controller->Plugin().CustomWidgetContext(),
            &widgets,
            nullptr
        };
        for (const GuiRenderCommand& command : queue)
        {
            if (!command.widget || !command.widget->definition)
            {
                continue;
            }
			if (!ApplyWidgetClip(view, *command.widget))
            {
                continue;
            }
            const gui::WidgetDefinition& definition =
                *command.widget->definition;
			const gui::GuiRgbaColor effect = ResolveWidgetEffectMultiplier(
				view,
				*command.widget,
				nowMilliseconds
			);
			const auto effected = [&effect](D3DCOLOR color)
			{
				return ModulateColor(color, effect);
			};
            switch (command.type)
            {
            case GuiRenderCommandType::WindowFrame:
                DrawSprite(
                    definition.frameSpriteName,
                    command.widget->rect,
					effected(ToD3DColor(
						1.0f, 1.0f, 1.0f, command.widget->opacity
					)),
                    ResolveImageScaleMode(definition.scaleMode),
                    &definition.nineSlice
                );
                break;
            case GuiRenderCommandType::Image:
			{
				const std::string spriteName =
					view.controller->ResolveWidgetSprite(*command.widget);
                DrawSprite(
					spriteName,
                    command.widget->rect,
					effected(ToD3DColor(
						1.0f, 1.0f, 1.0f, command.widget->opacity
					)),
                    ResolveImageScaleMode(definition.scaleMode),
					&definition.nineSlice,
					ResolveWidgetFrame(
						view,
						*command.widget,
						spriteName,
						nowMilliseconds
					),
					&command.widget->transform,
					&command.widget->rect
                );
                break;
			}
            case GuiRenderCommandType::Button:
            {
                const bool pressed = view.controller->IsWidgetPressed(
                    *command.widget
                );
				const std::string spriteName =
					view.controller->ResolveWidgetSprite(
						*command.widget,
						pressed
					);
                const float brightness = 
                     command.widget->enabled
                     ? 1.0f
                     : definition.disabledBrightness;
                const float alpha =
                     command.widget->opacity
                     * (
                       command.widget->enabled
                       ? 1.0f
                       : definition.disabledOpacity
                       );
                DrawSprite(
					spriteName,
                    command.widget->rect,
					effected(ToD3DColor(
						brightness, brightness, brightness, alpha
					)),
                    ResolveImageScaleMode(definition.scaleMode),
					&definition.nineSlice,
					ResolveWidgetFrame(
						view,
						*command.widget,
						spriteName,
						nowMilliseconds
					),
					&command.widget->transform,
					&command.widget->rect
                );
                break;
            }
            case GuiRenderCommandType::ColorBox:
                DrawQuad(
                    device,
                    command.widget->rect,
                    nullptr,
					effected(ToD3DColor(
                        definition.textColor[0],
                        definition.textColor[1],
                        definition.textColor[2],
                        command.widget->opacity
					)),
					&command.widget->transform,
					&command.widget->rect
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
							view.controller->ResolveWidgetNumber(
								*command.widget,
								definition.valueSource,
								0.0
							)
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
				const std::string& textureFile =
					definition.progressColorIndex == 1
						? resource->textureFile2
						: resource->textureFile1;
				GuiD3D9Texture* progressTexture = textures.ResolveAsset(
					device,
					application.Root(),
					application.Interpreter(),
					textureFile
				);
				if (progressTexture && fill.width > 0 && fill.height > 0)
				{
					gui::GuiRect source{
						0,
						0,
						progressTexture->width,
						progressTexture->height
					};
					if (resource->horizontal)
					{
						source.width = value > 0.0f
							? std::max(
								1,
								static_cast<int>(
									progressTexture->width * value
								)
							)
							: 0;
						if (definition.fillFromEnd)
						{
							source.x = progressTexture->width
								- source.width;
						}
					}
					else
					{
						source.height = value > 0.0f
							? std::max(
								1,
								static_cast<int>(
									progressTexture->height * value
								)
							)
							: 0;
						if (definition.fillFromEnd)
						{
							source.y = progressTexture->height
								- source.height;
						}
					}
					DrawTextureRegion(
						device,
						fill,
						progressTexture->texture,
						source,
						progressTexture->width,
						progressTexture->height,
						effected(ToD3DColor(
							1.0f,
							1.0f,
							1.0f,
							command.widget->opacity
						)),
						&command.widget->transform,
						&command.widget->rect
					);
				}
				else
				{
					DrawQuad(
						device,
						fill,
						nullptr,
						effected(ToD3DColor(
							color[0],
							color[1],
							color[2],
							command.widget->opacity
						)),
						&command.widget->transform,
						&command.widget->rect
					);
				}
                break;
            }
            case GuiRenderCommandType::ScrollBar:
				DrawScrollbar(
					view,
					*command.widget,
					effected(ToD3DColor(
						1.0f, 1.0f, 1.0f, command.widget->opacity
					))
				);
                break;
            case GuiRenderCommandType::IndexedMap:
            {
				const D3DCOLOR mapColor = effected(ToD3DColor(
					1.0f, 1.0f, 1.0f, command.widget->opacity
				));
                GuiIndexedMapD3D9DrawLayers layers;
                if (view.indexedMaps.ResolveDrawLayers(
                        *command.widget,
                        layers
                    ))
                {
                    DrawTextureQuad(device, layers.rect, layers.base,mapColor);
                    DrawTextureQuad(device, layers.rect, layers.overlay,mapColor);
                    DrawTextureQuad(device, layers.rect, layers.boundary,mapColor);
                    DrawTextureQuad(device, layers.rect, layers.hover,mapColor);
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
                    textRenderer.Resolve(slot, text),
					effected(ToD3DColor(
						1.0f, 1.0f, 1.0f, command.widget->opacity
					)),
					&command.widget->transform,
					&command.widget->rect
                );
                break;
            }
            case GuiRenderCommandType::Custom:
                view.controller->CustomWidgets().DrawWidget(
                    *command.widget,
                    customContext
                );
                ConfigureOverlayState(device);
                break;
            }
        }
        DrawGenericTooltip(view, nowMilliseconds);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    }

    void TickAndRender(IDirect3DDevice9* nextDevice)
    {
        if (!initialized || !nextDevice || nextDevice != device)
        {
            return;
        }
		D3DVIEWPORT9 layoutViewport{};
		if (FAILED(device->GetViewport(&layoutViewport))
			|| layoutViewport.Width == 0
			|| layoutViewport.Height == 0)
		{
			return;
		}
		int requiredCanvasWidth = 1;
		int requiredCanvasHeight = 1;
		for (const auto& session : sessions)
		{
			UpdateRootClientLayout(*session, layoutViewport);
			requiredCanvasWidth = std::max(
				requiredCanvasWidth,
				session->rootCanvasRect.x
					+ session->rootCanvasRect.width
			);
			requiredCanvasHeight = std::max(
				requiredCanvasHeight,
				session->rootCanvasRect.y
					+ session->rootCanvasRect.height
			);
		}
		std::string canvasError;
		if (!EnsureCanvasSize(
				requiredCanvasWidth,
				requiredCanvasHeight,
				canvasError
			))
		{
			WriteGuiDiagnostic(
				"D3D9 GUI canvas resize failed: " + canvasError
			);
			return;
		}
        const uint64_t now = GetTickCount64();
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
            if ((!session->controller->IsOpen() || !visible)
                && (session->hasCapturedCustom
                    || session->hasHoveredCustom
                    || session->hasFocusedCustom))
            {
                const uint32_t modifiers = ResolveInputModifiers();
                CancelCustomCapture(*session, modifiers);
                UpdateCustomHover(
                    *session,
                    nullptr,
                    session->lastCanvasMouseX,
                    session->lastCanvasMouseY,
                    modifiers
                );
                UpdateCustomFocus(
                    *session,
                    nullptr,
                    session->lastCanvasMouseX,
                    session->lastCanvasMouseY,
                    modifiers
                );
            }
            if (!session->controller->IsOpen() || !visible)
            {
                session->tooltipHoverKey.clear();
                session->tooltipHoverStartedMilliseconds = 0;
            }
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
            DrawSession(*session, now);

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

    gui::GuiCustomWidgetContext CustomWidgetContext(
        SessionView& view
    ) const
    {
        return {
            device,
            view.controller->Plugin().CustomWidgetContext()
        };
    }

    bool HandleCustomEvent(
        SessionView& view,
        const gui::GuiResolvedWidget& widget,
        const gui::GuiCustomInputEvent& event
    )
    {
        return view.controller->CustomWidgets().HandleEvent(
            widget,
            CustomWidgetContext(view),
            event
        );
    }

    bool HandleGlobalCustomEvent(
        SessionView& view,
        const std::vector<gui::GuiResolvedWidget>& widgets,
        const gui::GuiCustomInputEvent& event
    )
    {
        std::vector<GuiActionEvent> emittedEvents;
        const gui::GuiCustomWidgetContext context{
            device,
            view.controller->Plugin().CustomWidgetContext(),
            &widgets,
            &emittedEvents
        };
        const bool handled =
            view.controller->CustomWidgets().HandleGlobalEvent(
                widgets,
                context,
                event
            );
        if (!emittedEvents.empty())
        {
            view.controller->DispatchEvents(
                emittedEvents,
                event.mouseX,
                event.mouseY
            );
        }
        return handled;
    }

    const gui::GuiResolvedWidget* ResolveCustomTarget(
        SessionView& view,
        const gui::GuiResolvedWidget* target
    ) const
    {
        return target
            && view.controller->CustomWidgets().CanHandle(*target)
            ? target
            : nullptr;
    }

    void UpdateCustomHover(
        SessionView& view,
        const gui::GuiResolvedWidget* target,
        int mouseX,
        int mouseY,
        uint32_t modifiers
    )
    {
        const gui::GuiResolvedWidget* customTarget =
            ResolveCustomTarget(view, target);
        if (view.hasHoveredCustom
            && customTarget
            && SameWidgetIdentity(
                view.hoveredCustom,
                *customTarget
            ))
        {
            return;
        }

        if (view.hasHoveredCustom)
        {
            gui::GuiCustomInputEvent leave;
            leave.type = gui::GuiCustomInputEventType::PointerLeave;
            leave.mouseX = mouseX;
            leave.mouseY = mouseY;
            leave.modifiers = modifiers;
            HandleCustomEvent(view, view.hoveredCustom, leave);
            view.hoveredCustom = {};
            view.hasHoveredCustom = false;
        }
        if (!customTarget)
        {
            return;
        }

        view.hoveredCustom = *customTarget;
        view.hasHoveredCustom = true;
        gui::GuiCustomInputEvent enter;
        enter.type = gui::GuiCustomInputEventType::PointerEnter;
        enter.mouseX = mouseX;
        enter.mouseY = mouseY;
        enter.modifiers = modifiers;
        HandleCustomEvent(view, view.hoveredCustom, enter);
    }

    void UpdateCustomFocus(
        SessionView& view,
        const gui::GuiResolvedWidget* target,
        int mouseX,
        int mouseY,
        uint32_t modifiers
    )
    {
        const gui::GuiResolvedWidget* customTarget =
            ResolveCustomTarget(view, target);
        if (view.hasFocusedCustom
            && customTarget
            && SameWidgetIdentity(
                view.focusedCustom,
                *customTarget
            ))
        {
            return;
        }

        if (view.hasFocusedCustom)
        {
            gui::GuiCustomInputEvent lost;
            lost.type = gui::GuiCustomInputEventType::FocusLost;
            lost.mouseX = mouseX;
            lost.mouseY = mouseY;
            lost.modifiers = modifiers;
            HandleCustomEvent(view, view.focusedCustom, lost);
            view.focusedCustom = {};
            view.hasFocusedCustom = false;
        }
        if (!customTarget)
        {
            return;
        }

        view.focusedCustom = *customTarget;
        view.hasFocusedCustom = true;
        gui::GuiCustomInputEvent gained;
        gained.type = gui::GuiCustomInputEventType::FocusGained;
        gained.mouseX = mouseX;
        gained.mouseY = mouseY;
        gained.modifiers = modifiers;
        HandleCustomEvent(view, view.focusedCustom, gained);
    }

    bool DispatchCustomPointerMove(
        SessionView& view,
        const gui::GuiResolvedWidget* target,
        int mouseX,
        int mouseY,
        uint32_t modifiers
    )
    {
        view.lastCanvasMouseX = mouseX;
        view.lastCanvasMouseY = mouseY;
        UpdateCustomHover(view, target, mouseX, mouseY, modifiers);
        const gui::GuiResolvedWidget* recipient =
            view.hasCapturedCustom
                ? &view.capturedCustom
                : ResolveCustomTarget(view, target);
        if (!recipient)
        {
            return false;
        }
        gui::GuiCustomInputEvent event;
        event.type = gui::GuiCustomInputEventType::PointerMove;
        event.mouseX = mouseX;
        event.mouseY = mouseY;
        event.modifiers = modifiers;
        return HandleCustomEvent(view, *recipient, event);
    }

    bool DispatchCustomPointerDown(
        SessionView& view,
        const gui::GuiResolvedWidget* target,
        gui::GuiCustomPointerButton button,
        int mouseX,
        int mouseY,
        uint32_t modifiers
    )
    {
        const gui::GuiResolvedWidget* customTarget =
            ResolveCustomTarget(view, target);
        UpdateCustomFocus(
            view,
            customTarget,
            mouseX,
            mouseY,
            modifiers
        );
        if (!customTarget)
        {
            return false;
        }

        gui::GuiCustomInputEvent event;
        event.type = gui::GuiCustomInputEventType::PointerDown;
        event.button = button;
        event.mouseX = mouseX;
        event.mouseY = mouseY;
        event.modifiers = modifiers;
        const bool handled = HandleCustomEvent(
            view,
            *customTarget,
            event
        );
        if (handled)
        {
            view.capturedCustom = *customTarget;
            view.capturedButton = button;
            view.hasCapturedCustom = true;
            if (targetWindow)
            {
                SetCapture(targetWindow);
            }
        }
        return handled;
    }

    bool DispatchCustomPointerUp(
        SessionView& view,
        const gui::GuiResolvedWidget* target,
        gui::GuiCustomPointerButton button,
        int mouseX,
        int mouseY,
        uint32_t modifiers
    )
    {
        const bool releasesCapture = view.hasCapturedCustom
            && view.capturedButton == button;
        const gui::GuiResolvedWidget* recipient = releasesCapture
            ? &view.capturedCustom
            : ResolveCustomTarget(view, target);
        bool handled = false;
        if (recipient)
        {
            gui::GuiCustomInputEvent event;
            event.type = gui::GuiCustomInputEventType::PointerUp;
            event.button = button;
            event.mouseX = mouseX;
            event.mouseY = mouseY;
            event.modifiers = modifiers;
            handled = HandleCustomEvent(view, *recipient, event);
        }
        if (releasesCapture)
        {
            view.capturedCustom = {};
            view.capturedButton = gui::GuiCustomPointerButton::None;
            view.hasCapturedCustom = false;
            if (targetWindow && GetCapture() == targetWindow)
            {
                ReleaseCapture();
            }
        }
        if (handled)
        {
            view.controller->RefreshData();
        }
        return handled;
    }

    bool DispatchCustomWheel(
        SessionView& view,
        const gui::GuiResolvedWidget* target,
        int mouseX,
        int mouseY,
        int delta,
        bool horizontal,
        uint32_t modifiers
    )
    {
        const gui::GuiResolvedWidget* customTarget =
            ResolveCustomTarget(view, target);
        if (!customTarget)
        {
            return false;
        }
        gui::GuiCustomInputEvent event;
        event.type = gui::GuiCustomInputEventType::PointerWheel;
        event.mouseX = mouseX;
        event.mouseY = mouseY;
        event.wheelDelta = delta;
        event.horizontalWheel = horizontal;
        event.modifiers = modifiers;
        return HandleCustomEvent(view, *customTarget, event);
    }

    bool DispatchCustomFocusedEvent(
        SessionView& view,
        gui::GuiCustomInputEvent event
    )
    {
        if (!view.hasFocusedCustom)
        {
            return false;
        }
        event.mouseX = view.lastCanvasMouseX;
        event.mouseY = view.lastCanvasMouseY;
        return HandleCustomEvent(view, view.focusedCustom, event);
    }

    bool CancelCustomCapture(
        SessionView& view,
        uint32_t modifiers
    )
    {
        if (!view.hasCapturedCustom)
        {
            return false;
        }
        gui::GuiCustomInputEvent event;
        event.type = gui::GuiCustomInputEventType::Cancel;
        event.button = view.capturedButton;
        event.mouseX = view.lastCanvasMouseX;
        event.mouseY = view.lastCanvasMouseY;
        event.modifiers = modifiers;
        const bool handled = HandleCustomEvent(
            view,
            view.capturedCustom,
            event
        );
        view.capturedCustom = {};
        view.capturedButton = gui::GuiCustomPointerButton::None;
        view.hasCapturedCustom = false;
        if (targetWindow && GetCapture() == targetWindow)
        {
            ReleaseCapture();
        }
        return handled;
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

        const uint32_t keyboardModifiers = ResolveInputModifiers();
        if (message == WM_KILLFOCUS
            || message == WM_SETFOCUS
            || message == WM_CANCELMODE
            || message == WM_CAPTURECHANGED)
        {
            for (SessionView* session : OrderedSessions(true))
            {
                SessionView& view = *session;
                if (message == WM_CANCELMODE
                    || message == WM_CAPTURECHANGED
                    || message == WM_KILLFOCUS)
                {
                    CancelCustomCapture(view, keyboardModifiers);
                }
                if (message == WM_KILLFOCUS)
                {
                    UpdateCustomHover(
                        view,
                        nullptr,
                        view.lastCanvasMouseX,
                        view.lastCanvasMouseY,
                        keyboardModifiers
                    );
                    UpdateCustomFocus(
                        view,
                        nullptr,
                        view.lastCanvasMouseX,
                        view.lastCanvasMouseY,
                        keyboardModifiers
                    );
                }
                else if (message == WM_SETFOCUS
                    && view.hasFocusedCustom)
                {
                    gui::GuiCustomInputEvent event;
                    event.type =
                        gui::GuiCustomInputEventType::FocusGained;
                    event.modifiers = keyboardModifiers;
                    DispatchCustomFocusedEvent(view, event);
                }
            }
            return false;
        }

        if (message == WM_KEYDOWN
            || message == WM_SYSKEYDOWN
            || message == WM_KEYUP
            || message == WM_SYSKEYUP
            || message == WM_CHAR)
        {
            for (SessionView* session : OrderedSessions(true))
            {
                SessionView& view = *session;
                if (!view.controller->IsOpen()
                    || !view.effectiveVisible
                    || !view.hasFocusedCustom)
                {
                    continue;
                }
                gui::GuiCustomInputEvent event;
                event.modifiers = keyboardModifiers;
                event.repeatCount = LOWORD(lParam);
                event.repeated = (lParam & (1LL << 30)) != 0;
                if (message == WM_CHAR)
                {
                    event.type =
                        gui::GuiCustomInputEventType::TextInput;
                    event.character = static_cast<uint32_t>(wParam);
                }
                else
                {
                    event.type = message == WM_KEYDOWN
                            || message == WM_SYSKEYDOWN
                        ? gui::GuiCustomInputEventType::KeyDown
                        : gui::GuiCustomInputEventType::KeyUp;
                    event.keyCode = static_cast<uint32_t>(wParam);
                }
                if (DispatchCustomFocusedEvent(view, event))
                {
                    return true;
                }
                break;
            }
            return false;
        }

        if (message == WM_MOUSELEAVE)
        {
            for (SessionView* session : OrderedSessions(true))
            {
                SessionView& view = *session;
                if (!view.controller->IsOpen()
                    || !view.effectiveVisible)
                {
                    continue;
                }
                std::vector<gui::GuiResolvedWidget> widgets =
                    view.InteractiveWidgets();
                gui::GuiCustomInputEvent globalLeave;
                globalLeave.type =
                    gui::GuiCustomInputEventType::PointerLeave;
                globalLeave.mouseX = -1;
                globalLeave.mouseY = -1;
                globalLeave.modifiers = keyboardModifiers;
                HandleGlobalCustomEvent(
                    view,
                    widgets,
                    globalLeave
                );
                view.indexedMaps.HandleMove(widgets, -1, -1);
                DispatchCustomPointerMove(
                    view,
                    nullptr,
                    -1,
                    -1,
                    keyboardModifiers
                );
                view.controller->DispatchMove(widgets, -1, -1);
            }
            return false;
        }

        if (message == WM_MOUSEMOVE && targetWindow)
        {
            TRACKMOUSEEVENT tracking{
                sizeof(TRACKMOUSEEVENT),
                TME_LEAVE,
                targetWindow,
                0
            };
            TrackMouseEvent(&tracking);
        }

        int mouseX = GET_X_LPARAM(lParam);
        int mouseY = GET_Y_LPARAM(lParam);
        if (message == WM_MOUSEWHEEL
            || message == WM_MOUSEHWHEEL)
        {
            POINT point{mouseX, mouseY};
            ScreenToClient(targetWindow, &point);
            mouseX = point.x;
            mouseY = point.y;
        }
        const int windowMouseX = mouseX;
        const int windowMouseY = mouseY;
        const uint32_t mouseModifiers = ResolveInputModifiers(wParam);

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
				SetWindowOffset(
					view,
					view.dragOffsetX
						+ windowMouseX - view.dragMouseX,
					view.dragOffsetY
						+ windowMouseY - view.dragMouseY
				);
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
                    gui::GuiCustomInputEvent globalLeave;
                    globalLeave.type =
                        gui::GuiCustomInputEventType::PointerLeave;
                    globalLeave.mouseX = -1;
                    globalLeave.mouseY = -1;
                    globalLeave.modifiers = mouseModifiers;
                    HandleGlobalCustomEvent(
                        view,
                        widgets,
                        globalLeave
                    );
                    view.indexedMaps.HandleMove(widgets, -1, -1);
                    DispatchCustomPointerMove(
                        view,
                        nullptr,
                        -1,
                        -1,
                        mouseModifiers
                    );
                    view.controller->DispatchMove(widgets, -1, -1);
                }
                else if (IsPointerUpMessage(message))
                {
                    std::vector<gui::GuiResolvedWidget> widgets =
                        view.InteractiveWidgets();
                    bool pressedWasInteractive = false;
                    if (message == WM_LBUTTONUP)
                    {
                        gui::GuiCustomInputEvent globalRelease;
                        globalRelease.type =
                            gui::GuiCustomInputEventType::PointerUp;
                        globalRelease.button =
                            gui::GuiCustomPointerButton::Left;
                        globalRelease.mouseX = -1;
                        globalRelease.mouseY = -1;
                        globalRelease.modifiers = mouseModifiers;
                        if (HandleGlobalCustomEvent(
                                view,
                                widgets,
                                globalRelease
                            ))
                        {
                            return true;
                        }
                        const GuiRuntimeInputState& inputState =
                            view.controller->InputState();
                        pressedWasInteractive =
                            !inputState.pressedKey.empty();
                        view.indexedMaps.HandleRelease(
                            widgets,
                            -1,
                            -1
                        );
                        view.controller->DispatchRelease(
                            widgets,
                            -1,
                            -1
                        );
                    }
                    const bool customHandled =
                        DispatchCustomPointerUp(
                            view,
                            nullptr,
                            ResolvePointerButton(message, wParam),
                            -1,
                            -1,
                            mouseModifiers
                        );
                    if (customHandled || pressedWasInteractive)
                    {
                        return true;
                    }
                }
                continue;
            }
            if (IsPointerDownMessage(message))
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
            if (IsPointerDownMessage(message))
            {
                UpdateCustomFocus(
                    view,
                    target,
                    canvasMouseX,
                    canvasMouseY,
                    mouseModifiers
                );
            }
            if (message == WM_MOUSEMOVE)
            {
                gui::GuiCustomInputEvent globalMove;
                globalMove.type =
                    gui::GuiCustomInputEventType::PointerMove;
                globalMove.mouseX = canvasMouseX;
                globalMove.mouseY = canvasMouseY;
                globalMove.modifiers = mouseModifiers;
                if (HandleGlobalCustomEvent(
                        view,
                        widgets,
                        globalMove
                    ))
                {
                    view.indexedMaps.HandleMove(widgets, -1, -1);
                    DispatchCustomPointerMove(
                        view,
                        nullptr,
                        -1,
                        -1,
                        mouseModifiers
                    );
                    view.controller->DispatchMove(widgets, -1, -1);
                    return true;
                }
                view.indexedMaps.HandleMove(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                DispatchCustomPointerMove(
                    view,
                    target,
                    canvasMouseX,
                    canvasMouseY,
                    mouseModifiers
                );
                view.controller->DispatchMove(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                continue;
            }
            if (message == WM_MOUSEWHEEL
                || message == WM_MOUSEHWHEEL)
            {
                const int wheelDelta =
                    GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                if (DispatchCustomWheel(
                        view,
                        target,
                        canvasMouseX,
                        canvasMouseY,
                        wheelDelta,
                        message == WM_MOUSEHWHEEL,
                        mouseModifiers
                    ))
                {
                    return true;
                }
                if (message == WM_MOUSEWHEEL
                    && view.controller->ScrollListAt(
                        canvasMouseX,
                        canvasMouseY,
                        -wheelDelta
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
                gui::GuiCustomInputEvent globalPress;
                globalPress.type =
                    gui::GuiCustomInputEventType::PointerDown;
                globalPress.button =
                    gui::GuiCustomPointerButton::Left;
                globalPress.mouseX = canvasMouseX;
                globalPress.mouseY = canvasMouseY;
                globalPress.modifiers = mouseModifiers;
                if (HandleGlobalCustomEvent(
                        view,
                        widgets,
                        globalPress
                    ))
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
                const bool customHandled =
                    DispatchCustomPointerDown(
                        view,
                        target,
                        gui::GuiCustomPointerButton::Left,
                        canvasMouseX,
                        canvasMouseY,
                        mouseModifiers
                    );
                if (customHandled || IsInteractiveTarget(target))
                {
                    return true;
                }
            }
            if (IsPointerDownMessage(message)
                && message != WM_LBUTTONDOWN)
            {
                if (DispatchCustomPointerDown(
                        view,
                        target,
                        ResolvePointerButton(message, wParam),
                        canvasMouseX,
                        canvasMouseY,
                        mouseModifiers
                    ))
                {
                    return true;
                }
                continue;
            }
            if (message == WM_LBUTTONUP)
            {
                const GuiRuntimeInputState& inputState =
                    view.controller->InputState();
                const bool pressedWasInteractive =
                    !inputState.pressedKey.empty();
                gui::GuiCustomInputEvent globalRelease;
                globalRelease.type =
                    gui::GuiCustomInputEventType::PointerUp;
                globalRelease.button =
                    gui::GuiCustomPointerButton::Left;
                globalRelease.mouseX = canvasMouseX;
                globalRelease.mouseY = canvasMouseY;
                globalRelease.modifiers = mouseModifiers;
                if (HandleGlobalCustomEvent(
                        view,
                        widgets,
                        globalRelease
                    ))
                {
                    return true;
                }
                view.indexedMaps.HandleRelease(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                view.controller->DispatchRelease(
                    widgets,
                    canvasMouseX,
                    canvasMouseY
                );
                const bool customHandled =
                    DispatchCustomPointerUp(
                        view,
                        target,
                        gui::GuiCustomPointerButton::Left,
                        canvasMouseX,
                        canvasMouseY,
                        mouseModifiers
                    );
                if (customHandled
                    || pressedWasInteractive
                    || IsInteractiveTarget(target))
                {
                    return true;
                }
            }
            if (IsPointerUpMessage(message)
                && message != WM_LBUTTONUP)
            {
                if (DispatchCustomPointerUp(
                        view,
                        target,
                        ResolvePointerButton(message, wParam),
                        canvasMouseX,
                        canvasMouseY,
                        mouseModifiers
                    ))
                {
                    return true;
                }
                continue;
            }
        }
        if (windowManager.HasActiveModal()
            && (message == WM_LBUTTONDOWN
                || message == WM_LBUTTONUP
                || message == WM_RBUTTONDOWN
                || message == WM_RBUTTONUP
                || message == WM_MBUTTONDOWN
                || message == WM_MBUTTONUP
                || message == WM_XBUTTONDOWN
                || message == WM_XBUTTONUP
                || message == WM_MOUSEWHEEL
                || message == WM_MOUSEHWHEEL))
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
