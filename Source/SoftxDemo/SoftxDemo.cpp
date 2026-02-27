#define SOFTX_STATIC
#include <Windows.h>
#include <SoftX/SoftX.h>

#pragma comment(lib, "SoftX.lib")

using namespace SoftX;

// Глобальный флаг для выхода из цикла
bool g_running = true;

// Структура для константного буфера (матрица world-view-projection)
struct TransformCB {
    float4x4 wvp;
};

// Оконная процедура
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Вершинный шейдер – умножает позицию на матрицу wvp
VertexOutput vsTransform(const VertexInput& in, ConstantBuffer cb) {
    const TransformCB* transform = static_cast<const TransformCB*>(cb.Data());
    VertexOutput out;
    out.Position = transform->wvp * float4(in.Position.x, in.Position.y, in.Position.z, 1.0f);
    out.Color = in.Color;
    out.UV = in.UV;
    return out;
}

// Пиксельный шейдер – возвращает интерполированный цвет
float4 psColor(const VertexOutput& in, ConstantBuffer /*cb*/) {
    return in.Color;
}

// Вспомогательная функция для создания куба (24 вершины, 36 индексов)
void CreateCube(VertexBuffer& vb, IndexBuffer& ib) {
    // Координаты вершин куба (от -1 до 1)
    float3 corners[8] = {
        float3(-1, -1, -1), float3(1, -1, -1), float3(1,  1, -1), float3(-1,  1, -1),
        float3(-1, -1,  1), float3(1, -1,  1), float3(1,  1,  1), float3(-1,  1,  1)
    };

    // Цвета для каждой грани (R, G, B, A)
    float4 colors[6] = {
        float4(1, 0, 0, 1), // +X (красный)
        float4(0, 1, 0, 1), // -X (зелёный)
        float4(0, 0, 1, 1), // +Y (синий)
        float4(1, 1, 0, 1), // -Y (жёлтый)
        float4(0, 1, 1, 1), // +Z (голубой)
        float4(1, 0, 1, 1)  // -Z (фиолетовый)
    };

    // Определяем грани: для каждой грани задаём 4 индекса из corners[]
    int faces[6][4] = {
        {1, 5, 6, 2}, // +X
        {0, 3, 7, 4}, // -X
        {3, 2, 6, 7}, // +Y
        {0, 4, 5, 1}, // -Y
        {4, 7, 6, 5}, // +Z
        {0, 1, 2, 3}  // -Z
    };

    for (int f = 0; f < 6; ++f) {
        int idx[4] = { faces[f][0], faces[f][1], faces[f][2], faces[f][3] };
        float4 col = colors[f];

        int start = (int)vb.Size(); // запоминаем текущее количество вершин
        vb.Add({ corners[idx[0]], col, float2(0, 0) });
        vb.Add({ corners[idx[1]], col, float2(1, 0) });
        vb.Add({ corners[idx[2]], col, float2(1, 1) });
        vb.Add({ corners[idx[3]], col, float2(0, 1) });

        // Два треугольника (0-1-2 и 0-2-3)
        ib.Add(start);
        ib.Add(start + 1);
        ib.Add(start + 2);
        ib.Add(start);
        ib.Add(start + 2);
        ib.Add(start + 3);
    }
}

int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Регистрация класса окна
    const wchar_t CLASS_NAME[] = L"SoftX3DTest";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // Создание окна фиксированного размера 800x600
    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"SoftX 3D Test",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return -1;
    ShowWindow(hWnd, SW_SHOW);

    // Параметры устройства
    PresentParameters pp;
    pp.BackBufferSize = int2(800, 600);
    pp.hDeviceWindow = hWnd;
    pp.Windowed = true;

    // Создание устройства
    Device device(pp);

    // Создание данных куба
    VertexBuffer vb;
    IndexBuffer ib;
    CreateCube(vb, ib);

    // Установка шейдеров и буферов
    device.SetVertexShader(vsTransform);
    device.SetPixelShader(psColor);
    device.SetVertexBuffer(vb);
    device.SetIndexBuffer(ib);

    // Переменные для анимации
    float angle = 0.0f;
    LARGE_INTEGER freq, lastTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    // Главный цикл рендеринга
    MSG msg = {};
    while (g_running) {
        // Обработка сообщений
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Вычисление времени кадра
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        float deltaTime = float(double(currentTime.QuadPart - lastTime.QuadPart) / freq.QuadPart);
        lastTime = currentTime;
        angle += deltaTime * 0.5f; // вращение ~0.5 радиан в секунду

        // Матрицы
        float3 eye(0, 0, -50);
        float3 target(0, 0, 0);
        float3 up(0, 1, 0);
        float4x4 view = lookAtLH(eye, target, up);
        float4x4 model = rotationY(angle) * rotationX(angle * 0.3f);
        float aspect = 800.0f / 600.0f;
        float4x4 proj = perspectiveLH(3.14159f / 4.0f, aspect, 0.1f, 100.0f);
        TransformCB cb;
        cb.wvp = proj * view * model;
        ConstantBuffer cbuffer(&cb, sizeof(cb));
        device.SetConstantBuffer(cbuffer);

        // Очистка буферов
        device.Clear(float4(0.2f, 0.2f, 0.2f, 1.0f));
        device.ClearDepth(1.0f);

        // Рендеринг куба
        device.DrawIndexed(); // использует все индексы

        // Вывод на экран
        device.Present();
    }

    return 0;
}
