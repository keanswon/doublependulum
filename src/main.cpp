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


/** INDEPENDENT VARIABLES **/
/**
 * - angle 1 -- angle from vertical to rod 1
 * - angle 2 -- angle from vertical to rod 2
 * - mass 1 - mass of bob 1 (at the end of rod 1)
 * - mass 2 - mass of bob 2 (at the end of rod 2
 * - rod length 1 - length of rod 1
 * - rod length 2 - length of rod 2
 * 
 * TODO : add sliders for each of these variables
 */

const float ROD_WIDTH = 0.005;
const float CIRCLE_RADIUS = 0.02f;
float ROD_LENGTH = 0.3f;
float ROD_LENGTH2 = 0.3f; // length of rod 2
const float GRAVITY = 9.81f; // m/s^2
const float DAMPING = 0.992f; // Damping factor for pendulum motion
float BOB_MASS = 0.1f;
float BOB_MASS2 = 0.1f; // Mass of the rod (arbitrary value for simulation)

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
float theta1 = M_PI / 3.0f;   
float theta2 = M_PI / 2.0f;
float angularVelocity = 0.0f;
float angularVelocity2 = 0.0f;  // Initial velocity

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
        float theta2 = (2.0 * M_PI * i) / NUM_VERTICES;
        int idx = (i + 1) * 3;  // +1 because first vertex is center
        CircleVertices[idx] = cos(theta2) * CIRCLE_RADIUS;          // x
        CircleVertices[idx + 1] = sin(theta2) * CIRCLE_RADIUS;      // y
        CircleVertices[idx + 2] = 0.0f;                             // z
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

// TODO: input formula and params here

// ~~~~~ ANGLE 1 ~~~~~
/**  (M1 + M2) gsin(angle1) + 
 *  (M1 + M2) * L1 ANGACCEL1 + 
 *  M2 * L2 * ANGACCEL2 * cos(angle1 - angle2) + 
 *  M2 L2 THETADOT2^2 (sin(angle1 - angle2))
 *  = 0
*/

// ~~~~~ ANGLE 2 ~~~~~
// TODO

void UpdatePendulum(float dt) {
    float num1 = -GRAVITY * (2*BOB_MASS+ BOB_MASS2) * sin(theta1);
    float num2 = -BOB_MASS2 * GRAVITY * sin(theta1 - 2*theta2);
    float num3 = -2*sin(theta1 - theta2) * BOB_MASS2 *
                (angularVelocity2*angularVelocity2*ROD_LENGTH2 + angularVelocity*angularVelocity*ROD_LENGTH*cos(theta1 - theta2));
    float den1 = ROD_LENGTH * (2*BOB_MASS + BOB_MASS2 - BOB_MASS2*cos(2*theta1 - 2*theta2));
    float theta1_ddot = (num1 + num2 + num3) / den1;

    float num4 = 2*sin(theta1 - theta2) *
                (angularVelocity*angularVelocity*ROD_LENGTH*(BOB_MASS + BOB_MASS2) +
                GRAVITY*(BOB_MASS + BOB_MASS2)*cos(theta1) +
                angularVelocity2*angularVelocity2*ROD_LENGTH2*BOB_MASS2*cos(theta1 - theta2));
    float den2 = ROD_LENGTH2 * (2*BOB_MASS + BOB_MASS2 - BOB_MASS2*cos(2*theta1 - 2*theta2));
    float theta2_ddot = num4 / den2;


    // Update angular velocities
    angularVelocity += theta1_ddot * dt;
    angularVelocity2 += theta2_ddot * dt;

    // Update angles
    theta1 += angularVelocity * dt;
    theta2 += angularVelocity2 * dt;

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
    // this will be invalid for double pendulums
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
            theta1 = atan2(mouseX_ndc, -mouseY_ndc);
            angularVelocity = 0.0f;
        }
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        if (button != GLFW_MOUSE_BUTTON_LEFT) return;

        if (action == GLFW_PRESS) {
            // bob position from current angle (pivot at origin)
            float bobX =  sin(theta1) * ROD_LENGTH;
            float bobY =  -cos(theta1) * ROD_LENGTH;

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
        float interpolatedAngle = theta1;
        float interpolatedAngle2 = theta2;
        if (!dragging) {
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
        
        // Send transformation to shader and draw rods
        GLuint transformLoc = glGetUniformLocation(shaderProgram.ID, "transform");
        // 1) Unscaled base transforms (for correct circle shapes)
        glm::mat4 T1_noscale = glm::rotate(glm::mat4(1.0f), interpolatedAngle,  glm::vec3(0,0,1));
        glm::mat4 T2_noscale = glm::translate(T1_noscale, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));
        T2_noscale = glm::rotate(T2_noscale, interpolatedAngle2, glm::vec3(0,0,1));

        // 2) Scaled transforms for rods (scale along Y by each rod’s length)
        glm::mat4 Rod1 = glm::rotate(glm::mat4(1.0f), interpolatedAngle, glm::vec3(0,0,1));
        Rod1 = glm::scale(Rod1, glm::vec3(1.0f, ROD_LENGTH, 1.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(Rod1));
        DrawRect();

        glm::mat4 Rod2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        // place Rod2 at the end of Rod1, then rotate + scale it
        Rod2 = glm::rotate(glm::mat4(1.0f), interpolatedAngle, glm::vec3(0,0,1));
        Rod2 = glm::translate(Rod2, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));
        Rod2 = glm::rotate(Rod2, interpolatedAngle2, glm::vec3(0,0,1));
        Rod2 = glm::scale(Rod2, glm::vec3(1.0f, ROD_LENGTH2, 1.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(Rod2));
        DrawRect();

        // 3) Circles: use the unscaled transforms so they stay round
        glm::mat4 T1_noscale_bob = glm::translate(T1_noscale, glm::vec3(0.0f, -ROD_LENGTH, 0.0f));

        // Use the translated matrix here (not T1_noscale)
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(T1_noscale_bob));
        DrawCircle();  // first bob at end of rod1

        glm::mat4 Tbob = glm::translate(T2_noscale, glm::vec3(0.0f, -ROD_LENGTH2, 0.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(Tbob));
        DrawCircle();  // second bob at end of rod2


        
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