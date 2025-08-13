#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "shaderClass.h"

float h = 0.005f;           // fixed timestep
float accumulator = 0.0f;
float prevTime = glfwGetTime();

// mouse dragging vars -- outdated, but kept for reference
// bool dragging = false;
// double mouseX, mouseY;
// double lastMouseX, lastMouseY;

int winWidth = 800;
int winHeight = 800;
float mouseX_ndc = 0.0f;
float mouseY_ndc = 0.0f;

// Pendulum state variables
float theta1 = 0;   
float theta2 = 0;
float angularVelocity = 0.0f;
float angularVelocity2 = 0.0f;  // Initial velocity

const float ROD_WIDTH = 0.005;
const float CIRCLE_RADIUS = 0.02f;
const float GRAVITY = 9.81f; // m/s^2

float ROD_LENGTH = 0.3f;
float ROD_LENGTH2 = 0.3f; // length of rod 2
float DAMPING = 0.992f; // Damping factor for pendulum motion
float BOB_MASS = 0.1f;
float BOB_MASS2 = 0.1f;

// initial angles for reset
bool paused = false;
float theta1_init = theta1, theta2_init = theta2;

// functions to pause on change : one for sliders, one for inputs
inline bool PauseIf(bool changed, bool& paused) { paused |= changed; return changed; }

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    winWidth = width;
    winHeight = height;
    glViewport(0, 0, width, height);
}

// --- CREATING SHAPES ---
GLuint rectIndices[] = {
    0, 1, 2,
    2, 3, 0
};
VAO* rectVAO = nullptr;
VBO* rectVBO = nullptr;
EBO* rectEBO = nullptr;

