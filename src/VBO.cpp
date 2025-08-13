#include <vbo.h>

VBO::VBO(GLfloat* vertices, GLsizeiptr size) {
    // Generate buffer and bind it
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ARRAY_BUFFER, ID);
    
    // Copy vertices data into buffer
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void VBO::Bind() {
    // Bind the buffer
    glBindBuffer(GL_ARRAY_BUFFER, ID);
}

void VBO::Unbind() {
    // Unbind the buffer
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VBO::Delete() {
    // Delete the buffer
    glDeleteBuffers(1, &ID);
}