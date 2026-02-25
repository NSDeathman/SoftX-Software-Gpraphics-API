#pragma once
#include <vector>
#include <algorithm>
#include <limits>
#include "MicroMath.h"
#include <cassert>

class DepthBuffer {
public:
    DepthBuffer(int2 size)
        : m_width(size.x), m_height(size.y), m_depths(size.x * size.y, 1.0f) // по умолчанию 1.0 (дальнее)
    {
    }

    // Очистка заданным значением глубины (по умолчанию 1.0)
    void clear(float depth = 1.0f) {
        std::fill(m_depths.begin(), m_depths.end(), depth);
    }

    // Чтение глубины по координатам
    float read(int2 coords) const {
        if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height) {
            return m_depths[coords.y * m_width + coords.x];
        }
        return 1.0f; // за границами возвращаем дальнее
    }

    // Запись глубины по координатам (без проверки, для внутреннего использования)
    void write(int2 coords, float depth) {
        if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height) {
            m_depths[coords.y * m_width + coords.x] = depth;
        }
    }

    // Прямой доступ к данным (для быстрой пакетной записи)
    float* data() { return m_depths.data(); }
    const float* data() const { return m_depths.data(); }

    float& at(int2 coords) {
        assert(coords.x >= 0 && coords.x < m_width&& coords.y >= 0 && coords.y < m_height);
        return m_depths[coords.y * m_width + coords.x];
    }

    const float& at(int2 coords) const {
        assert(coords.x >= 0 && coords.x < m_width&& coords.y >= 0 && coords.y < m_height);
        return m_depths[coords.y * m_width + coords.x];
    }

    float& at(int index) {
        assert(index >= 0 && index < (int)m_depths.size());
        return m_depths[index];
    }

    const float& at(int index) const {
        assert(index >= 0 && index < (int)m_depths.size());
        return m_depths[index];
    }

    // Размеры
    int width() const { return m_width; }
    int height() const { return m_height; }
    int2 size() const { return int2(m_width, m_height); }

private:
    int m_width, m_height;
    std::vector<float> m_depths;
};
