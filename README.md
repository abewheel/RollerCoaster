# Roller Coaster


<p align="center">

  <img src="https://github.com/abewheel/RollerCoaster/blob/master/coaster.gif" alt="Coaster animation"/>

</p>



## Description


Roller Coaster is a Visual Studio C++ program that accepts a list of control matrices as a text file and creates a 3D roller coaster ride animation out of the equivalent Catmull-Rom splines.



## About the Project


This repository contains the Visual Studio 2017 project code for Roller Coaster. I wrote Roller Coaster in 2017 as a part of my Computer Graphics course with Dr. Hao Li, CSCI420.



## Technical Details


Roller Coaster was implemented in Visual Studio 2017 using C++ and OpenGL. It accepts a list of control matices to define each of the spline segments, uses an s value of 1/2, and interpolates the u parameter between 0.0 and 1.0 in increments of 0.01 for each spline segment. It calculates the spline tangents, normals, and binormals at each point for a realistic roller coaster ride simulation. Roller Coaster utilizes OpenGL's texture mapping functionality for the ground and sky box