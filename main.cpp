#include <SDL.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Face {
    std::vector<int> indices;
};

struct Color {
    unsigned char r, g, b, a;
};

class Framebuffer {
public:
    Framebuffer(int ancho, int alto) : ancho(ancho), alto(alto), pixeles(ancho * alto * 4, 0) {}

    void limpiar(Color color) {
        for (int y = 0; y < alto; ++y) {
            for (int x = 0; x < ancho; ++x) ponerPixel(x, y, color);
        }
    }

    void ponerPixel(int x, int y, Color color) {
        if (x < 0 || x >= ancho || y < 0 || y >= alto) return;
        const std::size_t i = static_cast<std::size_t>((y * ancho + x) * 4);
        pixeles[i] = color.r;
        pixeles[i + 1] = color.g;
        pixeles[i + 2] = color.b;
        pixeles[i + 3] = color.a;
    }

    int ancho;
    int alto;
    std::vector<unsigned char> pixeles;
};

bool loadOBJ(const std::string& path, std::vector<glm::vec3>& out_vertices, std::vector<Face>& out_faces) {
    std::ifstream archivo(path);
    if (!archivo) return false;

    std::string linea;
    while (std::getline(archivo, linea)) {
        std::istringstream entrada(linea);
        std::string tipo;
        entrada >> tipo;
        if (tipo == "v") {
            glm::vec3 vertice{};
            if (entrada >> vertice.x >> vertice.y >> vertice.z) out_vertices.push_back(vertice);
        } else if (tipo == "f") {
            Face cara;
            std::string token;
            while (entrada >> token) {
                const std::size_t separador = token.find('/');
                const std::string indice_texto = token.substr(0, separador);
                if (indice_texto.empty()) continue;
                int indice = std::stoi(indice_texto);
                if (indice < 0) indice = static_cast<int>(out_vertices.size()) + indice;
                else --indice;
                cara.indices.push_back(indice);
            }
            if (cara.indices.size() >= 3) out_faces.push_back(cara);
        }
    }
    return !out_vertices.empty() && !out_faces.empty();
}

void dibujarLinea(Framebuffer& buffer, int x0, int y0, int x1, int y1, Color color) {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        buffer.ponerPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int doble = 2 * error;
        if (doble >= dy) { error += dy; x0 += sx; }
        if (doble <= dx) { error += dx; y0 += sy; }
    }
}

void triangle(Framebuffer& buffer, int ax, int ay, int bx, int by, int cx, int cy, Color color) {
    dibujarLinea(buffer, ax, ay, bx, by, color);
    dibujarLinea(buffer, bx, by, cx, cy, color);
    dibujarLinea(buffer, cx, cy, ax, ay, color);
}

int main(int argc, char** argv) {
    const std::string ruta_obj = argc > 1 ? argv[1] : "nave_espacial.obj";
    std::vector<glm::vec3> vertices;
    std::vector<Face> caras;
    if (!loadOBJ(ruta_obj, vertices, caras)) {
        std::cerr << "No se pudo cargar el modelo OBJ: " << ruta_obj << '\n';
        return 1;
    }

    constexpr int ancho = 1200, alto = 900;
    Framebuffer framebuffer(ancho, alto);
    framebuffer.limpiar({18, 24, 38, 255});

    float min_x = vertices[0].x, max_x = vertices[0].x;
    float min_y = vertices[0].y, max_y = vertices[0].y;
    float min_z = vertices[0].z, max_z = vertices[0].z;
    for (const glm::vec3& v : vertices) {
        min_x = std::min(min_x, v.x); max_x = std::max(max_x, v.x);
        min_y = std::min(min_y, v.y); max_y = std::max(max_y, v.y);
        min_z = std::min(min_z, v.z); max_z = std::max(max_z, v.z);
    }
    const float centro_x = (min_x + max_x) * 0.5f;
    const float centro_y = (min_y + max_y) * 0.5f;
    const float centro_z = (min_z + max_z) * 0.5f;
    const float escala = std::min((ancho * 0.72f) / (max_x - min_x), (alto * 0.72f) / (max_z - min_z));
    const float angulo = 0.52f;
    const float coseno = std::cos(angulo), seno = std::sin(angulo);
    std::vector<std::pair<int, int>> pantalla;
    pantalla.reserve(vertices.size());
    for (const glm::vec3& v : vertices) {
        const float x = v.x - centro_x;
        const float y = v.y - centro_y;
        const float z = v.z - centro_z;
        const float x_rotado = x * coseno - y * seno;
        const float profundidad = x * seno + y * coseno;
        const float y_proyectado = z * 0.86f - profundidad * 0.50f;
        pantalla.emplace_back(static_cast<int>(ancho * 0.5f + x_rotado * escala),
                              static_cast<int>(alto * 0.5f - y_proyectado * escala));
    }

    const Color amarillo{255, 220, 72, 255};
    std::size_t triangulos = 0;
    for (const Face& cara : caras) {
        for (std::size_t i = 1; i + 1 < cara.indices.size(); ++i) {
            const int ia = cara.indices[0], ib = cara.indices[i], ic = cara.indices[i + 1];
            if (ia < 0 || ib < 0 || ic < 0 || ia >= static_cast<int>(pantalla.size()) ||
                ib >= static_cast<int>(pantalla.size()) || ic >= static_cast<int>(pantalla.size())) continue;
            triangle(framebuffer, pantalla[ia].first, pantalla[ia].second,
                     pantalla[ib].first, pantalla[ib].second,
                     pantalla[ic].first, pantalla[ic].second, amarillo);
            ++triangulos;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* ventana = SDL_CreateWindow("Lab 4 - Carga de modelos", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, ancho, alto, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderizador = ventana ? SDL_CreateRenderer(ventana, -1, SDL_RENDERER_SOFTWARE) : nullptr;
    SDL_Surface* superficie = SDL_CreateRGBSurfaceWithFormatFrom(framebuffer.pixeles.data(), ancho, alto,
        32, ancho * 4, SDL_PIXELFORMAT_RGBA32);
    if (!ventana || !renderizador || !superficie || SDL_SaveBMP(superficie, "captura_render.bmp") != 0) {
        std::cerr << "Error SDL: " << SDL_GetError() << '\n';
        if (superficie) SDL_FreeSurface(superficie);
        if (renderizador) SDL_DestroyRenderer(renderizador);
        if (ventana) SDL_DestroyWindow(ventana);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* textura = SDL_CreateTextureFromSurface(renderizador, superficie);
    SDL_RenderClear(renderizador);
    SDL_RenderCopy(renderizador, textura, nullptr, nullptr);
    SDL_RenderPresent(renderizador);
    std::cout << "Vertices: " << vertices.size() << ", caras: " << caras.size()
              << ", triangulos dibujados: " << triangulos << '\n';

    const bool solo_imagen = std::getenv("SDL_VIDEODRIVER") && std::string(std::getenv("SDL_VIDEODRIVER")) == "dummy";
    if (!solo_imagen) {
        bool salir = false;
        SDL_Event evento;
        while (!salir) {
            while (SDL_PollEvent(&evento)) if (evento.type == SDL_QUIT || evento.type == SDL_KEYDOWN) salir = true;
            SDL_Delay(16);
        }
    }

    SDL_DestroyTexture(textura);
    SDL_FreeSurface(superficie);
    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
    return 0;
}