void SetupRect(float rod_length=ROD_LENGTH) {
    GLfloat rectVertices[] = {
        -ROD_WIDTH,  0.0f, 0.0f,   // left mid (pivot)
         ROD_WIDTH,  0.0f, 0.0f,   // right mid (pivot)
         ROD_WIDTH, -1.0f, 0.0f,   // right end
        -ROD_WIDTH, -1.0f, 0.0f    // left end
    };
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

const uint NUM_VERTICES = 98;
GLfloat CircleVertices[(NUM_VERTICES + 2) * 3];
GLuint CircleIndices[NUM_VERTICES + 2];

VAO* circleVAO = nullptr;
VBO* circleVBO = nullptr;
EBO* circleEBO = nullptr;

void SetupCircle() {
    circleVAO = new VAO();
    
    // center vertex at (0, 0, 0)
    CircleVertices[0] = 0.0f;  
    CircleVertices[1] = 0.0f;  
    CircleVertices[2] = 0.0f;  

    for (int i = 0; i < NUM_VERTICES; ++i) {
        float theta2 = (2.0 * M_PI * i) / NUM_VERTICES;
        int idx = (i + 1) * 3;
        CircleVertices[idx] = cos(theta2) * CIRCLE_RADIUS;          // x
        CircleVertices[idx + 1] = sin(theta2) * CIRCLE_RADIUS;      // y
        CircleVertices[idx + 2] = 0.0f;                             // z
    }

    int lastIdx = (NUM_VERTICES + 1) * 3;
    CircleVertices[lastIdx] = CircleVertices[3];
    CircleVertices[lastIdx + 1] = CircleVertices[4];
    CircleVertices[lastIdx + 2] = CircleVertices[5];

    CircleIndices[0] = 0;
    for (int i = 0; i <= NUM_VERTICES; i++) {
        CircleIndices[i + 1] = i + 1;
    }

    circleVBO = new VBO(CircleVertices, sizeof(CircleVertices));
    circleEBO = new EBO(CircleIndices, sizeof(CircleIndices));
    
    circleVAO->Bind();
    circleVBO->Bind();
    circleEBO->Bind();

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    circleVAO->Unbind();
    circleVBO->Unbind();
    circleEBO->Unbind();
}

void DrawCircle() {
    circleVAO->Bind();
    glDrawElements(GL_TRIANGLE_FAN, NUM_VERTICES + 2, GL_UNSIGNED_INT, 0);
    circleVAO->Unbind();
}

// --- DONE WITH SHAPE SETUP ---

// --- PHYSICS FOR PENDULUM ---
void UpdatePendulum(float dt) {
    // intermediate calculations
    float num1 = -GRAVITY * (2*BOB_MASS+ BOB_MASS2) * sin(theta1);
    float num2 = -BOB_MASS2 * GRAVITY * sin(theta1 - 2*theta2);
    float num3 = -2*sin(theta1 - theta2) * BOB_MASS2 *
                (angularVelocity2*angularVelocity2*ROD_LENGTH2 + angularVelocity*angularVelocity*ROD_LENGTH*cos(theta1 - theta2));
    float den1 = ROD_LENGTH * (2*BOB_MASS + BOB_MASS2 - BOB_MASS2*cos(2*theta1 - 2*theta2));

    // angular accel for theta1
    float theta1_ddot = (num1 + num2 + num3) / den1;

    float num4 = 2*sin(theta1 - theta2) *
                (angularVelocity*angularVelocity*ROD_LENGTH*(BOB_MASS + BOB_MASS2) +
                GRAVITY*(BOB_MASS + BOB_MASS2)*cos(theta1) +
                angularVelocity2*angularVelocity2*ROD_LENGTH2*BOB_MASS2*cos(theta1 - theta2));
    float den2 = ROD_LENGTH2 * (2*BOB_MASS + BOB_MASS2 - BOB_MASS2*cos(2*theta1 - 2*theta2));
    
    // angular accel for theta2
    float theta2_ddot = num4 / den2;


    // update angular velocities
    angularVelocity += theta1_ddot * dt;
    angularVelocity2 += theta2_ddot * dt;

    // update angles
    theta1 += angularVelocity * dt;
    theta2 += angularVelocity2 * dt;

    // normalize angles to be between 0 and 2*PI
    theta1 = fmod(theta1, 2 * M_PI);
    theta2 = fmod(theta2, 2 * M_PI);

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
    
    /**
     * 
     * CURSOR DRAGGING LOGIC HERE
     * 
     */


    // Bring window into the current context
    glfwMakeContextCurrent(window);

    // Load OpenGL functions using GLAD
    gladLoadGL();
    glfwSwapInterval(1); // Enable vsync
    glEnable(GL_MULTISAMPLE);

    // --- IMGUI SETUP ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
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
        // build ui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImVec2 window_pos = ImVec2(io.DisplaySize.x - 10, io.DisplaySize.y - 10);
        ImVec2 window_pivot = ImVec2(1.0f, 1.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pivot);
        ImGui::SetNextWindowBgAlpha(0.8f);


        // helper function to pause on input commit
        auto PauseOnCommit = [&](bool changed) -> bool {
            bool committed = ImGui::IsItemDeactivatedAfterEdit();
            if (changed && committed) { 
                paused = true; 
                return true; 
            }
            return false;
        };

        // controls box
        if (ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Checkbox("Pause", &paused);

            ImGui::PushItemWidth(150);
            
            PauseIf(ImGui::SliderFloat("Damping", &DAMPING, 0.90f, 1.0f, "%.4f"), paused);

            if (PauseOnCommit(ImGui::InputFloat("Rod 1 length", &ROD_LENGTH, 0.05f, 0.8f, "%.01f"))) {
                ROD_LENGTH = glm::clamp(ROD_LENGTH, 0.05f, 0.5f);
            }

            if (PauseOnCommit(ImGui::InputFloat("Rod 2 length", &ROD_LENGTH2, 0.05f, 0.8f, "%.01f"))) {
                ROD_LENGTH2 = glm::clamp(ROD_LENGTH2, 0.05f, 0.5f);
            }

            if (PauseOnCommit(ImGui::InputFloat("Mass 1", &BOB_MASS, 0.05f, 0.8f, "%.2f"))) {
                BOB_MASS = glm::clamp(BOB_MASS, 0.05f, 2.0f);
            }

            if (PauseOnCommit(ImGui::InputFloat("Mass 2", &BOB_MASS2, 0.05f, 0.8f, "%.2f"))) {
                BOB_MASS2 = glm::clamp(BOB_MASS2, 0.05f, 2.0f);
            }

            // use PauseIf for angle sliders
            PauseIf(ImGui::SliderAngle("Angle 1", &theta1, -180.0f, 180.0f), paused);
            ImGui::SameLine();
            
            ImGui::PushItemWidth(100);
            static float a1_deg = 0.f, a2_deg = 0.f;
            a1_deg = theta1 * 180.0f / M_PI;
            
            if (PauseOnCommit(ImGui::InputFloat("Angle 1 (deg)", &a1_deg, 1.0f, 5.0f, "%.1f"))) {
                theta1 = a1_deg * M_PI / 180.0f;
                angularVelocity = 0.0f;
                accumulator = 0.0f;
            }
            ImGui::PopItemWidth();

            PauseIf(ImGui::SliderAngle("Angle 2", &theta2, -180.0f, 180.0f), paused);
            ImGui::SameLine();

            ImGui::PushItemWidth(100);
            a2_deg = theta2 * 180.0f / M_PI;
            
            if (PauseOnCommit(ImGui::InputFloat("Angle 2 (deg)", &a2_deg, 1.0f, 5.0f, "%.1f"))) {
                theta2 = a2_deg * M_PI / 180.0f; 
                angularVelocity2 = 0.0f;
                accumulator = 0.0f;
            }
            ImGui::PopItemWidth();
            ImGui::PopItemWidth();

            if (ImGui::Button("Reset")) {
                theta1 = theta1_init; 
                theta2 = theta2_init;
                angularVelocity = 0; 
                angularVelocity2 = 0;
                accumulator = 0;
                BOB_MASS2 = BOB_MASS = 1.0f;
                ROD_LENGTH = ROD_LENGTH2 = 0.3f;
                DAMPING = 0.992f;
                paused = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Start")) {
                paused = false;
                angularVelocity = 0.0f;
                angularVelocity2 = 0.0f;
            }
        }
        ImGui::End();

        // simulate pendulum
        float currTime = glfwGetTime();
        float frameTime = currTime - prevTime;
        prevTime = currTime;

        accumulator += frameTime;
        float interpolatedAngle = theta1;
        float interpolatedAngle2 = theta2;

        if (!paused) {
            while (accumulator >= h) {
                UpdatePendulum(h);
                accumulator -= h;
            }

            float alpha = accumulator / h;
            // Interpolate the angle for smoother rendering
            interpolatedAngle = theta1 + alpha * angularVelocity * h;
            interpolatedAngle2 = theta2 + alpha * angularVelocity2 * h;
        } else {
            accumulator = 0.0f;
            interpolatedAngle = theta1;
            interpolatedAngle2 = theta2;
        }

        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        shaderProgram.Activate();
        
        // draw shapes: two rods and two circles (bobs)
        GLuint transformLoc = glGetUniformLocation(shaderProgram.ID, "transform");
        
        glm::mat4 T1_noscale = glm::rotate(glm::mat4(1.0f), interpolatedAngle,  glm::vec3(0,0,1));
        glm::mat4 T2_noscale = glm::translate(T1_noscale, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));
        T2_noscale = glm::rotate(T2_noscale, interpolatedAngle2, glm::vec3(0,0,1));

        glm::mat4 Rod1 = glm::rotate(glm::mat4(1.0f), interpolatedAngle, glm::vec3(0,0,1));
        Rod1 = glm::scale(Rod1, glm::vec3(1.0f, ROD_LENGTH, 1.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(Rod1));
        DrawRect();

        glm::mat4 Rod2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        Rod2 = glm::rotate(glm::mat4(1.0f), interpolatedAngle, glm::vec3(0,0,1));
        Rod2 = glm::translate(Rod2, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));
        Rod2 = glm::rotate(Rod2, interpolatedAngle2, glm::vec3(0,0,1));
        Rod2 = glm::scale(Rod2, glm::vec3(1.0f, ROD_LENGTH2, 1.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(Rod2));
        DrawRect();

        glm::mat4 T1_noscale_bob = glm::translate(T1_noscale, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(T1_noscale_bob));
        DrawCircle();  // first bob at end of rod1

        glm::mat4 Tbob = glm::translate(T2_noscale, glm::vec3(0.0f, -ROD_LENGTH2, 0.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(Tbob));
        DrawCircle();  // second bob at end of rod2

        // render
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // swap buffers and poll events
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

// cursor dragging -- outdated, but kept for reference -- may reimplement for double pendulum
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
// this will be invalid for double pendulums
// glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos){

//     // using framebuffer size to get accurate mouse position
//     float sx, sy;
//     glfwGetWindowContentScale(w, &sx, &sy);       // e.g., 2.0, 2.0 on Retina
//     double x_fb = xpos * sx;
//     double y_fb = ypos * sy;

//     // safer: use the actual current viewport
//     int vp[4];
//     glGetIntegerv(GL_VIEWPORT, vp);               // {x, y, width, height}
//     mouseX_ndc =  2.0f * float(x_fb - vp[0]) / float(vp[2]) - 1.0f;
//     mouseY_ndc = -2.0f * float(y_fb - vp[1]) / float(vp[3]) + 1.0f;

//     if (dragging) {
//         theta1 = atan2(mouseX_ndc, -mouseY_ndc);
//         angularVelocity = 0.0f;
//     }
// });

// glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
//     if (button != GLFW_MOUSE_BUTTON_LEFT) return;

//     if (action == GLFW_PRESS) {
//         // bob position from current angle (pivot at origin)
//         float bobX =  sin(theta1) * ROD_LENGTH;
//         float bobY =  -cos(theta1) * ROD_LENGTH;

//         float dx = mouseX_ndc - bobX;
//         float dy = mouseY_ndc - bobY;
//         float dist = std::sqrt(dx*dx + dy*dy);

//         // pick radius lenient so it's easy to grab
//         if (dist < CIRCLE_RADIUS * 5.0f)  {
//             dragging = true;
//         }
//     } else if (action == GLFW_RELEASE) {
//         dragging = false;
//     }
// });