#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "shaderClass.h"

const float ROD_WIDTH = 0.005;
const float CIRCLE_RADIUS = 0.02f;
const float ROD_LENGTH = 0.3f;
const float GRAVITY = 9.81f; // m/s^2
const float DAMPING = 0.992f; // Damping factor for pendulum motion
const float ROD_MASS = 0.1f; // Mass of the rod (arbitrary value for simulation)

float h = 0.005f;           // fixed timestep
float accumulator = 0.0f;
float prevTime = glfwGetTime();

// mouse dragging vars
bool dragging = false;
double mouseX, mouseY;
double lastMouseX, lastMouseY;

int winWidth = 800;
int winHeight = 800;
float mouseX_ndc = 0.0f;
float mouseY_ndc = 0.0f;

// Pendulum state variables
float angle = M_PI / 3.0f;   
float angularVelocity = 0.0f;  // Initial velocity

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    winWidth = width;
    winHeight = height;
    glViewport(0, 0, width, height);
}


// --- CREATING SHAPES ---
GLfloat rectVertices[] = {
    -ROD_WIDTH,  0.0f, 0.0f,  // Left middle
     ROD_WIDTH,  0.0f, 0.0f,  // Right middle
     ROD_WIDTH,  -ROD_LENGTH, 0.0f,  // Right top
    -ROD_WIDTH,  -ROD_LENGTH, 0.0f   // Left top
};

GLuint rectIndices[] = {
    0, 1, 2,
    2, 3, 0
};
VAO* rectVAO = nullptr;
VBO* rectVBO = nullptr;
EBO* rectEBO = nullptr;

void SetupRect() {
    rectVAO = new VAO();
    rectVBO = new VBO(rectVertices, sizeof(rectVertices));
    rectEBO = new EBO(rectIndices, sizeof(rectIndices));
    rectVAO->Bind();
    rectVBO->Bind();
    rectEBO->Bind();

    // Set vertex attribute pointer for position (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    rectVAO->Unbind();
    rectVBO->Unbind();
    rectEBO->Unbind();
}

void DrawRect() {
    rectVAO->Bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    rectVAO->Unbind();
}

const uint NUM_VERTICES = 98;  // Number of vertices around the circle (excluding center)
GLfloat CircleVertices[(NUM_VERTICES + 2) * 3]; // +1 for center, +1 for closing vertex
GLuint CircleIndices[NUM_VERTICES + 2]; // +1 for center, +1 for closing vertex

VAO* circleVAO = nullptr;
VBO* circleVBO = nullptr;
EBO* circleEBO = nullptr;

void SetupCircle() {
    circleVAO = new VAO();
    
    // Set center vertex
    CircleVertices[0] = 0.0f;  // center x
    CircleVertices[1] = 0.0f;  // center y
    CircleVertices[2] = 0.0f;  // center z

    // Generate vertices around the circle
    for (int i = 0; i < NUM_VERTICES; ++i) {
        float angle2 = (2.0 * M_PI * i) / NUM_VERTICES;
        int idx = (i + 1) * 3;  // +1 because first vertex is center
        CircleVertices[idx] = cos(angle2) * CIRCLE_RADIUS;     // x
        CircleVertices[idx + 1] = sin(angle2) * CIRCLE_RADIUS; // y
        CircleVertices[idx + 2] = 0.0f;              // z
    }

    int lastIdx = (NUM_VERTICES + 1) * 3;
    CircleVertices[lastIdx] = CircleVertices[3];     // Same as first outer vertex
    CircleVertices[lastIdx + 1] = CircleVertices[4];
    CircleVertices[lastIdx + 2] = CircleVertices[5];

    // Set up indices for triangle fan
    CircleIndices[0] = 0;  // Center vertex
    for (int i = 0; i <= NUM_VERTICES; i++) {
        CircleIndices[i + 1] = i + 1;
    }

    circleVBO = new VBO(CircleVertices, sizeof(CircleVertices));
    circleEBO = new EBO(CircleIndices, sizeof(CircleIndices));
    
    circleVAO->Bind();
    circleVBO->Bind();
    circleEBO->Bind();

    // Set vertex attribute pointer for position (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    circleVAO->Unbind();
    circleVBO->Unbind();
    circleEBO->Unbind();
}

void DrawCircle() {
    circleVAO->Bind();  // Fixed: was using rectVAO
    glDrawElements(GL_TRIANGLE_FAN, NUM_VERTICES + 2, GL_UNSIGNED_INT, 0);
    circleVAO->Unbind();
}

// --- DONE WITH SHAPE SETUP ---


// --- PHYSICS FOR PENDULUM ---

