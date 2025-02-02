# Unreal-iFFT-Ocean-Simulation

This repository is Part 2 of an Unreal project oublished to my Artstation: https://www.artstation.com/artwork/vbYqaE

Included here are all the C++ scripts used for generating inverse Fast Fourier Transform ocean waves in real-time for the Unreal Engine 5.3 project.

The scripts also ensure adpative tessellation based on the camera's distance to the waves, as well as both static and movable run-time buoyancy. 
All bouyancy-related code can be found in MyBuoyancy.h and MyBuoyancy.cpp.

Attached is also a batch file that automates rebuilding from source of the Unreal C++ project, whenever double-clicked.

More optimization with multi-threading and a smoother transition between wave LODs are already ideas for future work.

<img width="1522" alt="Boat_above" src="https://github.com/user-attachments/assets/abd4d44f-46d6-4465-b662-99bdd5819497" />
