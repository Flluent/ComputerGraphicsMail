#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <string>

// SFML 3.0
#include <SFML/Window.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Graphics/Image.hpp> // <-- для загрузки текстур из файлов [web:47]

// OpenGL и математика
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ==================== КЛАСС ШЕЙДЕРА ====================
class Shader {
private:
    GLuint programID;

    static GLuint compileShader(const std::string& source, GLenum type) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Ошибка компиляции шейдера:\n" << infoLog << std::endl;
            return 0;
        }
        return shader;
    }

public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
        GLuint vertexShader = compileShader(vertexSrc, GL_VERTEX_SHADER);
        GLuint fragmentShader = compileShader(fragmentSrc, GL_FRAGMENT_SHADER);

        programID = glCreateProgram();
        glAttachShader(programID, vertexShader);
        glAttachShader(programID, fragmentShader);
        glLinkProgram(programID);

        GLint success;
        glGetProgramiv(programID, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(programID, 512, nullptr, infoLog);
            std::cerr << "Ошибка линковки шейдеров:\n" << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void use() const { glUseProgram(programID); }
    GLuint getID() const { return programID; }

    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(programID, name.c_str()), 1, glm::value_ptr(value));
    }

    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
    }

    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
    }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(programID, name.c_str()), (int)value);
    }
};

// ==================== КЛАСС МЕША (3D МОДЕЛИ) ====================
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

class Mesh {
private:
    GLuint VAO, VBO, EBO;
    size_t indicesCount;

public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
        indicesCount = indices.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Позиции
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);

        // Нормали
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        // Текстурные координаты
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    void draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    ~Mesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
};

// ==================== КЛАСС КАМЕРЫ ====================
class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;
    float zoom;

    Camera(glm::vec3 pos = glm::vec3(0.0f, 10.0f, 20.0f))
        : position(pos), front(glm::vec3(0.0f, -0.3f, -1.0f)), worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
        yaw(-90.0f), pitch(-20.0f), movementSpeed(10.0f), mouseSensitivity(0.1f), zoom(45.0f) {
        updateCameraVectors();
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void updateCameraVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);

        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};

// ==================== КЛАСС ДИРИЖАБЛЯ ====================
class Airship {
public:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 rotation;
    float speed;

    Airship() : position(0, 20, 0), velocity(0), rotation(0), speed(15.0f) {}

    void update(float deltaTime, const std::vector<bool>& keys) {
        velocity = glm::vec3(0);

        if (keys[0]) velocity.z -= 1; // W - вперед
        if (keys[1]) velocity.z += 1; // S - назад
        if (keys[2]) velocity.x -= 1; // A - влево
        if (keys[3]) velocity.x += 1; // D - вправо
        if (keys[4]) velocity.y += 1; // R - вверх
        if (keys[5]) velocity.y -= 1; // F - вниз

        if (glm::length(velocity) > 0) {
            velocity = glm::normalize(velocity) * speed * deltaTime;
        }

        position += velocity;

        // Ограничения
        position.y = glm::clamp(position.y, 5.0f, 50.0f);
        position.x = glm::clamp(position.x, -100.0f, 100.0f);
        position.z = glm::clamp(position.z, -100.0f, 100.0f);
    }

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(3.0f, 1.0f, 1.5f));
        return model;
    }
};

