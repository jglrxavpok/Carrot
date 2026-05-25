//
// Created by jglrxavpok on 18/05/2026.
//

#pragma once

#include <panels/EditorPanel.h>

namespace Peeler {
    class ViewportEditionPanel: public EditorPanel {
    public:
        ViewportEditionPanel(Peeler::Application& editor);

        void draw(const Carrot::Render::Context& renderContext) override;

        i32 getSelectedCompositionIndex() const;

        void loadCompositionsFromVFS();
        void saveCompositions();

    public:
        bool showViewportSettings = false;
        std::optional<i32> compositionRefresh; // Refresh currently used viewport composition when this is set (set to nullopt once done)
        std::string currentCompositionID;
        std::unordered_map<std::string, Carrot::Identifier> composition2selectedViewport;
        Carrot::Vector<Carrot::Pair<std::string, Carrot::Render::ViewportComposition>> compositions;
    };
}