TwinCity3D
A C++ real-time 3D graphics project focused on rendering, graphics programming, and understanding the systems behind interactive 3D scenes.






TwinCity3D is a graphics-programming project built in C++ to explore real-time 3D rendering and the underlying systems involved in creating an interactive graphical scene.

Overview

TwinCity3D is part of my ongoing exploration of computer graphics, C++, OpenGL, GLSL, and real-time rendering.

The project is less about using a high-level engine and more about understanding what happens underneath the abstractions that make interactive 3D graphics possible.

The goal is to progressively build an understanding of the graphics pipeline by working directly with the technologies responsible for turning scene data into pixels.

The core idea
Scene Data
    ↓
CPU / Application
    ↓
OpenGL Rendering Pipeline
    ↓
GPU
    ↓
GLSL Shaders
    ↓
Rendered Image


TwinCity3D serves as a practical environment for experimenting with these concepts while developing stronger foundations in graphics programming.

Demo

Add your best screenshot or short video here.

<!-- Recommended: - Use one high-quality screenshot as the primary image. - Add a short GIF/video showing the application running. - Prefer actual renders over decorative screenshots. -->
What I'm Exploring

TwinCity3D focuses on the practical side of real-time graphics programming.

Areas explored in the project include:

C++ application development
Real-time 3D rendering
OpenGL
GLSL shader programming
3D scene construction
Graphics-pipeline concepts
CPU/GPU interaction
Rendering architecture
Interactive graphics
Mathematical foundations of 3D graphics

The project is intentionally being developed as a learning and experimentation platform, with the long-term goal of moving toward more advanced rendering techniques and graphics-engine architecture.

Features

The exact feature list should evolve with the renderer.

Currently, the project is structured around:

C++-based graphics application
OpenGL rendering
GLSL shader programs
Interactive 3D scene
Real-time rendering
Visual experimentation
Modular project structure
Planned / Experimental

Some areas I plan to explore further include:

Advanced lighting
More sophisticated material systems
Shadow techniques
Post-processing
Rendering optimization
Ray tracing
GPU-oriented programming
More modular renderer architecture

Features should only be added to the completed list once they are actually implemented.

Rendering Pipeline

One of the main reasons for building TwinCity3D is to understand the path from application-level scene data to the final image.

At a high level:

┌──────────────────────┐
│     Scene / Input    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│    C++ Application   │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│       OpenGL         │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│    Vertex Shader     │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│    Rasterization     │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│   Fragment Shader    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│      Framebuffer     │
└──────────────────────┘


The exact stages and resources used by the project depend on the current renderer implementation.

Technical Architecture

The repository is organized around a Visual Studio C++ project:

TwinCity3D/
│
├── TwinCity3D/
│   └── Application / renderer source
│
├── include/
│   └── Header files and dependencies
│
├── lib-vc2022/
│   └── Visual Studio library dependencies
│
├── .github/
│   └── GitHub configuration
│
├── TwinCity3D.slnx
└── README.md


The project currently targets a Windows + Visual Studio development environment.

As the renderer grows, the architecture will continue moving toward clearer separation between:

Application
    │
    ├── Input
    │
    ├── Scene
    │
    └── Rendering
            │
            ├── GPU Resources
            ├── Shaders
            ├── Geometry
            └── Render State

Technology
Core
C++ — primary programming language
OpenGL — graphics API
GLSL — GPU shader programming
Visual Studio — development environment
Graphics Concepts
Real-time rendering
3D transformations
Coordinate systems
GPU rendering pipeline
Shader programming
Scene representation
Rasterization
Development
Git
Visual Studio
C++ project/solution workflow

Update this list whenever a new library or graphics subsystem becomes part of the actual implementation.

Why I Built This

I am interested in graphics programming because it sits at the intersection of:

Mathematics
Computer architecture
Systems programming
Algorithms
Hardware
Visual computing

Rather than treating graphics APIs as black boxes, I want to understand what happens underneath them.

TwinCity3D is one step in that process.

The project gives me a practical environment for moving from concepts such as:

Vertices
   ↓
Matrices
   ↓
Shaders
   ↓
GPU
   ↓
Pixels


toward a deeper understanding of:

Rendering
   ↓
GPU Architecture
   ↓
Rendering Systems
   ↓
Graphics Engines

What I Built

This section documents the parts of TwinCity3D that I implemented myself.

Application

Describe the application/window/input layer here.

Implemented:

[Add actual implementation]
[Add actual implementation]
[Add actual implementation]
Rendering

Describe the renderer and rendering flow.

Implemented:

[Add actual rendering features]
[Add actual rendering features]
[Add actual rendering features]
Shaders

Describe the GLSL programs and shader pipeline.

Implemented:

[Add actual shader stages]
[Add actual uniforms/resources]
[Add actual shader techniques]
Scene