// ==================== ГЕНЕРАТОР ПРИМИТИВОВ ====================
class PrimitiveGenerator {
public:
    static std::shared_ptr<Mesh> createCube(float size = 1.0f) {
        float s = size / 2.0f;
        std::vector<Vertex> vertices = {
            // Передняя грань
            {{-s, -s, s}, {0, 0, 1}, {0, 0}},
            {{s, -s, s}, {0, 0, 1}, {1, 0}},
            {{s, s, s}, {0, 0, 1}, {1, 1}},
            {{-s, s, s}, {0, 0, 1}, {0, 1}},

            // Задняя грань
            {{-s, -s, -s}, {0, 0, -1}, {1, 0}},
            {{-s, s, -s}, {0, 0, -1}, {1, 1}},
            {{s, s, -s}, {0, 0, -1}, {0, 1}},
            {{s, -s, -s}, {0, 0, -1}, {0, 0}},

            // Левая грань
            {{-s, -s, -s}, {-1, 0, 0}, {0, 0}},
            {{-s, -s, s}, {-1, 0, 0}, {1, 0}},
            {{-s, s, s}, {-1, 0, 0}, {1, 1}},
            {{-s, s, -s}, {-1, 0, 0}, {0, 1}},

            // Правая грань
            {{s, -s, s}, {1, 0, 0}, {0, 0}},
            {{s, -s, -s}, {1, 0, 0}, {1, 0}},
            {{s, s, -s}, {1, 0, 0}, {1, 1}},
            {{s, s, s}, {1, 0, 0}, {0, 1}},

            // Верхняя грань
            {{-s, s, s}, {0, 1, 0}, {0, 0}},
            {{s, s, s}, {0, 1, 0}, {1, 0}},
            {{s, s, -s}, {0, 1, 0}, {1, 1}},
            {{-s, s, -s}, {0, 1, 0}, {0, 1}},

            // Нижняя грань
            {{-s, -s, -s}, {0, -1, 0}, {0, 0}},
            {{s, -s, -s}, {0, -1, 0}, {1, 0}},
            {{s, -s, s}, {0, -1, 0}, {1, 1}},
            {{-s, -s, s}, {0, -1, 0}, {0, 1}}
        };

        std::vector<unsigned int> indices = {
            0,1,2, 2,3,0,      // перед
            4,5,6, 6,7,4,      // зад
            8,9,10, 10,11,8,   // лево
            12,13,14, 14,15,12,// право
            16,17,18, 18,19,16,// верх
            20,21,22, 22,23,20 // низ
        };

        return std::make_shared<Mesh>(vertices, indices);
    }

    static std::shared_ptr<Mesh> createSphere(float radius = 1.0f, int sectors = 16, int stacks = 16) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        float sectorStep = 2 * glm::pi<float>() / sectors;
        float stackStep = glm::pi<float>() / stacks;

        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = glm::pi<float>() / 2 - i * stackStep;
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);

            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * sectorStep;

                Vertex vertex;
                vertex.position.x = xy * cosf(sectorAngle);
                vertex.position.y = xy * sinf(sectorAngle);
                vertex.position.z = z;

                vertex.normal = glm::normalize(vertex.position);
                vertex.texCoord.x = (float)j / sectors;
                vertex.texCoord.y = (float)i / stacks;

                vertices.push_back(vertex);
            }
        }

        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;

            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                if (i != (stacks - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        return std::make_shared<Mesh>(vertices, indices);
    }

    static std::shared_ptr<Mesh> createCone(float radius = 1.0f, float height = 2.0f, int sectors = 16) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // Вершина конуса
        Vertex tip;
        tip.position = glm::vec3(0, height / 2, 0);
        tip.normal = glm::vec3(0, 1, 0);
        tip.texCoord = glm::vec2(0.5f, 1.0f);
        vertices.push_back(tip);

        // Центр основания
        Vertex center;
        center.position = glm::vec3(0, -height / 2, 0);
        center.normal = glm::vec3(0, -1, 0);
        center.texCoord = glm::vec2(0.5f, 0.5f);
        vertices.push_back(center);

        // Боковая поверхность и основание
        for (int i = 0; i <= sectors; ++i) {
            float angle = 2 * glm::pi<float>() * i / sectors;
            float x = cos(angle) * radius;
            float z = sin(angle) * radius;

            // Боковая точка
            Vertex side;
            side.position = glm::vec3(x, -height / 2, z);
            side.normal = glm::normalize(glm::vec3(x, radius / height, z));
            side.texCoord = glm::vec2((float)i / sectors, 0);
            vertices.push_back(side);
        }

        // Индексы для боковой поверхности
        for (int i = 0; i < sectors; ++i) {
            indices.push_back(0); // вершина
            indices.push_back(2 + i);
            indices.push_back(2 + i + 1);

            // Основание
            indices.push_back(1); // центр
            indices.push_back(2 + i + 1);
            indices.push_back(2 + i);
        }

        return std::make_shared<Mesh>(vertices, indices);
    }
};

