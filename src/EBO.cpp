#include <ebo.h>

EBO::EBO(GLuint* indices, GLsizeiptr size) {
    // Generate buffer and bind it
    glGenBuffers(1, &ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
    
    // Copy indices data into buffer
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}

void EBO::Bind() {
    // Bind the buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
}

void EBO::Unbind() {
    // Unbind the buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void EBO::Delete() {
    // Delete the buffer
    glDeleteBuffers(1, &ID);
}