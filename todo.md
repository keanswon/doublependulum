Don’t query the viewport every mouse move. Cache it on resize and reuse.
In your framebuffer_size_callback, you already have the size—just store it and set the viewport there (you’re doing that). Then in the cursor callback, remove the glGetIntegerv(GL_VIEWPORT, ...) and compute NDC using the cached winWidth/winHeight (in framebuffer units):

cpp
Copy
Edit
// cursor callback
float sx, sy;
glfwGetWindowContentScale(w, &sx, &sy);
double x_fb = xpos * sx, y_fb = ypos * sy;

mouseX_ndc =  2.0f * float(x_fb) / float(winWidth)  - 1.0f;
mouseY_ndc = -2.0f * float(y_fb) / float(winHeight) + 1.0f;