// ==================== ОСНОВНОЙ КЛАСС ИГРЫ ====================
class Game {
private:
    sf::Window window;
    std::unique_ptr<Shader> shader;
    Camera camera;
    Airship airship;
    float cameraDistance = 20.0f;
    float cameraTiltDeg = 25.0f;

    // Меши (модели примитивов)
    std::shared_ptr<Mesh> cubeMesh;
    std::shared_ptr<Mesh> sphereMesh;
    std::shared_ptr<Mesh> coneMesh;

    // Текстуры (поле / дирижабль / ёлка)
    GLuint groundTex = 0;
    GLuint airshipTex = 0;
    GLuint treeTex = 0;

    // Управление
    std::vector<bool> keys;
    bool wireframeMode;

    // Объекты сцены
    struct SceneObject {
        glm::vec3 position;
        glm::vec3 scale;
        glm::vec3 rotation;
        int type; // 0-дом, 1-елка, 2-облако, 3-шар, 4-декорация1, 5-декорация2
        bool delivered;
    };

    std::vector<SceneObject> houses;
    std::vector<SceneObject> clouds;
    std::vector<SceneObject> balloons;
    std::vector<SceneObject> decorations;
    SceneObject christmasTree;

    // Базовые позиции тучек + время анимации
    std::vector<glm::vec3> cloudBasePositions;
    float cloudTime = 0.0f;

    GLuint loadTextureFromFile(const std::string& path) {
        sf::Image img;
        if (!img.loadFromFile(path)) {
            std::cerr << "Не удалось загрузить текстуру: " << path << std::endl;
            return 0;
        }

        img.flipVertically(); // для UV-координат OpenGL [web:42]

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        const auto size = img.getSize();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)size.x, (GLsizei)size.y, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    const std::string vertexShaderSource = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;

        out vec3 FragPos;
        out vec3 Normal;
        out vec2 TexCoord;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(transpose(inverse(model))) * aNormal;
            TexCoord = aTexCoord;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";

    // Текстуры + направленный свет (без прожектора) [web:54]
    const std::string fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoord;

    struct Material {
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
        float shininess;
    };

    struct DirLight {
        vec3 direction;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
    };

    uniform Material material;
    uniform DirLight dirLight;
    uniform vec3 viewPos;

    uniform sampler2D diffuseTex;
    uniform bool useTexture;

    vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 baseColor) {
        vec3 lightDir = normalize(-light.direction);
        float diff = max(dot(normal, lightDir), 0.0);

        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

        vec3 ambient = light.ambient * baseColor;
        vec3 diffuse = light.diffuse * diff * baseColor;
        vec3 specular = light.specular * spec * material.specular;

        return ambient + diffuse + specular;
    }

    void main() {
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - FragPos);

        vec3 texColor = useTexture ? texture(diffuseTex, TexCoord).rgb : vec3(1.0);
        vec3 baseColor = material.diffuse * texColor;

        vec3 result = calcDirLight(dirLight, norm, viewDir, baseColor);
        FragColor = vec4(result, 1.0);
    }
    )glsl";