void UpdatePendulum(float dt) {
    float angularAcceleration = -(GRAVITY / ROD_LENGTH) * sin(angle);
    
    // Update angle with damping
    angularVelocity += angularAcceleration * dt;
    angularVelocity *= DAMPING; // Apply damping to reduce oscillation over time
    angle += angularVelocity * dt; // Apply damping to reduce oscillation over time
}

// --- DONE WITH PHYSICS FOR PENDULUM ---

int main(){
    // initialize GLFW
    glfwInit();

    // Tell GLFW what version of OpenGL we are using
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Tell it what functions we want to use
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_SAMPLES, 4);

    // Create window + error check
    GLFWwindow* window = glfwCreateWindow(800, 800, "OpenGL Window", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1; 
    }  
    
    // using window pixels -- outdated, but kept for reference
    // // init callbacks
    // glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
    //     // pixels -> NDC
    //     mouseX_ndc = 2.0f * float(xpos) / float(winWidth) - 1.0f;
    //     mouseY_ndc = 1.0f - 2.0f * float(ypos) / float(winHeight);

    //     std::cout << "Mouse Pos: (" << xpos << ", " << ypos << ")" << std::endl; // debugging
    //     std::cout << "Mouse NDC: (" << mouseX_ndc << ", " << mouseY_ndc << ")" << std::endl; // debugging
    //     std::cout << "Window Size: (" << winWidth << ", " << winHeight << ")" << std::endl; // debugging

    //     // fix aspect if not square
    //     if (winWidth != winHeight) {
    //         mouseX_ndc *= (float)winWidth / (float)winHeight;
    //     }

    //     if (dragging) {
    //         angle = atan2(mouseX_ndc, -mouseY_ndc);  // angle from +Y
    //         angularVelocity = 0.0f;                 // don’t fight the drag
    //     }
    // });

    // using framebuffer size for accurate mouse position
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos){

        // using framebuffer size to get accurate mouse position
        float sx, sy;
        glfwGetWindowContentScale(w, &sx, &sy);       // e.g., 2.0, 2.0 on Retina
        double x_fb = xpos * sx;
        double y_fb = ypos * sy;

        // safer: use the actual current viewport
        int vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);               // {x, y, width, height}
        mouseX_ndc =  2.0f * float(x_fb - vp[0]) / float(vp[2]) - 1.0f;
        mouseY_ndc = -2.0f * float(y_fb - vp[1]) / float(vp[3]) + 1.0f;

        if (dragging) {
            angle = atan2(mouseX_ndc, -mouseY_ndc);
            angularVelocity = 0.0f;
        }
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        if (button != GLFW_MOUSE_BUTTON_LEFT) return;

        if (action == GLFW_PRESS) {
            // bob position from current angle (pivot at origin)
            float bobX =  sin(angle) * ROD_LENGTH;
            float bobY =  -cos(angle) * ROD_LENGTH;

            float dx = mouseX_ndc - bobX;
            float dy = mouseY_ndc - bobY;
            float dist = std::sqrt(dx*dx + dy*dy);

            // pick radius lenient so it's easy to grab
            if (dist < CIRCLE_RADIUS * 5.0f)  {
                dragging = true;
            }
        } else if (action == GLFW_RELEASE) {
            dragging = false;
        }
    });


    // Bring window into the current context
    glfwMakeContextCurrent(window);

    // Load OpenGL functions using GLAD
    gladLoadGL();
    glfwSwapInterval(1); // Enable vsync
    glEnable(GL_MULTISAMPLE);
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    winWidth = width; winHeight = height;
    glViewport(0, 0, width, height);    

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    
    // Create shader program using ShaderClass
    Shader shaderProgram("shaders/default.vert", "shaders/default.frag");
    SetupRect();
    SetupCircle();

    while (!glfwWindowShouldClose(window)) {
        float currTime = glfwGetTime();
        float frameTime = currTime - prevTime;
        prevTime = currTime;

        accumulator += frameTime;
        float interpolatedAngle = angle;
        if (!dragging) {
            while (accumulator >= h) {
                UpdatePendulum(h);
                accumulator -= h;
            }

            float alpha = accumulator / h;
            // Interpolate the angle for smoother rendering
            interpolatedAngle = angle + alpha * angularVelocity * h;
        } else {
            accumulator = 0.0f;
            interpolatedAngle = angle;
        }

        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        shaderProgram.Activate();
        
        // Update transformation matrix for the rod
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::rotate(transform, interpolatedAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        
        // Send transformation to shader and draw rod
        GLuint transformLoc = glGetUniformLocation(shaderProgram.ID, "transform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
        DrawRect();

        // Update transform matrix for the bob (circle) - move it to end of rod
        transform = glm::translate(transform, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));
        DrawCircle();

        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    // Clean up resources
    shaderProgram.Delete();
    delete rectVAO;
    delete rectVBO;
    delete rectEBO;

    // terminate GLFW and clean up
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}