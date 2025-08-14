# Double Pendulum Simulation (OpenGL + ImGui)

A real-time, interactive **double pendulum** simulation built with **C++**, **OpenGL**, **GLFW**, **GLAD**, **GLM**, and **Dear ImGui**.  
The simulation allows you to adjust physical parameters, pause/resume motion, and visualize chaotic dynamics in real time.

<p align="center">
  <img src="img/demo.gif" alt="Short double pendulum demo" style="max-width: 100%; height: auto;" width="500"/>
</p>

The double pendulum motion is derived from deterministic equations: nothing is truly *random* — the motion appears unpredictable because the system is chaotic.  
Small differences in initial conditions grow exponentially over time.

---

## Video Demo

<p align="center">
  <a href="https://youtu.be/3Q9UUI3JO5o">
    <img src="img/window.png" alt="Watch the demo" style="max-width: 100%; height: auto;" width="500"/>
  </a>
</p>

*Click the image above to watch the simulation in action.*

---

## Features

- Accurate double pendulum physics with adjustable parameters (rod lengths, masses, damping, initial angles).
- Interactive Dear ImGui controls (pause, resume, reset, direct angle input).
- Smooth OpenGL rendering with MSAA.
- Parameter constraints to prevent instability.

---

## Screenshots

<p align="center">
  <table>
    <tr>
      <td align="center"><b>Control Panel</b></td>
      <td align="center"><b>Simulation</b></td>
    </tr>
    <tr>
      <td align="center"><img src="img/controls.png" alt="Controls Screenshot" style="max-width: 100%; height: auto;" width="350"/></td>
      <td align="center"><img src="img/window2.png" alt="Simulation Screenshot" style="max-width: 100%; height: auto;" width="350"/></td>
    </tr>
  </table>
</p>

---

## Dependencies

- GLFW
- GLAD
- GLM
- Dear ImGui
- C++17 or higher

---

## Building & Running

1. Clone the repository:
   ```bash
   git clone https://github.com/keanswon/doublependulum.git
   cd double-pendulum-opengl