public:
    Game() : keys(10, false), wireframeMode(false) {
        setlocale(LC_ALL, "RU");

        // Настройки контекста OpenGL
        sf::ContextSettings settings;
        settings.depthBits = 24;
        settings.stencilBits = 8;
        settings.antiAliasingLevel = 4;
        settings.majorVersion = 3;
        settings.minorVersion = 3;

        // Создание окна 1200x800 для игры
        window.create(
            sf::VideoMode({ 1200, 800 }),
            "Почтовый дирижабль",
            sf::Style::Default,
            sf::State::Windowed,
            settings
        );
        window.setVerticalSyncEnabled(true);

        // Инициализация GLEW
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Ошибка инициализации GLEW!" << std::endl;
            return;
        }

        // Настройка OpenGL
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // Создание мешей (модели)
        cubeMesh = PrimitiveGenerator::createCube(1.0f);
        sphereMesh = PrimitiveGenerator::createSphere(1.0f);
        coneMesh = PrimitiveGenerator::createCone(1.0f, 2.0f);

        // Создание шейдера
        shader = std::make_unique<Shader>(vertexShaderSource, fragmentShaderSource);

        // Загрузка текстур (пути можно поменять под свои файлы)
        groundTex = loadTextureFromFile("textures/ground.png");
        airshipTex = loadTextureFromFile("textures/airship.png");
        treeTex = loadTextureFromFile("textures/tree.png");

        shader->use();
        shader->setInt("diffuseTex", 0);      // sampler2D берёт из GL_TEXTURE0 [web:42]
        shader->setBool("useTexture", false); // по умолчанию

        // Генерация мира
        generateWorld();

        std::cout << "=== ИГРА 'ПОЧТОВЫЙ ДИРИЖАБЛЬ' ЗАПУЩЕНА ===" << std::endl;
        std::cout << "Управление:" << std::endl;
        std::cout << "  W/S/A/D - движение" << std::endl;
        std::cout << "  R/F - высота" << std::endl;
        std::cout << "  Пробел - сброс посылки" << std::endl;
        std::cout << "  Tab - режим отображения" << std::endl;
        std::cout << "  ESC - выход" << std::endl;
    }

    void generateWorld() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> posDist(-80, 80);
        std::uniform_real_distribution<> scaleDist(0.8, 1.5);
        std::uniform_real_distribution<> rotDist(0, 360);

        // Генерация 8 домов
        for (int i = 0; i < 8; ++i) {
            houses.push_back({
                glm::vec3(posDist(gen), 0, posDist(gen)),
                glm::vec3(scaleDist(gen), scaleDist(gen), scaleDist(gen)),
                glm::vec3(0, rotDist(gen), 0),
                0,
                false
                });
        }

        // Новогодняя елка в центре поля
        christmasTree = {
            glm::vec3(0, 0, 0),
            glm::vec3(2, 4, 2),
            glm::vec3(0, 0, 0),
            1,
            false
        };

        // Генерация облаков (7 штук)
        std::uniform_real_distribution<> heightDist(30, 50);
        for (int i = 0; i < 7; ++i) {
            clouds.push_back({
                glm::vec3(posDist(gen), heightDist(gen), posDist(gen)),
                glm::vec3(3, 1, 3),
                glm::vec3(0, 0, 0),
                2,
                false
                });
        }

        // Запомнить базовые позиции тучек (для движения "около места генерации")
        cloudBasePositions.clear();
        cloudBasePositions.reserve(clouds.size());
        for (const auto& c : clouds) cloudBasePositions.push_back(c.position);

        // Генерация воздушных шаров (4 штуки)
        for (int i = 0; i < 4; ++i) {
            balloons.push_back({
                glm::vec3(posDist(gen), 25 + i * 5, posDist(gen)),
                glm::vec3(1, 1.5, 1),
                glm::vec3(0, 0, 0),
                3,
                false
                });
        }

        // Генерация декораций (2 вида по 3 штуки)
        for (int i = 0; i < 3; ++i) {
            decorations.push_back({
                glm::vec3(posDist(gen), 0, posDist(gen)),
                glm::vec3(1, 1, 1),
                glm::vec3(0, rotDist(gen), 0),
                4,
                false
                });
        }
        for (int i = 0; i < 3; ++i) {
            decorations.push_back({
                glm::vec3(posDist(gen), 0, posDist(gen)),
                glm::vec3(1.2, 0.5, 1.2),
                glm::vec3(0, rotDist(gen), 0),
                5,
                false
                });
        }
    }

    void run() {
        sf::Clock clock;

        while (window.isOpen()) {
            float deltaTime = clock.restart().asSeconds();

            processEvents();
            update(deltaTime);
            render();
        }
    }

