//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Math {
    template<typename Scalar>
    class Rect2D {
    public:
        using Vec2Type = glm::vec<2, Scalar, glm::highp>;

        Rect2D() = default;
        Rect2D(const Rect2D<Scalar>& toCopy) = default;
        Rect2D(Rect2D<Scalar>&& toMove) = default;
        explicit Rect2D(Scalar minX, Scalar minY, Scalar maxX, Scalar maxY): center((minX+maxX)/2, (minY+maxY)/2), halfSize((maxX-minX)/2, (maxY-minY)/2) {}
        explicit Rect2D(Vec2Type center, Vec2Type halfSize): center(center), halfSize(halfSize) {}

    public:
        Scalar getMinX() const {
            return center.x - halfSize.x;
        }

        Scalar getMaxX() const {
            return center.x + halfSize.x;
        }

        Scalar getMinY() const {
            return center.y - halfSize.y;
        }

        Scalar getMaxY() const {
            return center.y + halfSize.y;
        }

        Vec2Type& getCenter() { return center; }
        const Vec2Type& getCenter() const { return center; }

        Vec2Type& getHalfSize() { return halfSize; }
        const Vec2Type& getHalfSize() const { return halfSize; }

        [[nodiscard]] Vec2Type getSize() { return halfSize * ((Scalar)2); }

    public:
        bool intersects(const Rect2D<Scalar>& other) const {
            return !(
                    getMaxX() < other.getMinX()
                    || getMaxY() < other.getMinY()
                    || getMinX() > other.getMaxX()
                    || getMinY() > other.getMaxY()
            );
        }

        bool isIn(const Vec2Type& point) const {
            return point.x >= getMinX() && point.x <= getMaxX() && point.y >= getMinY() && point.y <= getMaxY();
        }

    public:
        Rect2D<Scalar>& operator=(const Rect2D<Scalar>& other) {
            center = other.center;
            halfSize = other.halfSize;
            return *this;
        }

    private:
        Vec2Type center{0};
        Vec2Type halfSize{0};
    };

    using Rect2Df = Rect2D<float>;
    using Rect2Dd = Rect2D<double>;
    using Rect2Di = Rect2D<std::int32_t>;
    using Rect2Dui = Rect2D<std::uint32_t>;
}
