
# ofxMotionBlurCamera
OF addon implements "A Reconstruction Filter for Plausible Motion Blur"
http://graphics.cs.williams.edu/papers/MotionBlurI3D12/

![ofxMotionBlurCamera](https://raw.githubusercontent.com/hanasaan/ofxMotionBlurCamera/master/example/bin/data/motionblur.jpg)

## Description
This addon provides simple and easy-to-use motion blur camera.
The algorithm itself can be expanded to arbitrary obejcts, but it requires to override object draw process.
So this addons limits blur targets to camera so that the scene can be drawn in a normal way. 
