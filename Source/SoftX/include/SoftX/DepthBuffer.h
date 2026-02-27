#pragma once
#include <vector>
#include <algorithm>
#include <limits>
#include <cassert>

#include "LibInternal.h"
#include "Math.h"

SOFTX_BEGIN
class SOFTX_API DepthBuffer
{
  public:
	DepthBuffer(int2 size)
		: m_width(size.x), m_height(size.y), m_depths(size.x * size.y, 1.0f) // по умолчанию 1.0 (дальнее)
	{
	}

	// ќчистка заданным значением глубины
	void clear(float depth)
	{
		float* data = m_depths.data();
		size_t count = m_depths.size();

		__m128 depth4 = _mm_set1_ps(depth);
		size_t i = 0;

		// ѕотокова€ запись (минует кэш) Ц максимальна€ скорость,
		// но требует выравнивани€ адреса по 16 байт.
		for (; i + 4 <= count; i += 4)
		{
			_mm_stream_ps(data + i, depth4);
		}
		_mm_sfence(); // гарантирует видимость записей

		// ќстаток (менее 4 элементов)
		for (; i < count; ++i)
		{
			data[i] = depth;
		}
	}


	// „тение глубины по координатам
	float read(int2 coords) const
	{
		if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
		{
			return m_depths[coords.y * m_width + coords.x];
		}
		return 1.0f; // за границами возвращаем дальнее
	}

	// «апись глубины по координатам (без проверки, дл€ внутреннего использовани€)
	void write(int2 coords, float depth)
	{
		if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
		{
			m_depths[coords.y * m_width + coords.x] = depth;
		}
	}

	// ѕр€мой доступ к данным (дл€ быстрой пакетной записи)
	float* data()
	{
		return m_depths.data();
	}
	const float* data() const
	{
		return m_depths.data();
	}

	float& at(int2 coords)
	{
		assert(coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height);
		return m_depths[coords.y * m_width + coords.x];
	}

	const float& at(int2 coords) const
	{
		assert(coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height);
		return m_depths[coords.y * m_width + coords.x];
	}

	float& at(int index)
	{
		assert(index >= 0 && index < (int)m_depths.size());
		return m_depths[index];
	}

	const float& at(int index) const
	{
		assert(index >= 0 && index < (int)m_depths.size());
		return m_depths[index];
	}

	// –азмеры
	int width() const
	{
		return m_width;
	}
	int height() const
	{
		return m_height;
	}
	int2 size() const
	{
		return int2(m_width, m_height);
	}

  private:
	int m_width, m_height;
	std::vector<float> m_depths;
};
SOFTX_END
