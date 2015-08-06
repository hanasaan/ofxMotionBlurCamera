//
// Created by Yuya Hanai, https://github.com/hanasaan
//

// Based on the paper "A Reconstruction Filter for Plausible Motion Blur"
// http://graphics.cs.williams.edu/papers/MotionBlurI3D12/

#pragma once

#include "ofMain.h"

class ofxMotionBlurCamera : public ofCamera, public ofBaseDraws
{
public:
    ofxMotionBlurCamera();
    void setup(int width, int height, int internalformat = GL_RGBA, float k = 20);
    
    float getExposureTime() const { return exposure_time; }
    void setExposureTime(float e) { exposure_time = e; }

    int getS() const { return S; }
    void setS(int s) { S = s; }

    virtual void begin(ofRectangle viewport = ofGetCurrentViewport());
    
    virtual void end(); // auto draw is enabled by default
    void end(bool autodraw, bool update_prev = true);
    
    //explicitly draw current fbo.
    virtual void draw(float x, float y) { fbo_out.draw(x, y); }
    virtual void draw(float x, float y, float w, float h)  { fbo_out.draw(x, y, w, h); }
    virtual float getHeight() { return fbo_out.getHeight(); }
    virtual float getWidth() { return fbo_out.getWidth(); }

    ofFbo& getRawFbo() { return fbo_main; }
    ofFbo& getFbo() { return fbo_out; }
    
    void debugDraw();
protected:
    float exposure_time;
    int S;
    float K;
    
    ofFbo fbo_main;
    ofFbo fbo_velocity;
    ofFbo fbo_tile_max;
    ofFbo fbo_neighbor_max;
    ofFbo fbo_out;
    
    ofShader velocity_shader;
    ofShader tile_max_shader;
    ofShader neighbor_max_shader;
    ofShader reconstruction_shader;
    
    ofMatrix4x4 prev_pmv;
};

