#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include <imgui_internal.h>
#include "imgui_node_editor.h"
#include "imgui_extra_math.h"
#include "NodeEditorUtils.h"

// from https://github.com/thedmd/imgui-node-editor/issues/101
namespace Tools::NodeEditor {
    namespace ed = ax::NodeEditor;

    static float CalcMaxPopupHeightFromItemCount(int items_count) {
        ImGuiContext& g = *GImGui;
        if (items_count <= 0)
            return FLT_MAX;
        return (g.FontSize + g.Style.ItemSpacing.y) * items_count - g.Style.ItemSpacing.y +
               (g.Style.WindowPadding.y * 2);
    }

    bool BeginNodeCombo(const char *label, const char *preview_value, ImGuiComboFlags flags) {
        using namespace ImGui;

        // Always consume the SetNextWindowSizeConstraint() call in our early return paths
        ImGuiContext& g = *GImGui;
        bool has_window_size_constraint = (g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint) != 0;
        g.NextWindowData.Flags &= ~ImGuiNextWindowDataFlags_HasSizeConstraint;

        ImGuiWindow *window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) !=
                  (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const float expected_w = CalcItemWidth();
        const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : expected_w;
        const ImRect frame_bb(window->DC.CursorPos,
                              window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
        const ImRect total_bb(frame_bb.Min, frame_bb.Max +
                                            ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f,
                                                   0.0f));
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id, &frame_bb))
            return false;

        bool hovered, held;
        bool pressed = ButtonBehavior(frame_bb, id, &hovered, &held);
        bool popup_open = IsPopupOpen(id, ImGuiPopupFlags_None);

        const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
        const float value_x2 = ImMax(frame_bb.Min.x, frame_bb.Max.x - arrow_size);
        RenderNavHighlight(frame_bb, id);
        if (!(flags & ImGuiComboFlags_NoPreview))
            window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(value_x2, frame_bb.Max.y), frame_col,
                                            style.FrameRounding,
                                            (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawCornerFlags_All
                                                                                    : ImDrawCornerFlags_Left);
        if (!(flags & ImGuiComboFlags_NoArrowButton)) {
            ImU32 bg_col = GetColorU32((popup_open || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            ImU32 text_col = GetColorU32(ImGuiCol_Text);
            window->DrawList->AddRectFilled(ImVec2(value_x2, frame_bb.Min.y), frame_bb.Max, bg_col, style.FrameRounding,
                                            (w <= arrow_size) ? ImDrawCornerFlags_All : ImDrawCornerFlags_Right);
            if (value_x2 + arrow_size - style.FramePadding.x <= frame_bb.Max.x)
                RenderArrow(window->DrawList,
                            ImVec2(value_x2 + style.FramePadding.y, frame_bb.Min.y + style.FramePadding.y), text_col,
                            ImGuiDir_Down, 1.0f);
        }
        RenderFrameBorder(frame_bb.Min, frame_bb.Max, style.FrameRounding);
        if (preview_value != NULL && !(flags & ImGuiComboFlags_NoPreview))
            RenderTextClipped(frame_bb.Min + style.FramePadding, ImVec2(value_x2, frame_bb.Max.y), preview_value, NULL,
                              NULL, ImVec2(0.0f, 0.0f));
        if (label_size.x > 0)
            RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

        if ((pressed || g.NavActivateId == id) && !popup_open) {
            if (window->DC.NavLayerCurrent == 0)
                window->NavLastIds[0] = id;
            OpenPopupEx(id, ImGuiPopupFlags_None);
            popup_open = true;
        }

        if (!popup_open)
            return false;

        if (has_window_size_constraint) {
            g.NextWindowData.Flags |= ImGuiNextWindowDataFlags_HasSizeConstraint;
            g.NextWindowData.SizeConstraintRect.Min.x = ImMax(g.NextWindowData.SizeConstraintRect.Min.x, w);
        } else {
            if ((flags & ImGuiComboFlags_HeightMask_) == 0)
                flags |= ImGuiComboFlags_HeightRegular;
            IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_));    // Only one
            int popup_max_height_in_items = -1;
            if (flags & ImGuiComboFlags_HeightRegular) popup_max_height_in_items = 8;
            else if (flags & ImGuiComboFlags_HeightSmall) popup_max_height_in_items = 4;
            else if (flags & ImGuiComboFlags_HeightLarge) popup_max_height_in_items = 20;
            SetNextWindowSizeConstraints(ImVec2(w, 0.0f),
                                         ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items)));
        }

        char name[16];
        ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d",
                       g.BeginPopupStack.Size); // Recycle windows based on depth

        // Position the window given a custom constraint (peak into expected window size so we can position it)
        // This might be easier to express with an hypothetical SetNextWindowPosConstraints() function.
        if (ImGuiWindow * popup_window = FindWindowByName(name))
            if (popup_window->WasActive) {
                // Always override 'AutoPosLastDirection' to not leave a chance for a past value to affect us.
                ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
                if (flags & ImGuiComboFlags_PopupAlignLeft)
                    popup_window->AutoPosLastDirection = ImGuiDir_Left; // "Below, Toward Left"
                else
                    popup_window->AutoPosLastDirection = ImGuiDir_Down; // "Below, Toward Right (default)"
                ImRect r_outer = GetWindowAllowedExtentRect(popup_window);
                ImVec2 pos = FindBestWindowPosForPopupEx(frame_bb.GetBL(), size_expected,
                                                         &popup_window->AutoPosLastDirection, r_outer, frame_bb,
                                                         ImGuiPopupPositionPolicy_ComboBox);
                SetNextWindowPos(ed::CanvasToScreen(pos));
            }

        // We don't use BeginPopupEx() solely because we have a custom name string, which we could make an argument to BeginPopupEx()
        ImGuiWindowFlags window_flags =
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

        ed::Suspend();

        // Horizontally align ourselves with the framed text
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
        bool ret = Begin(name, NULL, window_flags);
        PopStyleVar();
        if (!ret) {
            EndPopup();
            IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
            return false;
        }
        return true;
    }

    void EndNodeCombo() {
        ImGui::EndPopup();

        ed::Resume();
    }
}