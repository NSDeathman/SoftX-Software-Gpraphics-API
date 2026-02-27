#pragma once

#include "Math.h"
#include "LibInternal.h"

SOFTX_BEGIN

class SOFTX_API IRenderTarget
{
  public:
	virtual ~IRenderTarget() = default;

	// Очистка цветом
	virtual void clear(const float4& color) = 0;

	// Установка пикселя
	virtual void set_pixel(int2 coords, const float4& color) = 0;

	// Размеры
	virtual int width() const = 0;
	virtual int height() const = 0;
	virtual int2 size() const = 0;
};

SOFTX_END