private:
    void processEvents() {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                handleKeyPress(keyPressed->code, true);
            }
            else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                handleKeyPress(keyReleased->code, false);
            }
        }
    }

    void handleKeyPress(sf::Keyboard::Key key, bool pressed) {
        switch (key) {
        case sf::Keyboard::Key::Escape:
            window.close();
            break;
        case sf::Keyboard::Key::W:
            keys[0] = pressed;
            break;
        case sf::Keyboard::Key::S:
            keys[1] = pressed;
            break;
        case sf::Keyboard::Key::A:
            keys[2] = pressed;
            break;
        case sf::Keyboard::Key::D:
            keys[3] = pressed;
            break;
        case sf::Keyboard::Key::R:
            keys[4] = pressed;
            break;
        case sf::Keyboard::Key::F:
            keys[5] = pressed;
            break;
        case sf::Keyboard::Key::Space:
            if (pressed) dropPackage();
            break;
        case sf::Keyboard::Key::Tab:
            if (pressed) wireframeMode = !wireframeMode;
            break;
        case sf::Keyboard::Key::Num1:
            if (pressed) cameraDistance = 15.0f;
            break;
        case sf::Keyboard::Key::Num2:
            if (pressed) cameraDistance = 20.0f;
            break;
        case sf::Keyboard::Key::Num3:
            if (pressed) cameraDistance = 30.0f;
            break;
        }
    }

    void update(float deltaTime) {
        airship.update(deltaTime, keys);

        // Движение тучек около базовых позиций (простая тригонометрия)
        cloudTime += deltaTime;
        for (size_t i = 0; i < clouds.size(); ++i) {
            auto& c = clouds[i];
            const glm::vec3 base = cloudBasePositions[i];

            float phase = (float)i * 0.7f;
            float t = cloudTime + phase;

            float dx = cosf(t * 0.6f) * 2.5f;
            float dz = sinf(t * 0.4f) * 3.0f;
            float dy = sinf(t * 0.9f) * 0.8f;

            c.position = base + glm::vec3(dx, dy, dz);
        }

        // Камера сзади сверху дирижабля, перспектива видит и землю
        float tiltRad = glm::radians(cameraTiltDeg);
        float back = cameraDistance * cos(tiltRad);
        float up = cameraDistance * sin(tiltRad);

        camera.position = airship.position + glm::vec3(0.0f, up, back);
        camera.front = glm::normalize(airship.position - camera.position);
        camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
        camera.updateCameraVectors();
    }

    void dropPackage() {
        std::cout << "Посылка сброшена! Позиция: ("
            << airship.position.x << ", "
            << airship.position.y << ", "
            << airship.position.z << ")" << std::endl;

        for (auto& house : houses) {
            if (!house.delivered) {
                float distance = glm::distance(airship.position, house.position);
                if (distance < 15.0f) {
                    house.delivered = true;
                    std::cout << "ПОПАДАНИЕ! Дом доставлен!" << std::endl;
                }
            }
        }
    }

    void render() {
        glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);

        shader->use();
        glActiveTexture(GL_TEXTURE0); // единый texture unit для sampler2D [web:42]

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.zoom),
            (float)window.getSize().x / (float)window.getSize().y,
            0.1f, 200.0f
        );
        glm::mat4 view = camera.getViewMatrix();

        shader->setMat4("projection", projection);
        shader->setMat4("view", view);
        shader->setVec3("viewPos", camera.position);

        // Глобальное освещение — направленный источник
        shader->setVec3("dirLight.direction", glm::vec3(-0.5f, -1.0f, -0.5f));
        shader->setVec3("dirLight.ambient", glm::vec3(0.3f, 0.3f, 0.3f));
        shader->setVec3("dirLight.diffuse", glm::vec3(0.5f, 0.5f, 0.5f));
        shader->setVec3("dirLight.specular", glm::vec3(0.3f, 0.3f, 0.3f));

        renderGround();
        renderAirship();
        renderChristmasTree();

        for (const auto& house : houses) {
            if (!house.delivered) renderHouse(house);
        }

        for (const auto& cloud : clouds) renderCloud(cloud);
        for (const auto& balloon : balloons) renderBalloon(balloon);
        for (const auto& deco : decorations) renderDecoration(deco);

        window.display();
    }

    void renderGround() {
        shader->setVec3("material.ambient", glm::vec3(0.2f, 0.6f, 0.2f));
        shader->setVec3("material.diffuse", glm::vec3(1.0f, 1.0f, 1.0f)); // цвет берём из текстуры
        shader->setVec3("material.specular", glm::vec3(0.1f, 0.1f, 0.1f));
        shader->setFloat("material.shininess", 32.0f);

        shader->setBool("useTexture", true);
        glBindTexture(GL_TEXTURE_2D, groundTex);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0, -0.5f, 0));
        model = glm::scale(model, glm::vec3(200, 1, 200));
        shader->setMat4("model", model);

        cubeMesh->draw();

        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
    }

    void renderAirship() {
        // Корпус дирижабля (сфера) с текстурой
        shader->setVec3("material.ambient", glm::vec3(0.8f, 0.2f, 0.2f));
        shader->setVec3("material.diffuse", glm::vec3(1.0f, 1.0f, 1.0f)); // берём из текстуры
        shader->setVec3("material.specular", glm::vec3(0.5f, 0.5f, 0.5f));
        shader->setFloat("material.shininess", 64.0f);

        shader->setBool("useTexture", true);
        glBindTexture(GL_TEXTURE_2D, airshipTex);

        shader->setMat4("model", airship.getModelMatrix());
        sphereMesh->draw();

        // Гондола (куб), оставим ту же текстуру (или можно сделать отдельную)
        glm::mat4 gondola = glm::mat4(1.0f);
        gondola = glm::translate(gondola, airship.position + glm::vec3(0, -2, 0));
        gondola = glm::scale(gondola, glm::vec3(1, 0.5f, 2));
        shader->setMat4("model", gondola);
        cubeMesh->draw();

        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
    }

    void renderHouse(const SceneObject& house) {
        // Дома оставлены без текстуры (по условию не требовалось)
        shader->setVec3("material.ambient", glm::vec3(0.7f, 0.5f, 0.3f));
        shader->setVec3("material.diffuse", glm::vec3(0.9f, 0.7f, 0.5f));
        shader->setVec3("material.specular", glm::vec3(0.1f, 0.1f, 0.1f));
        shader->setFloat("material.shininess", 16.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, house.position);
        model = glm::rotate(model, glm::radians(house.rotation.y), glm::vec3(0, 1, 0));
        model = glm::scale(model, house.scale);
        shader->setMat4("model", model);

        cubeMesh->draw();

        // Крыша
        glm::mat4 roof = model;
        roof = glm::translate(roof, glm::vec3(0, house.scale.y / 2 + 0.5f, 0));
        roof = glm::scale(roof, glm::vec3(1.2f, 0.5f, 1.2f));
        shader->setVec3("material.ambient", glm::vec3(0.8f, 0.2f, 0.2f));
        shader->setMat4("model", roof);
        coneMesh->draw();
    }

    void renderChristmasTree() {
        // Ёлка: используем treeTex и для ствола, и для ярусов (простое выполнение условия)
        shader->setBool("useTexture", true);
        glBindTexture(GL_TEXTURE_2D, treeTex);

        // Ствол
        shader->setVec3("material.ambient", glm::vec3(0.4f, 0.2f, 0.1f));
        shader->setVec3("material.diffuse", glm::vec3(1.0f, 1.0f, 1.0f));
        shader->setVec3("material.specular", glm::vec3(0.1f, 0.1f, 0.1f));
        shader->setFloat("material.shininess", 16.0f);

        glm::mat4 trunk = glm::mat4(1.0f);
        trunk = glm::translate(trunk, christmasTree.position);
        trunk = glm::scale(trunk, glm::vec3(0.3f, 1.0f, 0.3f));
        shader->setMat4("model", trunk);
        cubeMesh->draw();

        // Ярусы (листва)
        shader->setVec3("material.ambient", glm::vec3(0.1f, 0.5f, 0.1f));
        shader->setVec3("material.diffuse", glm::vec3(1.0f, 1.0f, 1.0f));

        for (int i = 0; i < 3; ++i) {
            glm::mat4 layer = glm::mat4(1.0f);
            layer = glm::translate(layer, christmasTree.position + glm::vec3(0, i * 1.2f, 0));
            layer = glm::scale(layer, glm::vec3(1.5f - i * 0.3f, 0.8f, 1.5f - i * 0.3f));
            shader->setMat4("model", layer);
            coneMesh->draw();
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
    }

    void renderCloud(const SceneObject& cloud) {
        // Тучки без текстуры
        shader->setVec3("material.ambient", glm::vec3(0.9f, 0.9f, 0.9f));
        shader->setVec3("material.diffuse", glm::vec3(1.0f, 1.0f, 1.0f));
        shader->setVec3("material.specular", glm::vec3(0.1f, 0.1f, 0.1f));
        shader->setFloat("material.shininess", 8.0f);

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (abs(i) + abs(j) <= 2) {
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, cloud.position + glm::vec3(i * 1.5f, j * 0.5f, 0));
                    model = glm::scale(model, cloud.scale * 0.8f);
                    shader->setMat4("model", model);
                    sphereMesh->draw();
                }
            }
        }
    }

    void renderBalloon(const SceneObject& balloon) {
        shader->setVec3("material.ambient", glm::vec3(0.8f, 0.2f, 0.2f));
        shader->setVec3("material.diffuse", glm::vec3(1.0f, 0.3f, 0.3f));
        shader->setVec3("material.specular", glm::vec3(0.5f, 0.5f, 0.5f));
        shader->setFloat("material.shininess", 32.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, balloon.position);
        model = glm::scale(model, balloon.scale);
        shader->setMat4("model", model);
        sphereMesh->draw();

        shader->setVec3("material.ambient", glm::vec3(0.4f, 0.2f, 0.1f));
        glm::mat4 basket = glm::mat4(1.0f);
        basket = glm::translate(basket, balloon.position + glm::vec3(0, -balloon.scale.y, 0));
        basket = glm::scale(basket, glm::vec3(0.5f, 0.3f, 0.5f));
        shader->setMat4("model", basket);
        cubeMesh->draw();
    }

    void renderDecoration(const SceneObject& deco) {
        if (deco.type == 4) {
            shader->setVec3("material.ambient", glm::vec3(0.3f, 0.3f, 0.8f));
            shader->setVec3("material.diffuse", glm::vec3(0.4f, 0.4f, 0.9f));
        }
        else {
            shader->setVec3("material.ambient", glm::vec3(0.8f, 0.8f, 0.3f));
            shader->setVec3("material.diffuse", glm::vec3(0.9f, 0.9f, 0.4f));
        }
        shader->setVec3("material.specular", glm::vec3(0.1f, 0.1f, 0.1f));
        shader->setFloat("material.shininess", 16.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, deco.position);
        model = glm::rotate(model, glm::radians(deco.rotation.y), glm::vec3(0, 1, 0));
        model = glm::scale(model, deco.scale);
        shader->setMat4("model", model);

        cubeMesh->draw();
    }
};

// ==================== ТОЧКА ВХОДА ====================
int main() {
    try {
        Game game;
        game.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