Describe how the 3D scene is represented and rendered.

Implemented:

[Add actual scene functionality]
[Add actual object/model functionality]
[Add actual transformation functionality]
Technical Challenges

Building graphics software introduces problems that are often hidden when using a high-level engine.

Some of the challenges I am working through include:

Coordinate Systems

Understanding how objects move through different coordinate spaces:

Model Space
     ↓
World Space
     ↓
View Space
     ↓
Clip Space
     ↓
Screen Space

CPU / GPU Responsibilities

Understanding which work belongs on the CPU and which work should be performed by the GPU.

Shader Debugging

Graphics bugs often do not behave like ordinary application bugs.

A small mistake in a transformation, buffer, uniform, or shader can result in:

Incorrect geometry
Black output
Incorrect colors
Flickering
Unexpected depth
Nothing being rendered

Working through these problems has been an important part of learning graphics programming.

Lessons Learned

TwinCity3D has helped me develop a stronger understanding of the relationship between software and graphics hardware.

Some of the most important lessons so far:

Rendering APIs expose a pipeline rather than a single drawing operation.
3D graphics depend heavily on linear algebra and coordinate transformations.
GPU programming requires thinking carefully about data flow.
Small state/configuration mistakes can have large visual consequences.
Graphics debugging requires both software and mathematical reasoning.
Good renderer architecture becomes increasingly important as features grow.

The project is still evolving, so this README will evolve with it.

Screenshots
Main Scene

Rendering

Shader / Graphics Experiment

<!-- Replace the paths above with actual screenshots. Recommended documentation strategy: 1. One hero screenshot 2. One close-up rendering screenshot 3. One technical/debug screenshot 4. Optional short video/GIF -->
Building
Requirements

Currently intended for:

Windows
Visual Studio
C++ compiler compatible with the project
OpenGL-capable GPU

The repository includes the Visual Studio solution:

TwinCity3D.slnx

Clone
git clone https://github.com/UsmanJ-Graphics/TwinCity3D.git
cd TwinCity3D

Build

Open:

TwinCity3D.slnx


in Visual Studio.

Select the appropriate configuration and platform, then build the solution.

If the project requires additional dependencies or configuration, document them here as the project evolves.

Project Structure
TwinCity3D/
│
├── .github/
│
├── TwinCity3D/
│   ├── source/
│   ├── shaders/
│   └── ...
│
├── include/
│
├── lib-vc2022/
│
├── .gitignore
│
├── TwinCity3D.slnx
└── README.md


The exact source layout may change as the renderer becomes more modular.

Development Roadmap

TwinCity3D is an evolving graphics project.

Current
C++
OpenGL
GLSL
Real-time 3D rendering
Graphics pipeline fundamentals
Next
More advanced rendering techniques
GPU-oriented programming
Shader experimentation
Rendering optimization
Ray tracing
Future
More complete renderer architecture
Advanced real-time rendering
GPU programming
Graphics-engine systems
Performance-oriented rendering

The objective is not simply to add features, but to understand the underlying systems well enough to eventually build more sophisticated graphics software.

Project Philosophy

Build graphics from first principles.

I am using TwinCity3D as a way to move beyond simply using graphics software toward understanding how graphics systems actually work.

The long-term progression I'm working toward is:

C++
  ↓
OpenGL
  ↓
GLSL
  ↓
Rendering
  ↓
GPU Programming
  ↓
Ray Tracing
  ↓
Rendering Systems
  ↓
Graphics Engine Architecture

Future Work

Potential areas for future development include:

 Improve renderer architecture
 Add more advanced lighting
 Explore shadow techniques
 Experiment with post-processing
 Improve shader organization
 Investigate rendering performance
 Explore GPU programming
 Implement ray-tracing experiments
 Improve documentation and debugging tools

This roadmap will change as the project develops.

Project Status

Status: Active Development

TwinCity3D is primarily a personal graphics-programming project and learning environment.

The project is expected to evolve as I learn more about:

Real-time rendering
GPU architecture
Shader programming
Rendering optimization
Graphics-engine design
Author
Muhammad Usman Javed

Computer Science student focused on:

C++ · Graphics Programming · OpenGL · GLSL · Real-Time Rendering

I am currently building toward deeper work in:

GPU Programming · Ray Tracing · Rendering Systems · Graphics Engine Architecture

Links
GitHub: https://github.com/UsmanJ-Graphics
TwinCity3D: https://github.com/UsmanJ-Graphics/TwinCity3D
Portfolio: https://portfolio-ashy-six-1be88igzjw.vercel.app/
License

Add the project's license information here.

If the project is intended to be open source, consider adding an appropriate LICENSE file to the repository.

<p align="center"> <strong>TwinCity3D</strong><br> Building graphics from first principles. </p>
