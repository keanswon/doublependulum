#include <vao.h>

VAO::VAO() {
    // Generate the Vertex Array Object
    glGenVertexArrays(1, &ID);
}

void VAO::LinkVBO(VBO& vbo, GLuint layout) {    
    // Bind the Vertex Buffer Object
    vbo.Bind();
    
    // Specify the layout of the vertex data
    glVertexAttribPointer(layout, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    
    // Enable the vertex attribute
    glEnableVertexAttribArray(layout);
    
    // Unbind the VBO
    vbo.Unbind();

}

void VAO::Bind() {
    // Bind the Vertex Array Object
    glBindVertexArray(ID);
}

void VAO::Unbind() {
    // Unbind the Vertex Array Object
    glBindVertexArray(0);
}

void VAO::Delete() {
    // Delete the Vertex Array Object
    glDeleteVertexArrays(1, &ID);
}

